from pathlib import Path
import hashlib,json,re,difflib
H=Path(__file__).resolve().parent
R=H.parents[2]
SDK=R/'ref/recompcore'
O=H/'overlay'
records=[]
def read(p):
    b=p.read_bytes()
    return b,b.decode('utf-8').replace('\r\n','\n')
def sub(s,a,b,n=1):
    assert s.count(a)==n,(a[:100],s.count(a),n)
    return s.replace(a,b)
def emit(rel,edit,root=False):
    src=(R if root else SDK)/rel
    b,s=read(src); t=edit(s)
    assert t!=s,rel
    out=O/('root' if root else 'sdk')/rel
    out.parent.mkdir(parents=True,exist_ok=True)
    n='\r\n' if b'\r\n' in b else '\n'
    payload=t.replace('\n',n).encode('utf-8');out.write_bytes(payload)
    records.append(dict(relative=rel,root=root,source=str(src),source_sha256=hashlib.sha256(b).hexdigest(),candidate=str(out),candidate_sha256=hashlib.sha256(payload).hexdigest(),bytes=len(payload)))
    return t

layout=r'''
// Experimental selective decoded layout. Mask zero retains the canonical
// 132-byte ABI. Other masks pack only present fields in canonical order;
// location numbers and shader bodies do not change. PN row is always kept.
inline constexpr std::uint32_t kVertexLocationCount = 14u;
inline constexpr std::uint32_t kVertexAllLocations = (1u << kVertexLocationCount) - 1u;
inline constexpr std::uint32_t kVertexLocationOffsets[kVertexLocationCount] =
    {0, 12, 16, 32, 48, 56, 64, 72, 88, 100, 108, 120, 80, 104};
inline constexpr std::uint32_t kVertexLocationBytes[kVertexLocationCount] =
    {12, 4, 16, 16, 8, 8, 8, 8, 12, 4, 12, 12, 8, 4};
inline constexpr std::uint32_t kVertexCanonicalLocations[kVertexLocationCount] =
    {0, 1, 2, 3, 4, 5, 6, 7, 12, 8, 9, 13, 10, 11};
struct DecodedVertexLayout {
  std::uint32_t offsets[kVertexLocationCount]{};
  std::uint32_t stride = kVertexStrideBytes;
};
inline DecodedVertexLayout decoded_vertex_layout(std::uint32_t mask) {
  DecodedVertexLayout layout;
  if (mask == 0u) {
    for (std::uint32_t location = 0; location < kVertexLocationCount; ++location)
      layout.offsets[location] = kVertexLocationOffsets[location];
    return layout;
  }
  layout.stride = 0;
  for (std::uint32_t location : kVertexCanonicalLocations) {
    layout.offsets[location] = UINT32_MAX;
    if ((mask & (1u << location)) != 0u) {
      layout.offsets[location] = layout.stride;
      layout.stride += kVertexLocationBytes[location];
    }
  }
  return layout;
}
inline bool vertex_layout_valid(std::uint32_t mask) {
  return mask == 0u || ((mask & 3u) == 3u && (mask & ~kVertexAllLocations) == 0u);
}
inline std::uint32_t vertex_stride_bytes(std::uint32_t mask) {
  if (mask == 0u) return kVertexStrideBytes;
  std::uint32_t bytes = 0;
  for (std::uint32_t location = 0; location < kVertexLocationCount; ++location)
    if ((mask & (1u << location)) != 0u) bytes += kVertexLocationBytes[location];
  return bytes;
}
inline bool vertex_location_present(std::uint32_t mask, std::uint32_t location) {
  return location < kVertexLocationCount && (mask == 0u || (mask & (1u << location)) != 0u);
}
inline std::uint32_t vertex_location_offset(std::uint32_t mask, std::uint32_t location) {
  if (location >= kVertexLocationCount) return UINT32_MAX;
  if (mask == 0u) return kVertexLocationOffsets[location];
  if (!vertex_location_present(mask, location)) return UINT32_MAX;
  std::uint32_t offset = 0;
  for (std::uint32_t at : kVertexCanonicalLocations) {
    if (at == location) return offset;
    if ((mask & (1u << at)) != 0u) offset += kVertexLocationBytes[at];
  }
  return UINT32_MAX;
}
inline std::uint32_t selective_vertex_mask(const ShaderKey& key) {
  std::uint32_t mask = 3u;
  if (key.has_color0) mask |= 1u << 2u;
  if (key.has_color1) mask |= 1u << 3u;
  for (std::uint32_t uv = 0; uv < kMaxTexGens; ++uv)
    if ((key.uv_mask & (1u << uv)) != 0u) mask |= 1u << (uv == 4u ? 12u : 4u + uv);
  // Retain all present N/B/T, including shader-unused attributes: the last
  // vertex advances the cross-draw caches even on an unlit draw.
  if (key.has_vertex_normal) mask |= 1u << 8u;
  if (key.has_tex_mtx_idx) mask |= 1u << 9u;
  if (key.has_vertex_binormal) mask |= 1u << 10u;
  if (key.has_vertex_tangent) mask |= 1u << 11u;
  // has_tex_mtx_idx covers all eight physical TEXMTXIDX attributes, while
  // tex_mtx_idx_mask describes only the five shader-supported texgens.
  // Preserve both canonical words even for shader-unused TEXMTXIDX5..7.
  if (key.has_tex_mtx_idx) mask |= 1u << 13u;
  return mask == kVertexAllLocations ? 0u : mask;
}
inline void full_vertex_defaults(float* vertex) {
  std::memset(vertex, 0, kVertexStrideBytes);
  for (std::uint32_t i = 4u; i < 12u; ++i) vertex[i] = 1.f;
}
'''
reconstruction=r'''
// Reconstruct every canonical word, including integer matrix-index bit
// patterns and defaults. This is the exact legacy-filter/ubershader fallback.
inline bool vertex_storage_valid(const DrawPlan& plan) {
  return vertex_layout_valid(plan.vertex_layout_mask) &&
         plan.vertex_floats == vertex_stride_bytes(plan.vertex_layout_mask) / 4u &&
         plan.vertices.size() % plan.vertex_floats == 0u &&
         plan.vertices.size() / plan.vertex_floats == plan.vertex_count;
}
inline bool copy_full_vertex(const DrawPlan& plan, std::uint32_t vertex, float* out) {
  if (!vertex_storage_valid(plan) || vertex >= plan.vertex_count || out == nullptr) return false;
  const float* in = plan.vertices.data() + static_cast<std::size_t>(vertex) * plan.vertex_floats;
  if (plan.vertex_layout_mask == 0u) { std::memcpy(out, in, kVertexStrideBytes); return true; }
  full_vertex_defaults(out);
  for (std::uint32_t location = 0; location < kVertexLocationCount; ++location)
    if (vertex_location_present(plan.vertex_layout_mask, location))
      std::memcpy(reinterpret_cast<std::uint8_t*>(out) + kVertexLocationOffsets[location],
                  reinterpret_cast<const std::uint8_t*>(in) + vertex_location_offset(plan.vertex_layout_mask, location),
                  kVertexLocationBytes[location]);
  return true;
}
inline bool materialize_full_vertices(DrawPlan& plan) {
  if (!vertex_storage_valid(plan)) return false;
  if (plan.vertex_layout_mask == 0u) return true;
  std::vector<float> full(static_cast<std::size_t>(plan.vertex_count) * kVertexFloats);
  for (std::uint32_t v = 0; v < plan.vertex_count; ++v)
    if (!copy_full_vertex(plan, v, full.data() + static_cast<std::size_t>(v) * kVertexFloats)) return false;
  plan.vertices.swap(full); plan.vertex_layout_mask = 0u; plan.vertex_floats = kVertexFloats;
  return true;
}
'''
def shader(s):
    s=sub(s,'#include <cstdint>','#include <cstdint>\n#include <cstring>')
    s=sub(s,'// --- Draw plan ---------------------------------------------------------------',layout+'\n// --- Draw plan ---------------------------------------------------------------')
    s=sub(s,'  std::uint32_t vertex_count = 0;','  std::uint32_t vertex_count = 0;\n  std::uint32_t vertex_layout_mask = 0; // zero: canonical full layout\n  std::uint32_t vertex_floats = kVertexFloats;')
    s=sub(s,'  std::vector<float> vertices; // kVertexFloats per vertex','  std::vector<float> vertices; // vertex_floats per vertex')
    return sub(s,'// --- EFB copy-to-texture (S16) ------------------------------------------------',reconstruction+'\n// --- EFB copy-to-texture (S16) ------------------------------------------------')
emit('GXRuntime/graphics/gxcore/include/gxruntime/gxcore/shader.hpp',shader)

statdecl='''
// Recording-thread-only diagnostic counters. Exact STATS=1 enables writes;
// report only after the FIFO worker has stopped. Not GPU hardware counters.
void note_selective_vertex_submission(std::uint32_t mask, std::uint32_t count);
void note_selective_vertex_fallback(bool legacy_filter);
void report_selective_vertex_stats();
'''
def coreh(s):
    s=sub(s,'// Loud-counter taxonomy',statdecl+'\n// Loud-counter taxonomy')
    s=sub(s,'void set_plan_filter(PlanFilter filter, void* user) {\n    plan_filter_ = filter; plan_filter_user_ = user;','void set_plan_filter(PlanFilter filter, void* user, bool understands_selective_vertices = false) {\n    plan_filter_ = filter; plan_filter_user_ = user;\n    plan_filter_selective_ = understands_selective_vertices;')
    return sub(s,'  PlanFilter plan_filter_ = nullptr;','  PlanFilter plan_filter_ = nullptr;\n  bool plan_filter_selective_ = false;')
emit('GXRuntime/graphics/gxcore/include/gxruntime/gxcore/gxcore.hpp',coreh)

stats=r'''
namespace {
struct SelectiveVertexStats {
  unsigned long long decoded_draws = 0, decoded_vertices = 0;
  unsigned long long decoded_full_bytes = 0, decoded_bytes = 0;
  unsigned long long submitted_draws = 0, submitted_vertices = 0;
  unsigned long long submitted_full_bytes = 0, submitted_bytes = 0;
  unsigned long long filter_fallbacks = 0, pipeline_fallbacks = 0;
  unsigned long long strides[kVertexFloats + 1u]{};
} g_selective_stats;
bool selective_stats_enabled() {
  static const bool enabled = [] {
    const char* env = std::getenv("DOL_GXCORE_SELECTIVE_STATS");
    return env != nullptr && env[0] == '1' && env[1] == '\0';
  }(); return enabled;
}
bool selective_vertices_enabled() {
  static const bool enabled = [] {
    const char* env = std::getenv("DOL_GXCORE_SELECTIVE_VERTICES");
    return env != nullptr && env[0] == '1' && env[1] == '\0';
  }(); return enabled;
}
}
void note_selective_vertex_submission(std::uint32_t mask, std::uint32_t count) {
  if (!selective_stats_enabled()) return;
  auto& s = g_selective_stats; ++s.submitted_draws; s.submitted_vertices += count;
  s.submitted_full_bytes += static_cast<unsigned long long>(count) * kVertexStrideBytes;
  s.submitted_bytes += static_cast<unsigned long long>(count) * vertex_stride_bytes(mask);
  ++s.strides[vertex_stride_bytes(mask) / 4u];
}
void note_selective_vertex_fallback(bool legacy_filter) {
  if (!selective_stats_enabled()) return;
  if (legacy_filter) ++g_selective_stats.filter_fallbacks;
  else ++g_selective_stats.pipeline_fallbacks;
}
void report_selective_vertex_stats() {
  if (!selective_stats_enabled()) return;
  const auto& s = g_selective_stats;
  std::fprintf(stderr, "[gx-selective] decoded_draws=%llu decoded_vertices=%llu decoded_full_bytes=%llu decoded_bytes=%llu submitted_draws=%llu submitted_vertices=%llu submitted_full_bytes=%llu submitted_bytes=%llu filter_fallbacks=%llu pipeline_fallbacks=%llu\n",
      s.decoded_draws,s.decoded_vertices,s.decoded_full_bytes,s.decoded_bytes,
      s.submitted_draws,s.submitted_vertices,s.submitted_full_bytes,s.submitted_bytes,
      s.filter_fallbacks,s.pipeline_fallbacks);
  std::fprintf(stderr, "[gx-selective] strides");
  for (std::uint32_t i = 0; i <= kVertexFloats; ++i)
    if (s.strides[i] != 0) std::fprintf(stderr, " %u=%llu", i * 4u, s.strides[i]);
  std::fprintf(stderr, "\n");
}
'''
def core(s):
    s=sub(s,'#include <cstring>','#include <cstring>\n#include <cstdlib>\n#include <cstdio>')
    s=sub(s,'namespace gxruntime::gxcore {','namespace gxruntime::gxcore {\n'+stats)
    s=sub(s,'  plan.vertices.assign(\n      static_cast<std::size_t>(draw.vertex_count) * kVertexFloats, 0.f);','''  plan.vertex_layout_mask = selective_vertices_enabled() ? selective_vertex_mask(key) : 0u;
  // A small format-keyed recipe cache avoids re-scanning fields for each
  // tiny particle draw. Reuse is geometry-independent; no guest bytes live
  // here. Every slot owns its offsets, with no borrowed lifetimes.
  struct LayoutRecipe { std::uint32_t mask = UINT32_MAX; DecodedVertexLayout layout{}; };
  static thread_local LayoutRecipe recipes[64];
  auto& recipe = recipes[((plan.vertex_layout_mask * 2654435761u) >> 26u) & 63u];
  if (recipe.mask != plan.vertex_layout_mask) {
    recipe.mask = plan.vertex_layout_mask;
    recipe.layout = decoded_vertex_layout(plan.vertex_layout_mask);
    for (auto& offset : recipe.layout.offsets) offset /= 4u;
  }
  plan.vertex_floats = recipe.layout.stride / 4u;
  const auto& field_offsets = recipe.layout.offsets;
  plan.vertices.assign(
      static_cast<std::size_t>(draw.vertex_count) * plan.vertex_floats, 0.f);''')
    s=sub(s,'static_cast<std::size_t>(v) * kVertexFloats;','static_cast<std::size_t>(v) * plan.vertex_floats;')
    s=sub(s,'    out_vertex[4] = out_vertex[5] = out_vertex[6] = out_vertex[7] = 1.f;\n    out_vertex[8] = out_vertex[9] = out_vertex[10] = out_vertex[11] = 1.f;','''    for (std::uint32_t location : {2u, 3u})
      if (vertex_location_present(plan.vertex_layout_mask, location))
        for (std::uint32_t c = 0; c < 4u; ++c) out_vertex[field_offsets[location] + c] = 1.f;''')
    s=sub(s,'out_vertex + (entry.out_slot == 0u ? 4u : 8u)','out_vertex + field_offsets[entry.out_slot == 0u ? 2u : 3u]')
    s=sub(s,'out_vertex + 12u + 2u * entry.out_slot','out_vertex + field_offsets[entry.out_slot == 4u ? 12u : 4u + entry.out_slot]')
    s=sub(s,'auto decode3 = [&](const std::uint8_t* src, std::uint32_t dst_off) {\n          float* dst = out_vertex + dst_off / 4u;','auto decode3 = [&](const std::uint8_t* src, std::uint32_t location) {\n          float* dst = out_vertex + field_offsets[location];')
    s=s.replace('decode3(element, kVertexNormalOffset)','decode3(element, 8u)').replace('decode3(element + 3u * scalar, kVertexBinormalOffset)','decode3(element + 3u * scalar, 10u)').replace('decode3(element + 6u * scalar, kVertexTangentOffset)','decode3(element + 6u * scalar, 11u)').replace('decode3(element, kVertexBinormalOffset)','decode3(element, 10u)').replace('decode3(element, kVertexTangentOffset)','decode3(element, 11u)')
    s=sub(s,'    std::memcpy(out_vertex + kVertexTexMtxIdxOffset / 4u, &texmtxidx_packed,\n                sizeof texmtxidx_packed);\n    std::memcpy(out_vertex + kVertexTexMtxIdxHiOffset / 4u,\n                &texmtxidx_packed_hi, sizeof texmtxidx_packed_hi);','''    if (vertex_location_present(plan.vertex_layout_mask, 9u))
      std::memcpy(out_vertex + field_offsets[9], &texmtxidx_packed, sizeof texmtxidx_packed);
    if (vertex_location_present(plan.vertex_layout_mask, 13u))
      std::memcpy(out_vertex + field_offsets[13], &texmtxidx_packed_hi, sizeof texmtxidx_packed_hi);''')
    s=sub(s,'static_cast<std::size_t>(plan.vertex_count - 1u) * kVertexFloats;','static_cast<std::size_t>(plan.vertex_count - 1u) * plan.vertex_floats;')
    s=sub(s,'last[kVertexNormalOffset / 4u + k]','last[field_offsets[8] + k]')
    s=sub(s,'last[kVertexBinormalOffset / 4u + k]','last[field_offsets[10] + k]')
    s=sub(s,'last[kVertexTangentOffset / 4u + k]','last[field_offsets[11] + k]')
    s=sub(s,'  plan.ok = true;\n  ++counters.draws_planned;','''  if (selective_stats_enabled()) {
    auto& s = g_selective_stats; ++s.decoded_draws; s.decoded_vertices += plan.vertex_count;
    s.decoded_full_bytes += static_cast<unsigned long long>(plan.vertex_count) * kVertexStrideBytes;
    s.decoded_bytes += static_cast<unsigned long long>(plan.vertex_count) * plan.vertex_floats * 4u;
  }
  plan.ok = true;
  ++counters.draws_planned;''')
    return sub(s,'  if (self->plan_filter_ != nullptr &&\n      !self->plan_filter_','''  if (self->plan_filter_ != nullptr && !self->plan_filter_selective_ && plan.vertex_layout_mask != 0u) {
    note_selective_vertex_fallback(true);
    if (!materialize_full_vertices(plan)) return;
  }
  if (self->plan_filter_ != nullptr &&
      !self->plan_filter_''')
emit('GXRuntime/graphics/gxcore/src/gxcore.cpp',core)

def drawh(s):
    s=sub(s,'  Range vertRange;','  Range vertRange;\n  uint32_t vertexLayoutMask = 0;\n  uint32_t vertexStride = gxruntime::gxcore::kVertexStrideBytes;')
    s=sub(s,'constexpr uint32_t GXCorePipelineConfigVersion = 13;','// v15: selective decoded vertex streams, distinct from the inactive raw v14.\nconstexpr uint32_t GXCorePipelineConfigVersion = 15;')
    return sub(s,'  uint32_t depthOnly = 0;','  uint32_t depthOnly = 0;\n  uint32_t vertexLayoutMask = 0; // zero: canonical full stream')
emit('GXRuntime/graphics/aurora/lib/gfx/gxcore_draw.hpp',drawh)

def draw(s):
    s=sub(s,'#include "gxcore_draw.hpp"','#include "gxcore_draw.hpp"\n#include <gxruntime/gxcore/gxcore.hpp>')
    start=s.index('  std::vector<wgpu::VertexAttribute> attributes{',s.index('wgpu::RenderPipeline create_pipeline'))
    end=s.index('  // Item-5 texgen inputs',start)
    s=s[:start]+'''  const uint32_t vertexMask = config.vertexLayoutMask;
  CHECK(gxc::vertex_layout_valid(vertexMask) && (!uber || vertexMask == 0u), "Invalid selective vertex layout");
  std::vector<wgpu::VertexAttribute> attributes;
  std::vector<wgpu::VertexAttribute> defaultAttributes;
  const auto addAttribute = [&](wgpu::VertexFormat format, uint32_t location) {
    auto& target = gxc::vertex_location_present(vertexMask, location) ? attributes : defaultAttributes;
    target.push_back(wgpu::VertexAttribute{
        .format = format,
        .offset = gxc::vertex_location_present(vertexMask, location)
                      ? gxc::vertex_location_offset(vertexMask, location)
                      : gxc::kVertexLocationOffsets[location],
        .shaderLocation = location,
    });
  };
  addAttribute(wgpu::VertexFormat::Float32x3, 0);
  addAttribute(wgpu::VertexFormat::Uint32, 1);
  addAttribute(wgpu::VertexFormat::Float32x4, 2);
  addAttribute(wgpu::VertexFormat::Float32x4, 3);
  for (uint32_t location = 4; location < 8; ++location)
    addAttribute(wgpu::VertexFormat::Float32x2, location);
'''+s[end:]
    s=re.sub(r'    attributes.push_back\(wgpu::VertexAttribute\{\n        \.format = (wgpu::VertexFormat::\w+),\n        \.offset = [^\n]+,\n        \.shaderLocation = (\d+),\n    \}\);',r'    addAttribute(\1, \2);',s)
    s=sub(s,'  const wgpu::VertexBufferLayout vertexLayout{\n      .arrayStride = gxc::kVertexStrideBytes,\n      .stepMode = wgpu::VertexStepMode::Vertex,\n      .attributeCount = attributes.size(),\n      .attributes = attributes.data(),\n  };','''  const std::array<wgpu::VertexBufferLayout, 2> vertexLayouts{{
      {.arrayStride = gxc::vertex_stride_bytes(vertexMask),
       .stepMode = wgpu::VertexStepMode::Vertex,
       .attributeCount = attributes.size(), .attributes = attributes.data()},
      {.arrayStride = gxc::kVertexStrideBytes,
       .stepMode = wgpu::VertexStepMode::Instance,
       .attributeCount = defaultAttributes.size(), .attributes = defaultAttributes.data()},
  }};''')
    s=sub(s,'              .bufferCount = 1,\n              .buffers = &vertexLayout,','              .bufferCount = defaultAttributes.empty() ? 1u : 2u,\n              .buffers = vertexLayouts.data(),')
    s=sub(s,'  bool indexBound = false;','  bool indexBound = false;\n  bool defaultsBound = false;')
    at=s.index('// What a draw, or one of a batch')
    s=s[:at]+'''// One canonical element supplies shader-declared absent color/UV inputs.
// All values match the full decoder. Instance zero is used by every draw.
static const wgpu::Buffer& selective_vertex_defaults() {
  static wgpu::Buffer buffer;
  static wgpu::Device owner;
  if (!buffer || owner.Get() != g_device.Get()) {
    buffer = nullptr; owner = g_device;
    const wgpu::BufferDescriptor descriptor{
        .label = "GXCore Selective Vertex Defaults", .usage = wgpu::BufferUsage::Vertex,
        .size = gxc::kVertexStrideBytes, .mappedAtCreation = true};
    buffer = g_device.CreateBuffer(&descriptor);
    gxc::full_vertex_defaults(static_cast<float*>(buffer.GetMappedRange(0, gxc::kVertexStrideBytes)));
    buffer.Unmap();
  }
  return buffer;
}

'''+s[at:]
    s=sub(s,'  const uint32_t firstIndex = data.idxRange.offset / sizeof(uint16_t);','''  const uint32_t firstIndex = data.idxRange.offset / sizeof(uint16_t);
  // Locations 2..7 are the always-declared colors and UV0..3. Every optional
  // declared input is retained by the format policy (including all N/B/T and
  // both matrix-index words), so only these six locations can need defaults.
  if (data.vertexLayoutMask != 0u && (data.vertexLayoutMask & 0xFCu) != 0xFCu && !g_pass.defaultsBound) {
    pass.SetVertexBuffer(1, selective_vertex_defaults()); g_pass.defaultsBound = true;
  }''')
    # Stride follows each recorded draw through interpolation and batching.
    for a,b in [('first * gxc::kVertexStrideBytes','first * data.vertexStride'),('uint64_t{part.firstVertex} * gxc::kVertexStrideBytes','uint64_t{part.firstVertex} * data.vertexStride'),('data.vertRange.offset % gxc::kVertexStrideBytes','data.vertRange.offset % data.vertexStride'),('data.vertRange.offset / gxc::kVertexStrideBytes','data.vertRange.offset / data.vertexStride')]:s=sub(s,a,b)
    s=sub(s,'  uint64_t frameId;\n  size_t slot;','  uint32_t vertexFloats = gxc::kVertexFloats;\n  uint64_t frameId;\n  size_t slot;')
    s=sub(s,'Range push_blended_vertices(size_t slot, const std::vector<float>& vertices, const float* positions,','Range push_blended_vertices(size_t slot, const std::vector<float>& vertices, uint32_t vertexFloats, const float* positions,')
    s=sub(s,'  const size_t count = scratch.size() / gxc::kVertexFloats;','  if (vertexFloats < 4u || vertexFloats > gxc::kVertexFloats || scratch.size() % vertexFloats != 0u) return {};\n  const size_t count = scratch.size() / vertexFloats;')
    s=sub(s,'scratch.data() + i * gxc::kVertexFloats +','scratch.data() + i * vertexFloats +')
    s=sub(s,'push_blended_vertices(job.slot, job.vertices, positions,','push_blended_vertices(job.slot, job.vertices, job.vertexFloats, positions,')
    s=sub(s,'  job.frameId = frameId;','  job.vertexFloats = plan.vertex_floats;\n  job.frameId = frameId;')
    s=sub(s,'push_blended_vertices(recording_frame_slot(), plan.vertices, tracedPositions,','push_blended_vertices(recording_frame_slot(), plan.vertices, plan.vertex_floats, tracedPositions,')
    s=sub(s,'bool submit_draw_plan(const gxc::DrawPlan& plan) {\n  if (!plan.ok || plan.vertex_count == 0 || plan.indices.empty()) {','''bool submit_draw_plan(const gxc::DrawPlan& sourcePlan) {
  if (!sourcePlan.ok || sourcePlan.vertex_count == 0 || sourcePlan.indices.empty() || !gxc::vertex_storage_valid(sourcePlan)) {''')
    s=sub(s,'  dump_draw(plan);','''  // Request the selective pipeline first. A cold/forced uber draw uses
  // canonical geometry AND canonical pipeline keys; recorded data never has
  // a later pipeline switch which changes the meaning of its vertex bytes.
  static gxc::DrawPlan fullPlan;
  const gxc::DrawPlan* selected = &sourcePlan;
  PipelineRef selectedColorPipeline = 0;
  PipelineRef selectedDepthPipeline = 0;
  if (sourcePlan.vertex_layout_mask != 0u) {
    const PipelineConfig desired{.version = GXCorePipelineConfigVersion,
        .key = sourcePlan.pipeline, .msaaSamples = get_sample_count(),
        .vertexLayoutMask = sourcePlan.vertex_layout_mask};
    const PipelineRef color = pipeline_ref(desired);
    bool ready = pipeline_ready(color);
    PipelineRef desiredDepthPipeline = 0;
    if (needs_early_depth_emulation(sourcePlan.pipeline)) {
      PipelineConfig depth = desired; depth.depthOnly = 1u;
      desiredDepthPipeline = pipeline_ref(depth);
      ready = pipeline_ready(desiredDepthPipeline) && ready;
    }
    if (ubershader_mode() == 2 || (ubershader_mode() != 0 && !ready)) {
      fullPlan = sourcePlan;
      if (!gxc::materialize_full_vertices(fullPlan)) return false;
      gxc::note_selective_vertex_fallback(false); selected = &fullPlan;
    } else {
      selectedColorPipeline = color;
      selectedDepthPipeline = desiredDepthPipeline;
    }
  }
  const gxc::DrawPlan& plan = *selected;
  const uint32_t vertexStride = plan.vertex_floats * 4u;
  dump_draw(plan);''')
    # All submission-side capacity/alignment operations use the plan's stride.
    s=s.replace('vertBytes + gxc::kVertexStrideBytes','vertBytes + vertexStride')
    s=sub(s,'      .msaaSamples = get_sample_count(),\n  };','      .msaaSamples = get_sample_count(),\n      .vertexLayoutMask = plan.vertex_layout_mask,\n  };')
    s=sub(s,'  const PipelineRef pipeline = pipeline_ref(colorConfig);','  const PipelineRef pipeline = selectedColorPipeline != 0 ? selectedColorPipeline : pipeline_ref(colorConfig);')
    s=sub(s,'    depthPipeline = pipeline_ref(depthConfig);','    depthPipeline = selectedDepthPipeline != 0 ? selectedDepthPipeline : pipeline_ref(depthConfig);')
    s=sub(s,'      uberConfig.depthOnly = 2u;','      uberConfig.depthOnly = 2u;\n      uberConfig.vertexLayoutMask = 0u;')
    s=sub(s,'next_vertex_offset(gxc::kVertexStrideBytes)','next_vertex_offset(vertexStride)')
    s=sub(s,'last->depthPipeline == depthPipeline &&','last->depthPipeline == depthPipeline &&\n        last->vertexLayoutMask == plan.vertex_layout_mask && last->vertexStride == vertexStride &&')
    s=sub(s,'last->vertRange.offset % gxc::kVertexStrideBytes','last->vertRange.offset % vertexStride')
    s=sub(s,'(vertexOffset - last->vertRange.offset) / gxc::kVertexStrideBytes','(vertexOffset - last->vertRange.offset) / vertexStride')
    s=sub(s,'      vertBytes, gxc::kVertexStrideBytes);','      vertBytes, vertexStride);\n  gxc::note_selective_vertex_submission(plan.vertex_layout_mask, plan.vertex_count);')
    # No diagnostic function call in ordinary timing runs.
    s=sub(s,'  gxc::note_selective_vertex_submission(plan.vertex_layout_mask, plan.vertex_count);','''  static const bool selectiveStats = [] {
    const char* env = std::getenv("DOL_GXCORE_SELECTIVE_STATS");
    return env != nullptr && env[0] == '1' && env[1] == '\\0';
  }();
  if (selectiveStats) gxc::note_selective_vertex_submission(plan.vertex_layout_mask, plan.vertex_count);''')
    return sub(s,'      .vertRange = vertRange,','      .vertRange = vertRange,\n      .vertexLayoutMask = plan.vertex_layout_mask,\n      .vertexStride = vertexStride,')
emit('GXRuntime/graphics/aurora/lib/gfx/gxcore_draw.cpp',draw)

def interp(s):
    s=sub(s,'  const size_t decoded = plan.vertices.size() / gxc::kVertexFloats;','  if (!gxc::vertex_storage_valid(plan)) return;\n  const size_t decoded = plan.vertices.size() / plan.vertex_floats;')
    s=s.replace('i * gxc::kVertexFloats + gxc::kVertexPosOffset','i * plan.vertex_floats + gxc::kVertexPosOffset').replace('static_cast<size_t>(count) * gxc::kVertexFloats','static_cast<size_t>(count) * plan.vertex_floats').replace('static_cast<size_t>(picks[i]) * gxc::kVertexFloats','static_cast<size_t>(picks[i]) * plan.vertex_floats')
    return s
emit('GXRuntime/graphics/aurora/lib/gfx/frame_interp.cpp',interp)
def cache(s):
    return sub(s,'  uber.depthOnly = 2u;','  uber.depthOnly = 2u;\n  uber.vertexLayoutMask = 0u; // canonical ubershader always reads full vertices')
emit('GXRuntime/graphics/aurora/lib/gfx/pipeline_cache.cpp',cache)
def graphics(s):
    s=sub(s,'            const unsigned stride = gxruntime::gxcore::kVertexFloats;','            const unsigned stride = plan.vertex_floats;')
    return sub(s,'                const float* p = plan.vertices.data() + v * stride;','                float full[gxruntime::gxcore::kVertexFloats];\n                if (!gxruntime::gxcore::copy_full_vertex(plan, v, full)) break;\n                const float* p = full;')
emit('GXRuntime/backends/aurora/aurora_graphics.cpp',graphics)
def backend(s):
    s=sub(s,'void* g_host_plan_filter_user=nullptr;','void* g_host_plan_filter_user=nullptr;\nbool g_host_plan_filter_selective=false;')
    s=sub(s,'gx_aurora::g_host_plan_filter=filter;gx_aurora::g_host_plan_filter_user=user;return true;','gx_aurora::g_host_plan_filter=filter;gx_aurora::g_host_plan_filter_user=user;\n    gx_aurora::g_host_plan_filter_selective=false;return true;')
    s=sub(s,'bool dol_aurora_gxcore_plan_filter_available(void) {','''bool dol_aurora_set_gxcore_plan_filter_selective_vertices(bool understands) {
    if(gx_aurora::g_initialized)return false;
    gx_aurora::g_host_plan_filter_selective=understands;return true;
}
bool dol_aurora_gxcore_plan_filter_available(void) {''')
    s=sub(s,'gx_aurora::host_plan_filter:nullptr,nullptr);','gx_aurora::host_plan_filter:nullptr,nullptr,gx_aurora::g_host_plan_filter_selective);')
    return sub(s,'        const auto& gaps = gx_aurora::g_core_sink.counters();','        gxruntime::gxcore::report_selective_vertex_stats();\n        const auto& gaps = gx_aurora::g_core_sink.counters();')
emit('GXRuntime/backends/aurora/aurora_backend.cpp',backend)
def public(s):
    return sub(s,'bool dol_aurora_gxcore_plan_filter_available(void);','''bool dol_aurora_gxcore_plan_filter_available(void);
/* Pre-init certification: this filter accepts DrawPlan::vertex_floats and
 * vertex_layout_mask, or never inspects/modifies vertices. Registration of a
 * new filter clears certification; uncertified filters receive canonical full
 * vertices. Only an audited host callback should opt in. */
bool dol_aurora_set_gxcore_plan_filter_selective_vertices(bool understands);''')
emit('GXRuntime/include/gxruntime/aurora_backend.h',public)
def hud(s):
    return sub(s,'    return dol_aurora_set_gxcore_plan_filter(bw_hud_renderer_filter,nullptr);','''    if(!dol_aurora_set_gxcore_plan_filter(bw_hud_renderer_filter,nullptr))return false;
    /* transform_plan changes uniforms/scissor/tint only; it never accesses
       decoded vertices, their offsets or their storage lifetime. */
    return dol_aurora_set_gxcore_plan_filter_selective_vertices(true);''')
emit('runtime/host/src/hud_renderer.cpp',hud,True)
(H/'source-receipt.json').write_text(json.dumps(dict(status='PRIVATE_SOURCE_OVERLAY_NOT_COMPILED',files=records),indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(status='PRIVATE_SOURCE_OVERLAY_NOT_COMPILED',files=len(records))))
