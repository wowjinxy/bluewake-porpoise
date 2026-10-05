/* Exact source loader branches + native ownership bridge and observer.
 * Native file/PPC linking and handler outcomes are fixtures, never game proof. */
#define main frozen_song_observer_tests
#include "song_item_observer_test.c"
#undef main
#include "song_host_adapter.h"
#include "rel_scratch_allocator.h"
#include <stdatomic.h>

typedef struct BlueWakeRelData {u32 module_id,section_index,linked_start,size;const u8* bytes;} BlueWakeRelData;
typedef struct BlueWakeRelAlias {u32 raw_start,raw_end,linked_start,text_size;} BlueWakeRelAlias;
typedef struct BlueWakeRelSlot {u32 owner,address,capacity;} BlueWakeRelSlot;
#define BLUEWAKE_MAX_REL_ALIASES 512u
#define BLUEWAKE_MAX_REL_SLOTS 512u
#define BLUEWAKE_DYNAMIC_SCRATCH_BASE 0x81820000u
#define BLUEWAKE_DYNAMIC_SCRATCH_LIMIT 0x81F80000u
static BlueWakeRelAlias g_rel_aliases[512];static u32 g_rel_alias_count,g_rel_alias_raw_min,g_rel_alias_raw_max;
static BlueWakeRelSlot g_rel_slots[512];static const BlueWakeRelData* g_rel_data;static u32 g_rel_data_count,g_module1_raw_base;
static bool g_module336_bss_alias_installed;
static u8* fixed_backing;static unsigned backing_calls;static bool backing_fail,backing_flip;
static uint32_t generation_calls,flip_generation_at,replace_ram_at;
static uint8_t* changed_ram;
static bool fixture_get_storage(u32 address,u32 size,u8** out){
    ++backing_calls;CHECK(address==0xC0E96420u&&size==0x390u);
    if(backing_flip)++g_ppc_guest_alias_generation;
    if(backing_fail)return false;if(!ppc_guest_alias_get_storage(address,size,out))return false;
    if(fixed_backing)*out=fixed_backing;return true;
}
static bool host_add_shared_guest_alias(u32 address,u32 size,const u8* initial){(void)address;(void)size;(void)initial;CHECK(false);return false;}
#define ppc_guest_alias_get_storage fixture_get_storage
#include "song_host_bridge.inc"
#undef ppc_guest_alias_get_storage
static int fixture_dispatch(CPUState* context,u32 address){(void)context;(void)address;return 1;}
static StaticRecompRelSection metadata[19];static StaticRecompRelModule rel_descriptor;static StaticRecompModuleDesc descriptor;
static void metadata_setup(void){
    memset(metadata,0,sizeof metadata);for(unsigned i=0;i<19;++i){metadata[i].module_id=257;metadata[i].section_index=i;}
    metadata[1]=(StaticRecompRelSection){257,1,0xC0E900E4u,0x5FBC};metadata[5]=(StaticRecompRelSection){257,5,0xC0E96420u,0x390};
    rel_descriptor=(StaticRecompRelModule){257,0,19,0x4C,36200,metadata,19};
    memset(&descriptor,0,sizeof descriptor);descriptor.abi_version=5;descriptor.cpu_abi_version=GXRUNTIME_CPU_ABI_VERSION;
    descriptor.cpu_state_size=sizeof cpu;memcpy(descriptor.game_id,"GZLE01",7);descriptor.dispatch=fixture_dispatch;
    descriptor.rel_modules=&rel_descriptor;descriptor.num_rel_modules=1;
}
static void bound_setup(void){
    host_song_owner_revoke();setup();metadata_setup();fixed_backing=linked_data;backing_calls=0;backing_fail=backing_flip=false;
    memset(g_rel_slots,0,sizeof g_rel_slots);memset(g_rel_aliases,0,sizeof g_rel_aliases);
    ppc_guest_alias_clear();CHECK(ppc_guest_alias_add_shared(0xC0E96420u,0x390u,linked_data));g_rel_alias_count=1;g_rel_alias_raw_min=aliases[0].raw_start;g_rel_alias_raw_max=aliases[0].raw_end;
    g_rel_aliases[0]=(BlueWakeRelAlias){aliases[0].raw_start,aliases[0].raw_end,aliases[0].linked_start,aliases[0].text_size};
    g_rel_slots[0]=(BlueWakeRelSlot){OWNER,MODULE,45248};g_rel_data=NULL;g_rel_data_count=0;
    host_song_owner_bind(&cpu,&descriptor,false);CHECK(g_song_host.active);bw_song_host_materialized(&g_song_host,&cpu,0);
    CHECK(g_song_host.load_tokens[0]!=0);
}
static bool owner(BwSongOwner* out){return host_song_owner_query(&g_song_host,&cpu,out);}
static void adapter_read_only(void){
    bound_setup();BwSongOwner out;CPUState c=cpu;BwSongHostAdapter h=g_song_host;
    BlueWakeRelSlot slot_copy[512];BlueWakeRelAlias alias_copy[512];u8 data_copy[0x390];
    memcpy(slot_copy,g_rel_slots,sizeof slot_copy);memcpy(alias_copy,g_rel_aliases,sizeof alias_copy);memcpy(data_copy,linked_data,sizeof data_copy);
    memcpy(before_ram,cpu.ram,cpu.ram_size);CHECK(owner(&out));CHECK(out.load_token==h.load_tokens[0]);
    CHECK(out.module_header==MODULE&&out.loader_owner==OWNER&&out.raw_text==0x818A5A84u);
    CHECK(!memcmp(&cpu,&c,sizeof c)&&!memcmp(before_ram,cpu.ram,cpu.ram_size));
    CHECK(!memcmp(&g_song_host,&h,sizeof h)&&!memcmp(slot_copy,g_rel_slots,sizeof slot_copy));
    CHECK(!memcmp(alias_copy,g_rel_aliases,sizeof alias_copy)&&!memcmp(data_copy,linked_data,sizeof data_copy));
    /* Unrelated aliases can change the registry count while Hr backing is stable. */
    u8 unrelated[4]={0};CHECK(ppc_guest_alias_add_shared(0x80700000u,4,unrelated));CHECK(owner(&out));CHECK(out.load_token==h.load_tokens[0]);
    CPUState other=cpu;unsigned reads=backing_calls;CHECK(!host_song_owner_query(&g_song_host,&other,&out));CHECK(backing_calls==reads);
    CHECK(!host_song_owner_query(NULL,&cpu,&out));CHECK(backing_calls==reads);
    u8* ram=cpu.ram;cpu.ram=(u8*)1;CHECK(!owner(&out));CHECK(backing_calls==reads);cpu.ram=ram;
    --cpu.ram_size;CHECK(!owner(&out));CHECK(backing_calls==reads);++cpu.ram_size;
    g_rel_alias_count=513;CHECK(!owner(&out));CHECK(backing_calls==reads);g_rel_alias_count=1;
    u8 replacement[0x390];memcpy(replacement,linked_data,sizeof replacement);fixed_backing=replacement;CHECK(!owner(&out));fixed_backing=linked_data;
    backing_fail=true;CHECK(!owner(&out));backing_fail=false;
    backing_flip=true;CHECK(!owner(&out));backing_flip=false;CHECK(owner(&out));
    g_rel_slots[0].owner=0x81FFFFF0u;CHECK(!owner(&out));g_rel_slots[0].owner=OWNER;
    BwSongHostAlias projected[1]={{g_rel_aliases[0].raw_start,g_rel_aliases[0].raw_end,g_rel_aliases[0].linked_start,g_rel_aliases[0].text_size}};
    BwSongHostSlot projected_slots[1]={{OWNER,MODULE,45248}};
    CHECK(!bw_song_host_query(&g_song_host,&cpu,projected,513,projected_slots,1,&out));
    CHECK(!bw_song_host_query(&g_song_host,&cpu,projected,1,projected_slots,513,&out));
    CHECK(!bw_song_host_query(&g_song_host,&cpu,projected,1,projected_slots,1,NULL));
    host_song_owner_revoke();reads=backing_calls;CHECK(!owner(&out));CHECK(backing_calls==reads);
    /* Even a backing pointer ABA cannot revive an explicitly revoked binding. */
    fixed_backing=linked_data;CHECK(!owner(&out));
}
static void bad_metadata(void){
    for(unsigned bad=0;bad<16;++bad){bound_setup();BwSongHostAdapter h={0};CPUState c=cpu;
        switch(bad){case 0:descriptor.abi_version=2;break;case 1:descriptor.abi_version=6;break;
        case 2:descriptor.cpu_abi_version++;break;case 3:descriptor.cpu_state_size--;break;
        case 4:descriptor.game_id[0]='X';break;case 5:descriptor.dispatch=NULL;break;
        case 6:descriptor.num_rel_modules=513;break;case 7:rel_descriptor.module_id=258;break;
        case 8:rel_descriptor.section_count=18;break;case 9:rel_descriptor.num_sections=20;break;
        case 10:rel_descriptor.section_info_offset=0x50;break;case 11:metadata[1].linked_start+=4;break;
        case 12:metadata[5].size--;break;case 13:metadata[18].module_id=258;break;
        case 14:c.ram_size=0x01000000;break;case 15:c.ram=NULL;break;}
        CHECK(!bw_song_host_bind(&h,&c,&descriptor,host_song_fixed_backing,host_song_alias_generation,NULL));CHECK(!h.active);
    }
    bound_setup();for(unsigned abi=3;abi<=5;++abi){descriptor.abi_version=abi;host_song_owner_bind(&cpu,&descriptor,false);CHECK(g_song_host.active);}
    StaticRecompRelModule duplicated[2]={rel_descriptor,rel_descriptor};descriptor.rel_modules=duplicated;descriptor.num_rel_modules=2;
    host_song_owner_bind(&cpu,&descriptor,false);CHECK(!g_song_host.active);
}

/* Compile and execute the actual private materializer/allocator/HLE branch.
 * Only FILE/heap operations are replaced for deterministic failure injection. */
typedef struct MockRelFile {long size;} MockRelFile;static MockRelFile mock_file;
static u8 file_bytes[0x110000];static unsigned allocation_calls,fail_allocation_call,open_calls,close_calls;
static bool open_fail,seek_fail,short_read;static u32 writes;static u64 token_during_write;
static MockRelFile* host_open_rel(const char* dir,const char* name){CHECK(dir&&name);++open_calls;return open_fail?NULL:&mock_file;}
static int fixture_seek(MockRelFile* file,long offset,int origin){CHECK(file==&mock_file&&offset==0&&origin==SEEK_END);return seek_fail?-1:0;}
static long fixture_tell(MockRelFile* file){CHECK(file==&mock_file);return file->size;}
static void fixture_rewind(MockRelFile* file){CHECK(file==&mock_file);}
static size_t fixture_read(void* bytes,size_t piece,size_t count,MockRelFile* file){CHECK(file==&mock_file&&piece==1&&count<=sizeof file_bytes);size_t n=short_read?count-1:count;memcpy(bytes,file_bytes,n);return n;}
static int fixture_close(MockRelFile* file){CHECK(file==&mock_file);++close_calls;return 0;}
static void* fixture_malloc(size_t n){++allocation_calls;if(allocation_calls==fail_allocation_call)return NULL;return malloc(n);}
static void fixture_write_journal(u32 offset,u32 size,void* user){(void)offset;(void)user;
    if(size==1){CHECK(g_song_host.sequence==token_during_write);++writes;}}
#define FILE MockRelFile
#define fseek fixture_seek
#define ftell fixture_tell
#define rewind fixture_rewind
#define fread fixture_read
#define fclose fixture_close
#define malloc fixture_malloc
#include "song_native_loader.inc"
#undef malloc
#undef fclose
#undef fread
#undef rewind
#undef ftell
#undef fseek
#undef FILE
static void guest_read_cstr(CPUState* c,u32 address,char* result,unsigned capacity){unsigned n=0;while(n+1<capacity&&(result[n]=(char)mem_read8(c,address+n)))++n;result[n]=0;}
static bool host_activate_rel_profile_list(CPUState* c,const BlueWakeRelData* data,u32 count,u32 id){(void)c;(void)data;(void)count;(void)id;CHECK(false);return false;}
static void actual_hle(bool* completed){
    const StaticRecompModuleDesc* mod=&descriptor;const BlueWakeRelData* rel_data=g_rel_data;u32 rel_data_count=g_rel_data_count;
    bool profile_prolog_called=false;cpu.pc=0x80240744u;
    do{
#include "song_native_materializer_hle.inc"
        *completed=false;break;
    }while(0);
    if(cpu.pc!=0x80240744u)*completed=true;
}
static void file32(unsigned a,u32 v){for(unsigned n=0;n<4;++n)file_bytes[a+n]=(u8)(v>>((3-n)*8));}
static void loader_setup(void){
    bound_setup();memset(g_rel_slots,0,sizeof g_rel_slots);memset(g_song_host.load_tokens,0,sizeof g_song_host.load_tokens);
    memset(g_rel_aliases,0,sizeof g_rel_aliases);g_rel_alias_count=0;g_rel_alias_raw_min=~0u;g_rel_alias_raw_max=0;
    memset(file_bytes,0xA5,sizeof file_bytes);mock_file.size=45248;file32(0,257);file32(0xC,19);file32(0x10,0x4C);file32(0x1C,3);
    memset(file_bytes+0x4C,0,19*8);file32(0x54,0xE5);file32(0x58,0x5FBC);file32(0x74,0x6420);file32(0x78,0x390);
    open_fail=seek_fail=short_read=false;allocation_calls=fail_allocation_call=open_calls=close_calls=writes=0;
    token_during_write=g_song_host.sequence;g_mem_write_journal=fixture_write_journal;
    w32(OWNER+0x10,0);w32(OWNER+0x1C,0x81600000);memcpy(cpu.ram+0x1600000,"d_a_hr",7);cpu.gpr[3]=OWNER;cpu.lr=0x80240100;
}
static void native_link_source_fixture(u32 module){
    /* OSLink primary-source format operation, simulated solely in a fixture.
     * Actual game OSLink execution is still a separate required qualification. */
    w32(0x800030C8,module);w32(0x800030CC,module);w32(module+4,0);w32(module+8,0);
    w32(module+0x10,module+0x4C);w32(module+0x54,module+0xE5);w32(module+0x74,module+0x6420);
}
static void actual_loader_cases(void){
    loader_setup();const u64 before=g_song_host.sequence;bool completed=false;actual_hle(&completed);
    CHECK(completed&&cpu.gpr[3]==1&&cpu.pc==0x80240100&&open_calls==1&&close_calls==1);
    CHECK(g_song_host.sequence==before+1&&g_song_host.load_tokens[0]==before+1&&writes==45248);
    const u32 module=g_rel_slots[0].address;CHECK(module==BLUEWAKE_DYNAMIC_SCRATCH_BASE);
    CHECK(g_rel_slots[0].owner==OWNER&&g_rel_slots[0].capacity==45248);
    CHECK(!memcmp(cpu.ram+module-0x80000000u,file_bytes,45248));CHECK(g_rel_alias_count==1);
    CHECK(g_rel_aliases[0].raw_start==module+0xE4&&g_rel_aliases[0].linked_start==0xC0E900E4);
    native_link_source_fixture(module);BwSongOwner out;CHECK(owner(&out)&&out.load_token==before+1);
    /* Actual HLE gate: repeated cache hit/budget entry does not call the loader. */
    for(unsigned n=0;n<3;++n){cpu.gpr[3]=OWNER;completed=true;actual_hle(&completed);CHECK(!completed);CHECK(open_calls==1&&g_song_host.sequence==before+1);}
    /* The materializer itself truly recopies for an explicit actual invocation.
     * Same owner/address is a new lifetime despite identical bytes. */
    w32(OWNER+0x10,0);u32 p=0,n=0;token_during_write=g_song_host.sequence;
    CHECK(host_materialize_rel(&cpu,"d_a_hr",OWNER,&p,&n));CHECK(p==module&&n==45248);
    CHECK(g_song_host.load_tokens[0]==before+2&&g_rel_alias_count==0);
    /* A different dead owner reclaims the exact allocation and a fresh token. */
    token_during_write=g_song_host.sequence;CHECK(host_materialize_rel(&cpu,"d_a_hr",OWNER+0x100,&p,&n));
    CHECK(p==module&&g_rel_slots[0].owner==OWNER+0x100&&g_song_host.load_tokens[0]==before+3);
    /* Insufficient old capacity takes the actual scratch allocator/new slot. */
    loader_setup();g_rel_slots[0]=(BlueWakeRelSlot){OWNER,0x81820000,32};w32(OWNER+0x10,0x81820000);
    CHECK(host_materialize_rel(&cpu,"d_a_hr",OWNER+0x100,&p,&n));CHECK(p==0x81820020&&g_rel_slots[1].owner==OWNER+0x100);
    /* Stale overlapping allocations and their old tokens are removed. */
    loader_setup();g_rel_slots[0]=(BlueWakeRelSlot){OWNER,0x81820000,45248};
    g_rel_slots[1]=(BlueWakeRelSlot){OWNER+0x100,0x81820020,32};g_song_host.load_tokens[1]=999;
    CHECK(host_materialize_rel(&cpu,"d_a_hr",OWNER,&p,&n));CHECK(!g_rel_slots[1].owner&&!g_song_host.load_tokens[1]);
    /* Native unrelated/BSS stores are unchanged by the token hook. */
    token_during_write=g_song_host.sequence;
    BlueWakeRelData images[2]={{257,6,0x81500000,8,NULL},{258,6,0x81500010,8,NULL}};
    memset(cpu.ram+0x1500000,0xCC,32);host_zero_rel_bss(&cpu,images,2,257);
    CHECK(!memcmp(cpu.ram+0x1500000,"\0\0\0\0\0\0\0\0",8)&&cpu.ram[0x1500010]==0xCC);
    CHECK(host_rel_section_linked_start(&descriptor,257,1)==0xC0E900E4&&host_rel_section_linked_start(NULL,257,1)==0);
    /* Counter exhaustion fails observation closed without breaking native loading. */
    loader_setup();g_song_host.sequence=UINT64_MAX;token_during_write=UINT64_MAX;
    CHECK(host_materialize_rel(&cpu,"d_a_hr",OWNER,&p,&n));CHECK(!g_song_host.active&&g_song_host.exhausted);
    CHECK(!g_song_host.load_tokens[0]);host_song_owner_bind(&cpu,&descriptor,false);CHECK(!g_song_host.active);
    /* A zeroed fresh adapter is only permitted as process startup storage. */
    memset(&g_song_host,0,sizeof g_song_host);g_mem_write_journal=NULL;
}
static void loader_failures(void){
    for(unsigned bad=0;bad<9;++bad){loader_setup();
        switch(bad){case 0:open_fail=true;break;case 1:seek_fail=true;break;case 2:mock_file.size=0;break;
        case 3:mock_file.size=-1;break;case 4:mock_file.size=0x100001;break;case 5:fail_allocation_call=1;break;
        case 6:short_read=true;break;case 7:fail_allocation_call=2;break;
        case 8:{static BlueWakeRelData occupied={999,1,BLUEWAKE_DYNAMIC_SCRATCH_BASE,BLUEWAKE_DYNAMIC_SCRATCH_LIMIT-BLUEWAKE_DYNAMIC_SCRATCH_BASE,NULL};g_rel_data=&occupied;g_rel_data_count=1;break;}}
        BwSongHostAdapter before=g_song_host;BlueWakeRelSlot slots_before[512];memcpy(slots_before,g_rel_slots,sizeof slots_before);
        u32 p=0xDEADBEEF,n=0xDEADBEEF;CHECK(!host_materialize_rel(&cpu,"d_a_hr",OWNER,&p,&n));
        CHECK(p==0xDEADBEEF&&n==0xDEADBEEF&&!memcmp(&before,&g_song_host,sizeof before));
        CHECK(!memcmp(slots_before,g_rel_slots,sizeof slots_before)&&writes==0);
        CHECK(close_calls==(open_fail?0u:1u));
    }
    loader_setup();for(unsigned i=0;i<512;++i){u32 own=0x81000000+i*32;g_rel_slots[i]=(BlueWakeRelSlot){own,0x81820000+i*32,32};w32(own+0x10,g_rel_slots[i].address);}
    const u64 token=g_song_host.sequence;u32 p=0,n=0;CHECK(!host_materialize_rel(&cpu,"d_a_hr",OWNER,&p,&n));CHECK(g_song_host.sequence==token&&writes==0);
    g_mem_write_journal=NULL;
}
static void native_reset(const BwGameEvent* event,void* user){(void)user;if(event->kind!=BW_GAME_EVENT_RESET)return;
#include "song_native_reset.inc"
}
static uint32_t guarded_generation(void* user){(void)user;
    if(++generation_calls==flip_generation_at)++g_ppc_guest_alias_generation;
    if(generation_calls==replace_ram_at)cpu.ram=changed_ram;
    return g_ppc_guest_alias_generation;
}
static void actual_native_aliases(void){
    BwSongOwner out;u8 zero_alias[4]={0};bound_setup();u32 gen=g_ppc_guest_alias_generation;
    CHECK(ppc_guest_alias_add_shared(0x800030C8,4,zero_alias));CHECK(g_ppc_guest_alias_generation!=gen);
    CPUState before=cpu;memcpy(before_ram,cpu.ram,cpu.ram_size);CHECK(!owner(&out));
    CHECK(!memcmp(&before,&cpu,sizeof cpu)&&!memcmp(before_ram,cpu.ram,cpu.ram_size));
    CHECK(ppc_guest_alias_remove(0x800030C8,4));CHECK(owner(&out));
    /* The inverse proves native resolution is used instead of flat bytes. */
    u8 list_head[4]={(u8)(MODULE>>24),(u8)(MODULE>>16),(u8)(MODULE>>8),(u8)MODULE};w32(0x800030C8,0);
    CHECK(ppc_guest_alias_add_shared(0x800030C8,4,list_head));CHECK(owner(&out));
    CHECK(ppc_guest_alias_remove(0x800030C8,4));CHECK(!owner(&out));
    bound_setup();observed(GET);CHECK(ppc_guest_alias_add_shared(OWNER+0x10,4,zero_alias));finish(0);CHECK(event_count==0);
    /* Restored live slot detection follows the same native-visible owner. */
    host_song_owner_bind(&cpu,&descriptor,true);CHECK(!g_song_host.load_tokens[0]);CHECK(!owner(&out));
    bound_setup();CHECK(ppc_guest_alias_add_shared(MODULE,4,zero_alias));CHECK(!owner(&out));
    bound_setup();CHECK(ppc_guest_alias_add_shared(TABLE+8,4,zero_alias));CHECK(!owner(&out));
    /* Earlier narrow/containing aliases shadow fixed Hr profile bytes in native
     * reads. Even identical shadow bytes must not borrow the exact.data proof. */
    bound_setup();host_song_owner_revoke();ppc_guest_alias_clear();u8 profile[4];memcpy(profile,linked_data+0x208,4);
    CHECK(ppc_guest_alias_add_shared(0xC0E96628,4,profile));CHECK(ppc_guest_alias_add_shared(0xC0E96420,0x390,linked_data));
    host_song_owner_bind(&cpu,&descriptor,true);CHECK(g_song_host.active);CHECK(!owner(&out));
    bound_setup();host_song_owner_revoke();ppc_guest_alias_clear();u8 wider[0x400];memset(wider,0,sizeof wider);memcpy(wider+0x20,linked_data,sizeof linked_data);
    CHECK(ppc_guest_alias_add_shared(0xC0E96400,0x400,wider));CHECK(ppc_guest_alias_add_shared(0xC0E96420,0x390,linked_data));
    host_song_owner_bind(&cpu,&descriptor,true);CHECK(!owner(&out));
    /* Source main clears aliases only after explicit revoke, which protects ABA. */
    bound_setup();host_song_owner_revoke();ppc_guest_alias_clear();CHECK(ppc_guest_alias_add_shared(0xC0E96420,0x390,linked_data));CHECK(!owner(&out));
}
static void pending_and_lifetime(void){
    BwGameEventSubscription sub=bluewake_game_events_subscribe(BW_GAME_EVENT_MASK(BW_GAME_EVENT_RESET),native_reset,NULL);CHECK(sub);
    /* Native CARD loads keep actual module tokens, but cancel pending calls. */
    bound_setup();u64 token=g_song_host.load_tokens[0];observed(GET);CHECK(bluewake_game_events_observes(RETURN));
    bluewake_game_events_reset(&cpu,BW_GAME_RESET_GAME_LOAD);CHECK(g_song_host.active&&g_song_host.load_tokens[0]==token);
    bluewake_game_events_retrace(&cpu);finish(0);CHECK(event_count==0);cpu.gpr[3]=0x6D;observed(GET);finish(0);CHECK(event_count==1);
    for(unsigned why=BW_GAME_RESET_ATTACH;why<=BW_GAME_RESET_MEMORY_REPLACED;++why){
        if(why==BW_GAME_RESET_GAME_LOAD)continue;bound_setup();observed(GET);bluewake_game_events_reset(&cpu,(BwGameResetReason)why);
        CHECK(!g_song_host.active&&!g_song_host.load_tokens[0]);bluewake_game_events_retrace(&cpu);finish(0);CHECK(event_count==0);
    }
    /* Same address+owner rematerialization invalidates a pending exact invocation. */
    bound_setup();observed(GET);bw_song_host_materialized(&g_song_host,&cpu,0);finish(0);CHECK(event_count==0);
    bound_setup();observed(GET);host_song_owner_revoke();host_song_owner_bind(&cpu,&descriptor,true);CHECK(g_song_host.load_tokens[0]>token);
    finish(0);CHECK(event_count==0);cpu.gpr[3]=0x6D;observed(GET);finish(0);CHECK(event_count==1);
    /* Rebinding is after all successful STATE writes: replacement failure leaves
     * no live observer, while prevalidation failure need not mutate anything. */
    bound_setup();token=g_song_host.load_tokens[0];observed(GET);CHECK(g_song_host.load_tokens[0]==token); /* prevalidation no-op */
    finish(0);CHECK(event_count==1); /* No machine replacement: pending remains meaningful. */
    bound_setup();observed(GET);host_song_owner_revoke();finish(0);CHECK(event_count==0);CHECK(!g_song_host.active);
    bound_setup();host_song_owner_revoke();CPUState* temporary=malloc(sizeof cpu);CHECK(temporary);*temporary=cpu;
    CHECK(bw_song_host_bind(&g_song_host,temporary,&descriptor,host_song_fixed_backing,host_song_alias_generation,NULL));
    bw_song_host_revoke(&g_song_host);free(temporary);BwSongOwner out;CHECK(!bw_song_host_query(&g_song_host,temporary,NULL,0,NULL,0,&out));
    /* Mid-read generation or RAM changes revoke access before any following raw
     * extension/fixed-profile dereference. ASan checks invalid changed pointer. */
    for(unsigned point=1;point<=70;++point){bound_setup();g_song_host.generation=guarded_generation;generation_calls=0;flip_generation_at=point;replace_ram_at=0;
        bool ok=owner(&out);if(generation_calls>=point)CHECK(!ok);else CHECK(ok);
    }
    for(unsigned point=1;point<=70;++point){bound_setup();g_song_host.generation=guarded_generation;generation_calls=0;flip_generation_at=0;replace_ram_at=point;changed_ram=(u8*)1;
        u8* old=cpu.ram;bool ok=owner(&out);cpu.ram=old;if(generation_calls>=point)CHECK(!ok);else CHECK(ok);
    }
    bound_setup();BwSongHostSlot saved[2]={{OWNER,MODULE,45248},{0x81FFFFF0,MODULE,45248}};
    bw_song_host_revoke(&g_song_host);host_song_owner_bind(&cpu,&descriptor,false);
    token=g_song_host.sequence;CHECK(bw_song_host_restore_slots(&g_song_host,&cpu,saved,2));CHECK(g_song_host.load_tokens[0]==token+1&&!g_song_host.load_tokens[1]);
    g_song_host.generation=guarded_generation;generation_calls=0;flip_generation_at=4;CHECK(!bw_song_host_restore_slots(&g_song_host,&cpu,saved,2));CHECK(!g_song_host.load_tokens[0]);
    CHECK(bluewake_game_events_unsubscribe(sub));
}
static void put_le(u8* p,u32 n){for(unsigned i=0;i<4;++i)p[i]=(u8)(n>>(i*8));}
static u8 blob[16000];
static size_t field(size_t at,const char* name,u32 size){size_t n=strlen(name);CHECK(at+6+n+size<sizeof blob);blob[at++]=(u8)n;blob[at++]=(u8)(n>>8);memcpy(blob+at,name,n);at+=n;put_le(blob+at,size);at+=4;memset(blob+at,0,size);return at+size;}
typedef struct StateEligibilityHeader {u64 chunk_digest;u32 chunk_count;char game_id[8];u32 mod_mask;} StateEligibilityHeader;
typedef struct StateEligibilityChunk {const u8* data;u64 size;} StateEligibilityChunk;
static unsigned hud_resume_calls,health_attach_calls;
static bool bw_health_host_attach(CPUState* current,const StaticRecompModuleDesc* module){
    CHECK(current==&cpu&&module==&descriptor);++health_attach_calls;return true;
}
static bool bw_hud_host_resume(CPUState* current,const StaticRecompModuleDesc* module){
    CHECK(current==&cpu&&module==&descriptor);++hud_resume_calls;return true;
}
static void actual_STATE_eligibility(CPUState* cpu,const StaticRecompModuleDesc* mod,
                                     StateEligibilityHeader saved,StateEligibilityHeader here,const StateEligibilityChunk* vars){
#include "song_native_STATE_eligibility.inc"
}
static void saved_layout(void){
    CHECK(sizeof(BlueWakeRelSlot)==12&&sizeof(BlueWakeRelAlias)==16);
    put_le(blob,3);size_t n=field(4,"g_rel_slots",512*12);n=field(n,"g_rel_aliases",512*16);n=field(n,"g_rel_alias_count",4);
    CHECK(bw_song_host_state_fields(blob,n));for(size_t size=0;size<n;++size)CHECK(!bw_song_host_state_fields(blob,size));
    CHECK(!bw_song_host_state_fields(NULL,n));CHECK(!bw_song_host_state_fields(blob,268435457));
    put_le(blob,4);n=field(n,"g_rel_alias_count",4);CHECK(!bw_song_host_state_fields(blob,n));
    put_le(blob,2);n=field(4,"g_rel_slots",512*12);n=field(n,"g_rel_aliases",512*16);CHECK(!bw_song_host_state_fields(blob,n));
    put_le(blob,3);n=field(4,"g_rel_slots",512*24);CHECK(n<sizeof blob); /* changed stride rejected */
    n=field(n,"g_rel_aliases",16);n=field(n,"g_rel_alias_count",4);CHECK(!bw_song_host_state_fields(blob,n));
    put_le(blob,4);n=field(4,"g_rel_slots",512*12);n=field(n,"unrelated_old_field",1);n=field(n,"g_rel_aliases",512*16);n=field(n,"g_rel_alias_count",4);
    CHECK(bw_song_host_state_fields(blob,n));blob[n]=0;CHECK(!bw_song_host_state_fields(blob,n+1));
    StateEligibilityHeader here={1234,81,"GZLE01",0x400};StateEligibilityChunk vars={blob,n};
    for(unsigned bad=0;bad<7;++bad){bound_setup();host_song_owner_revoke();hud_resume_calls=health_attach_calls=0;StateEligibilityHeader saved=here;StateEligibilityChunk candidate=vars;
        switch(bad){case 0:saved.chunk_digest++;break;case 1:saved.chunk_count++;break;case 2:saved.game_id[0]='X';break;
        case 3:candidate.size--;break;case 4:put_le(blob,3);break;case 5:break;case 6:saved.mod_mask^=1;break;}
        actual_STATE_eligibility(&cpu,&descriptor,saved,here,&candidate);
        CHECK(g_song_host.active==(bad>=5));if(bad>=5)CHECK(g_song_host.load_tokens[0]!=0);
        /* This is the real STATE compatibility branch, with HUD resume itself
         * mocked. A changed mod mask revokes HUD independently of REL fields. */
        CHECK(hud_resume_calls==((bad>=3&&bad<=5)?1u:0u));
        CHECK(health_attach_calls==hud_resume_calls);
        put_le(blob,4);
    }
    put_le(blob,65537);CHECK(!bw_song_host_state_fields(blob,n));
}
int main(void){
    CHECK(frozen_song_observer_tests()==0);
    cpu.ram=calloc(1,0x02000000);before_ram=malloc(0x02000000);CHECK(cpu.ram&&before_ram);
    BwGameEventSubscription sub=bluewake_game_events_subscribe(BW_GAME_EVENT_MASK(BW_GAME_EVENT_ITEM_AWARDED),record,NULL);CHECK(sub);
    adapter_read_only();bad_metadata();actual_loader_cases();loader_failures();actual_native_aliases();pending_and_lifetime();saved_layout();
    host_song_owner_revoke();bluewake_game_events_reset(NULL,BW_GAME_RESET_MODULE_RELOAD);CHECK(bluewake_game_events_unsubscribe(sub));
    ppc_guest_alias_clear();
    free(before_ram);free(cpu.ram);printf("Host adapter + actual loader/HLE branches: %u total checks PASS; no native host/game execution.\n",checks);return 0;
}
