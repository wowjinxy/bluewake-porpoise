#include "hud_customization_draw_plan.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

using gxruntime::gxcore::DrawPlan;
using bluewake::hud::PixelMultiplier;
using bluewake::hud::PlanResult;
using bluewake::hud::transform_plan;
static unsigned checks;
#define CHECK(expr) do { ++checks; if (!(expr)) { std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#expr); std::exit(1); } } while (0)

static DrawPlan native_plan(float left, float right, float top, float bottom) {
    DrawPlan p;
    p.ok=true; p.constants_id=735;
    p.constants.projection[0][0]=2.f/(right-left); p.constants.projection[0][3]=-(right+left)/(right-left);
    p.constants.projection[1][1]=-2.f/(bottom-top); p.constants.projection[1][3]=(bottom+top)/(bottom-top);
    p.constants.projection[2][2]=.00001f; p.constants.projection[3][3]=1;
    p.viewport_valid=true;
    p.viewport[0]=320; p.viewport[1]=-240; p.viewport[2]=16777215;
    p.viewport[3]=662; p.viewport[4]=582; p.viewport[5]=16777215;
    p.scissor_valid=true; p.scissor_x=20; p.scissor_y=30; p.scissor_width=120; p.scissor_height=50;
    p.vertex_count=4; p.draw_tag=42; p.draw_scope=39; p.draw_scope_part=8;
    p.tex_address=0x80500000u; p.has_texture=true;
    p.vertices={1,2,3,4,5,6}; p.indices={0,1,2,2,3,0};
    p.pixel_constants.colors[0][0]=173;
    p.constants.posnormalmatrix[0][0]=.31f; p.constants.transformmatrices[0][0]=3;
    return p;
}

static BwHudDescriptor native_descriptor() {
    BwHudConfig c; bw_hud_config_identity(&c);
    BwHudDescriptor d{}; d.config=c.groups[0]; d.group=BW_HUD_HEARTS;
    d.pane=0x80450000u; d.sequence=1; d.revision=2; d.epoch=d.generation=2;
    d.pivot_x=219; d.pivot_y=40;
    return d;
}

static bool same_plan(const DrawPlan& a, const DrawPlan& b) {
    return std::memcmp(static_cast<const gxruntime::gxcore::DrawPlanFields*>(&a),
                       static_cast<const gxruntime::gxcore::DrawPlanFields*>(&b),sizeof(gxruntime::gxcore::DrawPlanFields))==0 &&
        std::memcmp(&a.constants,&b.constants,sizeof a.constants)==0 &&
        std::memcmp(&a.constants_inputs,&b.constants_inputs,sizeof a.constants_inputs)==0 &&
        a.constants_id==b.constants_id && a.vertices==b.vertices && a.indices==b.indices;
}

static bool same_unowned_plan(DrawPlan a,DrawPlan b) {
#ifdef GXCORE_HUD_COLOR_PROTOTYPE
    a.pipeline.shader.hud_tint=b.pipeline.shader.hud_tint=0;
    for(float& c:a.pixel_constants.hud_multiplier)c=1;
    for(float& c:b.pixel_constants.hud_multiplier)c=1;
#endif
    return same_plan(a,b);
}

static float clip(const DrawPlan& p,unsigned row,const float xy[2]) {
    const auto& r=p.constants.projection[row]; return r[0]*xy[0]+r[1]*xy[1]+r[3];
}

static void test_affine(float left,float right,float top,float bottom) {
    auto plan=native_plan(left,right,top,bottom); const auto original=plan;
    auto d=native_descriptor(); d.config.scale=1.75f; d.config.offset_x=83.25f; d.config.offset_y=-15.5f;
    PixelMultiplier multiplier; CHECK(transform_plan(&d,plan,multiplier)==PlanResult::Applied);
    CHECK(!multiplier.enabled); CHECK(plan.constants_id==0);
    /* Actual row-dot GXCore projection composes identically to the logical
     * affine across native orthographic/aspect layouts and several positions. */
    for (const std::array<float,2> point : {std::array<float,2>{20,30},{219,40},{620,400},{-70,-15}}) {
        float logical[2]={point[0],point[1]}; bw_hud_transform_point(&d,logical);
        const float before[2]={point[0],point[1]};
        CHECK(std::fabs(clip(plan,0,before)-clip(original,0,logical))<2e-6f);
        CHECK(std::fabs(clip(plan,1,before)-clip(original,1,logical))<2e-6f);
    }
    const auto& p=original.constants.projection;
    const double tx=d.config.offset_x+(1.-d.config.scale)*d.pivot_x;
    const double ty=d.config.offset_y+(1.-d.config.scale)*d.pivot_y;
    const double ax=original.viewport[0]*static_cast<double>(p[0][0]);
    const double ay=original.viewport[1]*static_cast<double>(p[1][1]);
    const double bx=original.viewport[0]*static_cast<double>(p[0][3])+original.viewport[3]-342.;
    const double by=original.viewport[1]*static_cast<double>(p[1][3])+original.viewport[4]-342.;
    const double x0=d.config.scale*original.scissor_x+ax*tx+(1.-d.config.scale)*bx;
    const double y0=d.config.scale*original.scissor_y+ay*ty+(1.-d.config.scale)*by;
    const double x1=d.config.scale*(original.scissor_x+original.scissor_width)+ax*tx+(1.-d.config.scale)*bx;
    const double y1=d.config.scale*(original.scissor_y+original.scissor_height)+ay*ty+(1.-d.config.scale)*by;
    CHECK(plan.scissor_x==std::floor(x0)); CHECK(plan.scissor_y==std::floor(y0));
    CHECK(plan.scissor_width==std::ceil(x1)-std::floor(x0)); CHECK(plan.scissor_height==std::ceil(y1)-std::floor(y0));
    /* Restore ONLY fields the adapter owns, then compare all other actual
     * donor plan state, including vertices, textures, TEV, matrices and tags. */
    std::memcpy(plan.constants.projection,original.constants.projection,sizeof plan.constants.projection);
    plan.constants_id=original.constants_id;
    plan.scissor_x=original.scissor_x; plan.scissor_y=original.scissor_y;
    plan.scissor_width=original.scissor_width; plan.scissor_height=original.scissor_height;
    CHECK(same_plan(plan,original));
}

int main() {
    auto plan=native_plan(-9,659,-21,524); const auto identity=plan;
    auto d=native_descriptor(); PixelMultiplier tint;
    CHECK(transform_plan(&d,plan,tint)==PlanResult::Unchanged); CHECK(same_plan(plan,identity));
    CHECK(!tint.enabled);
    /* Aspect fixtures validate composition math, not game/native pixels. */
    test_affine(-9,659,-21,524); test_affine(-119,769,-21,524); test_affine(-61,711,-21,524);
    d.config.opacity=.5f; d.config.tint[0]=128; d.config.tint[2]=32; d.config.tint[3]=191;
    CHECK(transform_plan(&d,plan,tint)==PlanResult::Applied); CHECK(tint.enabled);
    CHECK(same_unowned_plan(plan,identity)); /* only owned final-pixel fields change */
    float rgba[4]={.25f,.5f,.75f,.3f}; const float native[4]={.25f,.5f,.75f,.3f};
    bw_hud_final_color(&d,rgba);
    for(unsigned i=0;i<4;++i) CHECK(std::fabs(rgba[i]-native[i]*tint.rgba[i])<1e-7f);
    /* The following untagged/identity draw resets final-pixel multiplier. */
    CHECK(transform_plan(nullptr,plan,tint)==PlanResult::Unchanged); CHECK(!tint.enabled);
    for(float x:tint.rgba) CHECK(x==1.f);
    CHECK(same_plan(plan,identity));
    d.config.visible=false; CHECK(transform_plan(&d,plan,tint)==PlanResult::Suppressed);
    CHECK(!tint.enabled); CHECK(same_plan(plan,identity));
    d.config.visible=true; d.config.opacity=0; CHECK(transform_plan(&d,plan,tint)==PlanResult::Suppressed);
    CHECK(same_plan(plan,identity));
    d=native_descriptor(); d.config.offset_x=10;
    plan.constants.projection[3][2]=-1.f; const auto perspective=plan;
    CHECK(transform_plan(&d,plan,tint)==PlanResult::Unsupported); CHECK(same_plan(plan,perspective));
    plan=identity; plan.viewport[0]=std::numeric_limits<float>::quiet_NaN(); const auto invalidViewport=plan;
    CHECK(transform_plan(&d,plan,tint)==PlanResult::Unsupported); CHECK(same_plan(plan,invalidViewport));
    plan=identity; plan.scissor_width=-1; const auto invalidClip=plan;
    CHECK(transform_plan(&d,plan,tint)==PlanResult::Unsupported); CHECK(same_plan(plan,invalidClip));
    plan=identity; d.pivot_x=std::numeric_limits<float>::infinity();
    CHECK(transform_plan(&d,plan,tint)==PlanResult::Unsupported); CHECK(same_plan(plan,identity));
    d=native_descriptor(); d.config.scale=2; d.pivot_x=1e30f;
    CHECK(transform_plan(&d,plan,tint)==PlanResult::Unsupported); CHECK(same_plan(plan,identity));
    /* Reapplying to a fresh native draw each frame has no cumulative drift. */
    d=native_descriptor(); d.config.offset_x=-95.5f;
    auto expected=identity; CHECK(transform_plan(&d,expected,tint)==PlanResult::Applied);
    for(unsigned frame=0;frame<1000;++frame) {
        plan=identity; CHECK(transform_plan(&d,plan,tint)==PlanResult::Applied);
        CHECK(same_plan(plan,expected));
    }
    std::printf("Actual GXCore DrawPlan identity, affine, scissor and isolation fixtures: %u checks PASS\n",checks);
    return 0;
}
