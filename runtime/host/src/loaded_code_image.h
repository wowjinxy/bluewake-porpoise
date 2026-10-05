// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded executable-image parser: bounded PE64 expected executable image, no OS calls.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace bw_ic_code {
inline constexpr std::size_t kMaxFile = 1024ull * 1024ull * 1024ull;
inline constexpr std::uint32_t kMaxSections = 96;
// Large translated modules contain millions of DIR64 entries. Keep a hard
// bound that covers the full module while retaining overflow and overlap checks.
inline constexpr std::uint32_t kMaxRelocations = 4 * 1024 * 1024;
struct ExecutableSpan { std::uint32_t rva = 0; std::vector<std::uint8_t> bytes; };
struct Image {
    std::uint64_t preferred_base = 0;
    std::uint32_t image_size = 0, headers_size = 0;
    std::vector<ExecutableSpan> executable;
};
// Checked parsing and DIR64 relocation; failure clears out. No guest data.
bool build_expected(const std::uint8_t* file, std::size_t size,
                    std::uint64_t actual_base, Image& out) noexcept;
bool executable_contains(const Image&, std::uint32_t rva, std::size_t size) noexcept;
} // namespace bw_ic_code
