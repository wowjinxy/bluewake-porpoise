// SPDX-License-Identifier: GPL-3.0-or-later
// Authored source/interface contract checks, not GPU execution.
#include <gxruntime/gxcore/shader.hpp>
#include "control/include/gxruntime/gxcore/shader.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>

namespace gx=gxruntime::gxcore;
namespace old=gxruntime::interface_control;
static unsigned long long checks=0,failures=0,cases=0;
#define CHECK(c) do {++checks;if(!(c)){++failures;if(failures<20)std::fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#c);}}while(0)
std::string section(const std::string& text,const std::string& start,const std::string& end="") {
  auto a=text.find(start);CHECK(a!=std::string::npos);if(a==std::string::npos)return{};
  auto b=end.empty()?text.size():text.find(end,a);CHECK(b!=std::string::npos);if(b==std::string::npos)return{};
  return text.substr(a,b-a);
}
void replace(std::string& text,const std::string& a,const std::string& b) {
  size_t pos=0;while((pos=text.find(a,pos))!=std::string::npos){text.replace(pos,a.size(),b);pos+=b.size();}
}
std::string vertex_math(const std::string& text,bool pruned) {
  auto body=section(text,"@vertex\nfn vs_main","    return o;\n}");
  if(!pruned)return body;
  std::istringstream input(body);std::string line,out;
  while(std::getline(input,line)) {
    if(line.find("    var gx_")==0)continue;
    if(line.find("    o.color")==0&&line.find(" = gx_color")!=std::string::npos)continue;
    if(line.find("    o.uv")==0&&line.find(" = gx_uv")!=std::string::npos)continue;
    out+=line+'\n';
  }
  replace(out,"gx_color0","o.color0");replace(out,"gx_color1","o.color1");
  for(unsigned i=0;i<gx::kMaxTexGens;++i)replace(out,"gx_uv"+std::to_string(i),"o.uv"+std::to_string(i));
  return out;
}
std::string fragment_math(const std::string& text) {
  auto body=section(text,"@fragment");std::istringstream input(body);std::string line,out;
  while(std::getline(input,line)) {
    if(line.find("    let col0i =")==0||line.find("    let col1i =")==0||line.find("    let fixpoint_uv")==0)continue;
    out+=line+'\n';
  }
  return out;
}
void check_key(const gx::ShaderKey& key) {
  ++cases;old::ShaderKey before{};static_assert(sizeof(before)==sizeof(key));std::memcpy(&before,&key,sizeof(key));
  for(bool sparse:{false,true}) {
    auto baseline=old::generate_wgsl(before,sparse);
    auto disabled=gx::generate_wgsl(key,sparse,false);
    CHECK(disabled==baseline);CHECK(gx::generate_wgsl(key,sparse)==baseline);
    if(!sparse)CHECK(gx::generate_wgsl(key)==baseline);
    auto pruned=gx::generate_wgsl(key,sparse,true);
    CHECK(section(pruned,"struct VertexIn","struct VertexOut")==section(baseline,"struct VertexIn","struct VertexOut"));
    CHECK(vertex_math(pruned,true)==vertex_math(baseline,false));
    CHECK(fragment_math(pruned)==fragment_math(baseline));
    auto outputs=section(pruned,"struct VertexOut","@vertex");
    auto fs=section(pruned,"@fragment");
    for(const char* color:{"color0","color1"})if(fs.find(std::string("in.")+color)!=std::string::npos)
      CHECK(outputs.find(std::string(color)+": vec4f")!=std::string::npos);
    for(unsigned i=0;i<gx::kMaxTexGens;++i) {
      const auto name="uv"+std::to_string(i);
      if(fs.find("in."+name)!=std::string::npos)CHECK(outputs.find(name+": vec3f")!=std::string::npos);
      if(outputs.find(name+": vec3f")!=std::string::npos) {
        const bool color1=key.tev_valid||[&](){for(unsigned t=0;t<key.num_tex_gens;++t)if(key.tex_gens[t].texgentype==unsigned(gx::TexGenType::Color1))return true;return false;}();
        CHECK(outputs.find("@location("+std::to_string((color1?2u:1u)+i)+") "+name+":")!=std::string::npos);
      }
    }
    CHECK(pruned.find("    var gx_color0 = vec4f(0.0);")!=std::string::npos);
    for(unsigned i=0;i<key.num_tex_gens;++i)CHECK(pruned.find("    var gx_uv"+std::to_string(i)+" = vec3f(0.0);")!=std::string::npos);
  }
}
void expect_outputs(gx::ShaderKey key,bool color0,bool color1,unsigned uv) {
  check_key(key);
  auto out=section(gx::generate_wgsl(key,true,true),"struct VertexOut","@vertex");
  CHECK((out.find("color0: vec4f")!=std::string::npos)==color0);
  CHECK((out.find("color1: vec4f")!=std::string::npos)==color1);
  for(unsigned i=0;i<gx::kMaxTexGens;++i)CHECK((out.find("uv"+std::to_string(i)+": vec3f")!=std::string::npos)==((uv&(1u<<i))!=0));
}
void edge_cases() {
  gx::ShaderKey key{};key.tev_valid=1;key.num_tev_stages=1;key.num_tex_gens=5;key.uv_mask=31;
  expect_outputs(key,false,false,0);
  key.tev_stages[0].cc_a=10;expect_outputs(key,true,false,0);
  key.tev_stages[0].tevorders_colorchan=1;expect_outputs(key,false,true,0);
  key.tev_stages[0].cc_a=0;key.tev_stages[0].ac_a=5;expect_outputs(key,false,true,0);
  key.tev_stages[0].tevorders_colorchan=7;expect_outputs(key,false,false,0);
  key.tev_stages[0].ac_a=0;key.textured=1;key.tev_stages[0].tevorders_enable=1;key.tev_stages[0].tevorders_texcoord=4;
  expect_outputs(key,false,false,16);
  key.tev_stages[0].tevorders_texcoord=7;expect_outputs(key,false,false,1);
  key.tev_stages[0].tevorders_enable=0;expect_outputs(key,false,false,0);
  key.num_ind_stages=1;key.tev_stages[0].ind_bump_alpha=1;key.ind_stages[0].texcoord=3;
  expect_outputs(key,false,false,8);
  key.ind_stages[0].texcoord=7;expect_outputs(key,false,false,1);
  key.tev_stages[0].ind_bump_alpha=0;expect_outputs(key,false,false,0);
  key.tex_gens[2].texgentype=unsigned(gx::TexGenType::Color1);key.tev_stages[0].tevorders_enable=1;key.tev_stages[0].tevorders_texcoord=2;
  expect_outputs(key,false,false,4); // color1 remains local for texgen only
  key.tex_gens[0].texgentype=unsigned(gx::TexGenType::EmbossMap);key.tex_gens[0].embosssourceshift=4;
  key.tev_stages[0].tevorders_texcoord=0;expect_outputs(key,false,false,1); // forward source keeps existing zero
  key.num_tev_stages=2;key.tev_stages[0].cc_a=10;key.tev_stages[0].tevorders_colorchan=0;
  key.tev_stages[1].ac_a=5;key.tev_stages[1].tevorders_colorchan=1;expect_outputs(key,true,true,1);
  key={};key.num_tex_gens=5;key.tex_gens[2].texgentype=unsigned(gx::TexGenType::Color1);
  expect_outputs(key,true,false,0);
  key.textured=1;expect_outputs(key,true,false,1);
}
int main() {
  for(unsigned shape=0;shape<4096;++shape) {
    gx::ShaderKey key{};key.num_tex_gens=shape%(gx::kMaxTexGens+1);key.uv_mask=31;
    key.tev_valid=(shape>>3)&1;key.textured=key.num_tex_gens?((shape>>4)&1):0;
    key.num_tev_stages=key.tev_valid?1+shape%gx::kMaxTevStages:0;
    key.num_ind_stages=key.textured&&key.tev_valid?(shape>>5)%(gx::kMaxIndirectStages+1):0;
    key.has_pos_mtx_idx=(shape>>6)&1;key.has_tex_mtx_idx=(shape>>7)&1;key.tex_mtx_idx_mask=31;
    key.has_vertex_normal=(shape>>8)&1;key.has_vertex_tangent=(shape>>9)&1;key.has_vertex_binormal=(shape>>10)&1;
    key.lit_valid=(shape>>11)&1;key.num_color_chans=2;key.chan_captured_mask=15;
    key.has_color0=1;key.has_color1=1;key.hud_tint=(shape>>5)&1;key.use_dst_alpha=(shape>>6)&1;
    for(unsigned i=0;i<key.num_tex_gens;++i){auto&t=key.tex_gens[i];t.texgentype=(shape+i)%4;t.sourcerow=(shape+i)%13;t.embosssourceshift=(shape+i)%key.num_tex_gens;t.embosslightshift=(shape+i)%8;t.projection=(shape+i)&1;}
    for(unsigned i=0;i<key.num_ind_stages;++i){key.ind_stages[i].texcoord=(shape+i)%8;key.ind_stages[i].texmap=(shape+i)%8;}
    for(unsigned i=0;i<key.num_tev_stages;++i){auto&s=key.tev_stages[i];s.cc_a=(shape+i)%16;s.ac_a=(shape+i)%8;s.tevorders_colorchan=(shape+i)%8;s.tevorders_texcoord=(shape+i)%8;s.tevorders_texmap=(shape+i)%8;s.tevorders_enable=(shape+i)&1;s.ind_stage=(shape+i)%4;s.ind_bump_alpha=key.num_ind_stages?(shape+i)%4:0;s.ind_matrix_index=key.num_ind_stages?(shape+i)%4:0;s.ind_format=(shape+i)%4;}
    check_key(key);
  }
  edge_cases();std::printf("Interface authored: %llu cases, %llu checks, %llu failures\n",cases,checks,failures);
  return failures?1:0;
}
