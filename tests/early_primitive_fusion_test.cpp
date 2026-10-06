// SPDX-License-Identifier: GPL-3.0-or-later
// Authored CPU-only packet/plan parity. No renderer, device or game authority.
#include "gxruntime/gxcore/gxcore.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
namespace ar = gxruntime::aurora_recomp;
namespace gxc = gxruntime::gxcore;
static unsigned checks, failures;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); } } while(0)

struct Input {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(4096u);
  bool available = true;
  DolGuestAddressResolver resolver{};
  Input() {
    dol_guest_address_resolver_init_callback(&resolver,resolve,this);
    for(unsigned i=0;i<128u;++i) for(unsigned j=0;j<3u;++j) {
      const float f=static_cast<float>(i*3u+j); std::uint32_t v; std::memcpy(&v,&f,4);
      for(unsigned k=0;k<4u;++k) bytes[i*12u+j*4u+k]=static_cast<std::uint8_t>(v>>(24u-k*8u));
    }
  }
  static bool resolve(void* u,u32 addr,u32 size,DolGuestAddressSpace space,DolGuestResourceKind kind,DolGuestResolvedRange* out) {
    auto& in=*static_cast<Input*>(u);
    if(!in.available || addr<0x1000u || addr-0x1000u>in.bytes.size() || size>in.bytes.size()-(addr-0x1000u)) return false;
    *out={in.bytes.data()+addr-0x1000u,addr,size,static_cast<u32>(in.bytes.size()-(addr-0x1000u)),space,kind}; return true;
  }
};
struct Capture {
  std::vector<ar::ConsumedDraw> draws;
  std::vector<gxc::DrawPlan> plans;
  std::vector<unsigned long long> ordinals;
  static void draw(const ar::ConsumedDraw& d,unsigned long long n,void* u) { auto& c=*static_cast<Capture*>(u);c.draws.push_back(d);c.ordinals.push_back(n); }
  static void plan(const gxc::DrawPlan& p,void* u) { static_cast<Capture*>(u)->plans.push_back(p); }
};
static ar::RenderPacket state(std::uint64_t seq,ar::RenderStateKind kind,unsigned index,unsigned value,unsigned aux=0) {
  ar::RenderPacket p{};p.kind=ar::RenderPacketKind::State;p.sequence=seq;p.state={.kind=kind,.index=index,.value=value,.aux0=aux};return p;
}
static ar::RenderPacket draw(std::uint64_t seq,unsigned primitive,const std::vector<std::uint8_t>& bytes,unsigned stride=1) {
  ar::RenderPacket p{};p.kind=ar::RenderPacketKind::Draw;p.sequence=seq;
  p.draw.primitive=primitive;p.draw.vertex_count=static_cast<unsigned>(bytes.size()/stride);p.draw.vertex_size=stride;
  p.draw.vertex_payload=bytes.data();p.draw.vertex_payload_size=static_cast<unsigned>(bytes.size());p.draw.xf_version=7;
  p.draw.transform_flags=ar::kDrawTransformProjectionValid;p.draw.projection[0]=p.draw.projection[2]=1.f;p.draw.projection[4]=-1.f;p.draw.projection_type=1;
  p.draw.position_matrix_valid_mask=1;p.draw.position_matrices[0][0]=p.draw.position_matrices[0][5]=p.draw.position_matrices[0][10]=1.f;return p;
}
static ar::RenderPacket span(std::uint64_t seq,const std::vector<std::uint8_t>& bytes) {
  ar::RenderPacket p{};p.kind=ar::RenderPacketKind::Resource;p.sequence=seq;p.resource.kind=ar::RenderResourceKind::IndexedArraySpan;
  p.resource.index=0;p.resource.count=static_cast<unsigned>(bytes.size());p.resource.size=(*std::max_element(bytes.begin(),bytes.end())+1u)*12u;
  p.resource.vertex_offset=0;p.resource.index_size=1;p.resource.element_size=12;return p;
}
static void setup(ar::ConsumingAuroraRenderSink& c,Input& in,Capture& capture) {
  c.set_streaming(true);c.set_assembly_totals(false);c.set_owned_array_inputs(true);c.set_guest_resolver(&in.resolver);c.set_draw_observer(Capture::draw,&capture);
  CHECK(c.submit_packet(state(1,ar::RenderStateKind::CpArrayBase,0,0x1000u)));
  CHECK(c.submit_packet(state(2,ar::RenderStateKind::CpArrayStride,0,12u)));
}
static gxc::GxCoreState plan_state() {
  gxc::GxCoreState s;s.reset();s.apply({.kind=ar::RenderStateKind::CpVcd,.index=0,.value=2u<<9u});
  s.apply({.kind=ar::RenderStateKind::CpVcd,.index=1,.value=0});
  s.apply({.kind=ar::RenderStateKind::CpVat,.index=0,.value=1u|(4u<<1u),.aux0=0});
  s.apply({.kind=ar::RenderStateKind::CpVat,.index=0,.value=0,.aux0=1});
  s.apply({.kind=ar::RenderStateKind::CpVat,.index=0,.value=0,.aux0=2});return s;
}
static std::vector<std::uint8_t> payload(unsigned count,unsigned start=0) {
  std::vector<std::uint8_t> v(count);for(unsigned i=0;i<count;++i)v[i]=static_cast<std::uint8_t>((start+i)%128u);return v;
}
static void topology() {
  const unsigned primitives[]={0x80,0x90,0x98,0xa0};const unsigned counts[]={4,6,5,5};
  for(unsigned a=0;a<4;++a)for(unsigned b=0;b<4;++b) {
    Input in;Capture fused,separate;ar::ConsumingAuroraRenderSink f,s;setup(f,in,fused);setup(s,in,separate);
    auto av=payload(counts[a]),bv=payload(counts[b],16);
    for(auto* c:{&f,&s}) {CHECK(c->submit_packet(draw(3,primitives[a],av)));CHECK(c->submit_packet(span(4,av)));}
    f.fuse_next_draw(1024);
    for(auto* c:{&f,&s}) {CHECK(c->submit_packet(draw(5,primitives[b],bv)));CHECK(c->submit_packet(span(6,bv)));c->flush_assembly();}
    CHECK(fused.draws.size()==1);CHECK(separate.draws.size()==2);CHECK(f.fused_draws()==1);
    gxc::GapCounters gaps{};const auto ps=plan_state();auto whole=ps.build_draw_plan(fused.draws[0],gaps),first=ps.build_draw_plan(separate.draws[0],gaps),second=ps.build_draw_plan(separate.draws[1],gaps);
    CHECK(whole.ok&&first.ok&&second.ok);
    auto expected=first.indices;for(auto x:second.indices)expected.push_back(static_cast<std::uint16_t>(x+counts[a]));CHECK(whole.indices==expected);
    auto vertices=first.vertices;vertices.insert(vertices.end(),second.vertices.begin(),second.vertices.end());CHECK(whole.vertices==vertices);
    CHECK(std::memcmp(&whole.pipeline,&first.pipeline,sizeof whole.pipeline)==0&&std::memcmp(&whole.pipeline,&second.pipeline,sizeof whole.pipeline)==0);
    CHECK(std::memcmp(&whole.constants,&first.constants,sizeof whole.constants)==0);
    CHECK(std::memcmp(&whole.pixel_constants,&first.pixel_constants,sizeof whole.pixel_constants)==0);
    CHECK(fused.draws[0].arrays[0].owned_data!=nullptr);CHECK(fused.draws[0].arrays[0].span_size==span(6,bv).resource.size);
    const auto retained=fused.draws[0];in.bytes.assign(in.bytes.size(),0xffu);f.reset();
    auto after=ps.build_draw_plan(retained,gaps);CHECK(after.vertices==whole.vertices);CHECK(after.indices==whole.indices);
  }
}
static void boundaries() {
  auto av=payload(4),bv=payload(5,8);
  for(unsigned mode=0;mode<14;++mode) {
    Input in;Capture cap;ar::ConsumingAuroraRenderSink c;setup(c,in,cap);
    CHECK(c.submit_packet(draw(3,0x98,av)));CHECK(c.submit_packet(span(4,av)));
    std::uint64_t seq=5;auto next=draw(7,0xa0,bv);
    switch(mode) {
    case 0: CHECK(c.submit_packet(state(seq++,ar::RenderStateKind::BpReg,0x41,8)));break;
    case 1: CHECK(c.submit_packet(state(seq++,ar::RenderStateKind::CpVat,0,1,0)));break;
    case 2: CHECK(c.submit_packet(state(seq++,ar::RenderStateKind::CpArrayBase,0,0x1010)));break;
    case 3: {ar::RenderPacket p{};p.kind=ar::RenderPacketKind::Resource;p.sequence=seq++;p.resource.kind=ar::RenderResourceKind::Texture;p.resource.address=0x1000;p.resource.size=32;CHECK(c.submit_packet(p));break;}
    case 4: {ar::RenderPacket p{};p.kind=ar::RenderPacketKind::Resource;p.sequence=seq++;p.resource.kind=ar::RenderResourceKind::Tlut;p.resource.size=32;CHECK(c.submit_packet(p));break;}
    case 5: {ar::RenderPacket p{};p.kind=ar::RenderPacketKind::Resource;p.sequence=seq++;p.resource.kind=ar::RenderResourceKind::CopyDestination;p.resource.format=0xf;CHECK(c.submit_packet(p));break;}
    case 6: next.draw.xf_version=8;break;
    case 7: next.draw.vtx_fmt=1;break;
    case 8: next.draw.current_pn_matrix=3;break;
    case 9: next.draw.position_matrix_valid_mask=3;break;
    case 10: next.draw.transform_flags|=ar::kDrawTransformViewportValid;break;
    case 11: c.flush_assembly();break;
    case 12: in.bytes[1]^=1;break;
    case 13: in.available=false;break;
    }
    next.sequence=seq++;c.fuse_next_draw(1024);CHECK(c.submit_packet(next));c.flush_assembly();CHECK(cap.draws.size()==2);CHECK(c.fused_draws()==0);
  }
  for(unsigned primitive:{0x80u,0x90u,0x98u,0xa0u,0xa8u,0xb0u,0xb8u})for(unsigned count=1;count<=6;++count) {
    const bool valid=primitive==0x80 ? count%4==0 : primitive==0x90 ? count%3==0 : (primitive==0x98||primitive==0xa0)&&count>=3;
    Input in;Capture cap;ar::ConsumingAuroraRenderSink c;setup(c,in,cap);auto shortv=payload(count);
    CHECK(c.submit_packet(draw(3,primitive,shortv)));CHECK(c.submit_packet(span(4,shortv)));c.fuse_next_draw(1024);
    CHECK(c.submit_packet(draw(5,0x98,bv)));CHECK(c.submit_packet(span(6,bv)));c.flush_assembly();CHECK(cap.draws.size()==(valid?1u:2u));
  }
  for(unsigned cap:{8u,9u,1024u}) {
    Input in;Capture capture;ar::ConsumingAuroraRenderSink c;setup(c,in,capture);
    CHECK(c.submit_packet(draw(3,0x98,av)));CHECK(c.submit_packet(span(4,av)));c.fuse_next_draw(cap);
    CHECK(c.submit_packet(draw(5,0x98,bv)));CHECK(c.submit_packet(span(6,bv)));c.flush_assembly();CHECK(capture.draws.size()==(cap>=9?1u:2u));
  }
}
static void core_boundaries() {
  for(unsigned mode=0;mode<8;++mode) {
    Input in;Capture captured;gxc::GxCoreSink sink;sink.set_guest_resolver(&in.resolver);sink.set_plan_observer(Capture::plan,&captured);
    std::uint64_t seq=1;auto send=[&](ar::RenderStateKind k,unsigned i,unsigned v,unsigned aux=0){CHECK(sink.submit_packet(state(seq++,k,i,v,aux)));};
    send(ar::RenderStateKind::CpVcd,0,(mode==1?1u:2u)<<9u);send(ar::RenderStateKind::CpVcd,1,0);
    send(ar::RenderStateKind::CpVat,0,1u|(4u<<1u));send(ar::RenderStateKind::CpVat,0,0,1);send(ar::RenderStateKind::CpVat,0,0,2);
    send(ar::RenderStateKind::CpArrayBase,0,0x1000);send(ar::RenderStateKind::CpArrayStride,0,12);
    if(mode==2)send(ar::RenderStateKind::BpReg,gxc::GxCoreState::kDrawTagRegister,42);
    if(mode==3){send(ar::RenderStateKind::BpReg,gxc::GxCoreState::kDrawScopeCountRegister,0x800002);send(ar::RenderStateKind::BpReg,gxc::GxCoreState::kDrawScopeRegister,77);}
    if(mode==4){for(unsigned i=0x6b;i<=0x79;++i)send(ar::RenderStateKind::BpReg,i,i==0x6b?1:0);send(ar::RenderStateKind::BpReg,0x6a,0x10001);}
    if(mode==5)send(ar::RenderStateKind::BpReg,0x6b,1); // incomplete authored HUD begin is also a boundary
    auto av=payload(4),bv=payload(5,8);CHECK(sink.submit_packet(draw(seq++,0x98,av)));CHECK(sink.submit_packet(span(seq++,av)));
    if(mode==6){auto saved=sink.save_state();CHECK(sink.load_state(saved.data(),saved.size()));}
    if(mode==7)sink.flush_frame();
    CHECK(sink.submit_packet(draw(seq++,0x98,bv)));CHECK(sink.submit_packet(span(seq++,bv)));sink.flush_frame();
    CHECK(captured.plans.size()==(mode==0?1u:2u));CHECK(sink.consumer().fused_draws()==(mode==0?1u:0u));
    if(mode==2)CHECK(captured.plans[0].draw_tag==42);
    if(mode==3){CHECK(captured.plans[0].draw_scope==77);CHECK(captured.plans[1].draw_scope==77);CHECK(captured.plans[1].draw_scope_part==2);}
  }
}

// Regression: guest arrays can change after can-append's Draw-time proof and
// before one of that Draw's trailing spans. The latest append must split,
// retaining exact old owners and every current span already delivered.
static void late_array_fallback() {
  for (unsigned prefix=1;prefix<=3;++prefix) for(unsigned mode=0;mode<5;++mode) {
    Input in; Capture cap; ar::ConsumingAuroraRenderSink c; setup(c,in,cap);
    std::uint64_t seq=3;
    auto av=payload(4),bv=payload(5,12),cv=payload(5,24);
    CHECK(c.submit_packet(draw(seq++,0x98,av))); CHECK(c.submit_packet(span(seq++,av)));
    for(unsigned i=1;i<prefix;++i) {
      c.fuse_next_draw(1024); CHECK(c.submit_packet(draw(seq++,0xa0,bv)));
      CHECK(c.submit_packet(span(seq++,bv)));
    }
    const auto prior=c.draws().back();
    c.fuse_next_draw(1024); auto d=draw(seq++,0x98,cv);
    d.draw.payload_pn_matrix_mask=4;
    d.draw.transform_flags|=ar::kDrawTransformPayloadPnMatrixValid;
    CHECK(c.submit_packet(d)); CHECK(c.fused_draws()==prefix);
    if(mode==0)in.bytes[1]^=0x80;
    if(mode==1)in.available=false;
    auto sp=span(seq++,cv);
    if(mode==2)sp.resource.element_size=8; // incompatible layout is unfused
    if(mode==3)sp.resource.index_size=2;
    if(mode==4){sp.resource.vertex_offset=1;sp.resource.element_size=8;}
    CHECK(c.submit_packet(sp));
    CHECK(c.failure_reason()==nullptr); CHECK(c.fused_draws()==prefix-1);
    CHECK(cap.draws.size()==1); CHECK(cap.ordinals[0]==prefix); CHECK(cap.draws[0].vertex_payload==prior.vertex_payload);
    CHECK(cap.draws[0].segments==prior.segments); CHECK(cap.draws[0].vertex_count==prior.vertex_count);
    CHECK(cap.draws[0].payload_pn_matrix_mask==prior.payload_pn_matrix_mask);
    CHECK(cap.draws[0].arrays[0].owned_data==prior.arrays[0].owned_data);
    CHECK(c.draws().back().vertex_payload==cv); CHECK(c.draws().back().segments.empty());
    CHECK(c.draws().back().payload_pn_matrix_mask==4); CHECK(c.draws().back().primitive==0x98);
    CHECK(c.draws().back().arrays[0].resolved==(mode!=1));
    if(mode==0)CHECK(std::memcmp(c.draws().back().arrays[0].host_data,in.bytes.data(),sp.resource.size)==0);
    c.flush_assembly(); CHECK(cap.draws.size()==2); CHECK(cap.ordinals[1]==prefix+1); CHECK(c.failure_reason()==nullptr);
    CHECK(c.draw_packets()==prefix+1); CHECK(c.vertex_inputs()==4+prefix*5);
    CHECK(c.payload_bytes()==4+prefix*5);
    if(mode==0) {
      gxc::GapCounters gaps{};auto ps=plan_state();
      auto old=ps.build_draw_plan(cap.draws[0],gaps),old_ref=ps.build_draw_plan(prior,gaps);
      CHECK(old.ok&&old_ref.ok); CHECK(old.vertices==old_ref.vertices); CHECK(old.indices==old_ref.indices);
      auto now=ps.build_draw_plan(cap.draws[1],gaps);
      Input fresh;fresh.bytes=in.bytes;Capture refcap;ar::ConsumingAuroraRenderSink ref;setup(ref,fresh,refcap);
      CHECK(ref.submit_packet(draw(3,0x98,cv)));CHECK(ref.submit_packet(span(4,cv)));ref.flush_assembly();
      auto separate=ps.build_draw_plan(refcap.draws[0],gaps);
      CHECK(now.ok&&separate.ok); CHECK(now.vertices==separate.vertices); CHECK(now.indices==separate.indices);
      // Subsequent guest mutation and reset cannot alter either captured owner.
      in.bytes.assign(in.bytes.size(),0x55);c.reset();
      auto old_after=ps.build_draw_plan(cap.draws[0],gaps),new_after=ps.build_draw_plan(cap.draws[1],gaps);
      CHECK(old_after.vertices==old.vertices);CHECK(new_after.vertices==now.vertices);
    }
  }
  // Roll back an already extended first attribute when a later attribute
  // rejects. Both the early incoming snapshot and the old owner must survive.
  for(unsigned mode=0;mode<3;++mode) {
    Input in;Capture cap;ar::ConsumingAuroraRenderSink c;setup(c,in,cap);
    CHECK(c.submit_packet(state(3,ar::RenderStateKind::CpArrayBase,1,0x1200)));
    CHECK(c.submit_packet(state(4,ar::RenderStateKind::CpArrayStride,1,12)));
    auto av=payload(4),bv=payload(5,12);auto a=span(6,av);auto b=a;b.sequence=7;b.resource.index=1;
    CHECK(c.submit_packet(draw(5,0x98,av)));CHECK(c.submit_packet(a));CHECK(c.submit_packet(b));
    auto prior=c.draws().back();c.fuse_next_draw(1024);CHECK(c.submit_packet(draw(8,0xa0,bv)));
    auto early=span(9,bv);CHECK(c.submit_packet(early));const auto early_owner=c.draws().back().arrays[0].owned_data;
    if(mode==0)in.bytes[0x201]^=0x40;
    if(mode==1)in.available=false;
    auto late=span(10,bv);late.resource.index=1;if(mode==2)late.resource.element_size=8;
    CHECK(c.submit_packet(late));CHECK(c.failure_reason()==nullptr);CHECK(c.fused_draws()==0);
    c.flush_assembly();CHECK(cap.draws.size()==2);
    CHECK(cap.draws[0].array_input_count==2);CHECK(cap.draws[1].array_input_count==2);
    CHECK(cap.draws[0].arrays[0].owned_data==prior.arrays[0].owned_data);
    CHECK(cap.draws[0].arrays[0].span_size==a.resource.size);
    CHECK(cap.draws[1].arrays[0].owned_data==early_owner);
    CHECK(cap.draws[1].arrays[0].span_size==early.resource.size);
    CHECK(cap.draws[1].arrays[1].resolved==(mode!=1));
    CHECK(c.resolved_array_inputs()==(mode==1?3u:4u));CHECK(c.unresolved_array_inputs()==(mode==1?1u:0u));
  }
  // Repeated span metadata must upsert bounded current-primitive inputs,
  // not overflow the 16-attribute undo snapshot.
  {
    Input in;Capture cap;ar::ConsumingAuroraRenderSink c;setup(c,in,cap);
    auto av=payload(4),bv=payload(5,12);CHECK(c.submit_packet(draw(3,0x98,av)));CHECK(c.submit_packet(span(4,av)));
    c.fuse_next_draw(1024);CHECK(c.submit_packet(draw(5,0xa0,bv)));
    for(unsigned i=0;i<40;++i)CHECK(c.submit_packet(span(6+i,bv)));
    in.bytes[1]^=1;CHECK(c.submit_packet(span(46,bv)));c.flush_assembly();
    CHECK(c.failure_reason()==nullptr);CHECK(cap.draws.size()==2);CHECK(cap.draws[1].array_input_count==1);
    CHECK(c.resolved_array_inputs()==2);CHECK(c.unresolved_array_inputs()==0);
  }
  // A new read attribute must also be retained before a later old one rejects.
  {
    Input in;Capture cap;ar::ConsumingAuroraRenderSink c;setup(c,in,cap);
    CHECK(c.submit_packet(state(3,ar::RenderStateKind::CpArrayBase,1,0x1200)));
    CHECK(c.submit_packet(state(4,ar::RenderStateKind::CpArrayStride,1,12)));
    auto av=payload(4),bv=payload(5,12);CHECK(c.submit_packet(draw(5,0x98,av)));CHECK(c.submit_packet(span(6,av)));
    c.fuse_next_draw(1024);CHECK(c.submit_packet(draw(7,0xa0,bv)));
    auto extra=span(8,bv);extra.resource.index=1;CHECK(c.submit_packet(extra));in.bytes[1]^=1;
    CHECK(c.submit_packet(span(9,bv)));c.flush_assembly();CHECK(cap.draws.size()==2);
    CHECK(cap.draws[0].array_input_count==1);CHECK(cap.draws[1].array_input_count==2);CHECK(c.failure_reason()==nullptr);
  }
}
int main(){topology();boundaries();core_boundaries();late_array_fallback();std::printf("gxcore_fusion_tests: %u checks, %u failures\n",checks,failures);return failures?1:0;}
