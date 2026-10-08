// SPDX-License-Identifier: GPL-3.0-or-later
// Actual frame-interpolation capture with compact/full layouts. No renderer.
#include "frame_interp.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
namespace fi=aurora::gfx::frame_interp;
namespace gx=gxruntime::gxcore;
static unsigned checks=0,failures=0;
#define CHECK(c) do{++checks;if(!(c)){++failures;std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#c);}}while(0)
int main(){
 for(unsigned mask=0;mask<=gx::kVertexAllLocations;++mask){
  if(mask!=0u&&!gx::vertex_layout_valid(mask))continue;
  const unsigned stride=gx::vertex_stride_bytes(mask)/4u;
  gx::DrawPlan p{};p.ok=true;p.vertex_count=6;p.vertex_floats=stride;p.vertex_layout_mask=mask;p.draw_tag=123;p.draw_tag_age=7;p.match_direct_position=true;
  std::vector<unsigned char> payload(6*12,0xA5);p.match_payload=payload.data();p.match_payload_size=payload.size();p.match_vertex_stride=12;
  p.vertices.resize(6*stride);
  for(unsigned i=0;i<p.vertices.size();++i)p.vertices[i]=float(i)+.125f;
  auto retained=p.vertices;fi::DrawInput out{};fi::capture_draw(p,out);
  CHECK(out.key!=0);CHECK(out.positions.size()==18);CHECK(out.age==7);
  for(unsigned v=0;v<6;++v)for(unsigned c=0;c<3;++c)CHECK(out.positions[3*v+c]==p.vertices[v*stride+c]);
  CHECK(p.vertices==retained);
  p.match_direct_position=false;p.draw_tag=0;fi::capture_draw(p,out);CHECK(out.positions.empty());CHECK(out.haveSamples);
  for(unsigned i=0;i<3;++i)for(unsigned c=0;c<3;++c)CHECK(out.samples[3*i+c]==p.vertices[(i==0?0:i==1?3:5)*stride+c]);
  p.draw_scope_part=17;fi::capture_draw(p,out);CHECK(out.positions.size()==18);CHECK(out.age==0);
  const auto saved=out.positions;p.vertices.clear();CHECK(out.positions==saved); // copied capture is independent
  p.vertices=retained;p.vertices.pop_back();fi::capture_draw(p,out);CHECK(out.positions.empty());CHECK(!out.haveSamples);
  p.vertices=retained;p.vertex_count=7;fi::capture_draw(p,out);CHECK(out.positions.empty());CHECK(!out.haveSamples);
  p.vertex_count=6;
  for(unsigned invalid:{0u,1u,stride+1u,34u,0xffffffffu}){p.vertex_floats=invalid;fi::capture_draw(p,out);CHECK(out.positions.empty());CHECK(!out.haveSamples);}
 }
 std::printf("selective_interp_layout: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
