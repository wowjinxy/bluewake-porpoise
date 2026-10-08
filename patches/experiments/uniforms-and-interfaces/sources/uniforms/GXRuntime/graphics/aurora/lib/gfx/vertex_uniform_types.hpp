// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
namespace aurora::gfx {
struct VertexUniformRanges {
  uint32_t block = 0;
  uint32_t matrices = 0;
  uint32_t lights = 0;
  uint32_t size = 0;
  bool operator==(const VertexUniformRanges&) const = default;
};

} // namespace aurora::gfx
