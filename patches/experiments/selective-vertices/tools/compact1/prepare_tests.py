from pathlib import Path
import re,hashlib,json
H=Path(__file__).resolve().parent;R=H.parents[2]
old=R/'build/performance-02-compact-vertices-20261006/candidate/GXRuntime/graphics/gxcore/tests/gxcore_vertex_layout_test.cpp'
s=old.read_text(encoding='utf-8')
start=s.index('#else\n const float*b=',s.index('static std::array<float,33> semantic'))
end=s.index('#endif',start)
s=s[:start]+'''#else
 CHECK(gx::copy_full_vertex(p,v,r.data()));
'''+s[end:]
start=s.index('#ifndef COMPACT_TEST_REFERENCE',s.index('static void run'))
end=s.index('#endif',start)
s=s[:start]+'''#ifndef COMPACT_TEST_REFERENCE
 const char*env=std::getenv("DOL_GXCORE_SELECTIVE_VERTICES");
 const bool selective=env&&env[0]=='1'&&env[1]=='\\0';
 CHECK(p.vertex_layout_mask==(selective?gx::selective_vertex_mask(p.pipeline.shader):0u));
 CHECK(p.vertices.size()==3*p.vertex_floats);CHECK(gx::vertex_storage_valid(p));
 auto full=p;CHECK(gx::materialize_full_vertices(full));CHECK(full.vertex_layout_mask==0u&&full.vertex_floats==33u);
 for(unsigned v=0;v<3;++v)CHECK(std::memcmp(full.vertices.data()+v*33,m.expected[v].data(),132)==0);
#else
 CHECK(p.vertices.size()==3*33u);CHECK(true);CHECK(true);
'''+s[end:]
s=s.replace('#include <cstdio>','#include <cstdio>\n#include <cstdlib>')
s=s.replace('bool full=false; };','bool full=false; std::array<std::shared_ptr<const std::vector<unsigned char>>,16> owners; };')
s=s.replace('r.owned_data=std::make_shared<const std::vector<unsigned char>>(arrays[a]);r.host_data=r.owned_data->data();r.host_available=r.owned_data->size();','m.owners[a]=std::make_shared<const std::vector<unsigned char>>(arrays[a]);r.host_data=m.owners[a]->data();r.host_available=m.owners[a]->size();')
s=s.replace('gx::GapCounters gaps{};auto p=m.state.build_draw_plan(copy,gaps);','''gx::GapCounters gaps{};gx::CachedVertexAttrs cached{};
 for(unsigned i=0;i<3;++i){cached.normal[i]=100.f+i;cached.binormal[i]=200.f+i;cached.tangent[i]=300.f+i;}
 const auto cachedBefore=cached;
 auto p=m.state.build_draw_plan(copy,gaps,&cached);''')
s=s.replace(' hash(&p.pipeline,sizeof(p.pipeline));',''' for(unsigned c=0;c<3;++c){
  const bool normal=p.pipeline.shader.has_vertex_normal!=0;
  const bool nbt=p.pipeline.shader.has_vertex_binormal!=0;
  CHECK(std::bit_cast<unsigned>(cached.normal[c])==std::bit_cast<unsigned>(normal?m.expected[2][22+c]:cachedBefore.normal[c]));
  CHECK(std::bit_cast<unsigned>(cached.binormal[c])==std::bit_cast<unsigned>(nbt?m.expected[2][27+c]:cachedBefore.binormal[c]));
  CHECK(std::bit_cast<unsigned>(cached.tangent[c])==std::bit_cast<unsigned>(nbt?m.expected[2][30+c]:cachedBefore.tangent[c]));
 }
 hash(&cached,sizeof(cached));
 hash(&p.pipeline,sizeof(p.pipeline));''')
extra=r'''
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
'''
s=s.replace('int main() {',extra+'\nint main() {\n#ifndef COMPACT_TEST_REFERENCE\n all_layout_masks();\n#endif')
s=s.replace(' std::printf("vertex_layout:',r'''
 for(unsigned colors=0;colors<4;++colors)for(unsigned uvs=0;uvs<32;++uvs){
  std::vector<Attr> attrs{{0,0,1,4,3,0}};
  for(unsigned c=0;c<2;++c)if(colors&(1u<<c))attrs.push_back({2,c,1,5,4,0});
  for(unsigned uv=0;uv<5;++uv)if(uvs&(1u<<uv))attrs.push_back({3,uv,1,4,2,0});
  run(make(attrs));
  attrs.push_back({1,0,3,3,9,0});run(make(attrs,0x3fu,true));
 }
 std::printf("vertex_layout:''')
# Attribute payload follows VCD order, even when stress cases append NBT last.
s=s.replace('static Made make(const std::vector<Attr>&attrs,unsigned matrices=0,bool nbt3=false,unsigned special=0) {','''static Made make(std::vector<Attr> attrs,unsigned matrices=0,bool nbt3=false,unsigned special=0) {
 std::stable_sort(attrs.begin(),attrs.end(),[](const Attr&a,const Attr&b){return a.kind<b.kind||(a.kind==b.kind&&a.slot<b.slot);});''')
s=s.replace('#include <array>','#include <array>\n#include <algorithm>')
p=H/'selective_decode_test.cpp';p.write_text(s,encoding='utf-8',newline='\n')
(H/'test-source-receipt.json').write_text(json.dumps(dict(source=dict(path=str(old),sha256=hashlib.sha256(old.read_bytes()).hexdigest()),candidate=dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()),status='NOT_COMPILED'),indent=2)+'\n')
print('Prepared selective_decode_test.cpp; no compiler invoked')
