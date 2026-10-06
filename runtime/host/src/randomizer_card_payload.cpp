// SPDX-License-Identifier: GPL-3.0-or-later
#include "randomizer_card_payload.h"
#include <cstring>

namespace bluewake::randomizer::card_payload {
namespace {
constexpr std::size_t Header = 40, Record = 68, Sector = 0x2000, FileSize = 0x18000;
constexpr unsigned MaxFiles = 127;
std::uint16_t be16(const std::uint8_t* p) { return std::uint16_t((unsigned(p[0]) << 8) | p[1]); }
std::uint32_t be32(const std::uint8_t* p) { return (std::uint32_t(be16(p)) << 16) | be16(p + 2); }
std::uint64_t be64(const std::uint8_t* p) { return (std::uint64_t(be32(p)) << 32) | be32(p + 4); }
std::uint32_t fnv(const std::uint8_t* p, std::size_t n) {
  std::uint32_t h = 2166136261u;
  for (std::size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 16777619u;
  return h;
}
unsigned name_size(const std::uint8_t* record) {
  unsigned n = 0; while (n < 32 && record[12 + n]) ++n; return n;
}
bool same_name(const std::uint8_t* a, const std::uint8_t* b) {
  const unsigned n = name_size(a);
  return n == name_size(b) && !std::memcmp(a + 44, b + 44, 6) &&
      !std::memcmp(a + 12, b + 12, n);
}
bool native_metadata(const std::uint8_t* record) {
  return be32(record + 4) == FileSize && (record[50] & 7u) == 1 &&
      be32(record + 52) == 0 && be16(record + 56) == 1 &&
      be16(record + 58) == 3 && be32(record + 60) == 0x1C00;
}
bool slot_ok(const std::uint8_t* p) {
  std::uint32_t sum = 0, inverse = 0;
  for (std::size_t i = 0; i < 0x768; ++i) {
    sum += p[i]; inverse += ~std::uint32_t(p[i]);
  }
  return be64(p + 0x768) == ((std::uint64_t(sum) << 32) | inverse);
}
bool game_ok(const std::uint8_t* game) {
  for (unsigned i = 0; i < 3; ++i) if (!slot_ok(game + i * 0x770)) return false;
  return true;
}
bool copy_ok(const std::uint8_t* p) {
  if (be32(p + 4) != 0 || !game_ok(p + 8)) return false;
  std::uint16_t sum = 0, inverse = 0;
  for (std::size_t i = 0; i < 0x1FFC; i += 2) {
    const auto word = be16(p + i);
    sum = std::uint16_t(sum + word);
    inverse = std::uint16_t(inverse + (word ^ 0xFFFFu));
  }
  return be32(p + 0x1FFC) == ((std::uint32_t(sum) << 16) | inverse);
}
struct Container {
  const std::uint8_t* records[MaxFiles] = {};
  const std::uint8_t* game_record = nullptr;
  std::uint32_t count = 0;
};
// The one bounded container parser is shared by validation and inspection.
// These pointers stay within this call and never appear in the public result.
Status parse(const std::uint8_t* card, std::size_t size, Container& parsed) {
  parsed = {};
  if (!card || size < Header || size > MaxCardBytes)
    return Status::InvalidInput;
  const auto mbits = be16(card + 12);
  const auto count = be32(card + 28), body = be32(card + 32);
  if (std::memcmp(card, "DOLCARD1", 8) || be32(card + 8) != 1 ||
      mbits < 4 || mbits > 128 || (mbits & (mbits - 1)) || be32(card + 16) != Sector ||
      count > MaxFiles || body != size - Header || be32(card + 36) != fnv(card + Header, body))
    return Status::InvalidContainer;
  bool ids[MaxFiles] = {};
  std::size_t offset = Header;
  std::uint64_t blocks = 0;
  const std::uint64_t capacity = std::uint64_t(mbits) * 16u - 5u;
  for (std::uint32_t i = 0; i < count; ++i) {
    if (size - offset < Record) return Status::InvalidContainer;
    const auto* r = card + offset;
    const auto id = be16(r);
    const auto length = be32(r + 4);
    if (id >= MaxFiles || ids[id] || length > size - offset - Record ||
        be32(r + 64) != fnv(r + Record, length)) return Status::InvalidContainer;
    ids[id] = true;
    for (std::uint32_t j = 0; j < i; ++j)
      if (same_name(r, parsed.records[j])) return Status::InvalidContainer;
    parsed.records[i] = r;
    blocks += (std::uint64_t(length) + Sector - 1u) / Sector;
    if (blocks > capacity) return Status::InvalidContainer;
    if (!std::memcmp(r + 44, "GZLE01", 6) && name_size(r) == 7 &&
        !std::memcmp(r + 12, "gczelda", 7)) {
      if (parsed.game_record) return Status::InvalidContainer;
      parsed.game_record = r;
    }
    offset += Record + length;
  }
  if (offset != size) return Status::InvalidContainer;
  if (!parsed.game_record) return Status::MissingGame;
  if (!native_metadata(parsed.game_record)) return Status::InvalidNativeMetadata;
  const auto* payload = parsed.game_record + Record;
  const auto* first = payload + Sector;
  const auto* second = payload + 2u * Sector;
  if (!copy_ok(first) || !copy_ok(second)) return Status::InvalidNativeChecksum;
  parsed.count = count;
  return Status::Matched;
}
Status checked(const std::uint8_t* card, std::size_t size, Container& parsed) {
  const auto status = parse(card, size, parsed);
  if (status != Status::Matched) return status;
  const auto* payload = parsed.game_record + Record;
  return std::memcmp(payload + Sector, payload + 2u * Sector, Sector) ?
      Status::PayloadMismatch : Status::Matched;
}
Match metadata(const std::uint8_t* card, const Container& parsed) {
  return {be64(card + 20), be32(parsed.game_record + Record + Sector), be16(parsed.game_record)};
}
} // namespace
Status validate(const std::uint8_t* card, std::size_t size,
                const std::uint8_t* captured, std::size_t captured_size,
                Match& out) noexcept {
  out = {};
  if (!card || !captured || size < Header || size > MaxCardBytes || captured_size != GameBytes)
    return Status::InvalidInput;
  Container parsed;
  const auto status = parse(card, size, parsed);
  if (status != Status::Matched) return status;
  const auto* payload = parsed.game_record + Record;
  const auto* first = payload + Sector;
  const auto* second = payload + 2u * Sector;
  if (!game_ok(captured)) return Status::InvalidNativeChecksum;
  if (std::memcmp(first, second, Sector) || std::memcmp(first + 8, captured, GameBytes) ||
      std::memcmp(second + 8, captured, GameBytes)) return Status::PayloadMismatch;
  out = metadata(card, parsed);
  return Status::Matched;
}
Status inspect(const std::uint8_t* card, std::size_t size,
               std::array<std::uint8_t, GameBytes>& game, Match& out) noexcept {
  game.fill(0); out = {};
  Container parsed;
  const auto status = checked(card, size, parsed);
  if (status != Status::Matched) return status;
  std::memcpy(game.data(), parsed.game_record + Record + Sector + 8, GameBytes);
  out = metadata(card, parsed);
  return Status::Matched;
}
bool preserves_baseline(const std::uint8_t* before, std::size_t before_size,
                        const std::uint8_t* after, std::size_t after_size,
                        std::uint8_t quest) noexcept {
  if (quest >= 3) return false;
  Container old, current;
  if (checked(before, before_size, old) != Status::Matched ||
      checked(after, after_size, current) != Status::Matched ||
      old.count != current.count || std::memcmp(before, after, 28) ||
      be16(old.game_record) != be16(current.game_record)) return false;
  // Native file identity, length and reserved record fields remain fixed.
  // Its native timestamp/status and banner may legitimately change.
  if (std::memcmp(old.game_record, current.game_record, 8) ||
      !same_name(old.game_record, current.game_record)) return false;
  for (std::uint32_t i = 0; i < old.count; ++i) {
    const auto* previous = old.records[i];
    if (previous == old.game_record) continue;
    const std::uint8_t* found = nullptr;
    for (std::uint32_t j = 0; j < current.count; ++j)
      if (be16(previous) == be16(current.records[j])) { found = current.records[j]; break; }
    if (!found || std::memcmp(previous, found, Record) ||
        std::memcmp(previous + Record, found + Record, be32(previous + 4))) return false;
  }
  const auto* old_payload = old.game_record + Record;
  const auto* new_payload = current.game_record + Record;
  for (unsigned slot = 0; slot < 3; ++slot)
    if (slot != quest && std::memcmp(old_payload + Sector + 8 + slot * 0x770,
                                    new_payload + Sector + 8 + slot * 0x770, 0x770)) return false;
  return !std::memcmp(old_payload + 3u * Sector, new_payload + 3u * Sector,
                      FileSize - 3u * Sector);
}
} // namespace bluewake::randomizer::card_payload
