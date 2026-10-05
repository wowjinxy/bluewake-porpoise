#ifndef BLUEWAKE_HUD_CUSTOMIZATION_DRAW_PLAN_H
#define BLUEWAKE_HUD_CUSTOMIZATION_DRAW_PLAN_H

#include "hud_customization.h"
#include "gxruntime/gxcore/shader.hpp"

namespace bluewake::hud {

/* Exact current GXCore DrawPlan fixture/adapter, no renderer or GPU linkage.
 * pixel_multiplier must be copied into BOTH generated and Uber FINAL fragment
 * paths before integration. Current unmodified donor has no such uniform;
 * changing vertex colors would fail opaque native J2DPicture TEV draws. */
struct PixelMultiplier {
    float rgba[4]{1.f, 1.f, 1.f, 1.f};
    bool enabled = false;
};
enum class PlanResult { Unchanged, Applied, Suppressed, Unsupported };

/* Invoke only on the copied plan, before interpolation and uniform dedup.
 * Orthographic projection is composed with the native logical affine. Scissor
 * follows the same projection/viewport mapping; camera, vertices, textures,
 * TEV state, position/normal/texture matrices remain untouched. Altered vertex
 * constants invalidate constants_id (0 forces byte-based cache comparison).
 * Every call resets pixel_multiplier, including untagged/unrelated draws. */
PlanResult transform_plan(const BwHudDescriptor* descriptor,
                          gxruntime::gxcore::DrawPlan& plan,
                          PixelMultiplier& pixel_multiplier);

} // namespace bluewake::hud
#endif
