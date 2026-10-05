#ifdef NDEBUG
#undef NDEBUG
#endif
#include "sprint.h"
#include "controls_bindings.h"
#include "game_events.h"
#include <SDL3/SDL.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <cstddef>
#include <vector>
static unsigned reads,writes,input_reads;
static BluewakeControlsActions input{};
static bool managed=true,fallback_click=false,fallback_diagonal=false;
static bool fallback_keys[SDL_SCANCODE_COUNT]{};
static u32 load_be(const unsigned char* p){return u32(p[0])<<24|u32(p[1])<<16|u32(p[2])<<8|p[3];}
static void store_be(unsigned char* p,u32 v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
extern "C" u32 mem_read32(CPUState* cpu,u32 a){++reads;assert(a==0x8035CEECu||a==0x8035CF1Cu);if(cpu->ram){const u32 offset=a-0x80000000u;assert(offset+4<=cpu->ram_size);return load_be(cpu->ram+offset);}return a==0x8035CEECu?cpu->speed:cpu->rate;}
extern "C" void mem_write32(CPUState* cpu,u32 a,u32 v){++writes;assert(a==0x8035CEECu||a==0x8035CF1Cu);if(cpu->ram){const u32 offset=a-0x80000000u;assert(offset+4<=cpu->ram_size);store_be(cpu->ram+offset,v);return;}(a==0x8035CEECu?cpu->speed:cpu->rate)=v;}
extern "C" bool bluewake_controls_read_actions(BluewakeControlsActions* out){++input_reads;*out=input;return managed;}
extern "C" void bluewake_controls_cancel_actions(){const auto epoch=input.generation+1;input={};input.generation=epoch;}
extern "C" const bool* SDL_GetKeyboardState(int* count){assert(!managed&&"Managed input fell through to native keyboard");*count=SDL_SCANCODE_COUNT;return fallback_keys;}
extern "C" SDL_JoystickID* SDL_GetGamepads(int* count){assert(!managed&&"Managed input enumerated raw devices");static SDL_JoystickID id=1;*count=1;return &id;}
extern "C" SDL_Gamepad* SDL_GetGamepadFromID(SDL_JoystickID id){assert(!managed&&id==1);return reinterpret_cast<SDL_Gamepad*>(uintptr_t(1));}
extern "C" bool SDL_GetGamepadButton(SDL_Gamepad*,SDL_GamepadButton button){assert(!managed&&button==SDL_GAMEPAD_BUTTON_LEFT_STICK);return fallback_click;}
extern "C" Sint16 SDL_GetGamepadAxis(SDL_Gamepad*,SDL_GamepadAxis axis){assert(!managed&&(axis==SDL_GAMEPAD_AXIS_LEFTX||axis==SDL_GAMEPAD_AXIS_LEFTY));return fallback_diagonal?32767:0;}
extern "C" void SDL_free(void*){}
extern "C" float SDL_sqrtf(float value){return std::sqrt(value);}
static u32 bits(float f){u32 b;std::memcpy(&b,&f,sizeof b);return b;}
static float value(u32 b){float f;std::memcpy(&f,&b,sizeof f);return f;}
static void tick(){bluewake_sprint_retrace();}
static const unsigned char* old_ram;
static unsigned restored_before_replace, load_callbacks, reset_callbacks, failure_reset_callbacks;
static bool allow_native_reset;
static CPUState* failure_reset_owner;
static void host_enhancement_reset(const BwGameEvent*,void*);
static void host_audio_diagnostics_report(const char* reason){assert(!std::strcmp(reason,"state-replace")||(allow_native_reset&&!std::strcmp(reason,"native-reset")));}
static void host_audio_owner_attach(CPUState* cpu){assert(!cpu||(allow_native_reset&&cpu==failure_reset_owner));}
static void bw_hud_host_suspend(){}
static void bw_health_host_suspend(){}
static void* state_memcpy(void* dst,const void* src,size_t size){
 if(old_ram&&dst==old_ram){assert(load_be(old_ram+0x35CEEC)==bits(17.f));assert(load_be(old_ram+0x35CF1C)==bits(2.3f));++restored_before_replace;}
 return std::memcpy(dst,src,size);
}
static void host_song_owner_revoke(){}
extern "C" void bluewake_game_events_reset(CPUState* cpu,BwGameResetReason reason){
 if(reason==BW_GAME_RESET_STATE_LOAD){assert(cpu);++reset_callbacks;return;/* Deliberately no Sprint reset here. */}
 assert(!cpu&&reason==BW_GAME_RESET_MODULE_RELOAD&&allow_native_reset&&failure_reset_owner);
 // Actual reset_internal first leaves the old scene, then emits RESET;
 // subscriptions survive reset and retain their existing user pointers.
 BwGameEvent event{};event.kind=BW_GAME_EVENT_SCENE_LEAVING;host_enhancement_reset(&event,failure_reset_owner);++failure_reset_callbacks;
 event.kind=BW_GAME_EVENT_RESET;event.reset_reason=reason;host_enhancement_reset(&event,failure_reset_owner);++failure_reset_callbacks;
}
static void loaded_callback(CPUState*){++load_callbacks;const auto r=reads,w=writes;tick();assert(reads==r&&writes==w);}
static void state_replace(CPUState* cpu,const CPUState* restored,const unsigned char* image){
 struct Chunk {const void* data;} cpu_chunk{restored},ram_chunk{image};
 const Chunk* chunk=&cpu_chunk;const Chunk* mem1=&ram_chunk;
 struct {size_t cpu_pod_size;} saved{offsetof(CPUState,ram)};
 #define memcpy state_memcpy
 #include "sprint_state_replace_under_test.inc"
 #undef memcpy
}
static void state_complete(CPUState* cpu){
 struct Module {void(*on_state_loaded)(CPUState*);} module{loaded_callback};const auto* mod=&module;
 #include "sprint_state_complete_under_test.inc"
}
static void repeat_main_prelude(){
 #include "sprint_main_prelude_under_test.inc"
}
static void state_lifetime_test(){
 std::vector<unsigned char> ram(0x400000),restored_ram(ram.size());
 store_be(ram.data()+0x35CEEC,bits(17.f));store_be(ram.data()+0x35CF1C,bits(2.3f));
 store_be(restored_ram.data()+0x35CEEC,bits(34.f));store_be(restored_ram.data()+0x35CF1C,bits(4.6f));
 CPUState cpu{0,0,0x12345678,ram.data(),u32(ram.size())},saved{0,0,0x87654321};
 input={};input.generation=70;bluewake_sprint_touch(false);
 assert(bluewake_sprint_configure_modes(BW_SPRINT_HOLD,BW_SPRINT_HOLD));bluewake_sprint_attach(&cpu);tick();tick();
 input.sprint_held=true;tick();assert(load_be(ram.data()+0x35CEEC)==bits(25.5f));
 old_ram=ram.data();state_replace(&cpu,&saved,restored_ram.data());old_ram=nullptr;
 assert(restored_before_replace==1&&cpu.sentinel==saved.sentinel);
 const auto detached_reads=reads,detached_writes=writes,detached_inputs=input_reads;tick();assert(reads==detached_reads&&writes==detached_writes&&input_reads==detached_inputs);
 state_complete(&cpu);assert(load_callbacks==1&&reset_callbacks==1);tick();tick();
 input.sprint_held=false;tick();input.sprint_held=true;tick();
 assert(load_be(ram.data()+0x35CEEC)==bits(51.f));assert(std::abs(value(load_be(ram.data()+0x35CF1C))-6.9f)<.001f);
 bluewake_sprint_cancel();assert(load_be(ram.data()+0x35CEEC)==bits(34.f));assert(load_be(ram.data()+0x35CF1C)==bits(4.6f));
 // Partial restore: no completion means no borrowed CPU can survive.
 state_replace(&cpu,&saved,restored_ram.data());const auto failed_reads=reads,failed_inputs=input_reads;tick();assert(reads==failed_reads&&input_reads==failed_inputs);
 state_complete(&cpu);tick();tick();input.sprint_held=true;tick();
 // Repeated main must detach before touching a CPU whose old storage vanished.
 cpu.ram=reinterpret_cast<unsigned char*>(uintptr_t(1));const auto lost_reads=reads,lost_writes=writes,lost_inputs=input_reads;
 repeat_main_prelude();tick();assert(reads==lost_reads&&writes==lost_writes&&input_reads==lost_inputs);bluewake_sprint_reset(nullptr);
}
static unsigned unrelated_resets;
static int g_song_host;
static void bluewake_autosave_note_native_save(CPUState*){++unrelated_resets;}
static void bw_song_host_revoke(int*){++unrelated_resets;}
static void bluewake_quick_items_reset(CPUState*){++unrelated_resets;}
static void bluewake_dialogue_speed_reset(CPUState*){++unrelated_resets;}
static void bluewake_enhancement_hooks_reset(CPUState*){++unrelated_resets;}
static void bw_health_host_reset(){++unrelated_resets;}
static void bluewake_autosave_reset(CPUState*,bool){++unrelated_resets;}
#include "sprint_scene_callback_under_test.inc"
static BwGameEventSubscription g_enhancement_reset_subscription;
static uint64_t subscription_mask;
static CPUState* subscription_owner;
extern "C" BwGameEventSubscription bluewake_game_events_subscribe(uint64_t mask,BwGameEventCallback callback,void* user){
 assert(callback==host_enhancement_reset&&user==subscription_owner);subscription_mask=mask;return 1;
}
static void scene_subscription(CPUState& cpu){
 #include "sprint_scene_subscription_under_test.inc"
}
static void scene_lifetime_test(){
 CPUState cpu{bits(17.f),bits(2.3f),0xAABBCCDD};subscription_owner=&cpu;scene_subscription(cpu);
 const uint64_t wanted=BW_GAME_EVENT_MASK(BW_GAME_EVENT_RESET)|BW_GAME_EVENT_MASK(BW_GAME_EVENT_SAVE_COMPLETED)|
     BW_GAME_EVENT_MASK(BW_GAME_EVENT_SCENE_LEAVING)|BW_GAME_EVENT_MASK(BW_GAME_EVENT_TRANSITION_STARTED)|BW_GAME_EVENT_MASK(BW_GAME_EVENT_SCENE_ENTERED);
 assert(subscription_mask==wanted&&g_enhancement_reset_subscription==1);
 for(auto kind:{BW_GAME_EVENT_SCENE_LEAVING,BW_GAME_EVENT_TRANSITION_STARTED,BW_GAME_EVENT_SCENE_ENTERED}){
  input={};input.generation+=100;cpu.speed=bits(17.f);cpu.rate=bits(2.3f);bluewake_sprint_touch(false);
  assert(bluewake_sprint_configure_modes(BW_SPRINT_TOGGLE,BW_SPRINT_TOGGLE));bluewake_sprint_attach(&cpu);tick();tick();
  input.sprint_held=true;input.sprint_keyboard_pressed=true;tick();assert(value(cpu.speed)==25.5f);
  BwGameEvent event{};event.kind=kind;host_enhancement_reset(&event,&cpu);
  assert(cpu.speed==bits(17.f)&&cpu.rate==bits(2.3f)&&unrelated_resets==0);
  // A native replacement baseline must be recaptured. Held/new-epoch press
  // cannot relatch before release, even if the source remained physically down.
  cpu.speed=bits(34.f);cpu.rate=bits(4.6f);input.sprint_held=true;input.sprint_keyboard_pressed=true;
  tick();input.sprint_keyboard_pressed=false;tick();tick();assert(value(cpu.speed)==34.f);
  input.sprint_held=false;tick();input.sprint_held=true;input.sprint_keyboard_pressed=true;tick();assert(value(cpu.speed)==51.f);
  bluewake_sprint_cancel();assert(value(cpu.speed)==34.f);bluewake_sprint_reset(nullptr);
 }
 _putenv_s("BLUEWAKE_SPRINT_SPEED","1");bluewake_sprint_attach(&cpu);
 const auto r=reads,w=writes,i=input_reads;BwGameEvent event{};event.kind=BW_GAME_EVENT_SCENE_ENTERED;
 host_enhancement_reset(&event,&cpu);tick();assert(reads==r&&writes==w&&input_reads==i);bluewake_sprint_reset(nullptr);
}
static void failed_load_after_reset(){
 #include "sprint_failed_load_after_reset_under_test.inc"
}
static void failed_load_notification_test(){
 _putenv_s("BLUEWAKE_SPRINT_SPEED","1.5");CPUState cpu{bits(17.f),bits(2.3f),0x11223344};
 input={};input.generation=500;assert(bluewake_sprint_configure_modes(BW_SPRINT_HOLD,BW_SPRINT_HOLD));
 bluewake_sprint_attach(&cpu);tick();tick();input.sprint_held=true;tick();assert(value(cpu.speed)==25.5f);
 bluewake_sprint_cancel();bluewake_sprint_reset(nullptr);assert(value(cpu.speed)==17.f);
 failure_reset_owner=&cpu;allow_native_reset=true;failed_load_after_reset();allow_native_reset=false;failure_reset_owner=nullptr;
 assert(failure_reset_callbacks==2&&unrelated_resets>0);
 const auto r=reads,w=writes,i=input_reads;tick();assert(reads==r&&writes==w&&input_reads==i);bluewake_sprint_reset(nullptr);
}
int main(){
 CPUState cpu{bits(17.f),bits(2.3f),0xAA55AA55};const CPUState original=cpu;
 _putenv_s("BLUEWAKE_SPRINT_SPEED","1");_putenv_s("BLUEWAKE_SPRINT_TRACE","0");
 bluewake_sprint_attach(&cpu);for(int i=0;i<20;++i)tick();
 assert(reads==0&&writes==0&&input_reads==0&&!std::memcmp(&cpu,&original,sizeof cpu));
 _putenv_s("BLUEWAKE_SPRINT_SPEED","nan");bluewake_sprint_attach(&cpu);tick();assert(reads==0&&writes==0);
 _putenv_s("BLUEWAKE_SPRINT_SPEED","1.5");bluewake_sprint_attach(&cpu);tick();tick();
 assert(writes==0);input.sprint_held=true;tick();assert(value(cpu.speed)==25.5f&&std::abs(value(cpu.rate)-3.45f)<.001f);
 // A UI/worker reload must publish only. Restoration happens on game retrace.
 const auto reload_reads=reads,reload_writes=writes,reload_inputs=input_reads;
 _putenv_s("BLUEWAKE_SPRINT_SPEED","1");
 std::thread worker([]{bluewake_sprint_reload();});worker.join();
 assert(reads==reload_reads&&writes==reload_writes&&input_reads==reload_inputs);
 assert(value(cpu.speed)==25.5f);tick();assert(cpu.speed==original.speed&&cpu.rate==original.rate);
 const auto disabled_reads=reads,disabled_writes=writes,disabled_inputs=input_reads;
 for(int i=0;i<20;++i)tick();
 assert(reads==disabled_reads&&writes==disabled_writes&&input_reads==disabled_inputs);
 _putenv_s("BLUEWAKE_SPRINT_SPEED","1.5");bluewake_sprint_reload();tick();tick();
 input.sprint_held=false;tick();input.sprint_held=true;tick();assert(value(cpu.speed)==25.5f);
 input.sprint_held=false;tick();assert(cpu.speed==original.speed&&cpu.rate==original.rate);
 assert(bluewake_sprint_configure_modes(BW_SPRINT_TOGGLE,BW_SPRINT_HOLD));tick();tick();
 input.sprint_keyboard_pressed=true;tick();input.sprint_keyboard_pressed=false;tick();assert(value(cpu.speed)==25.5f);
 input.blocked=true;tick();assert(cpu.speed==original.speed&&cpu.rate==original.rate);input.blocked=false;tick();tick();
 bluewake_sprint_touch(true);tick();assert(value(cpu.speed)==25.5f);bluewake_sprint_cancel();assert(cpu.speed==original.speed);
 // Holding touch across suspension cannot relatch; release/new press works.
 tick();tick();assert(cpu.speed==original.speed);bluewake_sprint_touch(false);tick();bluewake_sprint_touch(true);tick();assert(value(cpu.speed)==25.5f);
 // New native baseline at a machine boundary is preserved, not overwritten.
 cpu.speed=bits(34.f);cpu.rate=bits(4.6f);const auto count=writes;bluewake_sprint_reset(&cpu);
 assert(writes==count&&value(cpu.speed)==34.f&&value(cpu.rate)==4.6f);
 bluewake_sprint_touch(false);tick();tick();input.sprint_controller_down=true;tick();assert(value(cpu.speed)==51.f);
 bluewake_sprint_reset(&cpu);assert(value(cpu.speed)==34.f);tick();tick();assert(value(cpu.speed)==34.f);
 // Actual unmanaged read_pads computes sqrt(2) from a full diagonal. Toggle
 // must remain active beyond idle8 until that movement actually returns idle.
 managed=false;assert(bluewake_sprint_configure_modes(BW_SPRINT_HOLD,BW_SPRINT_TOGGLE));
 bluewake_sprint_touch(false);bluewake_sprint_attach(&cpu);tick();tick();
 fallback_diagonal=true;fallback_click=true;tick();assert(value(cpu.speed)==51.f);
 fallback_click=false;for(int i=0;i<12;++i){tick();assert(value(cpu.speed)==51.f);}
 fallback_diagonal=false;for(int i=0;i<7;++i){tick();assert(value(cpu.speed)==51.f);}
 tick();assert(value(cpu.speed)==34.f);managed=true;
 assert(cpu.sentinel==original.sentinel);const auto detached_reads=reads;bluewake_sprint_reset(nullptr);tick();assert(reads==detached_reads);
 state_lifetime_test();
 scene_lifetime_test();
 failed_load_notification_test();
 puts("PASS actual Sprint C boundary: disabled zero guest/input work, native parameter restoration and managed-only sources");
}
