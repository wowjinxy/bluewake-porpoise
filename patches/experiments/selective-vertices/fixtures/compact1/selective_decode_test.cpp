// SPDX-License-Identifier: GPL-3.0-or-later
// CPU decoder differential fixture. The same source also runs against the
// preceding full-layout implementation; it does not open a renderer/device.
#include "gxruntime/gxcore/gxcore.hpp"
#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
namespace ar = gxruntime::aurora_recomp;
namespace gx = gxruntime::gxcore;
static unsigned checks=0, failures=0, cases=0;
static std::uint64_t digest=1469598103934665603ull;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#x); } } while(0)
static void hash(const void* p,std::size_t n) { const auto*b=static_cast<const unsigned char*>(p); for(std::size_t i=0;i<n;++i) digest=(digest^b[i])*1099511628211ull; }
static void be(std::vector<unsigned char>& b,unsigned x,unsigned n) { for(unsigned i=n;i;--i)b.push_back(x>>(8*(i-1))); }
static unsigned size(unsigned fmt) { return fmt<2?1:fmt<4?2:4; }
struct Attr { unsigned kind,slot,type,fmt,count,frac; }; // kind: pos, normal, color, tex
struct Made { gx::GxCoreState state; ar::ConsumedDraw draw; std::array<std::array<float,33>,3> expected; bool full=false; std::array<std::shared_ptr<const std::vector<unsigned char>>,16> owners; };
static void scalar(std::vector<unsigned char>& b,unsigned fmt,unsigned seed,float& out,unsigned frac) {
 const int n=(seed&1)?-int(17+seed):int(17+seed);
 if(fmt==4) { const unsigned bits=seed==90?0x7fc12345u:seed==91?0x7f800000u:std::bit_cast<unsigned>(float(n)*.25f);be(b,bits,4);out=std::bit_cast<float>(bits); }
 else { const unsigned width=size(fmt);be(b,unsigned(n),width);const int v=fmt==0?int((unsigned)n&255):fmt==1?int(static_cast<signed char>(n)):fmt==2?int((unsigned)n&65535):int(static_cast<short>(n));out=float(v)/float(1u<<frac); }
}
static void color(std::vector<unsigned char>&b,unsigned fmt,unsigned seed,float*out) {
 const unsigned r=(seed*19+37)&255,g=(seed*31+83)&255,bl=(seed*7+151)&255,a=(seed*43+17)&255;
 switch(fmt) {
 case 0: { const unsigned v=(r>>3)<<11|(g>>2)<<5|(bl>>3);be(b,v,2);out[0]=float(((v>>11)&31)*8+((v>>13)&7))/255.f;out[1]=float(((v>>5)&63)*4+((v>>9)&3))/255.f;out[2]=float((v&31)*8+((v>>2)&7))/255.f;out[3]=1.f;break; }
 case 1:case 2: b.insert(b.end(),{(unsigned char)r,(unsigned char)g,(unsigned char)bl});if(fmt==2)b.push_back(0xA5);out[0]=r/255.f;out[1]=g/255.f;out[2]=bl/255.f;out[3]=1.f;break;
 case 3: { const unsigned v=(r>>4)<<12|(g>>4)<<8|(bl>>4)<<4|(a>>4);be(b,v,2);for(unsigned i=0;i<4;++i)out[i]=float((v>>(12-4*i))&15)/15.f;break; }
 case 4: { const unsigned v=(r>>2)<<18|(g>>2)<<12|(bl>>2)<<6|(a>>2);be(b,v,3);for(unsigned i=0;i<4;++i)out[i]=float((((v>>(18-6*i))&63)<<2)|(((v>>(18-6*i))&63)>>4))/255.f;break; }
 default: b.insert(b.end(),{(unsigned char)r,(unsigned char)g,(unsigned char)bl,(unsigned char)a});out[0]=r/255.f;out[1]=g/255.f;out[2]=bl/255.f;out[3]=a/255.f; }
}
static Made make(std::vector<Attr> attrs,unsigned matrices=0,bool nbt3=false,unsigned special=0) {
 std::stable_sort(attrs.begin(),attrs.end(),[](const Attr&a,const Attr&b){return a.kind<b.kind||(a.kind==b.kind&&a.slot<b.slot);});
 Made m{};m.state.reset();m.draw.primitive=0x90;m.draw.vertex_count=3;m.draw.current_pn_matrix=2;m.draw.sequence=7;m.draw.transform_flags=ar::kDrawTransformProjectionValid;m.draw.projection_type=1;m.draw.projection[0]=m.draw.projection[2]=1.f;m.draw.projection[4]=-1.f;m.draw.position_matrix_valid_mask=0x3ff;
 unsigned lo=matrices,hi=0,vat[3]={};
 for(auto&a:attrs) {
  if(a.kind==0){lo|=a.type<<9;vat[0]|=(a.count==3)|(a.fmt<<1)|(a.frac<<4);}
  else if(a.kind==1){lo|=a.type<<11;vat[0]|=(a.count==9)<<9|(a.fmt<<10)|(unsigned(nbt3)<<31);m.full|=a.count==9;}
  else if(a.kind==2){lo|=a.type<<(13+2*a.slot);vat[0]|=1u<<(13+4*a.slot)|(a.fmt<<(14+4*a.slot));m.full|=a.slot==1;}
  else {hi|=a.type<<(2*a.slot);m.full|=a.slot>=2&&a.slot<5;const unsigned eb[]={21,0,9,18,27,5,14,23},fb[]={22,1,10,19,28,6,15,24},fr[]={25,4,13,22,0,9,18,27};const unsigned group=a.slot==0?0:a.slot<5?1:2;vat[group]|=(unsigned(a.count==2)<<eb[a.slot])|(a.fmt<<fb[a.slot]);vat[a.slot==4?2:group]|=a.frac<<fr[a.slot];}
 }
 m.full|=(matrices&0x1fe)!=0;
 m.state.apply({.kind=ar::RenderStateKind::CpVcd,.index=0,.value=lo});m.state.apply({.kind=ar::RenderStateKind::CpVcd,.index=1,.value=hi});
 for(unsigned i=0;i<3;++i)m.state.apply({.kind=ar::RenderStateKind::CpVat,.index=0,.value=vat[i],.aux0=i});
 std::array<std::vector<unsigned char>,16> arrays;
 for(unsigned v=0;v<3;++v) {
  auto&ex=m.expected[v];ex.fill(0);for(unsigned c=0;c<8;++c)ex[4+c]=1.f;
  unsigned pn=6,tx[2]={};if(matrices&1){pn=3+3*v;m.draw.vertex_payload.push_back(pn);}ex[3]=std::bit_cast<float>(pn);
  for(unsigned t=0;t<8;++t)if(matrices&(2u<<t)){const unsigned val=30+t*3+v;m.draw.vertex_payload.push_back(val);tx[t/4]|=val<<(8*(t%4));}
  ex[25]=std::bit_cast<float>(tx[0]);ex[26]=std::bit_cast<float>(tx[1]);
  for(auto&a:attrs) {
   std::vector<unsigned char> bytes;float ignored[16]{};float*out=a.kind==0?ex.data():a.kind==1?ex.data()+22:a.kind==2?ex.data()+4+4*a.slot:ex.data()+12+2*a.slot; if(a.kind==3&&a.slot>=5)out=ignored;
   if(a.kind==2)color(bytes,a.fmt,v+3,out);
   else for(unsigned c=0;c<a.count;++c){unsigned dst=a.kind==1&&c>=3?c+2:c;scalar(bytes,a.fmt,special?special:3+v*11+c,out[dst],a.kind==1?(a.fmt==1?6:a.fmt==3?14:0):a.frac);}
   const unsigned attr=a.kind==0?0:a.kind==1?1:a.kind==2?2+a.slot:4+a.slot;
   if(a.type==1)m.draw.vertex_payload.insert(m.draw.vertex_payload.end(),bytes.begin(),bytes.end());
   else {
    if(a.kind==1&&nbt3&&a.count==9){const unsigned part=3*size(a.fmt);for(unsigned n=0;n<3;++n){be(m.draw.vertex_payload,v*3+n,a.type==2?1:2);arrays[attr].insert(arrays[attr].end(),bytes.begin()+n*part,bytes.begin()+(n+1)*part);}}
    else {be(m.draw.vertex_payload,v,a.type==2?1:2);arrays[attr].insert(arrays[attr].end(),bytes.begin(),bytes.end());}
   }
  }
 }
 m.draw.vertex_size=m.draw.vertex_payload.size()/3;
 for(unsigned a=0;a<16;++a)if(!arrays[a].empty()) {auto&r=m.draw.arrays[m.draw.array_input_count++];r.attr=a;m.owners[a]=std::make_shared<const std::vector<unsigned char>>(arrays[a]);r.host_data=m.owners[a]->data();r.host_available=m.owners[a]->size();r.stride=r.host_available/(a==1&&nbt3?9:3);r.resolved=true;r.indexed=true;}
 return m;
}
static std::array<float,33> semantic(const gx::DrawPlan&p,unsigned v) {
 std::array<float,33> r{};
#ifdef COMPACT_TEST_REFERENCE
 std::memcpy(r.data(),p.vertices.data()+v*gx::kVertexFloats,132);
#else
 CHECK(gx::copy_full_vertex(p,v,r.data()));
#endif
 return r;
}
static void run(Made m) {
 ++cases;ar::ConsumedDraw copy=m.draw;m.draw={};gx::GapCounters gaps{};gx::CachedVertexAttrs cached{};
 for(unsigned i=0;i<3;++i){cached.normal[i]=100.f+i;cached.binormal[i]=200.f+i;cached.tangent[i]=300.f+i;}
 const auto cachedBefore=cached;
 auto p=m.state.build_draw_plan(copy,gaps,&cached);CHECK(p.ok);if(!p.ok)return;
#ifndef COMPACT_TEST_REFERENCE
 const char*env=std::getenv("DOL_GXCORE_SELECTIVE_VERTICES");
 const bool selective=env&&env[0]=='1'&&env[1]=='\0';
 CHECK(p.vertex_layout_mask==(selective?gx::selective_vertex_mask(p.pipeline.shader):0u));
 CHECK(p.vertices.size()==3*p.vertex_floats);CHECK(gx::vertex_storage_valid(p));
 auto full=p;CHECK(gx::materialize_full_vertices(full));CHECK(full.vertex_layout_mask==0u&&full.vertex_floats==33u);
 for(unsigned v=0;v<3;++v)CHECK(std::memcmp(full.vertices.data()+v*33,m.expected[v].data(),132)==0);
#else
 CHECK(p.vertices.size()==3*33u);CHECK(true);CHECK(true);
#endif
 CHECK(p.vertex_count==3);CHECK(p.indices==std::vector<std::uint16_t>({0,1,2}));
 for(unsigned v=0;v<3;++v){auto s=semantic(p,v);for(unsigned k=0;k<33;++k)CHECK(std::bit_cast<unsigned>(s[k])==std::bit_cast<unsigned>(m.expected[v][k]));hash(s.data(),132);}
 for(unsigned c=0;c<3;++c){
  const bool normal=p.pipeline.shader.has_vertex_normal!=0;
  const bool nbt=p.pipeline.shader.has_vertex_binormal!=0;
  CHECK(std::bit_cast<unsigned>(cached.normal[c])==std::bit_cast<unsigned>(normal?m.expected[2][22+c]:cachedBefore.normal[c]));
  CHECK(std::bit_cast<unsigned>(cached.binormal[c])==std::bit_cast<unsigned>(nbt?m.expected[2][27+c]:cachedBefore.binormal[c]));
  CHECK(std::bit_cast<unsigned>(cached.tangent[c])==std::bit_cast<unsigned>(nbt?m.expected[2][30+c]:cachedBefore.tangent[c]));
 }
 hash(&cached,sizeof(cached));
 hash(&p.pipeline,sizeof(p.pipeline));hash(&p.constants,sizeof(p.constants));hash(&p.pixel_constants,sizeof(p.pixel_constants));auto shader=gx::generate_wgsl(p.pipeline.shader);hash(shader.data(),shader.size());
 // Every incomplete FIFO prefix must refuse safely, including index16 tails.
 for(std::size_t n=0;n<copy.vertex_payload.size();++n){auto bad=copy;bad.vertex_payload.resize(n);gx::GapCounters g;CHECK(!m.state.build_draw_plan(bad,g).ok);hash(&g.vertex_payload_overrun,sizeof(g.vertex_payload_overrun));}
 for(unsigned a=0;a<copy.array_input_count;++a){auto bad=copy;bad.arrays[a].host_available=0;gx::GapCounters g;CHECK(!m.state.build_draw_plan(bad,g).ok);bad=copy;bad.arrays[a].resolved=false;CHECK(!m.state.build_draw_plan(bad,g).ok);}
}

#ifndef COMPACT_TEST_REFERENCE
static void all_layout_masks() {
 for(unsigned mask=3;mask<=gx::kVertexAllLocations;mask+=4){
  gx::DrawPlan p;p.vertex_layout_mask=mask;p.vertex_floats=gx::vertex_stride_bytes(mask)/4;p.vertex_count=3;p.vertices.assign(3*p.vertex_floats,0.f);
  CHECK(gx::vertex_storage_valid(p));
  auto recipe=gx::decoded_vertex_layout(mask);CHECK(recipe.stride==p.vertex_floats*4);
  std::array<std::array<float,33>,3> expected;
  for(unsigned v=0;v<3;++v){gx::full_vertex_defaults(expected[v].data());
   for(unsigned l=0;l<gx::kVertexLocationCount;++l){
    CHECK(recipe.offsets[l]==gx::vertex_location_offset(mask,l));
    if(!(mask&(1u<<l)))continue;
    CHECK(recipe.offsets[l]+gx::kVertexLocationBytes[l]<=recipe.stride);
    for(unsigned c=0;c<gx::kVertexLocationBytes[l]/4;++c){
     unsigned bits=(v*239+l*17+c)%5==0?0x7fc12345u:(v*239+l*17+c)%5==1?0x80000000u:(v*239+l*17+c)%5==2?0x7f800000u:0xff00aa55u^(v*239+l*17+c);
     std::memcpy(reinterpret_cast<unsigned char*>(expected[v].data())+gx::kVertexLocationOffsets[l]+c*4,&bits,4);
     std::memcpy(reinterpret_cast<unsigned char*>(p.vertices.data()+v*p.vertex_floats)+recipe.offsets[l]+c*4,&bits,4);
    }
   }
   auto normalized=semantic(p,v);CHECK(std::memcmp(normalized.data(),expected[v].data(),132)==0);
  }
  auto full=p;CHECK(gx::materialize_full_vertices(full));
  for(unsigned v=0;v<3;++v)CHECK(std::memcmp(full.vertices.data()+v*33,expected[v].data(),132)==0);
  auto bad=p;bad.vertex_floats=0;CHECK(!gx::vertex_storage_valid(bad));CHECK(!gx::materialize_full_vertices(bad));
  bad=p;bad.vertices.pop_back();CHECK(!gx::materialize_full_vertices(bad));
  bad=p;bad.vertex_count=2;CHECK(!gx::materialize_full_vertices(bad));
 }
 gx::DrawPlan bad;bad.vertex_layout_mask=1u<<31;CHECK(!gx::vertex_storage_valid(bad));
}
#endif

int main() {
#ifndef COMPACT_TEST_REFERENCE
 all_layout_masks();
#endif
 for(unsigned fmt=0;fmt<5;++fmt)for(unsigned type=1;type<=3;++type)for(unsigned count=2;count<=3;++count)run(make({{0,0,type,fmt,count,fmt==4?0u:3u}}));
 for(unsigned fmt=0;fmt<6;++fmt)for(unsigned type=1;type<=3;++type)for(unsigned slot=0;slot<2;++slot)run(make({{0,0,1,4,3,0},{2,slot,type,fmt,4,0}}));
 for(unsigned slot=0;slot<8;++slot)for(unsigned fmt=0;fmt<5;++fmt)for(unsigned type=1;type<=3;++type)for(unsigned count=1;count<=2;++count)run(make({{0,0,1,4,3,0},{3,slot,type,fmt,count,fmt==4?0u:2u}}));
 for(unsigned fmt:{1u,3u,4u})for(unsigned type=1;type<=3;++type)for(unsigned count:{3u,9u})run(make({{0,0,1,4,3,0},{1,0,type,fmt,count,0}}));
 for(unsigned type:{2u,3u})for(unsigned fmt:{1u,3u,4u})run(make({{0,0,1,4,3,0},{1,0,type,fmt,9,0}},0,true));
 for(unsigned t=0;t<8;++t)run(make({{0,0,1,4,3,0}},1u|(2u<<t)));
 for(unsigned special:{90u,91u})run(make({{0,0,1,4,3,0},{3,1,1,4,2,0}},0,false,special));
 run(make({{0,0,3,3,3,5},{1,0,3,3,9,0},{2,0,3,5,4,0},{2,1,3,3,4,0},{3,0,3,3,2,3},{3,1,3,2,2,2},{3,2,3,4,2,0},{3,3,3,1,1,1},{3,4,3,0,2,0}},0x3fu));

 for(unsigned colors=0;colors<4;++colors)for(unsigned uvs=0;uvs<32;++uvs){
  std::vector<Attr> attrs{{0,0,1,4,3,0}};
  for(unsigned c=0;c<2;++c)if(colors&(1u<<c))attrs.push_back({2,c,1,5,4,0});
  for(unsigned uv=0;uv<5;++uv)if(uvs&(1u<<uv))attrs.push_back({3,uv,1,4,2,0});
  run(make(attrs));
  attrs.push_back({1,0,3,3,9,0});run(make(attrs,0x3fu,true));
 }
 std::printf("vertex_layout: %u cases, %u checks, %u failures, semantic=%016llx\n",cases,checks,failures,(unsigned long long)digest);return failures?1:0;
}
