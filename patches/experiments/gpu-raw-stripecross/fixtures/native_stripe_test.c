#include "native_stripe.h"
#include "gather_pipe.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#define RAM_BYTES 0x02000000u
__declspec(align(4096)) u8 bw_guest_mem1[RAM_BYTES];

void bluewake_composite_set_gather_pipe(BwGatherPipeWrite);
void bluewake_composite_set_gather_pipe_bytes(BwGatherPipeBytes);
void bluewake_stripe_oracle_first(CPUState*);
void bluewake_stripe_oracle_second(CPUState*);

static u32 seed = 0x8A715E21u;
static u32 random_word(void) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
static void need(bool ok, const char* message) { if (!ok) { fprintf(stderr,"FAIL %s seed=%08X\n",message,seed); exit(1); } }
static u8 logged[512];
static unsigned logged_count, word_calls, bytes_calls, ready_calls;
static bool ready_allow = true;
static void log_bytes(const u8* data, u32 size) {
    need(logged_count + size <= sizeof logged, "byte log capacity");
    memcpy(logged + logged_count, data, size); logged_count += size; bytes_calls++;
}
static void log_word(u64 value, u8 size) {
    need(size == 4u && logged_count + size <= sizeof logged,"word log shape");
    write_be32(logged + logged_count,(u32)value); logged_count += size; word_calls++;
}
static bool ready(void* user, const CPUState* cpu, u32 address) {
    need(user == (void*)(uintptr_t)0xABCDEFu,"capability user");
    need(cpu != NULL,"capability CPU");
    need(address == BLUEWAKE_GPU_STRIPE_FIRST || address == BLUEWAKE_GPU_STRIPE_SECOND,"exact observer range");
    ready_calls++;
    return ready_allow;
}
static void journal(u32 offset, u32 size, void* user) { (void)offset;(void)size;(void)user; abort(); }
static void reset_log(void) { memset(logged,0xCD,sizeof logged); logged_count=word_calls=bytes_calls=ready_calls=0; }
static void pipe(bool batch, u32 prefill) {
    bw_gather_pipe_length=0;
    bluewake_composite_set_gather_pipe(log_word);
    bluewake_composite_set_gather_pipe_bytes(batch ? log_bytes : NULL);
    for (unsigned i=0;i<sizeof bw_gather_pipe_buffer;i++) bw_gather_pipe_buffer[i]=(u8)(i*37u+11u);
    bw_gather_pipe_length=prefill;
    reset_log();
}
static CPUState input(u8* ram, unsigned which, unsigned index) {
    CPUState cpu; memset(&cpu,0,sizeof cpu);
    cpu.ram=ram; cpu.ram_size=RAM_BYTES;
    for (unsigned i=0;i<32;i++) {
        cpu.gpr[i]=random_word();
        u64 bits=((u64)random_word()<<32)|random_word();
        switch(index%13u) {
        case 0: bits=convert_to_double(random_word()); break;
        case 1: bits=(bits&0x800FFFFFFFFFFFFFull)|0x3FF0000000000000ull; break;
        case 2: bits=(i&1u)?0x8000000000000000ull:0; break;
        case 3: bits=(i&1u)?0xFFF0000000000000ull:0x7FF0000000000000ull; break;
        case 4: bits=0x7FF0000000000001ull+(i<<29); break;
        case 5: bits=0x7FF8000000000001ull+(i<<29); break;
        case 6: bits=(i&1u)?0x0000000000000001ull:0x8000000000000001ull; break;
        default: break;
        }
        cpu.fpr[i]=f64_value(bits);
        cpu.ps1[i]=f64_value(((u64)random_word()<<32)|random_word());
    }
    cpu.gpr[2]=GC_RAM_BASE+0x100000u+15972u;
    cpu.gpr[which==0?28:29]=0xCC010000u;
    cpu.pc=which==0?0x80263E78u:0x8026425Cu;
    cpu.cycle_observation_suffix=which==0?53u:52u;
    cpu.msr=random_word()|PPC_MSR_FP; cpu.fpscr=random_word();
    cpu.lr=random_word();cpu.ctr=random_word();cpu.cr=random_word();cpu.xer=random_word();cpu.hid2=random_word();
    for (unsigned g=0;g<8;g++)cpu.gqr[g]=random_word();
    cpu.reserve_valid=(index&1u)!=0;cpu.reserve_addr=random_word();
    cpu.cycle_budget=16384;cpu.downcount=-(s64)(100u+random_word()%500u);
    cpu.cycle_deadline_budget=(index&1u)?0:100000;cpu.cycle_deadline_active=random_word();
    return cpu;
}
static u64 memory_hash(const u8* ram) {
    u64 h=1469598103934665603ull;
    for(unsigned i=0;i<RAM_BYTES;i++)h=(h^ram[i])*1099511628211ull;
    return h;
}
static void positive(u8* ram, unsigned which, bool batch, unsigned cases) {
    for (unsigned i=0;i<cases;i++) {
        CPUState a=input(ram,which,i), b=a;
        const u32 prefill=batch?(i%216u):0;
        pipe(batch,prefill);
        if(which==0)bluewake_stripe_oracle_first(&a);else bluewake_stripe_oracle_second(&a);
        u8 expected_buffer[sizeof bw_gather_pipe_buffer],expected_log[sizeof logged];
        memcpy(expected_buffer,bw_gather_pipe_buffer,sizeof expected_buffer);
        memcpy(expected_log,logged,sizeof expected_log);
        const unsigned expected_count=logged_count;
        need(batch?word_calls==0&&bytes_calls==0:word_calls==10&&bytes_calls==0,"literal oracle actual store shape");
        const u32 expected_length=bw_gather_pipe_length;
        pipe(batch,prefill);
        need(bluewake_native_stripe_tail(&b,which==0?BLUEWAKE_GPU_STRIPE_FIRST:BLUEWAKE_GPU_STRIPE_SECOND)==1,"admit valid exact tail");
        if(memcmp(&a,&b,sizeof a)!=0) {
            const u8* x=(const u8*)&a;const u8* y=(const u8*)&b;
            for(unsigned n=0;n<sizeof a;n++)if(x[n]!=y[n]) {fprintf(stderr,"CPU offset=%u a=%02X b=%02X which=%u case=%u\n",n,x[n],y[n],which,i);break;}
            need(false,"complete CPUState incl PS1 FPSCR suffix/downcount");
        }
        need(memcmp(expected_buffer,bw_gather_pipe_buffer,sizeof expected_buffer)==0,"entire gather buffer including untouched bytes");
        need(expected_length==bw_gather_pipe_length,"gather length");
        need(expected_count==logged_count&&memcmp(expected_log,logged,sizeof expected_log)==0,"byte-exact FIFO stream");
        need(ready_calls==1,"exact observer consulted");
        need(batch?word_calls==0&&bytes_calls==0:word_calls==0&&bytes_calls==1,"single direct bulk callback or no batch callback");
    }
}
static unsigned negatives(u8* ram,unsigned which) {
    unsigned count=0;
    for(unsigned scenario=0;scenario<27;scenario++) {
        CPUState cpu=input(ram,which,scenario),before;
        u32 address=which==0?BLUEWAKE_GPU_STRIPE_FIRST:BLUEWAKE_GPU_STRIPE_SECOND;
        pipe(false,0); ready_allow=true;
        bluewake_composite_gpu_stripe_tails_v1(true,ready,log_bytes,(void*)(uintptr_t)0xABCDEFu);
        u8 alias[32];memset(alias,0x5A,sizeof alias);
        bool null_cpu=false;
        switch(scenario) {
        case 0:bluewake_composite_gpu_stripe_tails_v1(false,ready,log_bytes,NULL);break;
        case 1:bluewake_composite_gpu_stripe_tails_v1(true,NULL,log_bytes,NULL);break;
        case 2:bluewake_composite_gpu_stripe_tails_v1(true,ready,NULL,NULL);break;
        case 3:ready_allow=false;break;
        case 4:bw_gather_pipe_write=NULL;break;
        case 5:bw_gather_pipe_length=1;break;
        case 6:pipe(true,216);break;
        case 7:pipe(true,255);break;
        case 8:null_cpu=true;break;
        case 9:address++;break;
        case 10:cpu.pc++;break;
        case 11:cpu.cycle_observation_suffix++;break;
        case 12:cpu.msr&=~PPC_MSR_FP;break;
        case 13:cpu.exception=1;break;
        case 14:g_mem_write_journal=journal;break;
        case 15:cpu.gpr[2]=0xCC000000u+15972u;break;
        case 16:cpu.gpr[2]++;break;
        case 17:cpu.gpr[2]=GC_RAM_BASE+RAM_BYTES-4u+15972u;break;
        case 18:g_ppc_guest_aliases_overlap_mem1=true;break;
        case 19:cpu.gpr[which==0?28:29]=0xCC010004u;break;
        case 20:cpu.cycle_deadline_budget=cpu.cycle_observation_suffix-1u;break;
        case 21:cpu.cycle_budget=0;break;
        case 22:cpu.downcount=1;break;
        case 23:cpu.ram=NULL;break;
        case 24:need(ppc_guest_alias_add_shared(0xCC008000u,sizeof alias,alias),"register FIFO alias");break;
        case 25:cpu.gpr[2]|=0x40000000u;break;
        case 26:pipe(true,0xFFFFFFF0u);break;
        }
        before=cpu;
        const u32 length=bw_gather_pipe_length;
        u8 buffer[sizeof bw_gather_pipe_buffer];memcpy(buffer,bw_gather_pipe_buffer,sizeof buffer);
        need(bluewake_native_stripe_tail(null_cpu?NULL:&cpu,address)==0,"reject guarded case");
        need(memcmp(&before,&cpu,sizeof cpu)==0,"decline leaves complete CPUState");
        need(length==bw_gather_pipe_length&&memcmp(buffer,bw_gather_pipe_buffer,sizeof buffer)==0,"decline leaves full gather state");
        need(logged_count==0&&word_calls==0&&bytes_calls==0,"decline has no callbacks");
        for(unsigned n=0;n<sizeof alias;n++)need(alias[n]==0x5A,"FIFO alias unchanged");
        g_mem_write_journal=NULL;g_ppc_guest_aliases_overlap_mem1=false;ppc_guest_alias_clear();
        bw_gather_pipe_length=0;count++;
    }
    ready_allow=true;
    bluewake_composite_gpu_stripe_tails_v1(true,ready,log_bytes,(void*)(uintptr_t)0xABCDEFu);
    return count;
}
int main(int argc,char** argv) {
    unsigned cases=argc>1?(unsigned)strtoul(argv[1],NULL,10):20000u;
    need(cases>0&&cases<=1000000u,"bounded cases");
    u8* ram=bw_guest_mem1;
    for(unsigned i=0;i<RAM_BYTES;i++)ram[i]=(u8)(i*31u+7u);
    write_be32(ram+0x100000u,0);write_be32(ram+0x100004u,0x3F800000u);
    DWORD old;need(VirtualProtect(ram,RAM_BYTES,PAGE_READONLY,&old)!=0,"all guest RAM read-only catches stray writes");
    const u64 before_hash=memory_hash(ram);
    bluewake_composite_gpu_stripe_tails_v1(true,ready,log_bytes,(void*)(uintptr_t)0xABCDEFu);
    unsigned declined=0;
    for(unsigned which=0;which<2;which++) {
        positive(ram,which,false,cases);
        positive(ram,which,true,cases);
        declined+=negatives(ram,which);
    }
    need(memory_hash(ram)==before_hash,"all guest RAM bytes unchanged");
    printf("PASS exact frozen prepaid StripeCross tails accepted=%u declined=%u direct_bulk=one batch_flush=none RAM=%u suffix=5/4 cycles=unchanged\n",cases*4u,declined,RAM_BYTES);
    bluewake_gpu_stripe_tails_report();
    bw_gather_pipe_length=0;need(VirtualProtect(ram,RAM_BYTES,PAGE_READWRITE,&old)!=0,"restore fixture RAM protection");return 0;
}
