// SPDX-License-Identifier: GPL-3.0-or-later
#include "hud_renderer.h"
#if defined(BLUEWAKE_HUD_GXCORE) && BLUEWAKE_HUD_GXCORE
#include "hud_customization_draw_plan.h"
#include <gxruntime/gxcore/gxcore.hpp>
#include <gxruntime/aurora_backend.h>
#include <atomic>
namespace {
std::atomic<uint64_t> tagged{0},applied{0},suppressed{0},unsupported{0},malformed{0};
}
extern "C" bool bw_hud_renderer_install(void) {
    if(!dol_aurora_set_gxcore_plan_filter(bw_hud_renderer_filter,nullptr))return false;
    /* transform_plan changes uniforms/scissor/tint only; it never accesses
       decoded vertices, their offsets or their storage lifetime. */
    return dol_aurora_set_gxcore_plan_filter_selective_vertices(true);
}
extern "C" void bw_hud_renderer_stats(BwHudRendererStats* out) {
    if(!out)return;*out={tagged.load(),applied.load(),suppressed.load(),unsupported.load(),malformed.load()};
}
extern "C" bool bw_hud_renderer_filter(void* copied,const void* pending,void*) {
    if(!copied||!pending)return true;
    auto& plan=*static_cast<gxruntime::gxcore::DrawPlan*>(copied);
    const auto& state=*static_cast<const gxruntime::gxcore::GxCoreState*>(pending);
    bluewake::hud::PixelMultiplier multiplier;
    /* Clear owned tint even when an ordinary draw follows a tagged one. */
    bluewake::hud::transform_plan(nullptr,plan,multiplier);
    if(!state.bp_valid(0x6A)||state.bp(0x6A)!=0x10001)return true;
    uint32_t words[16];
    for(unsigned i=0;i<16;++i) {
        const uint8_t reg=static_cast<uint8_t>(0x6A+i);
        if(!state.bp_valid(reg)){++malformed;return true;}words[i]=state.bp(reg);
    }
    BwHudDescriptor descriptor;
    if(!bw_hud_descriptor_decode(words,&descriptor)){++malformed;return true;}
    ++tagged;
    switch(bluewake::hud::transform_plan(&descriptor,plan,multiplier)) {
    case bluewake::hud::PlanResult::Applied:++applied;break;
    case bluewake::hud::PlanResult::Suppressed:++suppressed;return false;
    case bluewake::hud::PlanResult::Unsupported:++unsupported;break;
    case bluewake::hud::PlanResult::Unchanged:break;
    }return true;
}
#else
extern "C" bool bw_hud_renderer_install(void){return false;}
extern "C" void bw_hud_renderer_stats(BwHudRendererStats* out){if(out)*out={};}
extern "C" bool bw_hud_renderer_filter(void*,const void*,void*){return true;}
#endif
