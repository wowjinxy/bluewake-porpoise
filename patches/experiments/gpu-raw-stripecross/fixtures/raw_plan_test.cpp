// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "raw_decoder.cpp"

namespace gxc = gxruntime::gxcore;
namespace ar = gxruntime::aurora_recomp;
static std::uint64_t cases = 0;
static void require(bool value, const char* what) {
  if (!value) { std::fprintf(stderr, "FAIL %s case=%llu\n", what, (unsigned long long)cases); std::abort(); }
}
struct Model {
  gxc::GxCoreState state{};
  ar::ConsumedDraw draw{};
  std::array<std::vector<std::uint8_t>, 16> storage{};
  gxc::DrawPlan reference{}, candidate{};
  gxc::CachedVertexAttrs reference_attrs{}, candidate_attrs{};
};
static std::uint32_t scalar_size(std::uint32_t format) { return format < 2 ? 1 : format < 4 ? 2 : 4; }
static void configure_state(Model& model, std::uint32_t lo, std::uint32_t hi, const std::array<std::uint32_t, 3>& vat) {
  model.state.apply({.kind=ar::RenderStateKind::CpVcd, .index=0, .value=lo});
  model.state.apply({.kind=ar::RenderStateKind::CpVcd, .index=1, .value=hi});
  for (unsigned i=0; i<3; ++i)
    model.state.apply({.kind=ar::RenderStateKind::CpVat, .index=0, .value=vat[i], .aux0=i});
}
static void initialize(Model& model, std::uint32_t lo, std::uint32_t hi,
                       const std::array<std::uint32_t, 3>& vat, unsigned count=4) {
  model.state.reset();
  configure_state(model, lo, hi, vat);
  gxc::WalkLayout walk{};
  require(gxc::derive_walk(lo, hi, vat.data(), walk), "derive actual walk");
  model.draw = ar::ConsumedDraw{};
  model.draw.primitive = 0x80;
  model.draw.vertex_count = count;
  model.draw.vertex_size = walk.vertex_size;
  model.draw.xf_version = 1;
  model.draw.transform_flags = ar::kDrawTransformProjectionValid | ar::kDrawTransformViewportValid;
  model.draw.projection_type = 1;
  model.draw.projection[0] = model.draw.projection[2] = 1.f;
  model.draw.projection[4] = -1.f;
  model.draw.position_matrix_valid_mask = 1;
  model.draw.position_matrices[0][0] = model.draw.position_matrices[0][5] = model.draw.position_matrices[0][10] = 1.f;
  model.draw.normal_matrix_word_mask[0] = 0x1ff;
  model.draw.normal_matrices[0][0] = model.draw.normal_matrices[0][4] = model.draw.normal_matrices[0][8] = 1.f;
  model.draw.viewport[0] = 320.f; model.draw.viewport[1] = -240.f;
  model.draw.viewport[3] = 662.f; model.draw.viewport[4] = 582.f;
  model.draw.viewport[2] = model.draw.viewport[5] = 16777215.f;
  for (unsigned e=0; e<walk.entry_count; ++e) {
    const auto& entry = walk.entries[e];
    if (entry.kind == gxc::WalkEntry::kPosMtxIdx || entry.kind == gxc::WalkEntry::kTexMtxIdx) continue;
    auto& storage = model.storage[entry.attr];
    const auto stride = entry.element_size + 3u;
    if (storage.empty()) {
      storage.resize(stride * 512u);
      for (unsigned index=0; index<512; ++index) {
        for (unsigned b=0; b<entry.element_size; ++b) storage[index*stride+b] = static_cast<std::uint8_t>(3+index+b*13+entry.attr*7);
        if (entry.kind != gxc::WalkEntry::kColor && entry.format == 4)
          for (unsigned c=0; c<entry.element_size/4; ++c) {
            const float number = 0.25f + float((index+c)%13);
            std::uint32_t bits; std::memcpy(&bits, &number, 4);
            for (unsigned b=0; b<4; ++b) storage[index*stride+4*c+b] = static_cast<std::uint8_t>(bits >> (24-8*b));
          }
      }
    }
    if (entry.vcd_type > 1 && !gxc::find_array(model.draw, entry.attr)) {
      auto& array = model.draw.arrays[model.draw.array_input_count++];
      array.attr = entry.attr; array.base = 0x80000000u+entry.attr*0x10000;
      array.stride = stride; array.span_size = entry.element_size;
      array.indexed = array.resolved = true; array.host_data = storage.data(); array.host_available = static_cast<unsigned>(storage.size());
    }
  }
  for (unsigned v=0; v<count; ++v)
    for (unsigned e=0; e<walk.entry_count; ++e) {
      const auto& entry = walk.entries[e];
      if (entry.kind == gxc::WalkEntry::kPosMtxIdx || entry.kind == gxc::WalkEntry::kTexMtxIdx) {
        model.draw.vertex_payload.push_back(static_cast<std::uint8_t>((v+e)*3)); continue;
      }
      const unsigned index = (v*3+e*47)%251;
      if (entry.vcd_type == 1) {
        const auto& storage = model.storage[entry.attr];
        const auto offset = index*(entry.element_size+3u);
        model.draw.vertex_payload.insert(model.draw.vertex_payload.end(), storage.begin()+offset, storage.begin()+offset+entry.element_size);
      } else if (entry.vcd_type == 2) model.draw.vertex_payload.push_back(static_cast<std::uint8_t>(index));
      else {
        model.draw.vertex_payload.push_back(static_cast<std::uint8_t>(index >> 8));
        model.draw.vertex_payload.push_back(static_cast<std::uint8_t>(index));
      }
    }
  model.reference = {}; model.candidate = {};
  model.reference_attrs = {{0.5f,0.25f,1.f},{1.f,2.f,3.f},{4.f,5.f,6.f}};
  model.candidate_attrs = model.reference_attrs;
}
static bool same_bytes(const std::vector<float>& a, const std::vector<float>& b) {
  return a.size()==b.size() && (a.empty() || std::memcmp(a.data(),b.data(),a.size()*sizeof(float))==0);
}

static unsigned admitted=0,declined=0,checks=0;
static void test(Model& m,bool expected_raw) {
  ++cases;gxc::GapCounters a{},b{};
  m.state.build_draw_plan_into(m.draw,a,&m.reference_attrs,m.reference);
  m.state.build_draw_plan_into(m.draw,b,&m.candidate_attrs,m.candidate,true);
  require(!m.reference.gpu_raw_pos_uv,"default API is decoded");
  require(m.candidate.gpu_raw_pos_uv==(expected_raw && m.reference.ok),"independent exact shape admission");
  if(m.candidate.gpu_raw_pos_uv) {
    ++admitted;require(m.candidate.vertices.empty(),"raw path bypasses decoded allocation");
    require(gxc::materialize_raw_pos_uv(m.candidate),"mandatory legacy fallback materializes");
  } else ++declined;
  require(same_bytes(m.reference.vertices,m.candidate.vertices),"bit exact legacy vertex slots incl PN row");
  require(m.reference.indices==m.candidate.indices,"same strip/quad topology");
  require(m.reference.ok==m.candidate.ok,"same drawable status");
  require(std::memcmp(&a,&b,sizeof a)==0,"same gap counters");
  require(std::memcmp(&m.reference.pipeline,&m.candidate.pipeline,sizeof m.reference.pipeline)==0,"same full pipeline");
  require(std::memcmp(&m.reference.constants,&m.candidate.constants,sizeof m.reference.constants)==0,"same transform/lighting/NBT constants");
  require(std::memcmp(&m.reference.pixel_constants,&m.candidate.pixel_constants,sizeof m.reference.pixel_constants)==0,"same TEV/fog constants");
  require(std::memcmp(&m.reference_attrs,&m.candidate_attrs,sizeof m.reference_attrs)==0,"same cross draw cached normal NBT");
  checks+=12;
}
int main() {
  // Independent enumeration: exact F32 XYZ + F32 ST admits; all other component
  // counts/formats, matrix bytes and unrelated attributes retain CPU decoding.
  for(unsigned pos=0;pos<5;++pos)for(unsigned tex=0;tex<5;++tex)
    for(unsigned pc=0;pc<2;++pc)for(unsigned tc=0;tc<2;++tc)
      for(unsigned frac: {0u,1u,15u,31u}) {
        Model m;initialize(m,1u<<9,1u,{pc|(pos<<1)|(tc<<21)|(tex<<22)|(frac<<25),0,0});
        test(m,pos==4&&tex==4&&pc==1&&tc==1);
      }
  for(unsigned extra:{1u,2u,256u,1u<<11,1u<<13}) {
    Model m;initialize(m,(1u<<9)|extra,1u,{1u|(4u<<1)|(4u<<10)|(5u<<14)|(1u<<21)|(4u<<22),0,0});test(m,false);
  }
  for(unsigned vcd: {2u,3u}) {
    Model m;initialize(m,vcd<<9,1u,{1u|(4u<<1)|(1u<<21)|(4u<<22),0,0});test(m,false);
    Model n;initialize(n,1u<<9,vcd,{1u|(4u<<1)|(1u<<21)|(4u<<22),0,0});test(n,false);
  }
  for(unsigned primitive:{0x80u,0x90u,0x98u,0xa0u,0xa8u,0xb0u,0xb8u,0u})
    for(unsigned count=1;count<11;++count)for(unsigned matrix: {0u,1u,3u,9u,40u}) {
      Model m;initialize(m,1u<<9,1u,{1u|(4u<<1)|(1u<<21)|(4u<<22),0,0},count);
      m.draw.primitive=primitive;m.draw.current_pn_matrix=matrix;test(m,true);
    }
  Model m;initialize(m,1u<<9,1u,{1u|(4u<<1)|(1u<<21)|(4u<<22),0,0});
  m.draw.vertex_payload.pop_back();test(m,false);
  m.draw.vertex_payload.push_back(0);m.draw.vertex_payload.push_back(0);test(m,false);
  m.draw.vertex_payload.pop_back();m.draw.cull_all=true;test(m,false);
  m.draw.cull_all=false;m.draw.transform_flags=0;test(m,false);
  // Exact unusual f32 bit patterns, signed zero, subnormals, infinities/NaN.
  initialize(m,1u<<9,1u,{1u|(4u<<1)|(1u<<21)|(4u<<22),0,0});
  unsigned i=0;for(unsigned word:{0u,0x80000000u,1u,0x007fffffu,0x00800000u,0x7f800000u,0xff800000u,0x7fc01234u,0x3f800001u}) {
    for(unsigned b=0;b<4;++b)m.draw.vertex_payload[i*4+b]=static_cast<unsigned char>(word>>(24-b*8));++i;
  }
  test(m,true);
  // Rejected materialization is transactional for malformed descriptors.
  gxc::GapCounters gaps{};gxc::DrawPlan p;m.state.build_draw_plan_into(m.draw,gaps,nullptr,p,true);
  auto original=p;p.match_payload_size--;
  require(!gxc::materialize_raw_pos_uv(p)&&p.gpu_raw_pos_uv&&p.vertices.empty(),"malformed fallback leaves raw descriptor unchanged");
  p=original;p.match_payload=nullptr;require(!gxc::materialize_raw_pos_uv(p)&&p.vertices.empty(),"null payload declines");
  require(admitted>200&&declined>200,"meaningful positive negative count");
  std::printf("PASS raw admission/materialization cases=%llu admitted=%u declined=%u checks=%u\n",(unsigned long long)cases,admitted,declined,checks);
}
