#include "climb.h"

#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GZLE01. The player (daPy_lk_c) is at kPlayerPointer; its fields:
enum {
    kPlayerPointer = 0x803CA74Cu,
    kCameraPointer = 0x803CA718u,
    kEventMode = 0x803C9EA2u,
    kMenuPause = 0x803F7097u,
    kStage = 0x803C9D3Cu,
    kBgRegistry = 0x803C5EA8u,
    kCurrentPos = 0x1F8u,     // fopAc_ac_c::current.pos
    kNoResetFlg0 = 0x29Cu,    // daPyFlg0_UNK100 (0x100): the game looks for ivy at all
    kNoResetFlg1 = 0x2A0u,    // daPyFlg1_VINE_CATCH (0x02000000): just let go of a wall
    kLinkLinChkPoly = 0x644u, // mLinkLinChk (0x630)'s cBgS_PolyInfo data (+0x14, 12 bytes)
    kPolyInfo = 0x940u,       // mPolyInfo's data (12 bytes; its vtable follows)
    kDemoType = 0x304u,
    kDemoMode = 0x314u,
    kAcchFlags = 0x494u,
    kAcchPos = 0x498u,
    kCurProc = 0x31D8u,       // mCurProc
    kFrontWallType = 0x34B9u, // mFrontWallType (u8): 1 a plain wall or none, 3 ivy
    kStickDistance = 0x35B0u, // mStickDistance (0..1)
    kLavaHeight = 0x35D4u,    // m35D4: lava or water under him (-inf when none)
    kModeFlg = 0x3618u,       // mModeFlg: MIDAIR 0x2, SWIM 0x40000
};
enum {
    kFrontPlane = 0x8010EEBCu,     // first GetTriPla return, before the native vertical-wall test
    kFrontReprobePlane = 0x8010F01Cu, // ground-level re-probe GetTriPla return
    kClimbPlane = 0x80135FFCu,     // climbing GetTriPla return, before the vertical-wall test
    kFlg0LooksForIvy = 0x100u,
    kFlg1VineCatch = 0x02000000u,
    kModeMidair = 0x2u,
    kModeSwim = 0x40000u,
    kProcClimbFirst = 0x3D, // CLIMB_UP_START, CLIMB_DOWN_START, CLIMB_MOVE_UP_DOWN,
    kProcClimbLast = 0x40,  // CLIMB_MOVE_SIDE
    kWallTypePlain = 1,
    kWallTypeIvy = 3,
};
// Where the hooks are (see climb.h's dispatch). Only calls into another
// translation unit come back through the dispatcher, so the hooks are the
// return sites of setFrontWallType's and setMoveBGCorrectClimb's collision
// queries (dBgS / cBgS), not their own entries.
enum {
    kFrontWallCode = 0x8010F0DCu,  // setFrontWallType's GetWallCode (0x8010F0D8) returns: r3 the code
    kFrontWallAbove = 0x8010F554u, // its LineCross at grabbing height (0x8010F550) returns: r3 a hit
    kClimbWallCode = 0x80135FE4u,  // setMoveBGCorrectClimb's GetWallCode (0x80135FE0) returns
    kPlayerExecute = 0x80122D30u,  // daPy_Execute__FP9daPy_lk_c: r3 the player
    kCameraDraw = 0x8017C350u,     // camera_draw: r3 the camera process (a view_class)
};
// view_class: fovy, aspect, then the eye and centre it is drawn from.
enum { kViewFovy = 0xD0u, kViewAspect = 0xD4u, kViewEye = 0xD8u, kViewCenter = 0xE4u };

static const double kFrameSeconds = 1.0 / 60.0; // guest VI, not render FPS
static const double kHangShare = 0.4;           // holding still costs this share
static const double kRefillSeconds = 3.0;       // empty to full on the ground
static const unsigned kRefillDelayFrames = 30;
static const float kGrabStick = 0.15f;

bool bluewake_climb_on;
static double g_seconds = 12.0; // a full wheel
static double g_stamina = 1.0;  // 0..1
static bool g_exhausted;        // ran out: no grabbing until full again
static bool g_candidate;        // this frame's classification found a plain wall
static u32 g_candidate_player, g_player, g_stage[2];
static unsigned long long g_candidate_frame;
static bool g_plain_contact;
static u32 g_plain_player, g_plain_stage[2];
static bool g_front_support;
static u32 g_front_player, g_front_bgw, g_front_plane, g_front_stage[2];
static u16 g_front_bg_index;
static unsigned long long g_front_frame, g_geometry_admissions[3];
static float g_front_normal[3], g_front_raw_normal[3];
static u8 g_candidate_poly[12]; // ... and the collision it hit
static unsigned long long g_frame, g_plain_climb_frame;
static unsigned g_ground_frames;
static u32 g_last_proc;
static bool g_trace;

// The wheel, for the overlay's thread.
static _Atomic float g_hud_stamina = 1.0f, g_hud_x, g_hud_y, g_hud_aspect = 4.0f / 3.0f, g_hud_alpha;
static _Atomic bool g_hud_exhausted, g_hud_in_view;
static float g_alpha;

static float read_f32(CPUState* cpu, u32 address) {
    const u32 bits = mem_read32(cpu, address);
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static bool guest_span(u32 address, u32 size) {
    return address >= 0x80000000u && size <= 0x01800000u && address <= 0x81800000u - size;
}

static bool player_ready(CPUState* cpu, u32 player) {
    return guest_span(player, kModeFlg + 4u) &&
           mem_read32(cpu, player + kAcchPos) == player + kCurrentPos &&
           mem_read8(cpu, kEventMode) == 0u && mem_read8(cpu, kMenuPause) == 0u &&
           mem_read16(cpu, player + kDemoType) == 0u && mem_read32(cpu, player + kDemoMode) == 0u;
}

// GZLE01 dBgS::GetPolyId1 (800A08C0..800A0A58) uses the registered
// background's triangle and polygon-info tables for GetWallCode. Read the
// same fields with bounds checks; an invalid/unused background is not support.
// No collision plane or triangle is modified, including real ivy's code 1.
static bool wall_code(CPUState* cpu, u32 player, u32* code, u16* bg_index, u32* bgw) {
    const u16 poly = mem_read16(cpu, player + kLinkLinChkPoly);
    const u16 bg = mem_read16(cpu, player + kLinkLinChkPoly + 2u);
    if (bg >= 256u)
        return false;
    const u32 row = kBgRegistry + (u32)bg * 20u;
    if ((mem_read32(cpu, row + 4u) & 1u) == 0u)
        return false;
    const u32 owner = mem_read32(cpu, row);
    if (!guest_span(owner, 0x98u))
        return false;
    const u32 data = mem_read32(cpu, owner + 0x94u);
    if (!guest_span(data, 0x30u))
        return false;
    const u32 poly_count = mem_read32(cpu, data + 8u);
    const u32 triangles = mem_read32(cpu, data + 12u);
    if (poly_count == 0u || poly_count > 0x01800000u / 10u || poly >= poly_count ||
        !guest_span(triangles, poly_count * 10u))
        return false;
    const u16 info = mem_read16(cpu, triangles + (u32)poly * 10u + 6u);
    const u32 info_count = mem_read32(cpu, data + 0x28u);
    const u32 infos = mem_read32(cpu, data + 0x2Cu);
    if (info_count == 0u || info_count > 0x01800000u / 16u || info >= info_count ||
        !guest_span(infos, info_count * 16u))
        return false;
    *code = (mem_read32(cpu, infos + (u32)info * 16u + 4u) >> 8u) & 15u;
    *bg_index = bg;
    *bgw = owner;
    return true;
}

static bool wall_normal(CPUState* cpu, u32 plane, float raw[3], float normal[3]) {
    if (!guest_span(plane, 12u))
        return false;
    float square = 0.0f;
    for (u32 axis = 0u; axis < 3u; ++axis) {
        raw[axis] = read_f32(cpu, plane + axis * 4u);
        if (!isfinite(raw[axis]))
            return false;
        square += raw[axis] * raw[axis];
    }
    // Game collision planes have unit normals; reject malformed records.
    if (!isfinite(square) || square < 0.8f || square > 1.2f)
        return false;
    const float length = sqrtf(square);
    for (u32 axis = 0u; axis < 3u; ++axis)
        normal[axis] = raw[axis] / length;
    // Thirty degrees from vertical still leaves a substantial wall face.
    return fabsf(normal[1]) <= 0.5f + 1e-6f;
}

static void trace_geometry(unsigned kind, u16 bg, const float normal[3]) {
    const unsigned long long count = ++g_geometry_admissions[kind];
    if (g_trace && (count <= 4u || count % 256u == 0u)) {
        static const char* const names[] = {"front-slope", "front-facet", "climb-slope"};
        fprintf(stderr, "[climb-geometry] %s count=%llu bg=%u normal=%.3f,%.3f,%.3f\n",
                names[kind], count, (unsigned)bg, normal[0], normal[1], normal[2]);
    }
}

static void clear_contact(void) {
    g_candidate = false;
    g_candidate_player = 0u;
    g_plain_contact = false;
    g_plain_player = 0u;
    g_front_support = false;
    g_ground_frames = 0u;
}

static void reset(void) {
    clear_contact();
    g_player = 0u;
    g_frame = g_plain_climb_frame = 0u;
    g_stamina = 1.0;
    g_exhausted = false;
    g_alpha = 0.0f;
    atomic_store(&g_hud_stamina, 1.0f);
    atomic_store(&g_hud_exhausted, false);
    atomic_store(&g_hud_alpha, 0.0f);
    atomic_store(&g_hud_in_view, false);
}

static void read_settings(void) {
    const char* on = getenv("BLUEWAKE_CLIMB");
    bluewake_climb_on = on != NULL && on[0] == '1';
    const char* seconds = getenv("BLUEWAKE_CLIMB_STAMINA");
    char* end = NULL;
    const double value = seconds != NULL ? strtod(seconds, &end) : 0.0;
    g_seconds = seconds != NULL && end != seconds && *end == '\0' && isfinite(value) &&
                value >= 1.0 && value <= 30.0 ? value : 12.0;
}

void bluewake_climb_reload(void) {
    read_settings();
    clear_contact();
    if (!bluewake_climb_on)
        reset();
}

void bluewake_climb_attach(CPUState* cpu) {
    (void)cpu;
    reset();
    memset(g_geometry_admissions, 0, sizeof g_geometry_admissions);
    read_settings();
    const char* trace = getenv("BLUEWAKE_CLIMB_TRACE");
    g_trace = trace != NULL && trace[0] == '1';
    if (bluewake_climb_on)
        fprintf(stderr, "[climb] Link climbs any wall, with %.0f seconds of stamina\n", g_seconds);
}

static bool can_grab(void) { return !g_exhausted && g_stamina > 0.0; }

static bool grab_ready(CPUState* cpu, u32 player) {
    if (!player_ready(cpu, player) || !can_grab() ||
        (mem_read32(cpu, player + kNoResetFlg0) & kFlg0LooksForIvy) == 0u ||
        (mem_read32(cpu, player + kNoResetFlg1) & kFlg1VineCatch) != 0u ||
        (mem_read32(cpu, player + kModeFlg) & kModeSwim) != 0u)
        return false;
    const float height = read_f32(cpu, player + kCurrentPos + 4u);
    const float lava = read_f32(cpu, player + kLavaHeight);
    const float stick = read_f32(cpu, player + kStickDistance);
    return isfinite(height) && !isnan(lava) && height - lava >= 125.0f &&
           isfinite(stick) && stick > kGrabStick;
}

// VI time keeps stamina independent of skipped translated entries and display
// interpolation. Contact expires if the guest stops performing climb checks.
static void player_frame(CPUState* cpu, u32 player) {
    const u32 proc = mem_read32(cpu, player + kCurProc);
    const bool climbing = proc >= kProcClimbFirst && proc <= kProcClimbLast;
    // setMoveBGCorrectClimb kept him on a plain wall in his last update.
    const bool plain = climbing && g_plain_contact && g_frame - g_plain_climb_frame <= 4u;
    const u32 mode = mem_read32(cpu, player + kModeFlg);
    const double before = g_stamina;
    if (plain) {
        const float stick = read_f32(cpu, player + kStickDistance);
        const bool still = !isfinite(stick) || stick <= kGrabStick;
        g_stamina -= (still ? kHangShare : 1.0) * kFrameSeconds / g_seconds;
        g_ground_frames = 0;
        if (g_stamina <= 1e-12) {
            g_stamina = 0.0;
            g_exhausted = true;
            if (g_trace)
                fprintf(stderr, "[climb] frame=%llu out of stamina: letting go\n", g_frame);
        }
    } else if ((proc == 4u || proc == 5u || proc == 6u) &&
               (mode & (kModeMidair | kModeSwim)) == 0u &&
               (mem_read32(cpu, player + kAcchFlags) & 0x20u) != 0u) {
        if (++g_ground_frames > kRefillDelayFrames && g_stamina < 1.0) {
            g_stamina += kFrameSeconds / kRefillSeconds;
            if (g_stamina >= 1.0 - 1e-12) {
                g_stamina = 1.0;
                if (g_exhausted && g_trace)
                    fprintf(stderr, "[climb] frame=%llu stamina full again\n", g_frame);
                g_exhausted = false;
            }
        }
    } else {
        g_ground_frames = 0;
    }
    if (g_trace && (proc != g_last_proc || (plain && (int)(before * 10.0) != (int)(g_stamina * 10.0))))
        fprintf(stderr, "[climb] frame=%llu proc=0x%X plain=%d stamina=%.2f%s wheel=%.2f,%.2f%s\n", g_frame, proc,
                plain ? 1 : 0, g_stamina, g_exhausted ? " (exhausted)" : "", atomic_load(&g_hud_x),
                atomic_load(&g_hud_y), atomic_load(&g_hud_in_view) ? "" : " (out of view)");
    g_last_proc = proc;

    // The wheel shows while it is not full, and fades a second after.
    const float target = plain || g_stamina < 1.0 ? 1.0f : 0.0f;
    g_alpha += (target - g_alpha) * (target > g_alpha ? 0.5f : 0.06f);
    if (g_alpha < 0.01f)
        g_alpha = 0.0f;
    atomic_store(&g_hud_stamina, (float)g_stamina);
    atomic_store(&g_hud_exhausted, g_exhausted);
    atomic_store(&g_hud_alpha, g_alpha);
}

// At camera_draw: where Link's shoulder is in this frame's picture.
static void camera_frame(CPUState* cpu, u32 view) {
    atomic_store(&g_hud_in_view, false);
    const u32 player = mem_read32(cpu, kPlayerPointer);
    if (!guest_span(view, kViewCenter + 12u) || !player_ready(cpu, player) || g_alpha == 0.0f)
        return;
    const float fovy = read_f32(cpu, view + kViewFovy), aspect = read_f32(cpu, view + kViewAspect);
    if (!(fovy > 1.0f && fovy < 179.0f && aspect > 0.5f && aspect < 4.0f))
        return;
    float eye[3], center[3], pos[3];
    for (int i = 0; i < 3; ++i) {
        eye[i] = read_f32(cpu, view + kViewEye + 4u * (u32)i);
        center[i] = read_f32(cpu, view + kViewCenter + 4u * (u32)i);
        pos[i] = read_f32(cpu, player + kCurrentPos + 4u * (u32)i);
        if (!isfinite(eye[i]) || !isfinite(center[i]) || !isfinite(pos[i]))
            return;
    }
    pos[1] += 90.0f;
    // A look-at frame: forward, right (y up), up.
    float f[3] = {center[0] - eye[0], center[1] - eye[1], center[2] - eye[2]};
    float n = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (!isfinite(n) || n < 1e-3f)
        return;
    for (int i = 0; i < 3; ++i)
        f[i] /= n;
    float r[3] = {f[1] * 0.0f - f[2] * 1.0f, f[2] * 0.0f - f[0] * 0.0f, f[0] * 1.0f - f[1] * 0.0f};
    n = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
    if (n < 1e-3f)
        return;
    for (int i = 0; i < 3; ++i)
        r[i] /= n;
    const float u[3] = {r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]};
    const float d[3] = {pos[0] - eye[0], pos[1] - eye[1], pos[2] - eye[2]};
    const float z = d[0] * f[0] + d[1] * f[1] + d[2] * f[2];
    if (!isfinite(z) || z < 10.0f) {
        atomic_store(&g_hud_in_view, false);
        return;
    }
    const float t = tanf(fovy * 0.5f * 3.14159265f / 180.0f);
    const float x = (d[0] * r[0] + d[1] * r[1] + d[2] * r[2]) / (z * t * aspect);
    const float y = (d[0] * u[0] + d[1] * u[1] + d[2] * u[2]) / (z * t);
    if (!isfinite(x) || !isfinite(y))
        return;
    atomic_store(&g_hud_x, 0.5f + 0.5f * x);
    atomic_store(&g_hud_y, 0.5f - 0.5f * y);
    atomic_store(&g_hud_aspect, aspect);
    atomic_store(&g_hud_in_view, fabsf(x) < 1.2f && fabsf(y) < 1.2f);
}

void bluewake_climb_retrace(CPUState* cpu) {
    if (!bluewake_climb_on || cpu == NULL)
        return;
    const u32 player = mem_read32(cpu, kPlayerPointer);
    const u32 stage[2] = {mem_read32(cpu, kStage), mem_read32(cpu, kStage + 4u)};
    if (player != g_player || memcmp(stage, g_stage, sizeof stage) != 0) {
        reset();
        g_player = player;
        memcpy(g_stage, stage, sizeof stage);
    }
    ++g_frame;
    if (!player_ready(cpu, player)) {
        clear_contact();
        g_alpha = 0.0f;
        atomic_store(&g_hud_alpha, 0.0f);
        atomic_store(&g_hud_in_view, false);
        return;
    }
    player_frame(cpu, player);
    camera_frame(cpu, mem_read32(cpu, kCameraPointer));
}

bool bluewake_climb_hud(float* fraction, bool* exhausted, float* x, float* y, float* aspect, float* alpha) {
    if (!bluewake_climb_on)
        return false;
    *alpha = atomic_load(&g_hud_alpha);
    if (*alpha <= 0.0f)
        return false;
    *fraction = atomic_load(&g_hud_stamina);
    *exhausted = atomic_load(&g_hud_exhausted);
    *aspect = atomic_load(&g_hud_aspect);
    if (atomic_load(&g_hud_in_view)) {
        *x = atomic_load(&g_hud_x);
        *y = atomic_load(&g_hud_y);
    } else {
        *x = 0.5f; // Link out of the picture: its middle
        *y = 0.45f;
    }
    return true;
}

void bluewake_climb_hook(CPUState* cpu, u32 address) {
    if (cpu == NULL)
        return;
    switch (address) {
    case kFrontPlane: {
        g_front_support = false;
        const u32 player = mem_read32(cpu, kPlayerPointer);
        u32 code, owner;
        u16 bg;
        float raw[3], normal[3];
        if (!bluewake_climb_on || cpu->gpr[29] != player || !grab_ready(cpu, player) ||
            !wall_code(cpu, player, &code, &bg, &owner) || code != 0u ||
            !wall_normal(cpu, cpu->gpr[3], raw, normal))
            break;
        g_front_support = true;
        g_front_player = player;
        g_front_bgw = owner;
        g_front_bg_index = bg;
        g_front_plane = cpu->gpr[3];
        g_front_frame = g_frame;
        g_front_stage[0] = mem_read32(cpu, kStage);
        g_front_stage[1] = mem_read32(cpu, kStage + 4u);
        memcpy(g_front_normal, normal, sizeof normal);
        memcpy(g_front_raw_normal, raw, sizeof raw);
        if (fabsf(raw[1]) > 0.05f) {
            // Preserve the native plane pointer and resume before its facing
            // check. The original collision ray already returned a real hit.
            cpu->gpr[31] = cpu->gpr[3];
            cpu->pc = 0x8010EED8u;
            trace_geometry(0u, bg, normal);
        }
        break;
    }
    case kFrontReprobePlane: {
        const u32 player = mem_read32(cpu, kPlayerPointer);
        u32 code, owner;
        u16 bg;
        float raw[3], normal[3];
        if (!bluewake_climb_on || !g_front_support || player != g_front_player ||
            cpu->gpr[29] != player || cpu->gpr[31] != g_front_plane ||
            g_frame - g_front_frame > 2u || !grab_ready(cpu, player) ||
            mem_read32(cpu, kStage) != g_front_stage[0] ||
            mem_read32(cpu, kStage + 4u) != g_front_stage[1] ||
            !wall_code(cpu, player, &code, &bg, &owner) || code != 0u ||
            bg != g_front_bg_index || owner != g_front_bgw ||
            !wall_normal(cpu, cpu->gpr[3], raw, normal))
            break;
        float dot = 0.0f, difference = 0.0f;
        for (u32 axis = 0u; axis < 3u; ++axis) {
            dot += normal[axis] * g_front_normal[axis];
            const float delta = raw[axis] - g_front_raw_normal[axis];
            difference += delta * delta;
        }
        if (dot < 0.8660254f - 1e-6f || difference < 1e-6f)
            break;
        // This continuation is only reached after a second successful ray.
        // Adjacent facets can differ; an absent hit still follows native fall.
        cpu->pc = 0x8010F0D0u;
        trace_geometry(1u, bg, normal);
        break;
    }
    case kClimbPlane: {
        const u32 player = mem_read32(cpu, kPlayerPointer);
        u32 code, owner;
        u16 bg;
        float raw[3], normal[3];
        if (!bluewake_climb_on || cpu->gpr[31] != player || !player_ready(cpu, player) ||
            !can_grab() || !g_plain_contact || g_frame - g_plain_climb_frame > 4u ||
            player != g_plain_player || mem_read32(cpu, kStage) != g_plain_stage[0] ||
            mem_read32(cpu, kStage + 4u) != g_plain_stage[1] ||
            !wall_code(cpu, player, &code, &bg, &owner) || code != 0u ||
            !wall_normal(cpu, cpu->gpr[3], raw, normal) || fabsf(raw[1]) <= 0.05f)
            break;
        // Keep the native ground, facing, new ray and ledge-transition checks.
        cpu->pc = 0x80136014u;
        trace_geometry(2u, bg, normal);
        break;
    }
    case kFrontWallCode: {
        // The game found a steep wall Link faces and asks what it is: a plain
        // wall is a candidate, with the collision it hit (the checks after
        // this reuse the line check).
        const u32 player = mem_read32(cpu, kPlayerPointer);
        g_candidate = bluewake_climb_on && cpu->gpr[3] == 0u && cpu->gpr[29] == player &&
                      player_ready(cpu, player);
        g_candidate_player = player;
        g_candidate_frame = g_frame;
        if (g_candidate)
            for (u32 i = 0; i < sizeof g_candidate_poly; ++i)
                g_candidate_poly[i] = mem_read8(cpu, player + kLinkLinChkPoly + i);
        break;
    }
    case kFrontWallAbove: {
        // A plain wall that also stands at the height Link grabs ledges at:
        // not a ledge to pull himself onto. The game would leave it a plain
        // wall, or one to sidle along (which it still decides after this, and
        // which then wins): it becomes ivy, under the conditions the game puts
        // on ivy, and in the air only while he is steered at it.
        const bool candidate = g_candidate;
        g_candidate = false;
        if (!bluewake_climb_on || !candidate || cpu->gpr[3] == 0u || !can_grab())
            break;
        const u32 player = mem_read32(cpu, kPlayerPointer);
        if (player != g_candidate_player || cpu->gpr[29] != player ||
            g_frame - g_candidate_frame > 2u || !grab_ready(cpu, player) ||
            mem_read8(cpu, player + kFrontWallType) != kWallTypePlain)
            break;
        const u32 mode = mem_read32(cpu, player + kModeFlg);
        // Shipwright's climb controls start a step only with deliberate input.
        // Wind Waker supplies normalized distance, so keep its native direction
        // and animation rather than copying OoT's stick/Player layout.
        for (u32 i = 0; i < sizeof g_candidate_poly; ++i)
            mem_write8(cpu, player + kPolyInfo + i, g_candidate_poly[i]);
        mem_write8(cpu, player + kFrontWallType, kWallTypeIvy);
        if (g_trace)
            fprintf(stderr, "[climb] frame=%llu grabs a wall%s (stamina %.2f)\n", g_frame,
                    (mode & kModeMidair) != 0u ? " in the air" : "", g_stamina);
        break;
    }
    case kClimbWallCode: {
        // Each climbing frame: a plain wall stays ivy while there is stamina.
        const u32 player = mem_read32(cpu, kPlayerPointer);
        if (!bluewake_climb_on || cpu->gpr[31] != player || !player_ready(cpu, player))
            break;
        // A watched return can be serviced before an interrupt ends its turn,
        // then serviced again when that same instruction resumes. Recognize
        // our rewritten code only with the original owned, fresh contact and
        // actual plain-wall metadata; genuine ivy must clear plain contact.
        if (cpu->gpr[3] == 1u && g_plain_contact && player == g_plain_player &&
            g_frame - g_plain_climb_frame <= 4u &&
            mem_read32(cpu, kStage) == g_plain_stage[0] &&
            mem_read32(cpu, kStage + 4u) == g_plain_stage[1]) {
            u32 code, owner;
            u16 bg;
            if (wall_code(cpu, player, &code, &bg, &owner) && code == 0u)
                cpu->gpr[3] = 0u;
        }
        g_plain_contact = false;
        if (cpu->gpr[3] == 0u && can_grab()) {
            cpu->gpr[3] = 1u;
            g_plain_climb_frame = g_frame;
            g_plain_contact = true;
            g_plain_player = player;
            g_plain_stage[0] = mem_read32(cpu, kStage);
            g_plain_stage[1] = mem_read32(cpu, kStage + 4u);
        }
        break;
    }
    case kPlayerExecute:
        // Older modules may expose this boundary; VI owns the clock for all
        // modules, so observing it must never charge stamina a second time.
        break;
    case kCameraDraw:
        if (bluewake_climb_on && cpu->gpr[3] == mem_read32(cpu, kCameraPointer))
            camera_frame(cpu, cpu->gpr[3]);
        break;
    default:
        break;
    }
}
