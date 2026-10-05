// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
namespace card_import_fixture {
using Bytes = std::vector<uint8_t>;
inline uint16_t u16(const uint8_t* p) { return uint16_t(p[0]) * 256 + p[1]; }
inline void p16(uint8_t* p, uint16_t n) { p[0] = uint8_t(n / 256); p[1] = uint8_t(n); }
inline void p32(uint8_t* p, uint32_t n) { p16(p, uint16_t(n >> 16)); p16(p + 2, uint16_t(n)); }
inline void p64(uint8_t* p, uint64_t n) { p32(p, uint32_t(n >> 32)); p32(p + 4, uint32_t(n)); }
inline void checksum(uint8_t* p, size_t size, uint8_t* target, bool normalize = true) {
    uint16_t a = 0, b = 0;
    for (size_t i = 0; i < size; i += 2) { a = uint16_t(a + u16(p + i)); b = uint16_t(b + (65535u - u16(p + i))); }
    p16(target, normalize && a == 65535 ? 0 : a); p16(target + 2, normalize && b == 65535 ? 0 : b);
}
inline void game_checksums(uint8_t* block) {
    for (size_t s = 0; s < 3; ++s) {
        auto* p = block + 8 + s * 0x770;
        uint32_t a = 0;
        for (size_t i = 0; i < 0x768; ++i) a += p[i];
        // Independent algebraic form of the native promoted-complement sum.
        p64(p + 0x768, uint64_t(a) << 32 | uint32_t(0u - a - 0x768u));
    }
    checksum(block, 0x1FFC, block + 0x1FFC, false);
}
inline Bytes gci() {
    Bytes result(64 + 0x18000, 0);
    auto* d = result.data(); std::memcpy(d, "GZLE01", 6); d[6] = 255; d[7] = 1;
    std::memcpy(d + 8, "gczelda", 7); p32(d + 0x28, 0x13579BDF);
    p16(d + 0x30, 1); p16(d + 0x32, 3); d[0x34] = 4; d[0x35] = 2;
    p16(d + 0x36, 0xFFFF); p16(d + 0x38, 12); p16(d + 0x3A, 0xFFFF); p32(d + 0x3C, 0x1C00);
    auto* payload = d + 64;
    for (size_t i = 0; i < 0x18000; ++i) payload[i] = uint8_t((i * 29 + i / 8192 * 43 + 7) & 255);
    for (size_t b : {size_t(1), size_t(2)}) {
        auto* block = payload + b * 8192; p32(block, 23); p32(block + 4, 0);
        game_checksums(block);
    }
    return result;
}
inline void raw_checksums(Bytes& raw) {
    auto* p = raw.data(); checksum(p, 0x1FC, p + 0x1FC);
    for (size_t b : {size_t(1), size_t(2)}) checksum(p + b * 8192, 0x1FFC, p + b * 8192 + 0x1FFC);
    for (size_t b : {size_t(3), size_t(4)}) checksum(p + b * 8192 + 4, 8192 - 4, p + b * 8192);
}
inline Bytes raw(const Bytes& gci_bytes) {
    Bytes result(64 * 8192, 255); auto* p = result.data();
    // Nonzero raw header bytes exercise XOR-derived native CARD serial.
    for (size_t i = 0; i < 32; ++i) p[i] = uint8_t(i * i + i + 1);
    p16(p + 0x20, 0); p16(p + 0x22, 4); p16(p + 0x24, 0);
    const uint16_t chain[] = {5, 9, 6, 11, 7, 13, 8, 15, 10, 16, 12, 14};
    for (size_t b : {size_t(1), size_t(2)}) {
        auto* dir = p + b * 8192;
        auto* foreign = dir; std::fill(foreign, foreign + 64, 0);
        std::memcpy(foreign, "ABCD99", 6); std::memcpy(foreign + 8, "another-game", 12);
        p16(foreign + 0x36, 17); p16(foreign + 0x38, 2);
        std::copy_n(gci_bytes.data(), 64, dir + 37 * 64); p16(dir + 37 * 64 + 0x36, chain[0]);
        p16(dir + 0x1FFA, uint16_t(b == 1 ? 10 : 9));
    }
    for (size_t b : {size_t(3), size_t(4)}) {
        auto* bat = p + b * 8192; std::fill(bat, bat + 8192, 0);
        p16(bat + 4, uint16_t(b == 3 ? 10 : 9)); p16(bat + 6, 45); p16(bat + 8, 18);
        for (size_t i = 0; i < 12; ++i) p16(bat + 10 + (chain[i] - 5) * 2, i == 11 ? 0xFFFF : chain[i + 1]);
        p16(bat + 10 + (17 - 5) * 2, 18); p16(bat + 10 + (18 - 5) * 2, 0xFFFF);
    }
    for (size_t i = 0; i < 12; ++i) std::copy_n(gci_bytes.data() + 64 + i * 8192, 8192, p + chain[i] * 8192);
    raw_checksums(result); return result;
}
}
