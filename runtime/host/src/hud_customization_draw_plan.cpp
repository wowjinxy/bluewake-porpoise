#include "hud_customization_draw_plan.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace bluewake::hud {
namespace {
bool native_orthographic(const gxruntime::gxcore::DrawPlan& plan) {
    const auto& p = plan.constants.projection;
    for (const auto& row : p) for (float v : row) if (!std::isfinite(v)) return false;
    return p[0][0] != 0.f && p[1][1] != 0.f && p[0][1] == 0.f && p[0][2] == 0.f &&
        p[1][0] == 0.f && p[1][2] == 0.f && p[2][0] == 0.f && p[2][1] == 0.f &&
        p[3][0] == 0.f && p[3][1] == 0.f && p[3][2] == 0.f && p[3][3] > 0.f;
}
bool identity(const BwHudGroupConfig& c) {
    return c.offset_x == 0.f && c.offset_y == 0.f && c.scale == 1.f && c.opacity == 1.f &&
        c.visible && c.tint[0] == 255 && c.tint[1] == 255 && c.tint[2] == 255 && c.tint[3] == 255;
}
bool scissor_int(double value) {
    return std::isfinite(value) && value >= -1048576. && value <= 1048576.;
}
} // namespace

PlanResult transform_plan(const BwHudDescriptor* d, gxruntime::gxcore::DrawPlan& plan,
                          PixelMultiplier& tint) {
    tint = PixelMultiplier{};
#ifdef GXCORE_HUD_COLOR_PROTOTYPE
    plan.pipeline.shader.hud_tint = 0u;
    for (float& channel : plan.pixel_constants.hud_multiplier) channel = 1.f;
#endif
    if (!d) return PlanResult::Unchanged;
    BwHudConfig validation;
    bw_hud_config_identity(&validation);
    if (static_cast<unsigned>(d->group) >= BW_HUD_GROUP_COUNT) return PlanResult::Unsupported;
    validation.groups[d->group] = d->config;
    if (!bw_hud_config_valid(&validation) || !std::isfinite(d->pivot_x) || !std::isfinite(d->pivot_y) ||
        std::fabs(d->pivot_x) > 8192.f || std::fabs(d->pivot_y) > 8192.f)
        return PlanResult::Unsupported;
    if (identity(d->config)) return PlanResult::Unchanged;
    if (!bw_hud_descriptor_visible(d)) return PlanResult::Suppressed;
    if (!plan.ok || !native_orthographic(plan)) return PlanResult::Unsupported;

    const float scale = d->config.scale;
    const float tx = d->config.offset_x + (1.f - scale) * d->pivot_x;
    const float ty = d->config.offset_y + (1.f - scale) * d->pivot_y;
    const bool affine = scale != 1.f || tx != 0.f || ty != 0.f;
    std::int32_t sx = plan.scissor_x, sy = plan.scissor_y;
    std::int32_t sw = plan.scissor_width, sh = plan.scissor_height;
    if (affine && plan.scissor_valid) {
        if (!plan.viewport_valid || sw < 0 || sh < 0) return PlanResult::Unsupported;
        for (float v : plan.viewport) if (!std::isfinite(v)) return PlanResult::Unsupported;
        if (plan.viewport[0] <= 0.f || plan.viewport[1] >= 0.f) return PlanResult::Unsupported;
        const auto& p = plan.constants.projection;
        const double ax = static_cast<double>(plan.viewport[0]) * p[0][0] / p[3][3];
        const double ay = static_cast<double>(plan.viewport[1]) * p[1][1] / p[3][3];
        const double bx = static_cast<double>(plan.viewport[0]) * p[0][3] / p[3][3] + plan.viewport[3] - 342.;
        const double by = static_cast<double>(plan.viewport[1]) * p[1][3] / p[3][3] + plan.viewport[4] - 342.;
        const double left = std::floor(scale * sx + ax * tx + (1. - scale) * bx);
        const double top = std::floor(scale * sy + ay * ty + (1. - scale) * by);
        const double right = std::ceil(scale * (static_cast<double>(sx) + sw) + ax * tx + (1. - scale) * bx);
        const double bottom = std::ceil(scale * (static_cast<double>(sy) + sh) + ay * ty + (1. - scale) * by);
        if (!scissor_int(left) || !scissor_int(top) || !scissor_int(right) || !scissor_int(bottom) ||
            !scissor_int(right-left) || !scissor_int(bottom-top)) return PlanResult::Unsupported;
        sx = static_cast<std::int32_t>(left); sy = static_cast<std::int32_t>(top);
        sw = static_cast<std::int32_t>(right-left); sh = static_cast<std::int32_t>(bottom-top);
    }
    /* Compose P*A using the actual row-dot shader convention. This moves
     * native geometry AFTER its authored draw matrices, before projection,
     * without changing normal/lighting/UV/position matrix banks. */
    if (affine) {
        auto& p = plan.constants.projection;
        for (unsigned row = 0; row < 4u; ++row) {
            p[row][3] += p[row][0] * tx + p[row][1] * ty;
            p[row][0] *= scale; p[row][1] *= scale;
        }
        plan.scissor_x = sx; plan.scissor_y = sy;
        plan.scissor_width = sw; plan.scissor_height = sh;
        plan.constants_id = 0;
    }
    for (unsigned i = 0; i < 4u; ++i) tint.rgba[i] = static_cast<float>(d->config.tint[i]) / 255.f;
    tint.rgba[3] *= d->config.opacity;
    tint.enabled = tint.rgba[0] != 1.f || tint.rgba[1] != 1.f || tint.rgba[2] != 1.f || tint.rgba[3] != 1.f;
#ifdef GXCORE_HUD_COLOR_PROTOTYPE
    /* Only the INACTIVE donor draft defines this ABI. The unchanged cached
     * renderer still receives tint as a separate contract, never vertex RGBA. */
    plan.pipeline.shader.hud_tint = tint.enabled ? 1u : 0u;
    if (tint.enabled) std::memcpy(plan.pixel_constants.hud_multiplier, tint.rgba, sizeof tint.rgba);
#endif
    return PlanResult::Applied;
}
} // namespace bluewake::hud
