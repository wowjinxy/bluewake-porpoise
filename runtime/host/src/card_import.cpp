// SPDX-License-Identifier: GPL-3.0-or-later
#include "card_import.h"
#include <algorithm>
#include <cstring>
#include <set>
#include <utility>

namespace {
constexpr size_t block_size = 0x2000, file_size = 0x18000;
uint16_t be16(const uint8_t* p) { return uint16_t(p[0]) << 8 | p[1]; }
uint32_t be32(const uint8_t* p) { return uint32_t(be16(p)) << 16 | be16(p + 2); }
uint64_t be64(const uint8_t* p) { return uint64_t(be32(p)) << 32 | be32(p + 4); }
void put16(uint8_t* p, uint16_t value) { p[0] = uint8_t(value >> 8); p[1] = uint8_t(value); }
void put32(uint8_t* p, uint32_t value) { put16(p, uint16_t(value >> 16)); put16(p + 2, uint16_t(value)); }
void put64(uint8_t* p, uint64_t value) { put32(p, uint32_t(value >> 32)); put32(p + 4, uint32_t(value)); }
bool reject(std::string& error, const char* message) { error = message; return false; }
bool size_mbits(uint16_t value) { return value >= 4 && value <= 128 && (value & (value - 1)) == 0; }
uint32_t hash32(const uint8_t* p, size_t size) {
    uint32_t value = 2166136261u;
    for (size_t i = 0; i < size; ++i) value = (value ^ p[i]) * 16777619u;
    return value;
}
uint64_t gci_serial(const std::vector<uint8_t>& bytes) {
    // GCI carries no card identity. Stable content-based identity avoids a new
    // serial on every retry; Wind Waker's payload itself is not serial-bound.
    uint64_t value = 14695981039346656037ull;
    for (uint8_t byte : bytes) value = (value ^ byte) * 1099511628211ull;
    return value ? value : 0x475A4C4530314743ull;
}
std::pair<uint16_t, uint16_t> checksums(const uint8_t* p, size_t size, bool normalize) {
    uint16_t sum = 0, inverse = 0;
    for (size_t i = 0; i < size; i += 2) {
        const uint16_t word = be16(p + i);
        sum = uint16_t(sum + word); inverse = uint16_t(inverse + (word ^ 0xFFFFu));
    }
    if (normalize) { if (sum == 0xFFFF) sum = 0; if (inverse == 0xFFFF) inverse = 0; }
    return {sum, inverse};
}
bool checksum_ok(const uint8_t* data, size_t size, const uint8_t* stored) {
    const auto expected = checksums(data, size, true);
    return expected.first == be16(stored) && expected.second == be16(stored + 2);
}
bool game_slot_ok(const uint8_t* p) {
    uint32_t sum = 0, inverse = 0;
    for (size_t i = 0; i < 0x768; ++i) {
        sum += p[i];
        // The game's u8 is promoted to int before ~: this is -(byte + 1),
        // not an eight-bit complement. Both accumulators wrap as u32.
        inverse += ~uint32_t(p[i]);
    }
    return be64(p + 0x768) == ((uint64_t(sum) << 32) | inverse);
}
bool game_copy_ok(const uint8_t* block) {
    if (be32(block + 4) != 0) return false;
    const auto checksum = checksums(block, 0x1FFC, false);
    if (be32(block + 0x1FFC) != (uint32_t(checksum.first) << 16 | checksum.second)) return false;
    for (size_t slot = 0; slot < 3; ++slot) if (!game_slot_ok(block + 8 + slot * 0x770)) return false;
    return true;
}
bool native_file(const uint8_t* entry, std::string& error) {
    if (std::memcmp(entry, "GZLE01", 6) != 0)
        return reject(error, "This import is not the supported GZLE01 Wind Waker save. Other games and regions were not imported.");
    if (std::memcmp(entry + 8, "gczelda", 7) != 0 || entry[15] != 0)
        return reject(error, "The Wind Waker game file must have its native gczelda filename.");
    if (be16(entry + 0x38) != 12 || (entry[7] & 7) != 1 || be32(entry + 0x2C) != 0 ||
        be16(entry + 0x30) != 1 || be16(entry + 0x32) != 3 || be32(entry + 0x3C) != 0x1C00)
        return reject(error, "The imported save's size or banner/icon metadata does not match Wind Waker's native CARD checks.");
    return true;
}
bool payload_ok(const std::vector<uint8_t>& payload, BwCardImportInfo& info, std::string& error) {
    if (payload.size() != file_size || !game_copy_ok(payload.data() + block_size))
        return reject(error, "The primary Wind Waker save copy has an unsupported version or damaged quest-log checksum. Recover it in a memory-card tool and export it again; no save bytes were repaired.");
    info.backup_copy_valid = game_copy_ok(payload.data() + 2 * block_size);
    return true;
}
std::vector<uint8_t> serialize(const uint8_t* entry, const std::vector<uint8_t>& payload,
                               uint16_t mbits, uint16_t encoding, uint64_t serial) {
    std::vector<uint8_t> output(40 + 68 + payload.size(), 0);
    auto* header = output.data(); auto* record = header + 40;
    std::memcpy(header, "DOLCARD1", 8); put32(header + 8, 1);
    put16(header + 12, mbits); put16(header + 14, encoding);
    put32(header + 16, uint32_t(block_size)); put64(header + 20, serial);
    put32(header + 28, 1); put32(header + 32, uint32_t(68 + payload.size()));
    put16(record, 0); put32(record + 4, uint32_t(payload.size()));
    put32(record + 8, be32(entry + 0x28)); std::memcpy(record + 12, entry + 8, 32);
    std::memcpy(record + 44, entry, 6); record[50] = entry[7]; record[51] = entry[0x34];
    put32(record + 52, be32(entry + 0x2C)); put16(record + 56, be16(entry + 0x30));
    put16(record + 58, be16(entry + 0x32)); put32(record + 60, be32(entry + 0x3C));
    put32(record + 64, hash32(payload.data(), payload.size()));
    std::copy(payload.begin(), payload.end(), output.begin() + 108);
    put32(header + 36, hash32(record, output.size() - 40));
    return output;
}
bool directory_ok(const uint8_t* p) { return checksum_ok(p, 0x1FFC, p + 0x1FFC); }
bool bat_ok(const uint8_t* p, size_t blocks) {
    if (!checksum_ok(p + 4, block_size - 4, p)) return false;
    size_t free = 0;
    // Native VerifyFAT uses physical free blocks, not lastAllocated or unused
    // map bytes. Those fields must not demote a checksum-valid newest copy.
    for (size_t index = 0; index < blocks - 5; ++index) {
        const uint16_t next = be16(p + 0xA + index * 2);
        if (next == 0) ++free;
    }
    return free == be16(p + 6);
}
int counter(uint16_t value) { return value < 0x8000 ? int(value) : int(value) - 0x10000; }
const uint8_t* active_copy(const uint8_t* first, const uint8_t* second,
                           bool valid_first, bool valid_second, size_t counter_offset) {
    if (!valid_first) return second;
    if (!valid_second) return first;
    // GC BIOS compares signed counters without wraparound protection. Use the
    // same selection; never silently substitute an older but coherent graph.
    return counter(be16(first + counter_offset)) >= counter(be16(second + counter_offset)) ? first : second;
}
bool raw(const std::vector<uint8_t>& source, std::vector<uint8_t>& container,
         BwCardImportInfo& info, std::string& error) {
    const auto* header = source.data();
    const uint16_t mbits = be16(header + 0x22), encoding = be16(header + 0x24);
    const size_t blocks = source.size() / block_size;
    if (!size_mbits(mbits) || blocks != size_t(mbits) * 16 || encoding > 1 || be16(header + 0x20) > 1 ||
        !checksum_ok(header, 0x1FC, header + 0x1FC))
        return reject(error, "The raw card has an invalid header, encoding, geometry or checksum.");
    const auto* dir_a = header + block_size; const auto* dir_b = dir_a + block_size;
    const auto* bat_a = dir_b + block_size; const auto* bat_b = bat_a + block_size;
    const bool da = directory_ok(dir_a), db = directory_ok(dir_b),
               ba = bat_ok(bat_a, blocks), bb = bat_ok(bat_b, blocks);
    if ((!da + !db + !ba + !bb) > 1)
        return reject(error, "More than one raw-card directory/allocation copy is damaged. Recover the card in a memory-card tool before importing.");
    info.redundant_filesystem_copy_used = !(da && db && ba && bb);
    const auto* dir = active_copy(dir_a, dir_b, da, db, 0x1FFA);
    const auto* bat = active_copy(bat_a, bat_b, ba, bb, 4);
    std::vector<bool> referenced(blocks, false);
    std::set<std::string> identities;
    std::vector<uint8_t> payload;
    const uint8_t* chosen = nullptr;
    for (size_t i = 0; i < 127; ++i) {
        const auto* entry = dir + i * 64;
        if (std::all_of(entry, entry + 4, [](uint8_t b) { return b == 0xFF; })) continue;
        const auto* name_end = std::find(entry + 8, entry + 40, uint8_t(0));
        if (name_end == entry + 8 || !identities.emplace(std::string(reinterpret_cast<const char*>(entry), 6) +
                std::string(reinterpret_cast<const char*>(entry + 8), size_t(name_end - entry - 8))).second)
            return reject(error, "The raw card contains an empty or duplicate game-file identity.");
        const uint16_t count = be16(entry + 0x38);
        uint16_t current = be16(entry + 0x36);
        if (count == 0 || count > blocks - 5)
            return reject(error, "The raw card contains an invalid file length.");
        const bool wanted = std::memcmp(entry, "GZLE01", 6) == 0 &&
            std::memcmp(entry + 8, "gczelda", 7) == 0 && entry[15] == 0;
        if (wanted && (chosen || !native_file(entry, error))) {
            if (chosen) reject(error, "The raw card contains more than one supported Wind Waker game file.");
            return false;
        }
        if (wanted) { chosen = entry; payload.reserve(file_size); }
        for (size_t n = 0; n < count; ++n) {
            if (current < 5 || current >= blocks || referenced[current])
                return reject(error, "The raw-card file graph has an out-of-range block, overlap or loop.");
            referenced[current] = true;
            if (wanted) payload.insert(payload.end(), header + current * block_size, header + (current + 1) * block_size);
            const auto next = be16(bat + 0xA + size_t(current - 5) * 2);
            if ((n + 1 == count && next != 0xFFFF) || (n + 1 < count && next == 0xFFFF))
                return reject(error, "The raw-card allocation chain does not match its directory file length.");
            current = next;
        }
    }
    for (size_t n = 5; n < blocks; ++n)
        if (referenced[n] != (be16(bat + 0xA + (n - 5) * 2) != 0))
            return reject(error, "The raw card has allocated blocks without a matching directory file.");
    if (!chosen) return reject(error, "No GZLE01 gczelda game file was found in this raw card.");
    if (!payload_ok(payload, info, error)) return false;
    uint64_t serial = 0;
    for (size_t i = 0; i < 32; i += 8) serial ^= be64(header + i);
    auto output = serialize(chosen, payload, mbits, encoding, serial);
    container.swap(output); info.format = BwCardImportInfo::Format::Raw;
    return true;
}
}

bool bluewake_card_import_convert(const std::vector<uint8_t>& source,
    std::vector<uint8_t>& container, BwCardImportInfo& info, std::string& error) {
    error.clear(); info = {};
    if (source.size() >= 64 * block_size && source.size() <= 2048 * block_size &&
        source.size() % block_size == 0 && size_mbits(uint16_t(source.size() / (16 * block_size))))
        return raw(source, container, info, error);
    if (source.size() != 64 + file_size)
        return reject(error, "Choose a valid BlueWake .card, GZLE01 Wind Waker .gci, or standard GameCube .raw memory card.");
    if (!native_file(source.data(), error)) return false;
    std::vector<uint8_t> payload(source.begin() + 64, source.end());
    if (!payload_ok(payload, info, error)) return false;
    auto output = serialize(source.data(), payload, 4, 0, gci_serial(source));
    container.swap(output); return true;
}
