// SPDX-License-Identifier: GPL-3.0-or-later
// CPU decoder differential fixture. The same source also runs against the
// preceding full-layout implementation; it does not open a renderer/device.
#include "gxruntime/gxcore/gxcore.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
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
struct Made { gx::GxCoreState state; ar::ConsumedDraw draw; std::array<std::array<float,33>,3> expected; bool full=false; };
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
static Made make(const std::vector<Attr>&attrs,unsigned matrices=0,bool nbt3=false,unsigned special=0) {
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
 for(unsigned a=0;a<16;++a)if(!arrays[a].empty()) {auto&r=m.draw.arrays[m.draw.array_input_count++];r.attr=a;r.owned_data=std::make_shared<const std::vector<unsigned char>>(arrays[a]);r.host_data=r.owned_data->data();r.host_available=r.owned_data->size();r.stride=r.host_available/(a==1&&nbt3?9:3);r.resolved=true;r.indexed=true;}
 return m;
}
static std::array<float,33> semantic(const gx::DrawPlan&p,unsigned v) {
 std::array<float,33> r{};
#ifdef COMPACT_TEST_REFERENCE
 std::memcpy(r.data(),p.vertices.data()+v*gx::kVertexFloats,132);
#else
 const float*b=p.vertices.data()+v*p.vertex_floats;
 std::memcpy(r.data(),b,32);for(unsigned c=0;c<4;++c)r[8+c]=p.vertex_floats==gx::kFullVertexFloats?b[gx::kVertexColor1Offset/4+c]:1.f;
 for(unsigned t=0;t<5;++t)if(t<2||p.vertex_floats==gx::kFullVertexFloats)std::memcpy(r.data()+12+2*t,b+gx::vertex_uv_offset(t)/4,8);
 std::memcpy(r.data()+22,b+gx::kVertexNormalOffset/4,12);
 if(p.vertex_floats==gx::kFullVertexFloats)std::memcpy(r.data()+25,b+gx::kVertexTexMtxIdxOffset/4,32);
#endif
 return r;
}
static void run(Made m) {
 ++cases;ar::ConsumedDraw copy=m.draw;m.draw={};gx::GapCounters gaps{};auto p=m.state.build_draw_plan(copy,gaps);CHECK(p.ok);if(!p.ok)return;
#ifndef COMPACT_TEST_REFERENCE
 CHECK(p.vertex_floats==(m.full?33u:15u));CHECK(p.vertices.size()==3*p.vertex_floats);CHECK(gx::vertex_layout_full(p.pipeline.shader)==m.full);
#else
 CHECK(p.vertices.size()==3*33u);CHECK(true);CHECK(true);
#endif
 CHECK(p.vertex_count==3);CHECK(p.indices==std::vector<std::uint16_t>({0,1,2}));
 for(unsigned v=0;v<3;++v){auto s=semantic(p,v);for(unsigned k=0;k<33;++k)CHECK(std::bit_cast<unsigned>(s[k])==std::bit_cast<unsigned>(m.expected[v][k]));hash(s.data(),132);}
 hash(&p.pipeline,sizeof(p.pipeline));hash(&p.constants,sizeof(p.constants));hash(&p.pixel_constants,sizeof(p.pixel_constants));auto shader=gx::generate_wgsl(p.pipeline.shader);hash(shader.data(),shader.size());
 // Every incomplete FIFO prefix must refuse safely, including index16 tails.
 for(std::size_t n=0;n<copy.vertex_payload.size();++n){auto bad=copy;bad.vertex_payload.resize(n);gx::GapCounters g;CHECK(!m.state.build_draw_plan(bad,g).ok);hash(&g.vertex_payload_overrun,sizeof(g.vertex_payload_overrun));}
 for(unsigned a=0;a<copy.array_input_count;++a){auto bad=copy;bad.arrays[a].host_available=0;gx::GapCounters g;CHECK(!m.state.build_draw_plan(bad,g).ok);bad=copy;bad.arrays[a].resolved=false;CHECK(!m.state.build_draw_plan(bad,g).ok);}
}
int main() {
 for(unsigned fmt=0;fmt<5;++fmt)for(unsigned type=1;type<=3;++type)for(unsigned count=2;count<=3;++count)run(make({{0,0,type,fmt,count,fmt==4?0u:3u}}));
 for(unsigned fmt=0;fmt<6;++fmt)for(unsigned type=1;type<=3;++type)for(unsigned slot=0;slot<2;++slot)run(make({{0,0,1,4,3,0},{2,slot,type,fmt,4,0}}));
 for(unsigned slot=0;slot<8;++slot)for(unsigned fmt=0;fmt<5;++fmt)for(unsigned type=1;type<=3;++type)for(unsigned count=1;count<=2;++count)run(make({{0,0,1,4,3,0},{3,slot,type,fmt,count,fmt==4?0u:2u}}));
 for(unsigned fmt:{1u,3u,4u})for(unsigned type=1;type<=3;++type)for(unsigned count:{3u,9u})run(make({{0,0,1,4,3,0},{1,0,type,fmt,count,0}}));
 for(unsigned type:{2u,3u})for(unsigned fmt:{1u,3u,4u})run(make({{0,0,1,4,3,0},{1,0,type,fmt,9,0}},0,true));
 for(unsigned t=0;t<8;++t)run(make({{0,0,1,4,3,0}},1u|(2u<<t)));
 for(unsigned special:{90u,91u})run(make({{0,0,1,4,3,0},{3,1,1,4,2,0}},0,false,special));
 run(make({{0,0,3,3,3,5},{1,0,3,3,9,0},{2,0,3,5,4,0},{2,1,3,3,4,0},{3,0,3,3,2,3},{3,1,3,2,2,2},{3,2,3,4,2,0},{3,3,3,1,1,1},{3,4,3,0,2,0}},0x3fu));
 std::printf("vertex_layout: %u cases, %u checks, %u failures, semantic=%016llx\n",cases,checks,failures,(unsigned long long)digest);return failures?1:0;
}
