#include <gxruntime/aurora_recomp/render_sink.hpp>
#include <gxruntime/gxcore/shader.hpp>
namespace gxruntime::aurora_recomp {
std::uint32_t build_topology_indices(GxPrimitive primitive,
                                     std::uint16_t vtx_start,
                                     std::uint16_t vtx_count,
                                     std::vector<std::uint16_t>* out) {
  std::uint32_t num = 0;
  const auto push = [&](std::uint16_t v) {
    if (out != nullptr)
      out->push_back(v);
    ++num;
  };
  switch (primitive) {
  case GxPrimitive::Quads:
    // Each quad -> two triangles {0,1,2} {2,3,0}. Trailing verts (< 4) ignored,
    // matching Aurora's v += 4 loop.
    for (std::uint16_t v = 0; v + 4u <= vtx_count; v += 4u) {
      const std::uint16_t i0 = static_cast<std::uint16_t>(vtx_start + v);
      const std::uint16_t i1 = static_cast<std::uint16_t>(vtx_start + v + 1);
      const std::uint16_t i2 = static_cast<std::uint16_t>(vtx_start + v + 2);
      const std::uint16_t i3 = static_cast<std::uint16_t>(vtx_start + v + 3);
      push(i0);
      push(i1);
      push(i2);
      push(i2);
      push(i3);
      push(i0);
    }
    break;
  case GxPrimitive::Triangles:
    for (std::uint16_t v = 0; v < vtx_count; ++v)
      push(static_cast<std::uint16_t>(vtx_start + v));
    break;
  case GxPrimitive::TriangleFan:
    for (std::uint16_t v = 0; v < vtx_count; ++v) {
      const std::uint16_t idx = static_cast<std::uint16_t>(vtx_start + v);
      if (v < 3u) {
        push(idx);
        continue;
      }
      push(vtx_start);
      push(static_cast<std::uint16_t>(idx - 1));
      push(idx);
    }
    break;
  case GxPrimitive::TriangleStrip:
    for (std::uint16_t v = 0; v < vtx_count; ++v) {
      const std::uint16_t idx = static_cast<std::uint16_t>(vtx_start + v);
      if (v < 3u) {
        push(idx);
        continue;
      }
      if ((v & 1u) == 0u) {
        push(static_cast<std::uint16_t>(idx - 2));
        push(static_cast<std::uint16_t>(idx - 1));
        push(idx);
      } else {
        push(static_cast<std::uint16_t>(idx - 1));
        push(static_cast<std::uint16_t>(idx - 2));
        push(idx);
      }
    }
    break;
  case GxPrimitive::Lines:
    for (std::uint16_t v = 0; v + 2u <= vtx_count; v += 2u) {
      push(static_cast<std::uint16_t>(vtx_start + v));
      push(static_cast<std::uint16_t>(vtx_start + v + 1u));
    }
    break;
  case GxPrimitive::LineStrip:
    for (std::uint16_t v = 0; v + 1u < vtx_count; ++v) {
      push(static_cast<std::uint16_t>(vtx_start + v));
      push(static_cast<std::uint16_t>(vtx_start + v + 1u));
    }
    break;
  case GxPrimitive::Points:
    for (std::uint16_t v = 0; v < vtx_count; ++v)
      push(static_cast<std::uint16_t>(vtx_start + v));
    break;
  default:
    break;
  }
  return num;
}
}
namespace gxruntime::gxcore {
bool channel_lit_path(const ShaderKey& k, unsigned j) {
  if ((k.chan_captured_mask & (1u << j)) == 0u)
    return false;
  const LightChanKey& col = k.litchan[j];
  const LightChanKey& alp = k.litchan[j + 2];
  return col.enablelighting != 0 || alp.enablelighting != 0 ||
         static_cast<MatSource>(col.matsource) == MatSource::Register ||
         static_cast<MatSource>(alp.matsource) == MatSource::Register;
}
}
