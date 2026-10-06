// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace bluewake::randomizer::card_payload {
constexpr std::size_t MaxCardBytes = 2u * 1024u * 1024u;
constexpr std::size_t GameBytes = 3u * 0x770u;
enum class Status {
  Matched, InvalidInput, InvalidContainer, MissingGame,
  InvalidNativeMetadata, InvalidNativeChecksum, PayloadMismatch
};
struct Match {
  std::uint64_t card_serial = 0;
  std::uint32_t save_count = 0;
  std::uint16_t file_number = 0;
};
// Immutable copied bytes only. No file/backend/guest read or pointer escapes.
// Both GZLE01 gczelda native save copies must be identical, checksum-valid,
// and contain the exact captured post-checksum three-quest STORE buffer.
// Header/photos are preserved and need not equal an earlier CARD. Matched
// establishes byte correspondence only, never native save success, lifetime,
// selected quest, seed ownership or durable paired-ledger publication.
// The adapter must separately prove those and snapshot this same mounted CARD
// under its runtime lock. Failure zeroes every output field.
Status validate(const std::uint8_t* card, std::size_t card_size,
                const std::uint8_t* captured_game, std::size_t captured_size,
                Match& out) noexcept;
// Before backend open: extract a COPY of the valid redundant native game
// payload. Both outputs are zero on failure. No selected-quest compatibility
// or actual native load is inferred from these bytes.
Status inspect(const std::uint8_t* card, std::size_t card_size,
               std::array<std::uint8_t, GameBytes>& game, Match& out) noexcept;
// Both containers must independently pass inspect. Allow the selected quest,
// native counters and banner/header to change during a real save; require exact
// other-quest, photograph and unrelated-file preservation. File order may
// change, but card identity and file IDs remain the same. No digest shortcut,
// native save success or publication authority is implied.
bool preserves_baseline(const std::uint8_t* before, std::size_t before_size,
                        const std::uint8_t* after, std::size_t after_size,
                        std::uint8_t selected_quest) noexcept;
} // namespace bluewake::randomizer::card_payload
