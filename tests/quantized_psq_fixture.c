/* SPDX-License-Identifier: GPL-3.0-or-later
 * ROM-free full-runtime PSQ differential fixture. Run with
 * tests/test_quantized_psq_runtime.py; no game/module execution is involved. */
#include "core/cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
#include <immintrin.h>
#ifdef BW_PSQ_GATHER_PROFILE
#include "gather_pipe.h"
BwGatherPipeWrite bw_gather_pipe_write = NULL;
BwGatherPipeBytes bw_gather_pipe_bytes = NULL;
u8 bw_gather_pipe_buffer[BW_GATHER_PIPE_BATCH + 8u];
u32 bw_gather_pipe_length;
void bw_gather_pipe_flush(void){fprintf(stderr,"unexpected gather drain/write\n");exit(9);}
#endif
#include "quantized_psq_helpers.h"

bool oracle_psq_load(CPUState*,u8,u32,bool,u8,bool,u32);
bool oracle_psq_store(CPUState*,u8,u32,bool,u8,bool,u32);
void ppc_set_mem_write_journal(PPCMemWriteJournal,void*);

/* Match the real module's BW_GUEST_MEM1 definition and 32 MiB bound. */
u8 bw_guest_mem1[0x02000000u];
static u8 exram[64],aliasram[64];
static CPUState cpu;
typedef struct {u32 kind,ea,width;u64 value;CPUState state;} Event;
typedef struct {CPUState cpu;Event events[4];u8 head[128],tail[128],exram[64],aliasram[64];u32 n;bool ok;} Result;
static Event events[4];
static u32 event_count,mutation; static u8 fr,gqridx;
static int forced_gqr=-1;
static u32 gqr_coverage;
static u64 inputs[2];
static u64 checks,cases,digest=1469598103934665603ull;

static void hash(const void*p,size_t n){const u8*b=p;for(size_t i=0;i<n;i++)digest=(digest^b[i])*1099511628211ull;}
static void record(u32 kind,u32 ea,u32 width,u64 value){
 if(event_count>=4){fprintf(stderr,"trace overflow\n");exit(2);}
 Event* e=&events[event_count++];e->kind=kind;e->ea=ea;e->width=width;e->value=value;memcpy(&e->state,&cpu,sizeof(cpu));
 /* Callback mutations deliberately exercise architectural sequencing. */
 if(mutation&1){
  cpu.fpr[fr]=f64_value(0x405EC00000000000ull+event_count);
  cpu.ps1[fr]=event_count==1 ? -233.75 : 777.25;
  cpu.gqr[gqridx&7]=event_count==1 ? 0x3F070106u : 0x20040005u;
  cpu.gpr[17]^=ea+width+event_count;cpu.hid2^=PPC_HID2_LSQE|PPC_HID2_PSE;
  cpu.pc+=0x14;cpu.fpscr^=0x00080000;cpu.downcount-=31;cpu.cycle_observation_suffix+=7;
 }
 if((mutation&2)&&event_count==1)ppc_program_exception(&cpu,PPC_PROGRAM_TRAP,0x80004444u);
 if((mutation&4)&&event_count==2)ppc_take_exception(&cpu,PPC_EXC_DSI,PPC_VECTOR_DSI,0x80005555u,0);
}
static u64 rd(CPUState*c,u32 ea,u8 size){(void)c;u64 v=inputs[event_count&1];record(1,ea,size,v);return v;}
static void wr(CPUState*c,u32 ea,u64 value,u8 size){(void)c;record(2,ea,size,value);}
static void journal(u32 offset,u32 size,void*user){(void)user;record(3,offset,size,0);}
static void* pointer_trap(CPUState*c,u32 ea,u32 size){(void)c;(void)ea;(void)size;fprintf(stderr,"unexpected unused external_pointer call\n");exit(3);}
static const u64 values[]={
 0x0000000000000000ull,0x8000000000000000ull,0x3FF0000000000000ull,0xBFF0000000000000ull,
 0x0000000000000001ull,0x8000000000000001ull,0x0010000000000000ull,0x8010000000000000ull,
 0x7FEFFFFFFFFFFFFFull,0xFFEFFFFFFFFFFFFFull,0x7FF0000000000000ull,0xFFF0000000000000ull,
 0x7FF8000000000001ull,0xFFF8000000004321ull,0x7FF0000000000001ull,0xFFF000000000007Full,
 0x406FDFFFFFFFFFFFull,0x406FE00000000000ull,0x406FE00000000001ull,0x4070000000000000ull,
 0x40EFFFDFFFFFFFFFull,0x40EFFFE000000000ull,0x40EFFFE000000001ull,0x40F0000000000000ull,
 0xC060000000000000ull,0x405FC00000000000ull,0xC0E0000000000000ull,0x40DFFFC000000000ull,
 0x3FEFFFFFFFFFFFFFull,0x3FF0000000000001ull,0xBFEFFFFFFFFFFFFFull,0xBFF0000000000001ull,
 0x380FFFFFC0000000ull,0x3810000000000000ull,0x47EFFFFFE0000000ull,0x47F0000000000000ull
};
static const u32 words[]={0,0x80000000,1,0x80000001,0x007FFFFF,0x00800000,0x3F800000,0xBF800000,
 0x7F7FFFFF,0xFF7FFFFF,0x7F800000,0xFF800000,0x7FC00001,0xFFC04321,0x7F800001,0xFF80007F,
 0xFFFF,0x8000,0xFF,0x80,0x7F,0x7FFF,0xFFFFFFFF,0xAAAAAAAA};
static u32 width(u32 t){return t==0?4:(t==4||t==6)?1:(t==5||t==7)?2:0;}
static void be32(u8*p,u32 v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static void reset(u32 type,s32 scale,bool w,bool indexed,bool enabled,u32 backend,u32 mut,u32 pat,bool store){
 (void)w;(void)indexed;
#ifdef BW_PSQ_GATHER_PROFILE
 if(bw_gather_pipe_write!=NULL||bw_gather_pipe_bytes!=NULL||bw_gather_pipe_length!=0u){fprintf(stderr,"gather trace guard\n");exit(10);}
#endif
 ppc_guest_alias_clear();ppc_set_mem_write_journal(NULL,NULL);
 memset(&cpu,0,sizeof(cpu));memset(events,0,sizeof(events));event_count=0;mutation=mut;
 memset(bw_guest_mem1,0xA5,128);memset(bw_guest_mem1+sizeof(bw_guest_mem1)-128,0x5A,128);
 memset(exram,0xC3,sizeof(exram));memset(aliasram,0x3C,sizeof(aliasram));
 fr=(u8)((pat+type+scale)&31);gqridx=(u8)((pat+scale)&15);
 if(forced_gqr>=0)gqridx=(u8)forced_gqr;
 cpu.ram=bw_guest_mem1;cpu.ram_size=sizeof(bw_guest_mem1);cpu.exram=exram;cpu.exram_size=sizeof(exram);
 cpu.external_read=rd;cpu.external_write=wr;cpu.external_pointer=pointer_trap;
 cpu.pc=0x80001000;cpu.msr=0x0000F073;cpu.lr=0x80009900;cpu.ctr=7;cpu.cr=0xABCDEF01;
 cpu.hid2=(enabled?PPC_HID2_LSQE:0)|((pat&1)?PPC_HID2_PSE:0);
 cpu.fpscr=pat&3;cpu.gpr[17]=0x76543210;cpu.downcount=-123;cpu.cycle_budget=400;
 for(u32 i=0;i<32;i++){cpu.fpr[i]=i+0.25;cpu.ps1[i]=-((f64)i)-0.75;cpu.gpr[i]^=i*0x10001;}
 cpu.fpr[fr]=f64_value(values[pat%(sizeof(values)/sizeof(*values))]);
 cpu.ps1[fr]=f64_value(values[(pat*7+9)%(sizeof(values)/sizeof(*values))]);
 u32 g=(type|((u32)(scale&63)<<8)|(type<<16)|((u32)(scale&63)<<24));
 for(u32 i=0;i<8;i++)cpu.gqr[i]=0x11001100u+i;
 cpu.gqr[gqridx&7]=g;
 inputs[0]=words[pat%(sizeof(words)/sizeof(*words))];inputs[1]=words[(pat*7+3)%(sizeof(words)/sizeof(*words))];
 for(u32 i=0;i<120;i+=4){be32(bw_guest_mem1+i,(u32)inputs[(i/4)&1]);be32(bw_guest_mem1+sizeof(bw_guest_mem1)-128+i,(u32)inputs[(i/4)&1]);}
 for(u32 i=0;i<56;i+=4){be32(exram+i,(u32)inputs[(i/4)&1]);be32(aliasram+i,(u32)inputs[(i/4)&1]);}
 if(backend==4){cpu.external_read=NULL;cpu.external_write=NULL;}
 if(backend==5){if(!ppc_guest_alias_add_shared(0x80000020u,sizeof(aliasram),aliasram))exit(4);}
 if(backend==6&&store)ppc_set_mem_write_journal(journal,NULL);
 cpu.reserve_valid=true;cpu.reserve_addr=0x80000020u;
}
static u32 address(u32 backend,u32 type){
 switch(backend){case 0:return 0x80000023u;case 1:return 0xCC000003u;case 2:return 0xC0000023u;
 case 3:return 0x90000003u;case 4:return 0xFFFFFFFEu;case 5:return 0x80000023u;
 case 6:return 0x80000023u;case 7:return 0x82000000u-width(type);default:return 0xCC000003u;}
}
static bool bad_store(CPUState*c,u8 f,u32 ea,bool w,u8 g,bool ix,u32 cia){
 f64 old=c->ps1[f];u32 q=c->gqr[g&7];u8 t=q&7;f32 fac=bw_composite_psq_power2(bw_composite_psq_scale(q>>8));
 if(!ix&&!(c->hid2&PPC_HID2_LSQE))return ppc_psq_store(c,f,ea,w,g,ix,cia);
 if(t<4)return ppc_psq_store(c,f,ea,w,g,ix,cia);
 bw_composite_psq_store_value(c,ea,t,fac,c->fpr[f]);if(!w)bw_composite_psq_store_value(c,ea+width(t),t,fac,old);return true;
}
static bool bad_load(CPUState*c,u8 f,u32 ea,bool w,u8 g,bool ix,u32 cia){
 u32 q=c->gqr[g&7];u8 t=(q>>16)&7;f32 fac=bw_composite_psq_power2(-bw_composite_psq_scale(q>>24));
 if((!ix&&!(c->hid2&PPC_HID2_LSQE))||t<4)return ppc_psq_load(c,f,ea,w,g,ix,cia);
 f64 a=bw_composite_psq_load_value(c,ea,t,fac);f64 b=w?1.:bw_composite_psq_load_value(c,ea+width(t),t,fac);
 c->fpr[f]=a;c->ps1[f]=b;return true;
}
static void run(Result*r,u32 path,u32 type,s32 scale,bool w,bool indexed,bool enabled,u32 backend,u32 mut,u32 pat,bool store){
 reset(type,scale,w,indexed,enabled,backend,mut,pat,store);u32 ea=address(backend,type);u32 cia=0x80008888u;
 bool ok;
 if(path==0)ok=store?oracle_psq_store(&cpu,fr,ea,w,gqridx,indexed,cia):oracle_psq_load(&cpu,fr,ea,w,gqridx,indexed,cia);
 else if(path==1)ok=store?ppc_psq_store(&cpu,fr,ea,w,gqridx,indexed,cia):ppc_psq_load(&cpu,fr,ea,w,gqridx,indexed,cia);
 else if(path==2)ok=store?ppc_psq_store_inline(&cpu,fr,ea,w,gqridx,indexed,cia):ppc_psq_load_inline(&cpu,fr,ea,w,gqridx,indexed,cia);
 else ok=store?bad_store(&cpu,fr,ea,w,gqridx,indexed,cia):bad_load(&cpu,fr,ea,w,gqridx,indexed,cia);
 memset(r,0,sizeof(*r));memcpy(&r->cpu,&cpu,sizeof(cpu));memcpy(r->events,events,sizeof(events));
 memcpy(r->head,bw_guest_mem1,128);memcpy(r->tail,bw_guest_mem1+sizeof(bw_guest_mem1)-128,128);
 memcpy(r->exram,exram,sizeof(exram));memcpy(r->aliasram,aliasram,sizeof(aliasram));r->n=event_count;r->ok=ok;
}
static Result ref,test;
static void one(u32 type,s32 scale,bool w,bool indexed,bool enabled,u32 backend,u32 mut,u32 pat,bool store){
 run(&ref,0,type,scale,w,indexed,enabled,backend,mut,pat,store);
 for(u32 path=1;path<=2;path++){
  run(&test,path,type,scale,w,indexed,enabled,backend,mut,pat,store);checks++;
  if(memcmp(&ref,&test,sizeof(ref))){
   size_t diff=0;while(diff<sizeof(ref)&&((u8*)&ref)[diff]==((u8*)&test)[diff])diff++;
   fprintf(stderr,"FAIL path=%u type=%u scale=%d w=%d ix=%d lsqe=%d backend=%u mut=%u pattern=%u store=%d byte=%zu events=%u/%u\n",path,type,scale,w,indexed,enabled,backend,mut,pat,store,diff,ref.n,test.n);exit(1);
  }
 }
 /* Hash only architectural values, excluding process-address pointers. */
 hash(ref.cpu.fpr,sizeof(ref.cpu.fpr));hash(ref.cpu.ps1,sizeof(ref.cpu.ps1));hash(&ref.cpu.pc,sizeof(ref.cpu.pc));hash(ref.head,sizeof(ref.head));hash(ref.tail,sizeof(ref.tail));cases++;
 gqr_coverage|=1u<<(gqridx&7u);
}
int main(void){
 unsigned saved=_mm_getcsr();int saved_round=fegetround();u32 mutants=0;
 for(u32 store=0;store<2;store++)for(u32 type=0;type<8;type++)for(s32 scale=-32;scale<32;scale++)
 for(u32 w=0;w<2;w++)for(u32 ix=0;ix<2;ix++)for(u32 lsqe=0;lsqe<2;lsqe++)for(u32 backend=0;backend<8;backend++){
  u32 p=(u32)(scale+32)*17+type*7+store*3+backend;
  one(type,scale,w,ix,lsqe,backend,backend==1||backend==6?1:0,p,store);
 }
 /* Exhaust both sides of the GQR0..3 / GQR4..7 optimization gate.
  * Do not rely only on indices derived from the pattern/scale grid above. */
 for(int g=0;g<8;g++){
  forced_gqr=g;
  for(u32 st=0;st<2;st++)for(u32 t=0;t<8;t++)for(s32 s=-32;s<32;s++)
  for(u32 w=0;w<2;w++)for(u32 ix=0;ix<2;ix++)for(u32 lsqe=0;lsqe<2;lsqe++)
   one(t,s,w,ix,lsqe,1,1,(u32)(s+32)*17+t*7+st*3+(u32)g,st);
 }
 forced_gqr=-1;
 /* Every integer type/scale against all special, saturation and cast inputs. */
 for(u32 type=4;type<8;type++)for(s32 scale=-32;scale<32;scale++)for(u32 p=0;p<sizeof(values)/sizeof(*values);p++){
  one(type,scale,0,0,1,1,0,p,1);one(type,scale,0,1,0,1,1,p,1);
 }
 /* Real exception entry during each MMIO lane; canonical execution continues. */
 for(u32 st=0;st<2;st++)for(u32 t=0;t<8;t++)for(u32 mut=2;mut<8;mut++)for(u32 w=0;w<2;w++)
  one(t,(s32)(t*9%64)-32,w,1,0,1,mut,t+mut,st);
 /* Host rounding plus FTZ/DAZ controls, with guest FPSCR differing independently. */
 for(u32 rn=0;rn<4;rn++)for(u32 flush=0;flush<4;flush++){
  const int rounds[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
  if(fesetround(rounds[rn])){fprintf(stderr,"fenv rounding unsupported\n");return 6;}
  _mm_setcsr((saved&~(0x6000u|0x8040u))|(rn<<13)|((flush&1)?0x8000:0)|((flush&2)?0x40:0));
  for(u32 t=0;t<8;t++)for(s32 s=-32;s<32;s+=7)for(u32 p=0;p<sizeof(values)/sizeof(*values);p+=3)
   one(t,s,0,1,0,1,1,p,1);
 }
 if(fesetround(saved_round)){fprintf(stderr,"fenv restore unsupported\n");return 7;}_mm_setcsr(saved);
 for(u32 st=0;st<2;st++){
  run(&ref,0,6,0,0,1,1,1,1,2,st);run(&test,3,6,0,0,1,1,1,1,2,st);
  if(!memcmp(&ref,&test,sizeof(ref))){fprintf(stderr,"negative mutant not rejected %u\n",st);return 5;}mutants++;
 }
 ppc_guest_alias_clear();ppc_set_mem_write_journal(NULL,NULL);
 if(gqr_coverage!=0xFFu){fprintf(stderr,"incomplete GQR coverage\n");return 8;}
 printf("{\"status\":\"PASS\",\"cases\":%llu,\"exact_result_comparisons\":%llu,\"negative_mutants_rejected\":%u,\"gqr_index_mask\":%u,\"explicit_gqr_gate_cases\":65536,\"digest\":\"%016llx\",\"cpu_bytes\":%zu,\"trace_event_bytes\":%zu}\n",cases,checks,mutants,gqr_coverage,digest,sizeof(CPUState),sizeof(Event));
 return 0;
}
