// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic copied bytes only. No files, CARD backend, guest or save authority.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "randomizer_card_payload.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <crtdbg.h>
#endif

using Bytes = std::vector<std::uint8_t>;
static unsigned checks;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #x); std::abort(); } ++checks; } while (0)
constexpr std::size_t Header = 40, RecordSize = 68, Sector = 0x2000;
constexpr std::size_t NativeFile = 0x18000, Quest = 0x770, QuestData = 0x768, GameBytes = 0x1650;

static void u16(Bytes& b, std::size_t at, std::uint16_t n) {
    CHECK(at + 2 <= b.size()); b[at] = std::uint8_t(n >> 8); b[at + 1] = std::uint8_t(n);
}
static void u32(Bytes& b, std::size_t at, std::uint32_t n) {
    CHECK(at + 4 <= b.size()); for (unsigned i = 0; i < 4; ++i) b[at + i] = std::uint8_t(n >> (24 - 8 * i));
}
static void u64(Bytes& b, std::size_t at, std::uint64_t n) {
    CHECK(at + 8 <= b.size()); for (unsigned i = 0; i < 8; ++i) b[at + i] = std::uint8_t(n >> (56 - 8 * i));
}
static std::uint32_t fnv(const std::uint8_t* p, std::size_t n) {
    std::uint32_t h = 0x811C9DC5u;
    for (std::size_t i = 0; i != n; ++i) h = (h ^ p[i]) * 0x01000193u;
    return h;
}
// Primary mDoMemCdRWm's complementary sums are computed algebraically here:
// sum(~element) = -(sum(element) + count) modulo the accumulator width.
// This oracle does not duplicate the production complement-accumulator loop.
static std::uint64_t quest_checksum(const std::uint8_t* p) {
    const auto sum = std::accumulate(p, p + QuestData, std::uint32_t(0));
    return (std::uint64_t(sum) << 32) | std::uint32_t(0u - sum - std::uint32_t(QuestData));
}
static std::uint32_t block_checksum(const std::uint8_t* p) {
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i != Sector - 4; i += 2) sum += std::uint32_t(p[i]) * 256u + p[i + 1];
    return (std::uint32_t(std::uint16_t(sum)) << 16) |
           std::uint16_t(0u - sum - std::uint32_t((Sector - 4) / 2));
}
static Bytes games(unsigned salt) {
    Bytes b(GameBytes);
    for (std::size_t slot = 0; slot != 3; ++slot) {
        const auto base = slot * Quest;
        for (std::size_t i = 0; i != QuestData; ++i) b[base + i] = std::uint8_t(i * 37u + slot * 53u + salt);
        u64(b, base + QuestData, quest_checksum(b.data() + base));
    }
    return b;
}
static Bytes native_file(const Bytes& game, std::uint32_t count) {
    CHECK(game.size() == GameBytes);
    Bytes b(NativeFile);
    // Header/banner and all nine photograph sectors are deliberately arbitrary:
    // this validator must preserve them without claiming picture validity.
    for (std::size_t i = 0; i != b.size(); ++i) b[i] = std::uint8_t(i * 17u + 91u);
    Bytes block(Sector, 0);
    u32(block, 0, count); u32(block, 4, 0);
    std::copy(game.begin(), game.end(), block.begin() + 8);
    for (std::size_t i = 8 + GameBytes; i != Sector - 4; ++i) block[i] = std::uint8_t(i * 11u);
    u32(block, Sector - 4, block_checksum(block.data()));
    std::copy(block.begin(), block.end(), b.begin() + Sector);
    std::copy(block.begin(), block.end(), b.begin() + 2 * Sector);
    return b;
}
struct Record {
    std::uint16_t id = 7;
    std::string name = "gczelda", game = "GZLE", company = "01";
    std::uint8_t banner = 1, permission = 4;
    std::uint32_t time = 0xABCDEF01u, icon_address = 0, comment_address = 0x1C00;
    std::uint16_t icon_format = 1, icon_speed = 3;
    Bytes data;
};
static Bytes container(const std::vector<Record>& files, std::uint16_t mbits = 4, std::uint16_t encoding = 0) {
    Bytes out(Header, 0);
    std::copy_n(reinterpret_cast<const std::uint8_t*>("DOLCARD1"), 8, out.begin());
    u32(out, 8, 1); u16(out, 12, mbits); u16(out, 14, encoding); u32(out, 16, Sector);
    u64(out, 20, 0x0123456789ABCDEFull); u32(out, 28, std::uint32_t(files.size()));
    for (const auto& f : files) {
        CHECK(f.name.size() <= 32 && f.game.size() == 4 && f.company.size() == 2);
        const auto at = out.size(); out.resize(at + RecordSize + f.data.size(), 0);
        u16(out, at, f.id); u32(out, at + 4, std::uint32_t(f.data.size())); u32(out, at + 8, f.time);
        std::copy(f.name.begin(), f.name.end(), out.begin() + at + 12);
        std::copy(f.game.begin(), f.game.end(), out.begin() + at + 44);
        std::copy(f.company.begin(), f.company.end(), out.begin() + at + 48);
        out[at + 50] = f.banner; out[at + 51] = f.permission;
        u32(out, at + 52, f.icon_address); u16(out, at + 56, f.icon_format); u16(out, at + 58, f.icon_speed);
        u32(out, at + 60, f.comment_address); u32(out, at + 64, fnv(f.data.data(), f.data.size()));
        std::copy(f.data.begin(), f.data.end(), out.begin() + at + RecordSize);
    }
    u32(out, 32, std::uint32_t(out.size() - Header)); u32(out, 36, fnv(out.data() + Header, out.size() - Header));
    return out;
}
static void body_hash(Bytes& b) { u32(b, 36, fnv(b.data() + Header, b.size() - Header)); }
static void native_hash(Bytes& b, std::size_t file_at = Header) {
    const auto payload = file_at + RecordSize;
    u32(b, file_at + 64, fnv(b.data() + payload, NativeFile)); body_hash(b);
}

namespace payload = bluewake::randomizer::card_payload;
using payload::Status;
static void zero(const payload::Match& m) { CHECK(m.card_serial == 0 && m.save_count == 0 && m.file_number == 0); }
static payload::Match matched(const Bytes& card, const Bytes& game) {
    const Bytes before = card, before_game = game;
    payload::Match m{99, 88, 77};
    CHECK(payload::validate(card.data(), card.size(), game.data(), game.size(), m) == Status::Matched);
    CHECK(card == before && game == before_game); return m;
}
static void rejects_at(const Bytes& card, const Bytes& game, Status status, unsigned caller) {
    const Bytes before = card, before_game = game;
    payload::Match m{99, 88, 77};
    const auto actual = payload::validate(card.data(), card.size(), game.data(), game.size(), m);
    if (actual != status) std::fprintf(stderr, "payload status mismatch caller=%u checks=%u actual=%d expected=%d card=%zu game=%zu\n",
                                      caller, checks, int(actual), int(status), card.size(), game.size());
    CHECK(actual == status);
    zero(m); CHECK(card == before && game == before_game);
}
#define rejects(c, g, s) rejects_at(c, g, s, __LINE__)
static void pointer_reject(const std::uint8_t* card, std::size_t card_size,
                           const std::uint8_t* game, std::size_t game_size, Status status) {
    payload::Match m{99, 88, 77}; CHECK(payload::validate(card, card_size, game, game_size, m) == status); zero(m);
}
static void outer_hash(Bytes& card, unsigned copy) {
    const auto at = Header + RecordSize + (copy + 1) * Sector;
    u32(card, at + Sector - 4, block_checksum(card.data() + at)); native_hash(card);
}
static void native_corruption(const Bytes& original, const Bytes& game) {
    const auto file = Header + RecordSize;
    for (unsigned copy = 0; copy != 2; ++copy) {
        const auto at = file + (copy + 1) * Sector;
        // Hashes are repaired so the native layers, rather than the outer FNV,
        // must reject each corruption. An intact other copy must not rescue it.
        for (std::size_t slot = 0; slot != 3; ++slot) {
            for (std::size_t field : {std::size_t(0), QuestData - 1, QuestData, Quest - 1}) {
                auto b = original; b[at + 8 + slot * Quest + field] ^= 0x81;
                outer_hash(b, copy); rejects(b, game, Status::InvalidNativeChecksum);
            }
        }
        auto b = original; b[at + Sector - 1] ^= 1; native_hash(b);
        rejects(b, game, Status::InvalidNativeChecksum);
        b = original; u32(b, at + 4, 1); outer_hash(b, copy);
        rejects(b, game, Status::InvalidNativeChecksum);
        b = original; u32(b, at, 42 + copy); outer_hash(b, copy);
        rejects(b, game, Status::PayloadMismatch); // Valid but stale counter.
        b = original; b[at + 8 + GameBytes] ^= 1; outer_hash(b, copy);
        rejects(b, game, Status::PayloadMismatch); // Full-block equality matters.
        // A fully valid different quest record still cannot match this request.
        b = original; const auto q = at + 8; b[q] ^= 1;
        u64(b, q + QuestData, quest_checksum(b.data() + q)); outer_hash(b, copy);
        rejects(b, game, Status::PayloadMismatch);
    }
}
static void inspection_delta(const Bytes& original, const Bytes& expected) {
    std::array<std::uint8_t, payload::GameBytes> copied;
    copied.fill(0xA5); payload::Match m{99, 88, 77};
    const auto unchanged = original;
    CHECK(payload::inspect(original.data(), original.size(), copied, m) == Status::Matched);
    CHECK(std::equal(copied.begin(), copied.end(), expected.begin()));
    CHECK(m.card_serial == 0x0123456789ABCDEFull && m.save_count == 41 && m.file_number == 7);
    CHECK(original == unchanged);
    auto changed = original;
    CHECK(payload::inspect(changed.data(), changed.size(), copied, m) == Status::Matched);
    changed[Header + RecordSize + Sector + 8] ^= 1;
    CHECK(std::equal(copied.begin(), copied.end(), expected.begin())); // Copied output never borrows input.
    const auto mutated_source = changed;
    copied[0] ^= 1; CHECK(changed == mutated_source); // Output writes do not mutate the source.
    auto rejects_inspect = [&](const std::uint8_t* bytes, std::size_t size, Status status) {
        copied.fill(0xA5); m = {99, 88, 77};
        CHECK(payload::inspect(bytes, size, copied, m) == status);
        zero(m); CHECK(std::all_of(copied.begin(), copied.end(), [](std::uint8_t v) { return v == 0; }));
    };
    rejects_inspect(nullptr, original.size(), Status::InvalidInput);
    for (std::size_t n : {std::size_t(0), Header - 1}) rejects_inspect(original.data(), n, Status::InvalidInput);
    Bytes oversized(payload::MaxCardBytes + 1, 0);
    rejects_inspect(oversized.data(), oversized.size(), Status::InvalidInput);
    changed = original; changed[0] ^= 1;
    rejects_inspect(changed.data(), changed.size(), Status::InvalidContainer);
    changed = original; changed[Header + 64] ^= 1; body_hash(changed);
    rejects_inspect(changed.data(), changed.size(), Status::InvalidContainer);
    changed = container({}); rejects_inspect(changed.data(), changed.size(), Status::MissingGame);
    changed = original; u16(changed, Header + 56, 0); body_hash(changed);
    rejects_inspect(changed.data(), changed.size(), Status::InvalidNativeMetadata);
    for (unsigned copy = 0; copy != 2; ++copy) {
        const auto at = Header + RecordSize + (copy + 1) * Sector;
        for (unsigned slot = 0; slot != 3; ++slot) {
            changed = original; changed[at + 8 + slot * Quest] ^= 1; outer_hash(changed, copy);
            rejects_inspect(changed.data(), changed.size(), Status::InvalidNativeChecksum);
        }
        changed = original; u32(changed, at, 42); outer_hash(changed, copy);
        rejects_inspect(changed.data(), changed.size(), Status::PayloadMismatch); // Checksum-valid stale copy.
        changed = original; changed[at + Sector - 1] ^= 1; native_hash(changed);
        rejects_inspect(changed.data(), changed.size(), Status::InvalidNativeChecksum);
    }
}
static void preservation_delta(const Record& ww, const Bytes& game) {
    Record a; a.id = 8; a.name = "other"; a.game = "ABCD"; a.data = Bytes{1, 2, 3};
    Record z = a; z.id = 9; z.name = "third"; z.data = Bytes{4, 5};
    const auto before = container({a, ww, z});
    auto preserve = [&](const Bytes& after, std::uint8_t quest, bool expected) {
        const auto original_before = before, original_after = after;
        CHECK(payload::preserves_baseline(before.data(), before.size(), after.data(), after.size(), quest) == expected);
        CHECK(before == original_before && after == original_after);
    };
    preserve(container({z, ww, a}), 0, true); // Reorder only.
    for (std::uint8_t selected = 0; selected != 3; ++selected) {
        auto changed_game = game; changed_game[selected * Quest + 5] ^= 0x51;
        u64(changed_game, selected * Quest + QuestData, quest_checksum(changed_game.data() + selected * Quest));
        auto selected_file = ww; selected_file.data = native_file(changed_game, 42);
        selected_file.time++; selected_file.banner = 0xF9; selected_file.permission = 0xFF;
        selected_file.data[0] ^= 1; selected_file.data[Sector - 1] ^= 1; // Real header/banner may update.
        const auto after = container({z, selected_file, a});
        matched(after, changed_game); preserve(after, selected, true);
        for (std::uint8_t other = 0; other != 3; ++other) if (other != selected) {
            auto extra_game = changed_game; extra_game[other * Quest + QuestData - 1] ^= 1;
            u64(extra_game, other * Quest + QuestData, quest_checksum(extra_game.data() + other * Quest));
            auto f = selected_file; f.data = native_file(extra_game, 42);
            preserve(container({z, f, a}), selected, false); // Valid but unselected quest changed.
        }
        for (std::size_t photo = 3; photo != 12; ++photo) {
            for (auto edge : {photo * Sector, (photo + 1) * Sector - 1}) {
                auto f = selected_file; f.data[edge] ^= 1;
                preserve(container({z, f, a}), selected, false); // Every photo's first and last byte.
            }
        }
    }
    auto first = ww; first.data = native_file(game, UINT32_MAX);
    auto last = ww; last.data = native_file(game, 0);
    const auto first_card = container({a, first, z}), last_card = container({z, last, a});
    CHECK(payload::preserves_baseline(first_card.data(), first_card.size(), last_card.data(), last_card.size(), 2));
    for (unsigned field = 0; field != 11; ++field) {
        auto changed = a;
        switch (field) {
        case 0: changed.id = 10; break;
        case 1: changed.name = "OTHER"; break;
        case 2: changed.game = "EFGH"; break;
        case 3: changed.company = "02"; break;
        case 4: changed.time++; break;
        case 5: changed.banner ^= 1; break;
        case 6: changed.permission ^= 1; break;
        case 7: changed.icon_address++; break;
        case 8: changed.icon_format++; break;
        case 9: changed.icon_speed++; break;
        case 10: changed.comment_address++; break;
        }
        preserve(container({z, ww, changed}), 0, false);
    }
    auto changed = a; changed.data[0] ^= 1; preserve(container({z, ww, changed}), 0, false);
    changed = a; changed.data.push_back(4); preserve(container({z, ww, changed}), 0, false);
    preserve(container({ww, z}), 0, false);
    changed.id = 10; changed.name = "fourth"; preserve(container({a, ww, z, changed}), 0, false);
    auto target = ww; target.id = 10; preserve(container({a, target, z}), 0, false);
    target = ww; target.name = "gczelda2"; preserve(container({a, target, z}), 0, false);
    target = ww; target.game = "GZLP"; preserve(container({a, target, z}), 0, false);
    target = ww; target.company = "02"; preserve(container({a, target, z}), 0, false);
    for (unsigned field = 0; field != 3; ++field) {
        auto bytes = before;
        if (field == 0) u64(bytes, 20, 0); // Card serial.
        if (field == 1) u16(bytes, 12, 8); // Capacity identity.
        if (field == 2) u16(bytes, 14, 1); // Encoding identity.
        preserve(bytes, 0, false);
    }
    for (std::uint8_t bad : {3, 255}) preserve(before, bad, false);
    CHECK(!payload::preserves_baseline(nullptr, before.size(), before.data(), before.size(), 0));
    CHECK(!payload::preserves_baseline(before.data(), before.size(), nullptr, before.size(), 0));
    CHECK(!payload::preserves_baseline(before.data(), Header - 1, before.data(), before.size(), 0));
    CHECK(!payload::preserves_baseline(before.data(), before.size(), before.data(), Header - 1, 0));
    Bytes too_large(payload::MaxCardBytes + 1, 0);
    CHECK(!payload::preserves_baseline(too_large.data(), too_large.size(), before.data(), before.size(), 0));
    CHECK(!payload::preserves_baseline(before.data(), before.size(), too_large.data(), too_large.size(), 0));
    auto malformed = before; malformed[0] ^= 1;
    CHECK(!payload::preserves_baseline(malformed.data(), malformed.size(), before.data(), before.size(), 0));
    CHECK(!payload::preserves_baseline(before.data(), before.size(), malformed.data(), malformed.size(), 0));
    target = ww; target.data[Sector + 8] ^= 1;
    const auto damaged = container({a, target, z});
    CHECK(!payload::preserves_baseline(damaged.data(), damaged.size(), before.data(), before.size(), 0));
    CHECK(!payload::preserves_baseline(before.data(), before.size(), damaged.data(), damaged.size(), 0));
}

int main() {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _CALL_REPORTFAULT); _set_error_mode(_OUT_TO_STDERR);
#endif
    CHECK(payload::GameBytes == GameBytes && payload::MaxCardBytes == 2u * 1024u * 1024u);
    CHECK(fnv(nullptr, 0) == 0x811C9DC5u);
    CHECK(fnv(reinterpret_cast<const std::uint8_t*>("abc"), 3) == 0x1A47E90Bu);
    Bytes all_zero(QuestData, 0), all_ff(QuestData, 255);
    CHECK(quest_checksum(all_zero.data()) == 0x00000000FFFFF898ull);
    CHECK(quest_checksum(all_ff.data()) == ((std::uint64_t(255u * QuestData) << 32) |
          std::uint32_t(0u - 256u * QuestData)));
    Bytes empty_block(Sector, 0); CHECK(block_checksum(empty_block.data()) == 0x0000F002u);
    std::fill(empty_block.begin(), empty_block.end(), 255);
    CHECK(block_checksum(empty_block.data()) == 0xF0020000u);
    const auto game = games(9);
    Record ww; ww.data = native_file(game, 41);
    const auto original = container({ww});
    auto m = matched(original, game);
    CHECK(m.card_serial == 0x0123456789ABCDEFull && m.save_count == 41 && m.file_number == 7);

    for (std::uint32_t counter : {0u, 1u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu}) {
        auto f = ww; f.id = 126; f.data = native_file(game, counter);
        const auto b = container({f}, 128, 1); m = matched(b, game);
        CHECK(m.save_count == counter && m.file_number == 126);
    }
    auto b = original; u64(b, 20, 0); m = matched(b, game); CHECK(m.card_serial == 0);
    b = original; u64(b, 20, UINT64_MAX); m = matched(b, game); CHECK(m.card_serial == UINT64_MAX);
    // Encoding is copied backend metadata, not a game/save success condition.
    b = original; u16(b, 14, 0xFFFF); matched(b, game);
    auto f = ww; f.id = 0; f.permission = 0xFF; f.time = 0; f.banner = 0xF9;
    auto unchanged_picture = f.data;
    std::fill(f.data.begin(), f.data.begin() + Sector, 0xD3);
    std::fill(f.data.begin() + 3 * Sector, f.data.end(), 0x6B);
    CHECK(!std::equal(f.data.begin(), f.data.begin() + Sector, unchanged_picture.begin()));
    m = matched(container({f}), game); CHECK(m.file_number == 0);
    f = ww; f.name = std::string("gczelda\0ignored", 15); matched(container({f}), game);
    // Unlike raw filesystem checksums, native game-block halves equal to FFFF
    // are not normalized to zero. Force each half through that exact boundary.
    for (std::uint16_t desired_sum : {0xFFFF, 0xF003}) {
        f = ww; const auto at = Sector + 8 + GameBytes;
        const auto current_sum = std::uint16_t(block_checksum(f.data.data() + Sector) >> 16);
        const auto old_word = std::uint16_t(unsigned(f.data[at]) * 256u + f.data[at + 1]);
        u16(f.data, at, std::uint16_t(old_word + desired_sum - current_sum));
        u32(f.data, 2 * Sector - 4, block_checksum(f.data.data() + Sector));
        CHECK(std::uint16_t(block_checksum(f.data.data() + Sector) >> 16) == desired_sum);
        std::copy_n(f.data.begin() + Sector, Sector, f.data.begin() + 2 * Sector);
        matched(container({f}), game);
    }

    pointer_reject(nullptr, original.size(), game.data(), game.size(), Status::InvalidInput);
    pointer_reject(original.data(), original.size(), nullptr, game.size(), Status::InvalidInput);
    pointer_reject(original.data(), original.size(), game.data(), game.size() - 1, Status::InvalidInput);
    pointer_reject(original.data(), original.size(), game.data(), game.size() + 1, Status::InvalidInput);
    Bytes oversized(payload::MaxCardBytes + 1, 0);
    rejects(oversized, game, Status::InvalidInput);
    // Structural prefix cuts use the original declared body length; every
    // header/record field boundary plus both native-copy ends is exercised.
    for (std::size_t n = 0; n != Header + RecordSize; ++n)
        pointer_reject(original.data(), n, game.data(), game.size(), n < Header ? Status::InvalidInput : Status::InvalidContainer);
    for (std::size_t n : {Header + RecordSize, Header + RecordSize + Sector,
                         Header + RecordSize + 2 * Sector - 1, Header + RecordSize + 3 * Sector - 1,
                         original.size() - 1})
        pointer_reject(original.data(), n, game.data(), game.size(), Status::InvalidContainer);
    // With an updated body hash/length, parsing must still reject a truncated
    // record, length overrun or missing counted record, not merely hash damage.
    for (std::size_t n : {Header, Header + RecordSize - 1, Header + RecordSize,
                         original.size() - 1}) {
        b.assign(original.begin(), original.begin() + n); u32(b, 32, std::uint32_t(n - Header)); body_hash(b);
        rejects(b, game, Status::InvalidContainer);
    }
    for (unsigned magic_byte = 0; magic_byte != 8; ++magic_byte) {
        b = original; b[magic_byte] ^= 1; rejects(b, game, Status::InvalidContainer);
    }
    b = original; u32(b, 8, 2); rejects(b, game, Status::InvalidContainer);
    for (std::uint16_t mbits : {0, 3, 12, 256}) { b = original; u16(b, 12, mbits); rejects(b, game, Status::InvalidContainer); }
    b = original; u32(b, 16, 0x1000); rejects(b, game, Status::InvalidContainer);
    for (std::uint32_t count : {0u, 2u, 128u, UINT32_MAX}) {
        b = original; u32(b, 28, count); rejects(b, game, Status::InvalidContainer);
    }
    b = original; u32(b, 32, std::uint32_t(b.size() - Header + 1)); rejects(b, game, Status::InvalidContainer);
    b = original; b[36] ^= 1; rejects(b, game, Status::InvalidContainer);
    b = original; b[Header + 64] ^= 1; body_hash(b); rejects(b, game, Status::InvalidContainer);
    b = original; b[Header + RecordSize] ^= 1; body_hash(b); rejects(b, game, Status::InvalidContainer);
    b = original; u16(b, Header, 127); body_hash(b); rejects(b, game, Status::InvalidContainer);
    b = original; u32(b, Header + 4, UINT32_MAX); body_hash(b); rejects(b, game, Status::InvalidContainer);
    b = original; b.push_back(0); u32(b, 32, std::uint32_t(b.size() - Header)); body_hash(b);
    rejects(b, game, Status::InvalidContainer);

    for (auto name : {std::string("other"), std::string("gczelda2"), std::string()}) {
        f = ww; f.name = name; rejects(container({f}), game, Status::MissingGame);
    }
    f = ww; f.game = "GZLP"; rejects(container({f}), game, Status::MissingGame);
    f = ww; f.company = "02"; rejects(container({f}), game, Status::MissingGame);
    Record other; other.id = 8; other.name = "other"; other.game = "ABCD"; other.data = Bytes{1, 2, 3};
    rejects(container({}), game, Status::MissingGame);
    matched(container({other, ww}), game); matched(container({ww, other}), game);
    auto empty_file = other; empty_file.data.clear(); matched(container({ww, empty_file}), game);
    f = other; f.id = ww.id; rejects(container({ww, f}), game, Status::InvalidContainer);
    f = ww; f.id = 8; rejects(container({ww, f}), game, Status::InvalidContainer);
    f = other; f.id = 9; f.name = std::string("other\0suffix", 12);
    rejects(container({ww, other, f}), game, Status::InvalidContainer);
    f.company = "02"; matched(container({ww, other, f}), game); // Scoped identity.
    other.data = Bytes(47 * Sector, 0xB5); matched(container({ww, other}), game);
    other.data.push_back(0); rejects(container({ww, other}), game, Status::InvalidContainer);
    other.data = Bytes(payload::MaxCardBytes - Header - 2 * RecordSize - NativeFile, 0xA5);
    auto maximum = container({ww, other}, 128);
    CHECK(maximum.size() == payload::MaxCardBytes); matched(maximum, game);
    std::vector<Record> many;
    f = ww; f.id = 126; many.push_back(f);
    for (std::uint16_t i = 0; i != 126; ++i) {
        Record r; r.id = i; r.game = "ABCD"; r.name = "aux" + std::to_string(i); r.data = Bytes{std::uint8_t(i)}; many.push_back(r);
    }
    m = matched(container(many, 16), game); CHECK(m.file_number == 126);
    other.data = Bytes{0}; // Isolate count128 without exceeding MaxCardBytes first.
    many.push_back(other); rejects(container(many, 16), game, Status::InvalidContainer);

    for (std::size_t length : {NativeFile - 1, NativeFile + 1}) {
        f = ww; f.data.resize(length); rejects(container({f}), game, Status::InvalidNativeMetadata);
    }
    for (std::uint8_t banner : {0, 2, 3, 5}) { f = ww; f.banner = banner; rejects(container({f}), game, Status::InvalidNativeMetadata); }
    f = ww; f.icon_address = 1; rejects(container({f}), game, Status::InvalidNativeMetadata);
    for (std::uint16_t format : {0, 2, 0x1001}) { f = ww; f.icon_format = format; rejects(container({f}), game, Status::InvalidNativeMetadata); }
    for (std::uint16_t speed : {0, 1, 0x4003}) { f = ww; f.icon_speed = speed; rejects(container({f}), game, Status::InvalidNativeMetadata); }
    f = ww; f.comment_address = 0; rejects(container({f}), game, Status::InvalidNativeMetadata);
    native_corruption(original, game);
    for (std::size_t slot = 0; slot != 3; ++slot) {
        auto captured = game; captured[slot * Quest + QuestData] ^= 1;
        rejects(original, captured, Status::InvalidNativeChecksum);
        captured = game; captured[slot * Quest] ^= 1;
        u64(captured, slot * Quest + QuestData, quest_checksum(captured.data() + slot * Quest));
        rejects(original, captured, Status::PayloadMismatch);
    }
    inspection_delta(original, game); preservation_delta(ww, game);
    std::printf("Randomizer CARD payload checks=%u; synthetic copied bytes only, no native save authority\n", checks);
    return 0;
}
