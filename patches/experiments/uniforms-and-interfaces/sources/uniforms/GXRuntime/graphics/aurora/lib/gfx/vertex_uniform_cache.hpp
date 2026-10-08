// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "vertex_uniform_types.hpp"
#include <gxruntime/gxcore/shader.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

namespace aurora::gfx {

// Canonical equality is needed by interpolation even when shader prefixes
// omit fields. Without interpolation, constants_id is sufficient evidence
// for repeat shortcuts and each bank otherwise compares its consumed bytes.
// Failed draws never commit identity. Turning interpolation off invalidates
// the snapshot immediately, including when that draw later fails to stage.
struct VertexConstantsIdentity {
  uint64_t frameId = 0;
  uint64_t constantsId = 0;
  bool snapshotValid = false;
  gxruntime::gxcore::VertexShaderConstants snapshot{};

  bool repeats(uint64_t frame, uint64_t id,
               const gxruntime::gxcore::VertexShaderConstants& constants, bool interpolating) {
    if (!interpolating) snapshotValid = false;
    if (frame == 0 || frame != frameId) return false;
    if (!interpolating) return id != 0 && id == constantsId;
    return snapshotValid && ((id != 0 && id == constantsId) ||
                            std::memcmp(&snapshot, &constants, sizeof(constants)) == 0);
  }
  void commit(uint64_t frame, uint64_t id,
              const gxruntime::gxcore::VertexShaderConstants& constants,
              bool interpolating, bool repeated) {
    if (interpolating && (!snapshotValid || frame != frameId || !repeated))
      std::memcpy(&snapshot, &constants, sizeof(constants));
    snapshotValid = interpolating;
    frameId = frame;
    constantsId = id;
  }
};

struct VertexPartRange {
  uint32_t offset = 0;
  uint32_t size = 0;
};

struct VertexPartCache {
  VertexPartRange range{};
  size_t staged = 0;
  uint64_t frameId = 0;
  uint64_t serial = 0;
  size_t matched = 0;
  std::vector<uint8_t> bytes;
};

struct VertexPartCaches {
  VertexPartCache part[3];
  uint64_t serial = 0;
};

// Invalidate shortcuts after any failure following a successful bank stage,
// too: the canonical identity still names the last successful draw, while
// parts may already hold constants from the abandoned attempted draw.
inline void invalidate_vertex_parts(VertexPartCaches& caches) {
  for (auto& part : caches.part) part.matched = 0;
}

// Push and Read use the caller's actual frame or interpolation staging area.
// A failed push leaves no usable result. Previously completed parts remain
// valid cache entries for a later attempt in the same owned frame.
template <bool CompareStaged, class Push, class Read>
bool stage_vertex_uniforms(VertexPartCaches& caches, uint64_t frameId,
                           const gxruntime::gxcore::VertexShaderConstants& constants,
                           const gxruntime::gxcore::VertexUniformUse& use,
                           bool repeats, bool dedup, bool sparse,
                           Push&& push, Read&& read, VertexUniformRanges& out) {
  namespace gxc = gxruntime::gxcore;
  out = {};
  const size_t reads[3] = {use.block, use.matrices, use.lights};
  if (frameId == 0 ||
      reads[0] > (sparse ? gxc::kVertexBlockBytes : sizeof(constants)) ||
      reads[1] > (sparse ? gxc::kVertexMatrixBytes : 0u) ||
      reads[2] > (sparse ? gxc::kVertexLightBytes : 0u))
    return false;
  const auto* base = reinterpret_cast<const uint8_t*>(&constants);
  const size_t starts[3] = {0, gxc::kVertexMatrixOffset, gxc::kVertexLightOffset};
  const uint64_t serial = ++caches.serial;
  uint32_t offsets[3]{};
  for (int i = 0; i < 3; ++i) {
    auto& cache = caches.part[i];
    const auto* data = base + starts[i];
    const bool current = cache.frameId == frameId && cache.range.size != 0;
    if (reads[i] == 0) {
      offsets[i] = current ? cache.range.offset : 0u;
      continue;
    }
    if (current && cache.staged >= reads[i] && dedup) {
      if (repeats && cache.serial == serial - 1 && cache.matched >= reads[i]) {
        cache.serial = serial;
        offsets[i] = cache.range.offset;
        continue;
      }
      const uint8_t* bytes;
      if constexpr (CompareStaged)
        bytes = read(cache.range);
      else
        bytes = cache.bytes.data();
      if (std::memcmp(bytes, data, reads[i]) == 0) {
        cache.serial = serial;
        cache.matched = reads[i];
        offsets[i] = cache.range.offset;
        continue;
      }
    }
    const VertexPartRange range = push(data, reads[i]);
    if (range.size == 0) {
      // The caller's repeats evidence names its last successful draw. A failed
      // attempt may already have changed earlier parts, so no part may use the
      // immediately-previous-serial shortcut until it compares bytes again.
      invalidate_vertex_parts(caches);
      return false;
    }
    cache.range = range;
    cache.staged = cache.matched = reads[i];
    cache.frameId = frameId;
    cache.serial = serial;
    if constexpr (!CompareStaged)
      cache.bytes.assign(data, data + reads[i]);
    offsets[i] = range.offset;
  }
  out = {.block = offsets[0], .matrices = offsets[1], .lights = offsets[2], .size = 1u};
  return true;
}

} // namespace aurora::gfx
