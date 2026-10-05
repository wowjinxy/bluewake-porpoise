// Source-only settings fixture adapter; no guest/backend is initialized.
#include "hud_settings_ui_mocks.h"
namespace {
BwHudSettingsUiEffects effects;
BwHudAvailability availability=BW_HUD_AVAILABLE;
}
BwHudSettingsUiEffects bw_hud_settings_ui_effects(){return effects;}
void bw_hud_settings_ui_availability(BwHudAvailability v){availability=v;}
extern "C" bool bw_hud_host_configure(const BwHudConfig* c){
    if(!bw_hud_config_valid(c))return false;++effects.queued_calls;effects.queued=*c;return true;
}
extern "C" bool bw_hud_host_snapshot(BwHudHostStatus* out) {
    if(!out)return false;*out={};out->availability=availability;return true;
}
extern "C" const char* bw_hud_host_availability_name(BwHudAvailability v){
    return v==BW_HUD_UNSUPPORTED_ASPECT?"Unsupported aspect fixture":"Source-only fixture";
}
