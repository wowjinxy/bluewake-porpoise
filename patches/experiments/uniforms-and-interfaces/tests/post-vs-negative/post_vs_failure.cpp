// SPDX-License-Identifier: GPL-3.0-or-later
// Authored A -> staged B -> failed PS -> repeated A regression.
#include "GXRuntime/graphics/aurora/lib/gfx/vertex_uniform_cache.hpp"
#include <vector>
#include <cstdio>
namespace gx=gxruntime::gxcore;
namespace ag=aurora::gfx;
template<bool CompareStaged> unsigned run() {
  gx::VertexShaderConstants a{},b{};
  std::memset(&a,0x11,sizeof(a));std::memset(&b,0x29,sizeof(b));
  ag::VertexPartCaches caches{};ag::VertexConstantsIdentity identity{};
  ag::VertexUniformRanges ranges{};
  std::vector<uint8_t> bytes(65536);size_t used=0;
  auto push=[&](const uint8_t* data,size_t count){size_t at=(used+255)&~size_t(255);std::memcpy(bytes.data()+at,data,count);used=at+count;return ag::VertexPartRange{uint32_t(at),uint32_t(count)};};
  auto read=[&](ag::VertexPartRange r){return bytes.data()+r.offset;};
  const gx::VertexUniformUse use{gx::kVertexBlockBytes,gx::kVertexMatrixBytes,gx::kVertexLightBytes};
  if(!ag::stage_vertex_uniforms<CompareStaged>(caches,13,a,use,false,true,true,push,read,ranges))return 99999;
  identity.commit(13,31,a,true,false);
  if(!ag::stage_vertex_uniforms<CompareStaged>(caches,13,b,use,false,true,true,push,read,ranges))return 99999;
  // The production PS allocation guard calls this exact helper in source2.
  // Its intentional omission recreates source1's post-VS failure defect.
#ifdef QUALIFY_POST_VS_FIXED
  ag::invalidate_vertex_parts(caches);
#endif
  if(!identity.repeats(13,31,a,true))return 99999;
  if(!ag::stage_vertex_uniforms<CompareStaged>(caches,13,a,use,true,true,true,push,read,ranges))return 99999;
  const uint32_t offsets[]={ranges.block,ranges.matrices,ranges.lights};
  const uint32_t starts[]={0,gx::kVertexMatrixOffset,gx::kVertexLightOffset};
  const uint32_t counts[]={use.block,use.matrices,use.lights};
  const auto* expected=reinterpret_cast<const uint8_t*>(&a);unsigned mismatches=0;
  for(unsigned p=0;p<3;++p)for(unsigned i=0;i<counts[p];++i)mismatches+=bytes[offsets[p]+i]!=expected[starts[p]+i];
  return mismatches;
}
int main(){auto mismatch=run<false>()+run<true>();std::printf("PostVS A/B/fail/A: mismatched_bytes=%u, failed=%u\n",mismatch,mismatch?1u:0u);return mismatch?1:0;}
