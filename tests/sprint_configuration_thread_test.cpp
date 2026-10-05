#ifdef NDEBUG
#undef NDEBUG
#endif
#include "sprint_input.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>
int main(){
 std::atomic<bool> done{false};
 std::thread writer([&](){for(unsigned i=0;i<50000;++i)
   assert(bluewake_sprint_configure_modes(i%2?BW_SPRINT_HOLD:BW_SPRINT_TOGGLE,i%2?BW_SPRINT_TOGGLE:BW_SPRINT_HOLD));
   done.store(true,std::memory_order_release);});
 BwSprintInputState state{};BwSprintInput input{};input.generation=1;input.controller_tilt=1;
 unsigned reads=0;
 do {BwSprintMode key,pad;bluewake_sprint_modes(&key,&pad);assert(key!=pad);
   (void)bw_sprint_input_step(&state,&input);++reads;
 }while(!done.load(std::memory_order_acquire));
 writer.join();assert(reads);puts("PASS atomic mode-pair publication during game-thread state sampling");
}
