/* Inactive differential experiment. This fixture uses the real
 * pinned CPU/gather memory services. It never substitutes a memory simulator.
 * The original, prepaid-copy and cold-twin functions share the same input,
 * complete CPU comparison, writable MEM1 bytes, aliases, MEM2 and observers. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "core/cpu.h"
#include "gather_pipe.h"
#include "cold_twin_memory.h"
#include <assert.h>
#include <fenv.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <xmmintrin.h>

#define MEM1_BYTES 0x02000000u
#define PAGE_BYTES 4096u
#define LOW_BYTES 8192u
#define WRITABLE_BYTES (LOW_BYTES + PAGE_BYTES)
#define TRACE_MAX 32u
#define RANDOM_CASES 1000u
_Static_assert(sizeof(CPUState) == 3552u, "accepted C2 ABI-6 CPU required");
_Static_assert(BW_GUEST_MEM1_SIZE == MEM1_BYTES, "fixed MEM1 must be 32 MiB");
__attribute__((aligned(4096))) u8 bw_guest_mem1[MEM1_BYTES];
CPUState bw_guest_cpu;
extern void ppc_set_mem_write_journal(PPCMemWriteJournal, void*);

/* Qualified production budget policy. The corpus
 * avoids signed overflow in D+dc, negation of B and the eight-cycle subtraction. */
#define DOLRECOMP_C_LOOP_CYCLE_BUDGET (ctx->cycle_budget)
static bool dolrecomp_block_can_precharge(const CPUState* ctx, u32 cycles) {
    const s64 remaining = ctx->cycle_deadline_budget + ctx->downcount;
    return ctx->cycle_deadline_budget <= 0 || (remaining >= 0 && (u64)remaining >= cycles);
}
static bool dolrecomp_charge_precise(CPUState* ctx, u32 cycles, u32 resume) {
    if (ctx->downcount <= -(s64)DOLRECOMP_C_LOOP_CYCLE_BUDGET) {
        ctx->pc = resume;
        return false;
    }
    ctx->downcount -= cycles;
    return true;
}

enum { MUTATE_D=1, MUTATE_META=2, MUTATE_ALIAS=4, MUTATE_JOURNAL=8,
       MUTATE_REGS=16, MUTATE_BUDGET=32, MUTATE_EXCEPTION=64 };
typedef struct Event {
    CPUState cpu;
    u32 kind, address, size;
    u64 value;
    u8 low[768], tail[16], alias[128], mem2[128];
} Event;
static Event trace[TRACE_MAX], wanted_trace[TRACE_MAX];
static unsigned trace_count, mode, current_shape, current_scenario;
static unsigned io_reads, io_writes, journals, observations;
static CPUState *active;
static u8 alias_storage[128], mem2_storage[128], other_ram[128];
static u8 initial_alias[128], initial_mem2[128], initial_other[128];
static u8 initial_ram[WRITABLE_BYTES], wanted_ram[WRITABLE_BYTES];
static u8 wanted_alias[128], wanted_mem2[128], wanted_other[128];
static u8 wanted_pipe[BW_GATHER_PIPE_BATCH+8u];
static unsigned wanted_pipe_length;
static u32 seed=0xC01D7A11u;
static void journal(u32 offset, u32 size, void* user);
static u32 random32(void) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed;
}
static void require(int ok, const char* message) {
    if (!ok) {
        fprintf(stderr,"COLD_TWIN_FAIL shape=%u scenario=%u check=%s seed=%08X\n",
                current_shape,current_scenario,message,seed);
        exit(1);
    }
}
static void capture_ram(u8* out) {
    memcpy(out,bw_guest_mem1,LOW_BYTES);
    memcpy(out+LOW_BYTES,bw_guest_mem1+MEM1_BYTES-PAGE_BYTES,PAGE_BYTES);
}
static void restore_ram(const u8* in) {
    memcpy(bw_guest_mem1,in,LOW_BYTES);
    memcpy(bw_guest_mem1+MEM1_BYTES-PAGE_BYTES,in+LOW_BYTES,PAGE_BYTES);
}
static int same_ram(const u8* expected) {
    return !memcmp(expected,bw_guest_mem1,LOW_BYTES) &&
        !memcmp(expected+LOW_BYTES,bw_guest_mem1+MEM1_BYTES-PAGE_BYTES,PAGE_BYTES);
}
static void record(CPUState* cpu,u32 kind,u32 address,u64 value,u32 size) {
    require(trace_count<TRACE_MAX,"bounded observer trace");
    Event* e=&trace[trace_count++];
    memset(e,0,sizeof(*e)); e->cpu=*cpu;
    e->kind=kind; e->address=address; e->value=value; e->size=size;
    memcpy(e->low,bw_guest_mem1+0x100u,sizeof(e->low));
    memcpy(e->tail,bw_guest_mem1+MEM1_BYTES-16u,sizeof(e->tail));
    memcpy(e->alias,alias_storage,sizeof(e->alias));
    memcpy(e->mem2,mem2_storage,sizeof(e->mem2));
    if (mode&MUTATE_D) cpu->cycle_deadline_budget=1+(mode%6u);
    if (mode&MUTATE_META) { cpu->pc^=0x104u; cpu->cycle_observation_suffix^=0x15u; }
    if (mode&MUTATE_ALIAS) {
        u8* p=NULL;
        if (ppc_guest_alias_get_storage(0x80000200u,sizeof(alias_storage),&p))
            require(ppc_guest_alias_remove(0x80000200u,sizeof(alias_storage)),"remove alias");
        else require(ppc_guest_alias_add_shared(0x80000200u,sizeof(alias_storage),alias_storage),"add alias");
    }
    if (mode&MUTATE_JOURNAL) ppc_set_mem_write_journal(journal,active);
    if (mode&MUTATE_REGS) {
        cpu->gpr[3]^=0xD1FF30A5u; cpu->gpr[4]=0x80000300u;
        cpu->gpr[5]=0x80000200u; cpu->gpr[6]=0x80000308u;
    }
    if (mode&MUTATE_BUDGET) { cpu->cycle_budget=16; cpu->downcount=-16; }
    if (mode&MUTATE_EXCEPTION) cpu->exception^=PPC_EXC_DSI;
}
static void observe(CPUState* cpu) { ++observations; record(cpu,1,0,0,0); }
static u64 io_read(CPUState* cpu,u32 address,u8 size) {
    ++io_reads; record(cpu,2,address,0,size);
    return 0x123456789ABCDEF0ull^address^size;
}
static void io_write(CPUState* cpu,u32 address,u64 value,u8 size) {
    ++io_writes; record(cpu,3,address,value,size);
}
static void journal(u32 offset,u32 size,void* user) {
    require(user==active,"journal user owner");
    ++journals; record(active,4,offset,0,size);
}

#define mem_read8 bw_mem_read8
#define mem_read16 bw_mem_read16
#define mem_read32 bw_mem_read32
#define mem_read64 bw_mem_read64
#define mem_write8 bw_mem_write8
#define mem_write16 bw_mem_write16
#define mem_write32 bw_mem_write32
#define mem_write64 bw_mem_write64
typedef void (*Run)(CPUState*);
typedef struct Shape { Run variants[3]; const char* name; } Shape;
#ifdef FIXTURE_FIXED_CPU
#define ctx (&bw_guest_cpu)
#endif
#include "rewritten_cases.h"
#ifdef FIXTURE_FIXED_CPU
#undef ctx
#endif

typedef struct Snapshot {
    CPUState cpu;
    unsigned count,reads,writes,journal_count,observe_count;
    u32 generation;
    bool overlap;
    PPCMemWriteJournal journal;
    void* journal_user;
    int exceptions,rounding;
    unsigned mxcsr;
} Snapshot;
static CPUState initial_cpu;
static Snapshot snapshot(void) {
    Snapshot s; memset(&s,0,sizeof(s)); s.cpu=*active;
    s.count=trace_count; s.reads=io_reads; s.writes=io_writes;
    s.journal_count=journals; s.observe_count=observations;
    s.generation=g_ppc_guest_alias_generation;
    s.overlap=g_ppc_guest_aliases_overlap_mem1;
    s.journal=g_mem_write_journal; s.journal_user=g_mem_write_journal_user;
    s.exceptions=fetestexcept(FE_ALL_EXCEPT); s.rounding=fegetround(); s.mxcsr=_mm_getcsr();
    return s;
}
static void reset_arm(unsigned alias_on,unsigned journal_on) {
    restore_ram(initial_ram);
    memcpy(alias_storage,initial_alias,sizeof(alias_storage));
    memcpy(mem2_storage,initial_mem2,sizeof(mem2_storage));
    memcpy(other_ram,initial_other,sizeof(other_ram));
    ppc_guest_alias_clear();
    if (alias_on) require(ppc_guest_alias_add_shared(0x80000200u,sizeof(alias_storage),alias_storage),"initial alias");
    /* Compare relative generation deterministically; reset itself is test setup. */
    g_ppc_guest_alias_generation=0x12340000u;
    bw_guest_cpu=initial_cpu; active=&bw_guest_cpu;
    ppc_set_mem_write_journal(journal_on?journal:NULL,active);
    bw_gather_pipe_write=NULL; bw_gather_pipe_bytes=NULL; bw_gather_pipe_length=0;
    memset(bw_gather_pipe_buffer,0,sizeof(bw_gather_pipe_buffer));
    trace_count=io_reads=io_writes=journals=observations=0;
    memset(trace,0,sizeof(trace));
    require(feclearexcept(FE_ALL_EXCEPT)==0,"clear fenv");
    require(fesetround(FE_TONEAREST)==0,"round fenv");
    _mm_setcsr((_mm_getcsr()&~0xE07Fu)|0x1F80u);
}
static void capture_wanted(void) {
    capture_ram(wanted_ram);
    memcpy(wanted_alias,alias_storage,sizeof(wanted_alias));
    memcpy(wanted_mem2,mem2_storage,sizeof(wanted_mem2));
    memcpy(wanted_other,other_ram,sizeof(wanted_other));
    memcpy(wanted_trace,trace,sizeof(wanted_trace));
    memcpy(wanted_pipe,bw_gather_pipe_buffer,sizeof(wanted_pipe));
    wanted_pipe_length=bw_gather_pipe_length;
}
static int equal_arm(const Snapshot* a,const Snapshot* b) {
    return !memcmp(a,b,sizeof(*a)) && same_ram(wanted_ram) &&
        !memcmp(wanted_alias,alias_storage,sizeof(wanted_alias)) &&
        !memcmp(wanted_mem2,mem2_storage,sizeof(wanted_mem2)) &&
        !memcmp(wanted_other,other_ram,sizeof(wanted_other)) &&
        !memcmp(wanted_trace,trace,sizeof(wanted_trace)) &&
        !memcmp(wanted_pipe,bw_gather_pipe_buffer,sizeof(wanted_pipe)) &&
        wanted_pipe_length==bw_gather_pipe_length;
}
static void seed_case(unsigned scenario) {
    memset(&initial_cpu,0,sizeof(initial_cpu));
    for (unsigned i=0;i<32;++i) {
        initial_cpu.gpr[i]=random32();
        u64 bits=((u64)random32()<<32)|random32(); memcpy(&initial_cpu.fpr[i],&bits,8);
        bits=((u64)random32()<<32)|random32(); memcpy(&initial_cpu.ps1[i],&bits,8);
    }
    initial_cpu.pc=random32(); initial_cpu.lr=random32(); initial_cpu.ctr=random32();
    initial_cpu.cr=random32(); initial_cpu.xer=random32(); initial_cpu.fpscr=random32();
    initial_cpu.msr=random32(); initial_cpu.cycle_observation_suffix=random32();
    initial_cpu.ram=bw_guest_mem1; initial_cpu.ram_size=MEM1_BYTES;
    if (scenario%17u==0) { initial_cpu.ram=other_ram; initial_cpu.ram_size=sizeof(other_ram); }
    if (scenario%19u==0) { initial_cpu.ram=NULL; initial_cpu.ram_size=0; }
    initial_cpu.exram=mem2_storage; initial_cpu.exram_size=sizeof(mem2_storage);
    initial_cpu.external_read=io_read; initial_cpu.external_write=io_write;
    initial_cpu.downcount=(s64)(random32()%32u)-12;
    initial_cpu.cycle_budget=scenario%5u ? (s64)(1+random32()%32u):0;
    if (scenario%29u==0) initial_cpu.cycle_budget=-3;
    initial_cpu.cycle_deadline_budget=scenario%4u ? (s64)(1+random32()%48u):0;
    initial_cpu.reserve_valid=scenario%3u==0; initial_cpu.reserve_addr=0x80000200u;
    const u32 addresses[]={0x80000100u,0x80000200u,0xC0000100u,
        0xCC006C00u,0x81FFFFFFu,0x90000003u,0x80000301u};
    initial_cpu.gpr[4]=addresses[scenario%7u];
    initial_cpu.gpr[5]=addresses[(scenario/7u)%7u];
    initial_cpu.gpr[6]=addresses[(scenario/49u)%7u];
    initial_cpu.gpr[10]=scenario%3u;
    for (unsigned i=0;i<WRITABLE_BYTES;++i) initial_ram[i]=(u8)random32();
    for (unsigned i=0;i<128u;++i) {
        initial_alias[i]=(u8)random32(); initial_mem2[i]=(u8)random32(); initial_other[i]=(u8)random32();
    }
    mode=scenario%128u;
}
/* Corpus restriction for RA=RS address consumers: the first loaded word is
 * a known safe cached-MEM1 pointer, and the intervening add adds zero. These
 * shapes test original alias/update ordering rather than arbitrary pointers
 * into protected RAM. Bodies, guard, callbacks and original accesses unchanged. */
static void constrain_alias_payload(unsigned shape) {
    const char* name=cases[shape].name;
    if (!strstr(name,"store_update_alias") && !strstr(name,"indexed_store_alias")) return;
    initial_cpu.gpr[8]=0;
    const u32 pointer=__builtin_bswap32(0x80000180u);
    memcpy(initial_ram+0x100u,&pointer,4);
    memcpy(initial_ram+0x200u,&pointer,4);
    memcpy(initial_ram+0x301u,&pointer,4);
    memcpy(initial_alias,&pointer,4);
    memcpy(initial_mem2+3u,&pointer,4);
}
static unsigned equivalent_case(unsigned shape,unsigned scenario,unsigned alias_on,unsigned journal_on) {
    Snapshot wanted;
    current_shape=shape; current_scenario=scenario;
    for (unsigned variant=0;variant<3;++variant) {
        reset_arm(alias_on,journal_on);
        cases[shape].variants[variant](active);
        Snapshot got=snapshot();
        if (!variant) { wanted=got; capture_wanted(); }
        else if (!equal_arm(&wanted,&got)) {
            fprintf(stderr,"shape_name=%s variant=%u pc=%08X/%08X suffix=%u/%u dc=%lld/%lld events=%u/%u\n",
                    cases[shape].name,variant,wanted.cpu.pc,got.cpu.pc,
                    wanted.cpu.cycle_observation_suffix,got.cpu.cycle_observation_suffix,
                    (long long)wanted.cpu.downcount,(long long)got.cpu.downcount,wanted.count,got.count);
            require(0,"full CPU/RAM/alias/MEM2/trace/fenv parity");
        }
    }
    return 3;
}
static void protect_readonly(void) {
    DWORD old;
    require(VirtualProtect(bw_guest_mem1,MEM1_BYTES,PAGE_READONLY,&old)!=0,"protect all MEM1");
    require(VirtualProtect(bw_guest_mem1,LOW_BYTES,PAGE_READWRITE,&old)!=0,"low writable pages");
    require(VirtualProtect(bw_guest_mem1+MEM1_BYTES-PAGE_BYTES,PAGE_BYTES,PAGE_READWRITE,&old)!=0,"tail writable page");
}
static void readonly_zero(void) {
    for (u32 i=LOW_BYTES;i<MEM1_BYTES-PAGE_BYTES;++i)
        require(bw_guest_mem1[i]==0,"all remaining MEM1 unchanged");
}
int main(void) {
    unsigned runs=0,directed=0,mutants=0;
    protect_readonly(); readonly_zero();
    for (unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);++c)
        for (unsigned s=0;s<RANDOM_CASES;++s) {
            seed_case(s); constrain_alias_payload(c); runs+=equivalent_case(c,s,(s&4u)!=0,(s&8u)!=0);
        }
    /* Deterministic grid: ordinary fast RAM; first-part cold miss; later miss
     * after committed RAM store; reserve/journal; callback-mutated metadata,
     * deadline, aliases and register updates. All start prepaid unless a row
     * explicitly exercises a precise/deadline entry. */
    for (unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);++c)
        for (unsigned s=0;s<24u;++s) {
            seed_case(123u+s); mode=s<8u?0u:1u<<(s%7u);
            initial_cpu.downcount=20; initial_cpu.cycle_budget=256;
            initial_cpu.cycle_deadline_budget=256; initial_cpu.reserve_valid=false;
            initial_cpu.gpr[3]=0x80000180u; initial_cpu.gpr[4]=0x80000100u;
            initial_cpu.gpr[5]=0x80000200u; initial_cpu.gpr[6]=0x80000300u; initial_cpu.gpr[10]=0;
            if(s==1u || s==8u || s==9u) initial_cpu.gpr[4]=0xCC006C00u;
            if(s==2u || s>=10u) initial_cpu.gpr[6]=0xCC006C00u;
            if(s==3u) initial_cpu.gpr[5]=0xC0000200u;
            if(s==4u) initial_cpu.reserve_valid=true;
            if(s==5u) initial_cpu.cycle_deadline_budget=1;
            if(s==6u) { initial_cpu.downcount=-256; initial_cpu.cycle_deadline_budget=0; }
            if(s==7u) { initial_cpu.downcount=0; initial_cpu.cycle_budget=0; initial_cpu.cycle_deadline_budget=0; }
            constrain_alias_payload(c);
            runs+=equivalent_case(c,1000u+s,s==16u,s==17u); ++directed;
        }
    /* Actual consumer mutants. Their first RAM read must take the cold arm;
     * skipping its original precise part or clearing prepaid must diverge. */
    seed_case(555u); mode=0; initial_cpu.downcount=20;
    initial_cpu.cycle_budget=256; initial_cpu.cycle_deadline_budget=256;
    initial_cpu.reserve_valid=false; initial_cpu.gpr[4]=0xCC006C00u;
    initial_cpu.gpr[5]=0x80000200u; initial_cpu.gpr[6]=0x80000300u;
    reset_arm(0,0); mutant_original(active); Snapshot wanted=snapshot(); capture_wanted();
    Run negatives[]={mutant_wrong_label,mutant_prepaid_false};
    for(unsigned i=0;i<2u;++i) {
        reset_arm(0,0); negatives[i](active); Snapshot got=snapshot();
        require(!equal_arm(&wanted,&got),"behavioral consumer mutant detected"); ++mutants;
    }
    readonly_zero();
    ppc_set_mem_write_journal(NULL,NULL); ppc_guest_alias_clear();
    require(runs==sizeof(cases)/sizeof(cases[0])*(RANDOM_CASES+24u)*3u,"run accounting");
    require(mutants==2u,"both consumer mutants detected");
    printf("COLD_TWIN_DIFFERENTIAL_PASS shapes=%u scenarios=%u runs=%u directed=%u mutants=%u CPU=%u MEM1=%u writable=%u readonly=1 traces=full fenv=1 timing=0\n",
           (unsigned)(sizeof(cases)/sizeof(cases[0])),RANDOM_CASES+24u,runs,directed,mutants,
           (unsigned)sizeof(CPUState),MEM1_BYTES,WRITABLE_BYTES);
    return 0;
}
