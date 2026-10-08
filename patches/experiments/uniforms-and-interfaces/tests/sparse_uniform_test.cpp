// SPDX-License-Identifier: GPL-3.0-or-later
// Actual sparse staging and WGSL generators; authored CPU staging, no GPU.
#include "GXRuntime/graphics/aurora/lib/gfx/vertex_uniform_cache.hpp"
#include "staging_budget.hpp"
#include "control/include/gxruntime/gxcore/shader.hpp"
#include "runtime_env_exact.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <regex>
#include <string>
#include <vector>

namespace gx = gxruntime::gxcore;
namespace old = gxruntime::uniform_control;
namespace ag = aurora::gfx;
static uint64_t checks = 0, failures = 0;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; if (failures < 20) std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); } } while (0)

struct Arena {
  std::vector<uint8_t> bytes = std::vector<uint8_t>(1u << 20, 0xA5);
  size_t used = 0, copied = 0, aligned = 0, pushes = 0, limit = bytes.size(), alignment = 256;
  ag::VertexPartRange push(const uint8_t* data, size_t n) {
    size_t at = ag::staging_project(used, 0, alignment);
    if (n > limit || at > limit - n) return {};
    std::memcpy(bytes.data() + at, data, n);
    aligned += at + n - used; used = at + n; copied += n; ++pushes;
    return {uint32_t(at), uint32_t(n)};
  }
  const uint8_t* read(ag::VertexPartRange r) const { CHECK(r.offset + r.size <= used); return bytes.data() + r.offset; }
};

template<bool CompareStaged>
bool stage(ag::VertexPartCaches& caches, Arena& arena, uint64_t frame,
           const gx::VertexShaderConstants& constants, gx::VertexUniformUse use,
           bool repeats, bool dedup, bool sparse, ag::VertexUniformRanges& ranges) {
  return ag::stage_vertex_uniforms<CompareStaged>(caches, frame, constants, use, repeats, dedup, sparse,
      [&](const uint8_t* p, size_t n) { return arena.push(p, n); },
      [&](ag::VertexPartRange r) { return arena.read(r); }, ranges);
}

void values(gx::VertexShaderConstants& c, uint32_t salt) {
  auto* raw = reinterpret_cast<uint8_t*>(&c);
  for (size_t i = 0; i < sizeof(c); ++i) raw[i] = uint8_t((i * 37u + salt * 19u) ^ (i >> 3));
}

void verify(const Arena& arena, const gx::VertexShaderConstants& c,
            gx::VertexUniformUse use, ag::VertexUniformRanges r) {
  CHECK(r.size != 0);
  const size_t offsets[3] = {r.block, r.matrices, r.lights};
  const size_t starts[3] = {0, gx::kVertexMatrixOffset, gx::kVertexLightOffset};
  const size_t reads[3] = {use.block, use.matrices, use.lights};
  const auto* raw = reinterpret_cast<const uint8_t*>(&c);
  for (unsigned p = 0; p < 3; ++p) {
    if (reads[p] == 0) continue;
    CHECK(offsets[p] % arena.alignment == 0);
    CHECK(offsets[p] + reads[p] <= arena.used);
    for (size_t i = 0; i < reads[p]; ++i) CHECK(arena.bytes[offsets[p] + i] == raw[starts[p] + i]);
  }
}

template<bool CompareStaged>
void cache_cases() {
  gx::VertexShaderConstants c{}; values(c, 1);
  gx::ShaderKey plain{};
  const auto small = gx::vertex_uniform_use(plain,true);
  const gx::VertexUniformUse full{gx::kVertexBlockBytes,gx::kVertexMatrixBytes,gx::kVertexLightBytes};
  ag::VertexPartCaches caches{}; Arena arena; ag::VertexUniformRanges r{}, first{};
  CHECK(stage<CompareStaged>(caches,arena,1,c,small,false,true,true,r)); first = r; verify(arena,c,small,r);
  size_t saved = arena.used;
  CHECK(stage<CompareStaged>(caches,arena,1,c,small,true,true,true,r)); CHECK(r == first); CHECK(arena.used == saved);
  // A larger shader shape must not inherit a previous smaller-prefix match.
  CHECK(stage<CompareStaged>(caches,arena,1,c,full,true,true,true,r)); verify(arena,c,full,r); CHECK(arena.used > saved);
  const auto all = r; saved = arena.used;
  CHECK(stage<CompareStaged>(caches,arena,1,c,small,true,true,true,r)); CHECK(arena.used == saved);
  // Mutate an omitted part, then restore the larger shape: no stale repeat.
  c.lights[7].color[3] ^= 127;
  CHECK(stage<CompareStaged>(caches,arena,1,c,small,false,true,true,r)); CHECK(arena.used == saved);
  CHECK(stage<CompareStaged>(caches,arena,1,c,full,true,true,true,r)); CHECK(r.lights != all.lights); verify(arena,c,full,r);
  // Mutate every byte which any shader can read, one at a time.
  auto* raw = reinterpret_cast<uint8_t*>(&c);
  for (size_t i = 0; i < sizeof(c); ++i) {
    raw[i] ^= 1; CHECK(stage<CompareStaged>(caches,arena,1,c,full,false,true,true,r)); verify(arena,c,full,r);
    if (arena.used > arena.bytes.size() / 2) { arena.used = 0; caches = {}; }
  }
  saved = arena.pushes;
  CHECK(stage<CompareStaged>(caches,arena,2,c,full,true,true,true,r)); CHECK(arena.pushes == saved + 3); verify(arena,c,full,r);
  saved = arena.pushes;
  CHECK(stage<CompareStaged>(caches,arena,2,c,full,true,false,true,r)); CHECK(arena.pushes == saved + 3);
  // Overflow after part zero: output remains unusable; subsequent retry is exact.
  Arena shortArena; shortArena.limit = gx::kVertexBlockBytes;
  ag::VertexPartCaches shortCaches{}; r = first;
  CHECK(!stage<CompareStaged>(shortCaches,shortArena,7,c,full,false,true,true,r)); CHECK(r.size == 0);
  shortArena.limit = shortArena.bytes.size();
  CHECK(stage<CompareStaged>(shortCaches,shortArena,7,c,full,false,true,true,r)); verify(shortArena,c,full,r);
  const size_t before = shortArena.pushes;
  CHECK(!stage<CompareStaged>(shortCaches,shortArena,0,c,full,false,true,true,r)); CHECK(shortArena.pushes == before);
  CHECK(!stage<CompareStaged>(shortCaches,shortArena,7,c,{gx::kVertexBlockBytes+1,0,0},false,true,true,r));
  // Successful A -> partially staged B -> caller repeats successful A.
  // Failed B must never lend its earlier prefix to A's repeat shortcut.
  gx::VertexShaderConstants a{}, b{}; values(a,11); values(b,29);
  Arena retryArena; ag::VertexPartCaches retryCaches{};
  CHECK(stage<CompareStaged>(retryCaches,retryArena,9,a,full,false,true,true,r));
  retryArena.limit = ag::staging_project(retryArena.used,0,retryArena.alignment) + gx::kVertexBlockBytes;
  const size_t partialPushes = retryArena.pushes;
  CHECK(!stage<CompareStaged>(retryCaches,retryArena,9,b,full,false,true,true,r));
  CHECK(retryArena.pushes == partialPushes + 1); CHECK(r.size == 0);
  retryArena.limit = retryArena.bytes.size();
  CHECK(stage<CompareStaged>(retryCaches,retryArena,9,a,full,true,true,true,r));
  verify(retryArena,a,full,r);
  // A full control stores every byte through its single binding.
  Arena control; ag::VertexPartCaches controlCache{};
  CHECK(stage<CompareStaged>(controlCache,control,1,c,gx::vertex_uniform_use(plain,false),false,true,false,r));
  CHECK(control.pushes == 1); CHECK(control.copied == sizeof(c)); verify(control,c,gx::vertex_uniform_use(plain,false),r);
}

std::string body(std::string w, const char* start) {
  size_t at = w.find(start); CHECK(at != std::string::npos); w.erase(0, at);
  for (const char* from : {"vsm.", "vsl."}) {
    size_t i = 0; while ((i = w.find(from,i)) != std::string::npos) { w.replace(i,4,"vsc."); i += 4; }
  }
  std::string normalized; normalized.reserve(w.size());
  for (size_t i = 0; i < w.size(); ++i) {
    if (w[i] == '/' && i + 1 < w.size() && w[i + 1] == '/') {
      while (i < w.size() && w[i] != '\n') ++i;
    } else if (static_cast<unsigned char>(w[i]) > 32u) normalized.push_back(w[i]);
  }
  return normalized;
}

struct Field { const char* name; unsigned part; size_t offset, bytes; };
const Field fields[] = {
  {"posnormalmatrix",0,offsetof(gx::VertexShaderConstants,posnormalmatrix),96},
  {"projection",0,offsetof(gx::VertexShaderConstants,projection),64},
  {"texmatrices",0,offsetof(gx::VertexShaderConstants,texmatrices),384},
  {"materials",0,offsetof(gx::VertexShaderConstants,materials),64},
  {"cached_normal",0,offsetof(gx::VertexShaderConstants,cached_normal),16},
  {"cached_tangent",0,offsetof(gx::VertexShaderConstants,cached_tangent),16},
  {"cached_binormal",0,offsetof(gx::VertexShaderConstants,cached_binormal),16},
  {"transformmatrices",1,0,1024}, {"normalmatrices",1,1024,512}, {"lights",2,0,640}
};

void binding_references(const std::string& program, bool sparse) {
  // Inspect raw generated WGSL before parity normalization: a reference to an
  // undeclared uniform bank is a real WebGPU validation error.
  for (const char* bank : {"vsc", "vsm", "vsl", "psc"}) {
    if (program.find(std::string(bank)+".") != std::string::npos)
      CHECK(program.find(std::string("var<uniform> ")+bank+":") != std::string::npos);
  }
  if (!sparse) {
    CHECK(program.find("vsm.") == std::string::npos);
    CHECK(program.find("vsl.") == std::string::npos);
  }
}
void tev_lighting_regressions() {
  for (unsigned light=0;light<8;++light) for(unsigned channel=0;channel<2;++channel)
  for(unsigned attenuation=0;attenuation<4;++attenuation) for(unsigned diffuse=0;diffuse<3;++diffuse)
  for(bool matrix_index : {false,true}) {
    gx::ShaderKey key{};key.tev_valid=1;key.num_tev_stages=1;
    key.lit_valid=1;key.num_color_chans=2;key.chan_captured_mask=3;
    key.has_vertex_normal=1;key.has_pos_mtx_idx=matrix_index;
    key.has_vertex_tangent=1;key.has_vertex_binormal=1;
    key.num_tex_gens=2;key.tex_gens[1].texgentype=1;
    key.tex_gens[1].embosslightshift=uint8_t(light);key.tex_gens[1].embosssourceshift=0;
    for(unsigned c : {channel,channel+2}) {
      key.litchan[c].enablelighting=1;key.litchan[c].light_mask=uint8_t(1u<<light);
      key.litchan[c].attnfunc=uint8_t(attenuation);key.litchan[c].diffusefunc=uint8_t(diffuse);
    }
    old::ShaderKey original{};std::memcpy(&original,&key,sizeof(key));
    const auto control=gx::generate_wgsl(key,false),sparse=gx::generate_wgsl(key,true);
    binding_references(control,false);binding_references(sparse,true);
    CHECK(control.find("vsc.lights[")!=std::string::npos);
    CHECK(sparse.find("vsl.lights[")!=std::string::npos);
    CHECK(body(control,"struct VertexIn")==body(old::generate_wgsl(original),"struct VertexIn"));
    CHECK(body(control,"struct VertexIn")==body(sparse,"struct VertexIn"));
  }
}
void shaders() {
  static_assert(sizeof(gx::ShaderKey) == sizeof(old::ShaderKey));
  static_assert(sizeof(gx::VertexShaderConstants) == sizeof(old::VertexShaderConstants));
  for (unsigned shape = 0; shape < 4096; ++shape) {
    gx::ShaderKey k{};
    k.num_tex_gens = uint8_t(shape % (gx::kMaxTexGens + 1));
    k.has_pos_mtx_idx = (shape >> 3) & 1;
    k.has_tex_mtx_idx = (shape >> 4) & 1; k.tex_mtx_idx_mask = uint8_t(shape >> 6);
    k.lit_valid = (shape >> 5) & 1; k.tev_valid = (shape >> 6) & 1;
    k.hud_tint = (shape >> 7) & 1;
    k.num_tev_stages = uint8_t(1u + shape % gx::kMaxTevStages);
    k.has_vertex_normal = (shape >> 8) & 1;
    k.has_vertex_tangent = (shape >> 9) & 1; k.has_vertex_binormal = (shape >> 10) & 1;
    k.num_color_chans = uint8_t((shape >> 10) % 3); k.chan_captured_mask = uint8_t(shape >> 8);
    for (unsigned c = 0; c < 4; ++c) {
      k.litchan[c].matsource = uint8_t((shape >> (c+1)) & 1);
      k.litchan[c].ambsource = uint8_t((shape >> (c+2)) & 1);
      k.litchan[c].enablelighting = uint8_t((shape >> (c+3)) & 1);
      k.litchan[c].light_mask = uint8_t(shape); k.litchan[c].attnfunc = uint8_t(shape % 4);
      k.litchan[c].diffusefunc = uint8_t(shape % 3);
    }
    for (unsigned t = 0; t < k.num_tex_gens; ++t) {
      k.tex_gens[t].texgentype = uint8_t((shape + t) % 4);
      k.tex_gens[t].sourcerow = uint8_t((shape + t) % 13);
      k.tex_gens[t].embosslightshift = uint8_t((shape + t) % 8);
      k.tex_gens[t].embosssourceshift = uint8_t(t == 0 ? 0 : t - 1);
    }
    old::ShaderKey before{}; std::memcpy(&before,&k,sizeof(k));
    const auto sparse = gx::generate_wgsl(k,true), control = gx::generate_wgsl(k,false);
    CHECK(gx::generate_wgsl(k) == control);
    const auto defaultUse = gx::vertex_uniform_use(k);
    CHECK(defaultUse.block == sizeof(gx::VertexShaderConstants) && defaultUse.matrices == 0 && defaultUse.lights == 0);
    binding_references(control,false); binding_references(sparse,true);
    CHECK(body(sparse,"struct VertexIn") == body(control,"struct VertexIn"));
    CHECK(body(sparse,"struct VertexIn") == body(old::generate_wgsl(before),"struct VertexIn"));
    const auto use = gx::vertex_uniform_use(k,true);
    const size_t reads[] = {use.block,use.matrices,use.lights};
    CHECK(use.block <= gx::kVertexBlockBytes); CHECK(use.matrices <= gx::kVertexMatrixBytes); CHECK(use.lights <= gx::kVertexLightBytes);
    const std::string math = sparse.substr(sparse.find("struct VertexIn"));
    for (const auto& f : fields) {
      const char* binding = f.part == 0 ? "vsc." : f.part == 1 ? "vsm." : "vsl.";
      if (math.find(std::string(binding)+f.name) != std::string::npos) {
        size_t end = f.offset + f.bytes;
        if (std::strcmp(f.name,"texmatrices") == 0) end = f.offset + 48u * k.num_tex_gens;
        CHECK(reads[f.part] >= end);
      }
    }
  }
  tev_lighting_regressions();
  for (bool dual : {false,true}) {
    CHECK(gx::generate_uber_wgsl(dual) == gx::generate_uber_wgsl(dual,false));
    CHECK(body(gx::generate_uber_wgsl(dual,true),"fn kb(") == body(gx::generate_uber_wgsl(dual,false),"fn kb("));
    CHECK(body(gx::generate_uber_wgsl(dual,true),"fn kb(") == body(old::generate_uber_wgsl(dual),"fn kb("));
    CHECK(gx::generate_uber_wgsl(dual,true).find("hud_multiplier") != std::string::npos);
    binding_references(gx::generate_uber_wgsl(dual,false),false);
    binding_references(gx::generate_uber_wgsl(dual,true),true);
  }
}

void capacity_and_bytes() {
  for (size_t alignment : {1u,16u,64u,256u,512u}) for (size_t initial = 0; initial < alignment; ++initial) {
    size_t sparse = initial;
    for (size_t n : {size_t(gx::kVertexBlockBytes),size_t(gx::kVertexMatrixBytes),size_t(gx::kVertexLightBytes)})
      sparse = ag::staging_project(sparse,n,alignment);
    const size_t reserve = sizeof(gx::VertexShaderConstants) + 2 * (alignment-1);
    const size_t bound = ag::staging_project(initial,reserve,alignment);
    CHECK(sparse <= bound);
    ag::StagingLimits limits{{0,0,bound+2048,0},alignment,1,2048};
    CHECK(ag::staging_fits({0,0,initial,0},{0,0,reserve,0,0},limits));
    --limits.capacity.uniforms; CHECK(!ag::staging_fits({0,0,initial,0},{0,0,reserve,0,0},limits));
  }
  gx::VertexShaderConstants c{}; values(c,7); gx::ShaderKey plain{};
  Arena control,sparse; ag::VertexPartCaches cc{},sc{}; ag::VertexUniformRanges r{};
  for (unsigned i = 0; i < 1000; ++i) {
    c.posnormalmatrix[0][3] = float(i);
    CHECK(stage<false>(cc,control,1,c,gx::vertex_uniform_use(plain,false),false,true,false,r));
    CHECK(stage<false>(sc,sparse,1,c,gx::vertex_uniform_use(plain,true),false,true,true,r));
    if (control.used > control.bytes.size()/2) { control.used=0; cc={}; }
  }
  CHECK(control.copied == 1000*sizeof(c)); CHECK(sparse.copied == 1000*160);
  std::printf("Uniform authored bytes: control=%zu sparse=%zu; aligned=%zu/%zu\n",control.copied,sparse.copied,control.aligned,sparse.aligned);
}


void canonical_identity_cases() {
  ag::VertexConstantsIdentity identity{};
  gx::VertexShaderConstants a{}, b{}; values(a, 11); values(b, 29);
  auto initial = identity.snapshot;
  CHECK(!identity.repeats(1, 7, a, false));
  identity.commit(1, 7, a, false, false);
  CHECK(!identity.snapshotValid);
  CHECK(std::memcmp(&identity.snapshot, &initial, sizeof(initial)) == 0);
  CHECK(identity.repeats(1, 7, a, false));
  CHECK(!identity.repeats(1, 0, a, false));
  CHECK(!identity.repeats(1, 8, a, false));
  CHECK(!identity.repeats(2, 7, a, false));
  CHECK(!identity.repeats(0, 7, a, false));
  // Turning interpolation on requires a real canonical snapshot even when
  // the previous off-mode draw had the same stable constants_id.
  CHECK(!identity.repeats(1, 7, a, true));
  identity.commit(1, 7, a, true, false);
  CHECK(identity.snapshotValid);
  CHECK(std::memcmp(&identity.snapshot, &a, sizeof(a)) == 0);
  CHECK(identity.repeats(1, 7, a, true));
  CHECK(identity.repeats(1, 0, a, true));
  CHECK(identity.repeats(1, 9, a, true));
  CHECK(!identity.repeats(1, 0, b, true));
  CHECK(!identity.repeats(1, 9, b, true));
  // A failed B did not commit: full interpolation identity still names A.
  CHECK(identity.repeats(1, 0, a, true));
  identity.commit(1, 0, b, true, false);
  CHECK(!identity.repeats(1, 0, a, true));
  CHECK(identity.repeats(1, 0, b, true));
  // A failed off-mode draw still invalidates canonical snapshot admission.
  CHECK(!identity.repeats(1, 0, a, false));
  CHECK(!identity.snapshotValid);
  CHECK(!identity.repeats(1, 0, b, true));
  identity.commit(1, 0, b, true, false);
  CHECK(identity.repeats(1, 0, b, true));
  CHECK(!identity.repeats(2, 0, b, true));
  identity.commit(2, 0, b, true, false);
  CHECK(identity.repeats(2, 0, b, true));
  // A staging segment changes packet identity even for repeated constants.
  ag::VertexPartCaches caches{}; Arena arena; ag::VertexUniformRanges r{};
  const gx::VertexUniformUse all{gx::kVertexBlockBytes,gx::kVertexMatrixBytes,gx::kVertexLightBytes};
  CHECK(stage<false>(caches,arena,2,b,all,true,true,true,r));
  const auto pushes = arena.pushes;
  CHECK(stage<false>(caches,arena,3,b,all,true,true,true,r));
  CHECK(arena.pushes == pushes + 3);
  verify(arena,b,all,r);
  // Bytewise interpolation equality is sensitive to omitted light/matrix
  // state; sparse prefix equality never becomes canonical identity evidence.
  identity.commit(3, 0, b, true, false);
  b.lights[7].color[3] ^= 1;
  CHECK(!identity.repeats(3, 0, b, true));
  b.transformmatrices[63][3] = 123.f;
  CHECK(!identity.repeats(3, 0, b, true));
}


template<bool CompareStaged>
void post_vertex_failure_cases() {
  gx::VertexShaderConstants a{},b{}; values(a,11);values(b,29);
  const gx::VertexUniformUse all{gx::kVertexBlockBytes,gx::kVertexMatrixBytes,gx::kVertexLightBytes};
  Arena arena;ag::VertexPartCaches caches{};ag::VertexConstantsIdentity identity{};
  ag::VertexUniformRanges r{};
  CHECK(stage<CompareStaged>(caches,arena,13,a,all,false,true,true,r));
  identity.commit(13,31,a,true,false);
  CHECK(!identity.repeats(13,47,b,true));
  CHECK(stage<CompareStaged>(caches,arena,13,b,all,false,true,true,r));
  // Simulate the actual post-VS pixel allocation guard. B's bank staging was
  // successful, but the draw returned before canonical identity committed.
#ifndef TEST_SKIP_POST_VS_INVALIDATION
  ag::invalidate_vertex_parts(caches);
#endif
  CHECK(identity.repeats(13,31,a,true));
  CHECK(stage<CompareStaged>(caches,arena,13,a,all,true,true,true,r));
  verify(arena,a,all,r);
}

int main(int argc, char** argv) {
  if (argc == 2 && std::strcmp(argv[1], "--environment-only") == 0) {
    std::printf("Sparse runtime opt-in: %d\n", sparse_vertex_uniforms_enabled() ? 1 : 0);
    return 0;
  }
  cache_cases<false>(); cache_cases<true>(); canonical_identity_cases(); post_vertex_failure_cases<false>(); post_vertex_failure_cases<true>(); shaders(); capacity_and_bytes();
  std::printf("Sparse uniforms: %llu checks, %llu failures\n",static_cast<unsigned long long>(checks),static_cast<unsigned long long>(failures));
  return failures ? 1 : 0;
}
