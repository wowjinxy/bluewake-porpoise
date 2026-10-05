// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_game.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
namespace {
enum Command {None,Join,Host,Leave};
struct PendingCommand{Command kind=None;BwNetworkConfig config{};char bind[64]{};};
std::mutex mutex;
BwNetworkPreferences mounted{};
BwNetworkGameSnapshot published{};
PendingCommand command;
bool prepared=false,ever_attached=false,boot_token=false;
/* Everything below belongs exclusively to the game thread. */
CPUState* cpu=nullptr;BwNetworkSession* session=nullptr;BwNetworkServer* server=nullptr;
BwGameEventSubscription subscription=0;
BwProgressionState known{},deferred{},authoritative{};
bool loaded=false,cold=false,native_ready=false,exporting=false,dirty=false,trace=false;
bool announced=false;
bool reseeding=true;
uint64_t connection_generation=0,ticks=0,captured=0,applied=0,unchanged=0,invalid=0,queue_failures=0;
char message[160]="Offline";
void say(const char* text){std::snprintf(message,sizeof message,"%s",text);if(trace)std::fprintf(stderr,"[network] %s\n",text);}
void log(const char* verb,BwProgressionDelta d){if(trace)std::fprintf(stderr,"[network] %s key=%u value=%u name=%s\n",verb,d.key,d.value,bw_progression_key_name(d.key));}
void publish(){BwNetworkGameSnapshot s{};s.mounted_room_mode=mounted.room_mode;s.mounted=mounted.config;
    s.native_load_authorized=loaded;s.clean_boot_authorized=cold&&native_ready;s.exporting=exporting;s.local_host=server!=nullptr;
    for(uint32_t value:deferred.values)if(value)++s.deferred_updates;
    s.captured=captured;s.applied=applied;s.unchanged=unchanged;s.invalid=invalid;s.queue_failures=queue_failures;
    bw_network_status(session,&s.session);if(server)s.session.local_server_port=bw_network_server_port(server);
    std::snprintf(s.message,sizeof s.message,"%s",message);
    std::lock_guard<std::mutex> lock(mutex);s.pending_command=command.kind!=None;published=s;
}
void stop(){if(session)bw_network_leave(session);if(server){bw_network_server_stop(server);server=nullptr;}exporting=false;known={};deferred={};authoritative={};connection_generation=0;announced=false;reseeding=true;}
void event(const BwGameEvent* e,void*){
    if(e->kind==BW_GAME_EVENT_RESET){
        loaded=e->reset_reason==BW_GAME_RESET_GAME_LOAD;cold=false;native_ready=false;known={};deferred={};dirty=true;announced=false;reseeding=true;
        if(loaded)say("Native room CARD loaded; waiting for gameplay controls");
        else say("State/reset/reload revoked room export; native room CARD load required");
    }else if(e->kind==BW_GAME_EVENT_PLAYER_UPDATED){
        if(e->scene.active&&e->scene.player_valid&&e->scene.controls_ready)native_ready=true;
    }else if(e->kind==BW_GAME_EVENT_INVENTORY_CHANGED||e->kind==BW_GAME_EVENT_PROGRESSION_CHANGED){dirty=true;}
}
bool enqueue(Command kind,const BwNetworkConfig* c,const char* bind){
    if(!c||!bw_network_config_valid(c,nullptr,0))return false;
    if(kind==Host){if(!bind||std::strlen(bind)>=64||!*bind)return false;
        for(const char* p=bind;*p;++p)if(!((*p>='0'&&*p<='9')||*p=='.'))return false;}
    std::lock_guard<std::mutex> lock(mutex);
    if(!prepared||!mounted.room_mode||!bw_network_same_room(&mounted.config,c)||command.kind!=None)return false;
    command={};command.kind=kind;command.config=*c;if(bind)std::snprintf(command.bind,sizeof command.bind,"%s",bind);published.pending_command=true;return true;
}
void run_command(const PendingCommand& c){
    if(c.kind==None)return;
    stop();
    if(c.kind==Leave){say("Left session; room CARD remains mounted");return;}
    if(!session)session=bw_network_create();if(!session){say("Cannot create network session");return;}
    if(c.kind==Host){server=bw_network_server_start(c.bind,c.config.port);if(!server){say("Cannot start local server; choose an available port and restart room routing");return;}}
    auto selected=c.config;if(c.kind==Host)selected.create_room=true;
    if(!bw_network_join(session,&selected)){if(server){bw_network_server_stop(server);server=nullptr;}say("Cannot join mounted room session");return;}
    exporting=true;dirty=true;say(loaded||cold?"Connecting; waiting for native gameplay readiness":"Connecting; native room CARD load required before progress sharing");
}
}
extern "C" bool bw_network_game_prepare(const BwNetworkPreferences* p){
    if(!p||!bw_network_config_valid(&p->config,nullptr,0))return false;
    std::lock_guard<std::mutex> lock(mutex);if(prepared||ever_attached)return false;
    mounted=*p;prepared=true;boot_token=p->room_mode;published={};published.mounted_room_mode=p->room_mode;published.mounted=p->config;
    std::snprintf(published.message,sizeof published.message,"%s",p->room_mode?"Room CARD selected; restart required to change room":"Personal CARD selected; restart required for a room");return true;
}
extern "C" bool bw_network_game_request_join(const BwNetworkConfig* c){return enqueue(Join,c,nullptr);}
extern "C" bool bw_network_game_request_host(const BwNetworkConfig* c,const char* bind){return enqueue(Host,c,bind);}
extern "C" void bw_network_game_request_leave(){std::lock_guard<std::mutex> lock(mutex);
    if(!prepared)return;command={};command.kind=Leave;published.pending_command=true;}
extern "C" void bw_network_game_snapshot(BwNetworkGameSnapshot* s){if(!s)return;std::lock_guard<std::mutex> lock(mutex);*s=published;}
extern "C" void bw_network_game_attach(CPUState* state){
    if(cpu||!state)return;
    {std::lock_guard<std::mutex> lock(mutex);if(!prepared)return;cpu=state;cold=boot_token;boot_token=false;ever_attached=true;}
    const char* env=std::getenv("BLUEWAKE_NETWORK_TRACE");trace=env&&std::strcmp(env,"1")==0;
    if(!mounted.room_mode){publish();return;}
    const uint64_t mask=BW_GAME_EVENT_MASK(BW_GAME_EVENT_RESET)|BW_GAME_EVENT_MASK(BW_GAME_EVENT_PLAYER_UPDATED)|
        BW_GAME_EVENT_MASK(BW_GAME_EVENT_INVENTORY_CHANGED)|BW_GAME_EVENT_MASK(BW_GAME_EVENT_PROGRESSION_CHANGED);
    subscription=bluewake_game_events_subscribe(mask,event,nullptr);
    if(!subscription){cold=false;say("Cannot subscribe to native game events; room sharing disabled");publish();return;}
    session=bw_network_create();if(!session){cold=false;say("Cannot create network worker; room sharing disabled");publish();return;}
    PendingCommand initial;{std::lock_guard<std::mutex> lock(mutex);if(command.kind==None&&cold){
        initial.config=mounted.config;
        initial.kind=mounted.config.create_room&&(!std::strcmp(mounted.config.server,"127.0.0.1")||!std::strcmp(mounted.config.server,"localhost"))?Host:Join;
        std::strcpy(initial.bind,"127.0.0.1");}}
    run_command(initial);publish();
}
extern "C" void bw_network_game_retrace(CPUState* state){
    if(!cpu||state!=cpu)return;
    PendingCommand current;{std::lock_guard<std::mutex> lock(mutex);current=command;command={};}
    run_command(current);++ticks;
    BwNetworkSnapshot net{};bw_network_status(session,&net);
    if(net.generation!=connection_generation){connection_generation=net.generation;dirty=true;known={};announced=false;reseeding=true;}
    BwGameScene scene{};bluewake_game_events_scene(&scene,nullptr,nullptr);
    const bool ready=scene.active&&scene.player_valid&&scene.controls_ready&&!scene.paused&&!scene.event_running&&!scene.transitioning;
    /* Primary native source: d_s_play::phase_00 initializes dSv_info for the
     * opening/new quest; it may enter playable scenes without card_to_memory.
     * Only the one pre-CARD cold-boot token plus an actual completed native
     * player execute can authorize that path. Any later reset revokes it. */
    const bool authorized=loaded||(cold&&native_ready);
    if(exporting&&authorized&&ready&&net.status==BW_NETWORK_CONNECTED){
        if(!announced){say("Connected; sharing permanent progression in mounted room");announced=true;}
        BwNetworkUpdate update;for(unsigned n=0;n<BW_NETWORK_QUEUE&&bw_network_poll(session,&update);++n){
            if(update.generation!=net.generation){++invalid;continue;}if(!bw_progression_valid(update.delta)){++invalid;continue;}
            bw_progression_merge(&authoritative,update.delta);bw_progression_merge(&deferred,update.delta);dirty=true;
        }
        /* Later native awards can assign a lower camera/bow/wallet/capacity
         * directly. Keep room-canonical permanent values after consuming an
         * inbound delta and reconcile once native facts mark RAM dirty. This
         * never bypasses recollection/GTower or native scene apply guards. */
        if(dirty||ticks%60==0)for(uint16_t key=0;key<BW_PROGRESSION_KEYS;++key)if(authoritative.values[key])
            bw_progression_merge(&deferred,{key,authoritative.values[key]});
        for(uint16_t key=0;key<BW_PROGRESSION_KEYS;++key)if(deferred.values[key]){
            const BwProgressionDelta d={key,deferred.values[key]};const auto result=bw_progression_apply(cpu,&scene,d);
            if(result==BW_PROGRESS_DEFERRED)continue;
            deferred.values[key]=0;
            if(result==BW_PROGRESS_INVALID){++invalid;continue;}
            if(result==BW_PROGRESS_APPLIED){++applied;log("applied",d);}else ++unchanged;
            if(!reseeding)bw_progression_merge(&known,d);
        }
        /* Retry unsent facts from RAM; known advances ONLY after queue accepts.
         * Periodic scan also re-seeds a room after server process restart.
         * Whitelist is read only after native room load/cold-boot eligibility. */
        if(dirty||ticks%60==0){BwProgressionState local{};if(bw_progression_snapshot(cpu,&local)){
            dirty=false;for(uint16_t key=0;key<BW_PROGRESSION_KEYS;++key)if(local.values[key]){
                const BwProgressionDelta d={key,local.values[key]};BwProgressionState candidate=known;
                if(!bw_progression_merge(&candidate,d))continue;
                if(!bw_network_submit(session,d)){++queue_failures;dirty=true;break;}
                known=candidate;bw_progression_merge(&authoritative,d);++captured;log("captured",d);
            }
            if(!dirty)reseeding=false;
        }}
        if(ticks%6==0)bw_network_presence(session,&scene);
    }
    publish();
}
extern "C" void bw_network_game_detach(){
    if(subscription){bluewake_game_events_unsubscribe(subscription);subscription=0;}
    stop();bw_network_destroy(session);session=nullptr;cpu=nullptr;loaded=false;cold=false;native_ready=false;dirty=false;
    say("Game detached; native room CARD load required after reattach");publish();
}
