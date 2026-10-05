// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic packets through the actual GxCoreSink and consuming frontend.
// No captured guest data, renderer initialization, device or GPU is required.
#include "hud_customization_draw_plan.h"
#include <gxruntime/gxcore/gxcore.hpp>
#ifdef BLUEWAKE_HUD_STREAM_HOST_FILTER
#include "hud_renderer.h"
#endif
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace gx = gxruntime::gxcore;
namespace ar = gxruntime::aurora_recomp;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

struct Seen {
    gx::DrawPlan native;
    gx::DrawPlan filtered;
    std::array<uint32_t, 16> words{};
    bool tagged = false;
    bool accepted = false;
    bool observed = false;
};

static bool same_plan(const gx::DrawPlan& a, const gx::DrawPlan& b) {
    return std::memcmp(static_cast<const gx::DrawPlanFields*>(&a),
                       static_cast<const gx::DrawPlanFields*>(&b), sizeof(gx::DrawPlanFields)) == 0 &&
        std::memcmp(&a.constants, &b.constants, sizeof a.constants) == 0 &&
        std::memcmp(&a.constants_inputs, &b.constants_inputs, sizeof a.constants_inputs) == 0 &&
        std::memcmp(a.texgen_row, b.texgen_row, sizeof a.texgen_row) == 0 &&
        a.constants_unresolved == b.constants_unresolved && a.constants_id == b.constants_id &&
        a.vertices == b.vertices && a.indices == b.indices;
}

struct Stream {
    gx::GxCoreSink sink;
    std::vector<Seen> seen;
    uint64_t packet_sequence = 0;
    unsigned copies = 0;

    static bool filter(gx::DrawPlan& plan, const gx::GxCoreState& pending, void* user) {
        auto& self = *static_cast<Stream*>(user);
        if (!plan.ok) std::fprintf(stderr, "actual stream plan: %s\n", plan.skip_reason);
        CHECK(plan.ok);
        Seen item;
        item.native = plan;
        bool complete = true;
        for (unsigned i = 0; i < 16; ++i) {
            const auto reg = static_cast<uint8_t>(0x6Au + i);
            complete = complete && pending.bp_valid(reg);
            item.words[i] = pending.bp(reg);
        }
        BwHudDescriptor descriptor{};
        item.tagged = complete && bw_hud_descriptor_decode(item.words.data(), &descriptor);
#ifdef BLUEWAKE_HUD_STREAM_HOST_FILTER
        // The ignored integration variant also runs the exact proposed host
        // filter. Its backend setter seam remains isolated from public tests.
        item.accepted = bw_hud_renderer_filter(&plan, &pending, nullptr);
#else
        bluewake::hud::PixelMultiplier multiplier;
        item.accepted = bluewake::hud::transform_plan(item.tagged ? &descriptor : nullptr,
                            plan, multiplier) != bluewake::hud::PlanResult::Suppressed;
#endif
        item.filtered = plan;
        self.seen.push_back(std::move(item));
        return self.seen.back().accepted;
    }

    static void observe(const gx::DrawPlan& plan, void* user) {
        auto& self = *static_cast<Stream*>(user);
        CHECK(!self.seen.empty());
        CHECK(self.seen.back().accepted && !self.seen.back().observed);
        CHECK(same_plan(plan, self.seen.back().filtered));
        self.seen.back().observed = true;
    }

    static void copy(const gx::EfbCopyCommand& command, void* user) {
        auto& self = *static_cast<Stream*>(user);
        CHECK(command.format == 0xFu);
        ++self.copies;
    }

    void submit(ar::RenderPacket packet) {
        packet.sequence = ++packet_sequence;
        CHECK(sink.submit_packet(packet));
        CHECK(sink.failure_reason() == nullptr);
    }

    void state(ar::RenderStateKind kind, uint32_t reg, uint32_t value, uint32_t aux = 0) {
        ar::RenderPacket packet;
        packet.kind = ar::RenderPacketKind::State;
        packet.state = {.kind = kind, .index = reg, .value = value, .aux0 = aux};
        submit(packet);
    }

    void bp(uint32_t reg, uint32_t value) { state(ar::RenderStateKind::BpReg, reg, value); }

    Stream() {
        sink.set_plan_filter(filter, this);
        sink.set_plan_observer(observe, this);
        sink.set_copy_observer(copy, this);
        bp(0x00, 0); bp(0x40, 0x17); bp(0x41, 0x18);
        state(ar::RenderStateKind::CpVcd, 0, 1u << 9);
        state(ar::RenderStateKind::CpVcd, 1, 0);
        state(ar::RenderStateKind::CpVat, 0, 1u | (4u << 1), 0);
        state(ar::RenderStateKind::CpVat, 0, 0, 1);
        state(ar::RenderStateKind::CpVat, 0, 0, 2);
        bp(0x20, 342u | (342u << 12));
        bp(0x21, 981u | (821u << 12));
    }

    static std::array<uint32_t, 15> fields(unsigned sequence, unsigned epoch = 2,
                                           unsigned generation = 2, bool visible = true) {
        std::array<uint32_t, 15> w{sequence, 1, visible ? 0x100u : 0u,
            10u * 256u, 0xFFFFFFu - (8u * 256u - 1u), 5120, 32768,
            0x80FF40, 191, 219u * 256u, 40u * 256u, 0x450000u / 4u,
            epoch, generation, 0};
        uint32_t sum = 2166136261u;
        for (unsigned i = 0; i < 14; ++i)
            for (unsigned b = 0; b < 3; ++b) { sum ^= (w[i] >> (8u * b)) & 255u; sum *= 16777619u; }
        w[14] = sum & 0xFFFFFFu;
        return w;
    }

    void fields(const std::array<uint32_t, 15>& w, unsigned first, unsigned last) {
        for (unsigned i = first; i < last; ++i) bp(0x6Bu + i, w[i]);
    }
    void begin(unsigned sequence, unsigned epoch = 2, unsigned generation = 2, bool visible = true) {
        const auto w = fields(sequence, epoch, generation, visible);
        fields(w, 0, 15); bp(0x6A, 0x10001);
    }
    void end(unsigned sequence) { bp(0x6B, sequence); bp(0x6A, 0x10002); }
    void reset() { bp(0x6A, 0x10003); }

    void draw() {
        std::array<uint8_t, 36> payload{};
        const float position[9]{20, 30, 0, 120, 30, 0, 20, 70, 0};
        for (unsigned i = 0; i < 9; ++i) {
            uint32_t bits;
            std::memcpy(&bits, &position[i], sizeof bits);
            for (unsigned b = 0; b < 4; ++b) payload[i * 4 + b] = static_cast<uint8_t>(bits >> ((3u - b) * 8u));
        }
        ar::RenderPacket packet;
        packet.kind = ar::RenderPacketKind::Draw;
        auto& d = packet.draw;
        d.primitive = 0x90; d.vertex_count = 3; d.vertex_size = 12;
        d.vertex_payload = payload.data(); d.vertex_payload_size = static_cast<uint32_t>(payload.size());
        d.transform_flags = ar::kDrawTransformProjectionValid | ar::kDrawTransformViewportValid;
        d.projection_type = 1;
        d.projection[0] = 2.f / 668.f; d.projection[1] = -1;
        d.projection[2] = -2.f / 545.f; d.projection[3] = 1;
        d.projection[4] = -1;
        d.position_matrix_valid_mask = 1;
        d.position_matrices[0][0] = d.position_matrices[0][5] = d.position_matrices[0][10] = 1;
        d.viewport[0] = 320; d.viewport[1] = -240; d.viewport[2] = 16777215;
        d.viewport[3] = 662; d.viewport[4] = 582; d.viewport[5] = 16777215;
        submit(packet); // The actual consumer copies this borrowed stack payload.
    }

    void flush() {
        const auto count = seen.size();
        sink.flush_frame();
        CHECK(sink.failure_reason() == nullptr);
        CHECK(seen.size() == count || seen.size() == count + 1);
    }

    void display_copy() {
        ar::RenderPacket packet;
        packet.kind = ar::RenderPacketKind::Resource;
        packet.resource.kind = ar::RenderResourceKind::CopyDestination;
        packet.resource.format = 0xF;
        packet.resource.width = 640; packet.resource.height = 480;
        submit(packet);
        flush(); // Aurora services the copy with this separate presentation flush.
    }

    void tagged(std::size_t index, unsigned sequence) const {
        CHECK(index < seen.size());
        const auto& item = seen[index];
        CHECK(item.tagged && item.accepted && item.observed);
        CHECK(item.words[1] == sequence);
        const auto expected = fields(sequence, item.words[13], item.words[14]);
        for (unsigned i = 0; i < 15; ++i) CHECK(item.words[i + 1] == expected[i]);
        CHECK(item.filtered.pipeline.shader.hud_tint == 1);
        CHECK(item.filtered.pixel_constants.hud_multiplier[0] == 128.f / 255.f);
        CHECK(item.filtered.pixel_constants.hud_multiplier[3] == .5f * 191.f / 255.f);
        CHECK(item.filtered.constants_id == 0);
        BwHudDescriptor descriptor;
        CHECK(bw_hud_descriptor_decode(item.words.data(), &descriptor));
        auto expected_plan = item.native;
        bluewake::hud::PixelMultiplier multiplier;
        CHECK(bluewake::hud::transform_plan(&descriptor, expected_plan, multiplier) == bluewake::hud::PlanResult::Applied);
        CHECK(same_plan(expected_plan, item.filtered));
        auto unowned = item.filtered;
        unowned.pipeline.shader.hud_tint = item.native.pipeline.shader.hud_tint;
        std::memcpy(unowned.pixel_constants.hud_multiplier, item.native.pixel_constants.hud_multiplier,
                    sizeof unowned.pixel_constants.hud_multiplier);
        std::memcpy(unowned.constants.projection, item.native.constants.projection, sizeof unowned.constants.projection);
        unowned.constants_id = item.native.constants_id;
        unowned.scissor_x = item.native.scissor_x; unowned.scissor_y = item.native.scissor_y;
        unowned.scissor_width = item.native.scissor_width; unowned.scissor_height = item.native.scissor_height;
        CHECK(same_plan(unowned, item.native)); // Every field outside owned affine/tint is exact.
    }

    void ordinary(std::size_t index) const {
        CHECK(index < seen.size());
        const auto& item = seen[index];
        CHECK(!item.tagged && item.accepted && item.observed);
        CHECK(same_plan(item.native, item.filtered));
        CHECK(item.filtered.pipeline.shader.hud_tint == 0);
        for (float channel : item.filtered.pixel_constants.hud_multiplier) CHECK(channel == 1.f);
    }
};

static void active_across_presentation() {
    Stream s;
    s.begin(1); s.draw(); s.flush(); s.tagged(0, 1);
    const auto first = s.seen[0].words;
    for (unsigned suffix = 0; suffix < 7; ++suffix) {
        s.draw();
        if (suffix == 2) s.display_copy(); else s.flush();
        s.tagged(suffix + 1, 1);
        CHECK(s.seen[suffix + 1].words == first);
    }
    CHECK(s.copies == 1);
    s.end(1); s.draw(); s.flush(); s.ordinary(8);
    const auto completed = s.seen.size(); s.flush(); s.flush();
    CHECK(s.seen.size() == completed); // Flush is not another draw or scope event.
}

static void pending_next_draw_and_partial_descriptor() {
    Stream s;
    s.draw(); // This draw is native even though BEGIN arrives before its flush.
    s.begin(1); s.flush(); s.ordinary(0);
    s.draw(); s.flush(); s.tagged(1, 1);
    s.end(1);
    s.draw(); // END is in this snapshot; next descriptor is only partly in FIFO.
    const auto second = Stream::fields(2);
    s.fields(second, 0, 7); s.flush(); s.ordinary(2);
    s.fields(second, 7, 15); s.bp(0x6A, 0x10001);
    s.draw(); s.flush(); s.tagged(3, 2);
    s.end(2); s.draw(); s.flush(); s.ordinary(4);

    // An empty presentation can also split the initial descriptor itself.
    Stream empty;
    const auto first = Stream::fields(1);
    empty.fields(first, 0, 8); empty.flush(); CHECK(empty.seen.empty());
    empty.fields(first, 8, 15); empty.bp(0x6A, 0x10001);
    empty.draw(); empty.flush(); empty.tagged(0, 1);
}

static void ordered_end_reset_and_restore() {
    Stream s;
    s.begin(1); s.draw(); s.end(1); s.flush(); s.tagged(0, 1);
    s.draw(); s.flush(); s.ordinary(1); // END does not retroactively change pending draw.
    s.begin(2); s.draw(); s.reset(); s.flush(); s.tagged(2, 2);
    s.draw(); s.flush(); s.ordinary(3);
    s.begin(1, 3); s.draw(); s.flush(); s.tagged(4, 1);
    const auto snapshot = s.sink.save_state(); // Contains a live active scope.
    CHECK(s.sink.load_state(snapshot.data(), snapshot.size()));
    s.draw(); s.flush(); s.ordinary(5); // Explicit restore revokes borrowed scope.
    s.begin(1, 4); s.draw(); s.flush(); s.tagged(6, 1);
    s.end(1); s.begin(2, 4); s.end(2); s.flush(); // Zero-draw leaf remains balanced.
    s.draw(); s.flush(); s.ordinary(7);
}

static void no_leak_after_invalid_and_hidden_scope() {
    Stream s;
    s.begin(1); s.draw(); s.flush(); s.tagged(0, 1);
    s.begin(2); s.draw(); s.flush(); s.ordinary(1); // A nested BEGIN cancels.
    s.begin(2); s.draw(); s.flush(); s.tagged(2, 2);
    s.end(99); s.draw(); s.flush(); s.ordinary(3); // Mismatched END fails closed.
    s.begin(3, 2, 2, false); s.draw(); s.flush();
    CHECK(s.seen.size() == 5 && s.seen[4].tagged);
    CHECK(!s.seen[4].accepted && !s.seen[4].observed);
    s.draw(); s.flush(); CHECK(!s.seen[5].accepted && !s.seen[5].observed);
    s.end(3); s.bp(0x7E, 42); s.draw(); s.flush(); s.ordinary(6);
    CHECK(s.seen[6].filtered.draw_tag == 42); // Native particle ownership is unrelated.
    s.begin(4); s.bp(0x79, 0); s.draw(); s.flush(); s.ordinary(7); // Corrupt checksum remains native.
}

int main() {
    active_across_presentation();
    pending_next_draw_and_partial_descriptor();
    ordered_end_reset_and_restore();
    no_leak_after_invalid_and_hidden_scope();
#ifdef BLUEWAKE_HUD_STREAM_HOST_FILTER
    std::printf("HUD actual GXCore stream + proposed host filter: %u checks PASS; no device/GPU\n", checks);
#else
    std::printf("HUD actual GXCore stream/presentation/restore: %u checks PASS; no device/GPU\n", checks);
#endif
}
