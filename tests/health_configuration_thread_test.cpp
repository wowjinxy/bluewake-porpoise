// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "health_host.h"
#include "game_events.h"
#include <atomic>
#include <cassert>
#include <thread>
#include <cstdio>
/* No CPU or events are ever attached. Only actual public configuration CABI
 * is exercised concurrently. Unsynchronized mutable Settings is not used. */
extern "C" BwGameEventSubscription bluewake_game_events_subscribe(uint64_t,BwGameEventCallback,void*){assert(false);return 0;}
extern "C" bool bluewake_game_events_unsubscribe(BwGameEventSubscription){assert(false);return false;}
extern "C" bool bluewake_game_events_scene(BwGameScene*,uint64_t*,uint64_t*){assert(false);return false;}
extern "C" void bluewake_game_events_stats(BwGameEventStats*){assert(false);}
int main(){
    assert(bw_health_host_prepare_room(false));assert(bw_health_host_configure(512,128));std::atomic<bool> done{false};
    std::thread writer([&]{for(unsigned i=0;i<100000;++i)assert(bw_health_host_configure(i&1?512:1024,i&1?128:64));done=true;});
    unsigned reads=0;while(!done.load()){const auto c=bw_health_host_configuration();assert((c.damage_q8==512&&c.healing_q8==128)||(c.damage_q8==1024&&c.healing_q8==64));++reads;}writer.join();
    assert(bw_health_host_configure(256,256));assert(bw_health_host_prepare_room(true));
    std::thread blocked([&]{for(unsigned i=0;i<100000;++i){assert(!bw_health_host_configure(512,128));assert(bw_health_host_configure(256,256));}});
    for(unsigned i=0;i<100000;++i){const auto c=bw_health_host_configuration();assert(c.damage_q8==256&&c.healing_q8==256);assert(bw_health_host_room_locked());}
    blocked.join();assert(bw_health_host_prepare_room(false));
    std::printf("Health copied atomic mailbox100000 paired edits +100000 native-only room attempts PASS (%u concurrent reads)\n",reads);
}
