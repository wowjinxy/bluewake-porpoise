// SPDX-License-Identifier: GPL-3.0-or-later
// Authored FIFO/GPU EFB color oracle. No game, present, input or audio calls.
// POSITION/COLOR VAT and transforms follow the existing public gxcore pixel
// fixtures; FIFO encoding follows frontend_replay_test.cpp.
#include "gxruntime/aurora_backend.h"
#include "gxruntime/platform.h"
#include "gfx/common.hpp"
#include "window.hpp"
#include "dolphin/vi/vi_internal.hpp"

#include <aurora/aurora.h>
#include <SDL3/SDL.h>

extern "C" {
#include "efb_peek.h"
}

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
#ifdef CHECK
#undef CHECK
#endif
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; \
  std::fprintf(stderr, "EFB GPU FAIL line %d: %s\n", __LINE__, #c); } } while (0)

void environment(const char* name, const char* value) {
#ifdef _WIN32
  CHECK(_putenv_s(name, value) == 0);
#else
  CHECK(setenv(name, value, 1) == 0);
#endif
}

void u32(std::vector<uint8_t>& fifo, uint32_t word) {
  fifo.push_back(uint8_t(word >> 24)); fifo.push_back(uint8_t(word >> 16));
  fifo.push_back(uint8_t(word >> 8)); fifo.push_back(uint8_t(word));
}
void f32(std::vector<uint8_t>& fifo, float value) {
  uint32_t word = 0;
  std::memcpy(&word, &value, sizeof word);
  u32(fifo, word);
}
void cp(std::vector<uint8_t>& fifo, uint8_t reg, uint32_t word) {
  fifo.push_back(0x08); fifo.push_back(reg); u32(fifo, word);
}
void bp(std::vector<uint8_t>& fifo, uint8_t reg, uint32_t word) {
  fifo.push_back(0x61); u32(fifo, uint32_t(reg) << 24 | (word & 0x00FFFFFF));
}
void xf(std::vector<uint8_t>& fifo, uint16_t reg,
        const std::vector<uint32_t>& words) {
  fifo.push_back(0x10);
  u32(fifo, uint32_t(words.size() - 1) << 16 | reg);
  for (auto word : words) u32(fifo, word);
}
void xf_floats(std::vector<uint8_t>& fifo, uint16_t reg,
               const std::vector<float>& values) {
  std::vector<uint32_t> words;
  for (float value : values) {
    uint32_t word = 0;
    std::memcpy(&word, &value, sizeof word);
    words.push_back(word);
  }
  xf(fifo, reg, words);
}

void setup(std::vector<uint8_t>& fifo) {
  cp(fifo, 0x50, (1u << 9) | (1u << 13)); // direct XYZ + COLOR0
  cp(fifo, 0x60, 0);
  cp(fifo, 0x70, 1u | (4u << 1) | (5u << 14)); // XYZ f32 + RGBA8
  cp(fifo, 0x80, 0); cp(fifo, 0x90, 0);
  xf_floats(fifo, 0, {1,0,0,0, 0,1,0,0, 0,0,1,0});
  xf(fifo, 0x1018, {0}); // PN matrix 0
  xf_floats(fifo, 0x101A, {320,-240,16777215,662,582,16777215});
  xf_floats(fifo, 0x1020, {1,0,1,0,-1,0});
  xf(fifo, 0x1026, {1}); // orthographic
  xf(fifo, 0x1009, {1}); // one vertex color channel, no lighting
  xf(fifo, 0x100E, {1}); xf(fifo, 0x1010, {1});
  bp(fifo, 0x00, 1u << 4); // one color channel, zero textures, no culling
  bp(fifo, 0x40, 0); // depth disabled
  bp(fifo, 0x41, (1u << 3) | (1u << 4)); // color/alpha updates, no blending
  bp(fifo, 0x43, 1); // RGBA6_Z24
  bp(fifo, 0x20, (342u << 12) | 342u);
  bp(fifo, 0x21, (981u << 12) | 821u);
  bp(fifo, 0x59, 171u | (171u << 10)); // native +342 scissor bias
  // One actual TEV stage passes vertex RGB/alpha through, without textures.
  bp(fifo, 0xC0, 15u | (15u << 4) | (15u << 8) | (10u << 12) | (1u << 19));
  bp(fifo, 0xC1, (7u << 4) | (7u << 7) | (7u << 10) | (5u << 13) | (1u << 19));
  // Explicit identity swap table 0: a reset table selects R for every channel.
  bp(fifo, 0xF6, 0u | (1u << 2)); // R, G
  bp(fifo, 0xF7, 2u | (3u << 2)); // B, A
  bp(fifo, 0xF3, (7u << 16) | (7u << 19)); // alpha compare always
}

void rectangle(std::vector<uint8_t>& fifo, float left, float top,
               float right, float bottom, uint8_t id,
               const std::array<uint8_t,4>& color) {
  bp(fifo, 0x42, 0x100u | (uint32_t(id) << 2)); // destination alpha enabled
  fifo.push_back(0x80); fifo.push_back(0); fifo.push_back(4); // GX_QUADS, VAT0
  const float positions[4][2]{{left,top},{right,top},{right,bottom},{left,bottom}};
  for (const auto& position : positions) {
    f32(fifo, position[0] / 320.f - 1.f);
    f32(fifo, 1.f - position[1] / 240.f);
    f32(fifo, 0);
    fifo.insert(fifo.end(), color.begin(), color.end());
  }
}
void submit(const std::vector<uint8_t>& fifo) {
  CHECK(!fifo.empty() && fifo.size() < 16384);
  // The production gather-pipe callback, not direct draw-plan submission.
  if (dol_platform_gx_write_bytes_available())
    dol_platform_gx_write_bytes(fifo.data(), uint32_t(fifo.size()));
  else
    for (auto byte : fifo) dol_platform_gx_write(byte, 1);
  // This is the host GXDrawDone-equivalent frontend barrier. The peek must
  // itself flush the consumer's final draw before its preserving submission.
  dol_platform_gx_flush();
}

bool color_reader(void*, uint16_t x, uint16_t y, uint16_t alpha, uint32_t* out) {
  return dol_aurora_gx_peek_argb(x, y, alpha, out);
}
uint32_t peek(uint16_t x, uint16_t y, uint16_t mode) {
  uint16_t decodedX = 0xFFFF, decodedY = 0xFFFF;
  const uint32_t address = 0xC8000000u | (uint32_t(y) << 12) | (uint32_t(x) << 2);
  CHECK(bluewake_efb_color_address(address, 4, &decodedX, &decodedY));
  CHECK(decodedX == x && decodedY == y);
  BluewakeEfbPeek state{};
  state.alpha_read = mode;
  bool available = false;
  const auto value = bluewake_efb_color_read(&state, decodedX, decodedY,
                                           color_reader, nullptr, &available);
  CHECK(available);
  return value;
}
void check_pixel(uint16_t x, uint16_t y, uint16_t mode, uint32_t expected) {
  const auto actual = peek(x, y, mode);
  CHECK(actual == expected);
  if (actual != expected)
    std::fprintf(stderr, "EFB GPU pixel (%u,%u) mode=%u frame=%llu: expected=%08X actual=%08X\n",
                 unsigned(x), unsigned(y), unsigned(mode),
                 static_cast<unsigned long long>(aurora::gfx::current_frame_id()),
                 unsigned(expected), unsigned(actual));
}
void hidden_contract() {
  SDL_Window* window = aurora::window::get_sdl_window();
  CHECK(window != nullptr);
  if (window != nullptr) {
    CHECK(SDL_GetWindowFromID(SDL_GetWindowID(window)) == window);
    const auto flags = SDL_GetWindowFlags(window);
    CHECK((flags & SDL_WINDOW_HIDDEN) != 0);
    CHECK((flags & (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_FULLSCREEN)) == 0);
  }
  CHECK(SDL_WasInit(SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD) == 0);
  CHECK(aurora_get_shown_frames() == 0);
}
} // namespace

int main(int argc, char** argv) {
  float scale = 0;
  if (argc == 2 && std::strcmp(argv[1], "1") == 0) scale = 1;
  if (argc == 2 && std::strcmp(argv[1], "1.5") == 0) scale = 1.5f;
  if (argc == 2 && std::strcmp(argv[1], "2") == 0) scale = 2;
  if (scale == 0) {
    std::fprintf(stderr, "EFB GPU: expected scale 1, 1.5 or 2\n");
    return 2;
  }
  environment("AURORA_SYNC_PIPELINES", "1");
  environment("DOL_GX_CORE", "1");
  environment("DOL_AURORA_RENDER_SCALE", argv[1]);
  environment("DOL_AURORA_CACHE_DIR", "cache");
  aurora_set_frame_buffer_scale(scale); // before initialization/first draw
  const AuroraBackendConfig config{
      .app_name = "BlueWake noninteractive EFB oracle", .window_width = 640,
      .window_height = 480, .vsync = false, .allow_texture_dumps = false,
      .info_logging = true, .graphics_logging = false, .force_untextured = false,
      .noninteractive = true};
  char name[] = "bluewake_efb_color_pixels_test";
  char* initializeArgv[]{name, nullptr};
  if (!dol_aurora_initialize(1, initializeArgv, &config)) {
    std::fprintf(stderr, "EFB GPU: initialization failed\n");
    return 2;
  }
  hidden_contract();
  const auto backend = aurora_get_backend();
  CHECK(backend >= BACKEND_D3D11 && backend <= BACKEND_WEBGPU);
  if (failures != 0) {
    dol_aurora_shutdown();
    return 1; // never render after a failed hidden/no-device startup contract
  }
  const auto logical = aurora::vi::configured_fb_size();
  const auto target = aurora::gfx::get_render_target_size();
  CHECK(logical.x == 640 && logical.y == 480);
  CHECK(target.x == uint32_t(640 * scale) && target.y == uint32_t(480 * scale));
  std::vector<uint8_t> fifo;
  setup(fifo);
  rectangle(fifo, 0,0,640,480, 63, {0,0,255,255}); // background alpha FC
  rectangle(fifo, 80,120,240,360, 37, {255,0,0,255});
  submit(fifo);
  const auto before = aurora::gfx::current_frame_id();
  CHECK(before != 0);
  check_pixel(160,240,6,0x96FF0000u); // 0x94 quantizes/expands to 0x96
  const auto first = aurora::gfx::current_frame_id();
  CHECK(first > before);
  check_pixel(480,240,6,0xFF0000FFu);
  check_pixel(160,240,4,0x00FF0000u);
  check_pixel(160,240,5,0xFFFF0000u);
  for (unsigned repeat = 0; repeat < 32; ++repeat)
    check_pixel(160,240,6,0x96FF0000u);
  CHECK(aurora::gfx::current_frame_id() == first); // one capture, no FIFO change

  fifo.clear();
  rectangle(fifo, 400,120,560,360, 9, {0,255,0,255});
  submit(fifo);
  CHECK(aurora::gfx::current_frame_id() == first);
  check_pixel(480,240,6,0x2400FF00u); // new FIFO invalidates old snapshot
  const auto second = aurora::gfx::current_frame_id();
  CHECK(second > first);
  check_pixel(160,240,6,0x96FF0000u); // preserving segment kept first region
  check_pixel(320,240,6,0xFF0000FFu); // untouched background
  CHECK(aurora::gfx::current_frame_id() == second);

  fifo.clear();
  for (unsigned stripe = 0; stripe < 16; ++stripe)
    rectangle(fifo, 64.f + stripe * 8,64,72.f + stripe * 8,96,
              stripe % 2 == 0 ? 5 : 47, {255,255,255,255});
  submit(fifo);
  const auto patternBefore = aurora::gfx::current_frame_id();
  for (unsigned stripe = 0; stripe < 16; ++stripe) {
    const uint32_t expected = stripe % 2 == 0 ? 5 : 47;
    // One logical pixel inside either boundary; all scale variants keep the
    // sampled physical pixel entirely inside the 8-pixel solid stripe.
    CHECK((peek(uint16_t(65 + stripe * 8),80,6) >> 26) == expected);
    CHECK((peek(uint16_t(70 + stripe * 8),80,6) >> 26) == expected);
  }
  const auto patternAfter = aurora::gfx::current_frame_id();
  CHECK(patternAfter > patternBefore);
  check_pixel(160,240,6,0x96FF0000u);
  check_pixel(480,240,6,0x2400FF00u);
  CHECK(aurora::gfx::current_frame_id() == patternAfter);

  // Alternating single-logical-pixel stripes expose filtered resampling at
  // scale 1.5. The production mapping samples floor((x + 0.5) * scale), so
  // each checked physical pixel center stays in its authored stripe.
  fifo.clear();
  for (unsigned stripe = 0; stripe < 16; ++stripe)
    rectangle(fifo, 64.f + stripe,112,65.f + stripe,128,
              stripe % 2 == 0 ? 5 : 47, {255,255,255,255});
  submit(fifo);
  const auto nearestBefore = aurora::gfx::current_frame_id();
  for (unsigned stripe = 0; stripe < 16; ++stripe)
    CHECK((peek(uint16_t(64 + stripe),120,6) >> 26) ==
          (stripe % 2 == 0 ? 5u : 47u));
  const auto nearestAfter = aurora::gfx::current_frame_id();
  CHECK(nearestAfter > nearestBefore);
  check_pixel(160,240,6,0x96FF0000u);
  check_pixel(480,240,6,0x2400FF00u);
  check_pixel(320,240,6,0xFF0000FFu);
  CHECK(aurora::gfx::current_frame_id() == nearestAfter);
  hidden_contract();
  dol_aurora_shutdown();
  CHECK(aurora_get_shown_frames() == 0);
  CHECK(SDL_WasInit(SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD) == 0);
  std::printf("EFB GPU: scale=%g target=%ux%u checks=%u failures=%u\n",
              scale, target.x, target.y, checks, failures);
  return failures == 0 ? 0 : 1;
}
