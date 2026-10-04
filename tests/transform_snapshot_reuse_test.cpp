// SPDX-License-Identifier: GPL-3.0-or-later
// Compare this fixture's complete transform digest with the pinned full-copy
// frontend, as well as checking per-draw payload and save/reset boundaries.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "gxruntime/aurora_recomp/retail_gx_frontend.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

using namespace gxruntime::aurora_recomp;
using Bytes = std::vector<std::uint8_t>;

static void push_u32(Bytes& out, std::uint32_t value) {
  for (unsigned shift : {24u, 16u, 8u, 0u})
    out.push_back(static_cast<std::uint8_t>(value >> shift));
}
static std::uint32_t bits(float value) {
  std::uint32_t word;
  std::memcpy(&word, &value, sizeof(word));
  return word;
}
static void cp(Bytes& out, std::uint8_t reg, std::uint32_t value) {
  out.push_back(DOL_GX_CMD_LOAD_CP_REG); out.push_back(reg); push_u32(out, value);
}
static void xf(Bytes& out, std::uint16_t base, const std::vector<std::uint32_t>& words) {
  assert(!words.empty());
  out.push_back(DOL_GX_CMD_LOAD_XF_REG);
  push_u32(out, (static_cast<std::uint32_t>(words.size() - 1) << 16) | base);
  for (auto word : words) push_u32(out, word);
}
static void matrix(Bytes& out, std::uint16_t base, unsigned count, float seed) {
  std::vector<std::uint32_t> words;
  for (unsigned i = 0; i < count; ++i) words.push_back(bits(seed + i));
  xf(out, base, words);
}

struct Expected {
  std::uint32_t payload_mask;
  std::uint8_t post_mask, normalize;
  float post_translation;
};

class CheckingSink final : public AuroraRenderSink {
public:
  ConsumingAuroraRenderSink sink;
  std::vector<Expected> expected;
  std::uint64_t hash = 1469598103934665603ull;
  std::size_t draws = 0;
  CheckingSink() { sink.set_streaming(true); }
  void hash_bytes(const void* data, std::size_t size) {
    auto p = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) hash = (hash ^ p[i]) * 1099511628211ull;
  }
  bool submit_packet(const RenderPacket& packet) override {
    if (!sink.submit_packet(packet)) return false;
    if (packet.kind != RenderPacketKind::Draw) return true;
    assert(draws < expected.size());
    const auto& d = sink.draws().back();
    const auto& e = expected[draws++];
    assert(d.payload_pn_matrix_mask == e.payload_mask);
    assert(d.post_tex_mask == e.post_mask);
    assert(d.post_tex_normalize == e.normalize);
    if (e.post_mask) assert(d.post_tex_rows[0][3] == e.post_translation);
    assert(d.vertex_count == 3 && d.vertex_payload.size() == 39);
    // No struct padding, host pointers or inactive post rows enter the digest.
#define HASH_FIELD(name) hash_bytes(&d.name, sizeof(d.name))
    HASH_FIELD(xf_version); HASH_FIELD(transform_flags);
    HASH_FIELD(current_pn_matrix); HASH_FIELD(payload_pn_matrix_mask);
    HASH_FIELD(position_matrix_valid_mask); HASH_FIELD(viewport);
    HASH_FIELD(projection); HASH_FIELD(projection_type);
    HASH_FIELD(position_matrices); HASH_FIELD(normal_matrices);
    HASH_FIELD(normal_matrix_word_mask); HASH_FIELD(light_words);
    HASH_FIELD(light_word_mask); HASH_FIELD(chan_regs); HASH_FIELD(chan_reg_mask);
    HASH_FIELD(tex_matrices); HASH_FIELD(tex_matrix_word_mask);
    HASH_FIELD(xf_regs); HASH_FIELD(xf_reg_mask);
    HASH_FIELD(post_tex_mask); HASH_FIELD(post_tex_normalize);
#undef HASH_FIELD
    for (unsigned i = 0; i < 8; ++i)
      if (d.post_tex_mask & (1u << i)) hash_bytes(d.post_tex_rows[i], sizeof(d.post_tex_rows[i]));
    hash_bytes(d.vertex_payload.data(), d.vertex_payload.size());
    return true;
  }
};

int main(int argc, char** argv) {
  const bool long_verify = argc == 2 && std::strcmp(argv[1], "--long-verify") == 0;
  auto frontend = std::make_unique<RetailGxFrontend>();
  CheckingSink check;
  frontend->set_packet_drain_enabled(true);
  Bytes fifo;
  auto flush = [&] {
    assert(frontend->write_fifo(fifo));
    if (!frontend->flush(&check)) {
      std::fprintf(stderr, "frontend=%s sink=%s\n", frontend->last_error(), check.sink.failure_reason());
      assert(false);
    }
    fifo.clear();
  };
  auto layout = [&] {
    assert(frontend->set_vertex_layout(0, 13));
    cp(fifo, DOL_GX_CP_REG_VCD_LO, 1u | (1u << 9));
    cp(fifo, DOL_GX_CP_REG_VAT_GRP0, 1u | (4u << 1));
  };
  auto draw = [&](unsigned pn, std::uint8_t post, std::uint8_t norm, float translation) {
    fifo.push_back(0x90); fifo.push_back(0); fifo.push_back(3);
    for (unsigned v = 0; v < 3; ++v) {
      fifo.push_back(static_cast<std::uint8_t>(pn * 3));
      for (unsigned axis = 0; axis < 3; ++axis) push_u32(fifo, bits(float(v + axis)));
    }
    check.expected.push_back({1u << pn, post, norm, translation});
  };
  layout();
  matrix(fifo, 0, 24, 1.f); // two position matrices
  matrix(fifo, DOL_GX_XF_NORMAL_MATRIX_BASE, 9, 30.f);
  matrix(fifo, DOL_GX_XF_TEX_MATRIX_BASE, 12, 50.f);
  matrix(fifo, DOL_GX_XF_LIGHT_BASE, 16, 70.f);
  xf(fifo, DOL_GX_XF_CHAN_REG_BASE, {1,2,3,4,5,6,7,8,9});
  matrix(fifo, DOL_GX_XF_VIEWPORT_BASE, 6, 100.f);
  matrix(fifo, DOL_GX_XF_PROJECTION_BASE, 6, 200.f);
  xf(fifo, DOL_GX_XF_PROJECTION_TYPE, {1});
  xf(fifo, 0x103f, {1}); xf(fifo, 0x1040, {0}); xf(fifo, 0x1050, {0});
  matrix(fifo, 0x500, 12, 2.f);
  // Same XF version but different per-vertex matrix indices, including across
  // queue growth and flush/drain boundaries. BP events must not disturb reuse.
  for (unsigned i = 0; i < 96; ++i) {
    draw(i & 1, 1, 0, 5.f);
    fifo.push_back(DOL_GX_CMD_LOAD_BP_REG); push_u32(fifo, 0x41000000u | i);
  }
  flush();
  for (unsigned i = 0; i < 64; ++i) draw(i & 1, 1, 0, 5.f);
  flush();
  const auto saved = frontend->save_state();
  // Mutate every block before a repeat. Full-copy baseline digest verifies all
  // block bytes, validity masks and scalar fields survive transitions.
  for (auto base : {0u, 0x400u, 0x78u, 0x600u, 0x1009u, 0x101au, 0x1020u, 0x1040u}) {
    xf(fifo, static_cast<std::uint16_t>(base), {bits(17.f)});
    draw(0, 1, 0, 5.f); draw(1, 1, 0, 5.f);
  }
  xf(fifo, 0x503, {bits(9.f)}); draw(0, 1, 0, 9.f); draw(1, 1, 0, 9.f);
  xf(fifo, 0x1050, {1u << 8}); draw(0, 1, 1, 9.f); draw(1, 1, 1, 9.f);
  xf(fifo, 0x1012, {0}); draw(0, 0, 0, 0.f); draw(1, 0, 0, 0.f);
  flush();
  assert(frontend->load_state(saved.data(), saved.size()));
  draw(1, 1, 0, 5.f); draw(0, 1, 0, 5.f); flush();
  frontend->reset(); check.sink.reset(); check.sink.set_streaming(true);
  frontend->set_packet_drain_enabled(true); layout();
  draw(0, 0, 0, 0.f); draw(1, 0, 0, 0.f); flush();
  if (long_verify) {
    // Cross the upstream one-million-draw verification reporting boundary.
    for (unsigned chunk = 0; chunk < 32770; ++chunk) {
      for (unsigned i = 0; i < 32; ++i) draw(i & 1, 0, 0, 0.f);
      flush();
    }
  }
  assert(check.draws == check.expected.size());
  // Unversioned packets cannot reuse a retained draw, even when both versions
  // are zero. This covers callers outside the retail frontend.
  ConsumingAuroraRenderSink unversioned;
  unversioned.set_streaming(true);
  RenderPacket direct{};
  direct.kind = RenderPacketKind::Draw;
  direct.sequence = 1;
  direct.draw.vertex_count = 1;
  direct.draw.vertex_size = 1;
  direct.draw.viewport[0] = 17.f;
  assert(unversioned.submit_packet(direct));
  direct.sequence = 2;
  direct.draw.viewport[0] = 29.f;
  assert(unversioned.submit_packet(direct));
  assert(unversioned.draws().back().viewport[0] == 29.f);
  std::printf("draws=%zu transform_digest=%016llx\n", check.draws,
              static_cast<unsigned long long>(check.hash));
}
