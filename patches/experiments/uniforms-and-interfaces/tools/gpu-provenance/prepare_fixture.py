"""Prepare source-only, independently compiled ABI fixtures; no compiler/GPU use."""
from pathlib import Path
import hashlib
import json

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[2]
BASE = ROOT / 'build/k7-adaptations-20261008/pixels1/compact_pixels_test.cpp'
BASE_SHA = '4c8509384aea0c812d247f73c20d033fa3dd052b956d12fbdce4344cd1d99bba'
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert sha(BASE) == BASE_SHA
s = BASE.read_text(encoding='utf-8')

def replace(a, b):
    global s
    assert s.count(a) == 1, (a[:90], s.count(a))
    s = s.replace(a, b)

replace('// Synthetic rectangles/textures only.',
        '// SPDX-License-Identifier: GPL-3.0-or-later\n// Adapted from the repository-authored selective/raw pixel fixtures.\n// Synthetic rectangles/textures only.')
replace('#include <cstdint>', '#include <cstdint>\n#include <map>')
replace('struct Case { std::string name; std::vector<Draw> draws; Image expected; };', r'''
struct Case { std::string name; std::vector<Draw> draws; Image expected;
    bool primary_oracle=false;Pixel primary{}; };
struct Variant { const char* tag; bool selective; bool sparse; bool uber; bool prune; };
static std::vector<Variant> variants() {
    std::vector<Variant> v{{"specialized-full",false,false,false,false},
                           {"specialized-selective",true,false,false,false},
                           {"uber-full",false,false,true,false}};
#if defined(K7_UNIFORMS_CANDIDATE)
    v.push_back({"sparse-full",false,true,false,false});
    v.push_back({"sparse-selective",true,true,false,false});
    v.push_back({"uber-sparse",false,true,true,false});
#endif
#if defined(K7_INTERFACES_CANDIDATE)
    v.push_back({"pruned-full",false,false,false,true});
    v.push_back({"pruned-selective",true,false,false,true});
    v.push_back({"pruned-sparse-full",false,true,false,true});
    v.push_back({"pruned-sparse-selective",true,true,false,true});
#endif
    return v;
}
''')
replace('    WGPUTexture texture{}; WGPUTextureView texture_view{}; WGPUSampler sampler{};',
        '    WGPUTexture texture{}; WGPUTextureView texture_view{}; WGPUSampler sampler{};\n'
        '    std::map<std::string,WGPURenderPipeline> pipelines;\n'
        '    std::ofstream bindings{"uniform-bindings.tsv"};')
replace('    ~Device() {', '    ~Device() {\n        for(const auto& entry:pipelines)wgpuRenderPipelineRelease(entry.second);')
replace('WGPURenderPipeline pipeline(const gx::DrawPlan& p, unsigned mask, bool blend,\n                                const std::string& label)',
        'WGPURenderPipeline pipeline(const gx::DrawPlan& p, unsigned mask, bool blend,\n                                const std::string& label,const Variant& variant)')
replace('        const auto code=gx::generate_wgsl(k);', r'''
        std::string code;
#if defined(K7_INTERFACES_CANDIDATE)
        code=variant.uber?gx::generate_uber_wgsl(k.use_dst_alpha!=0,variant.sparse):gx::generate_wgsl(k,variant.sparse,variant.prune);
#elif defined(K7_UNIFORMS_CANDIDATE)
        CHECK(!variant.prune);
        code=variant.uber?gx::generate_uber_wgsl(k.use_dst_alpha!=0,variant.sparse):gx::generate_wgsl(k,variant.sparse);
#else
        CHECK(!variant.sparse&&!variant.prune);
        code=variant.uber?gx::generate_uber_wgsl(k.use_dst_alpha!=0):gx::generate_wgsl(k);
#endif
''')
replace('        std::ofstream(label+".wgsl") << code;',
        '        std::ofstream(label+".wgsl") << code;\n'
        '        const auto cache_key=code+"\\n// fixture-state:"+std::to_string(mask)+":"+std::to_string(blend)+":"+std::to_string(p.pipeline.depth_test)+":"+std::to_string(p.pipeline.depth_update);\n'
        '        if(const auto found=pipelines.find(cache_key);found!=pipelines.end())return found->second;')
replace('        pd.depthStencil=&ds;pd.fragment=&fs;',
        '        auto explicit_layout=pipeline_layout(k,variant);pd.layout=explicit_layout;\n'
        '        pd.depthStencil=&ds;pd.fragment=&fs;')
replace('auto pipe=wgpuDeviceCreateRenderPipeline(device,&pd);wgpuShaderModuleRelease(shader);',
        'auto pipe=wgpuDeviceCreateRenderPipeline(device,&pd);wgpuPipelineLayoutRelease(explicit_layout);wgpuShaderModuleRelease(shader);')
replace('CHECK(pipe);return pipe;', 'CHECK(pipe);pipelines.emplace(cache_key,pipe);return pipe;')
start=s.index('    // The shader text is identical')
s=s[:start]+r'''
    WGPUBindGroupLayout uniform_layout(unsigned parts) {
        std::vector<WGPUBindGroupLayoutEntry> entries;
        for(unsigned i=0;i<parts;++i) {
            WGPUBindGroupLayoutEntry e=WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;e.binding=i;
            e.visibility=WGPUShaderStage_Vertex|WGPUShaderStage_Fragment;
            e.buffer.type=WGPUBufferBindingType_Uniform;e.buffer.hasDynamicOffset=true;
            entries.push_back(e);
        }
        WGPUBindGroupLayoutDescriptor d=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;d.entryCount=entries.size();d.entries=entries.data();
        return wgpuDeviceCreateBindGroupLayout(device,&d);
    }
    WGPUBindGroupLayout texture_layout(unsigned mask) {
        // The specialized generator flattens <=one texmap onto tex0/samp0.
        if(gx::texmap_popcount(mask)<=1u)mask=1u;
        std::vector<WGPUBindGroupLayoutEntry> entries;
        for(unsigned t=0;t<8;++t)if(mask&(1u<<t)) {
            WGPUBindGroupLayoutEntry a=WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;a.binding=2u*t;a.visibility=WGPUShaderStage_Fragment;
            a.texture.sampleType=WGPUTextureSampleType_Float;a.texture.viewDimension=WGPUTextureViewDimension_2D;entries.push_back(a);
            WGPUBindGroupLayoutEntry b=WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;b.binding=2u*t+1u;b.visibility=WGPUShaderStage_Fragment;
            b.sampler.type=WGPUSamplerBindingType_Filtering;entries.push_back(b);
        }
        WGPUBindGroupLayoutDescriptor d=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;d.entryCount=entries.size();d.entries=entries.data();
        return wgpuDeviceCreateBindGroupLayout(device,&d);
    }
    WGPUPipelineLayout pipeline_layout(const gx::ShaderKey& key,const Variant& variant) {
        const bool ps=variant.uber||key.tev_valid||key.hud_tint;
        const bool tex=variant.uber||key.textured;
        std::vector<WGPUBindGroupLayout> groups;
        WGPUBindGroupLayoutDescriptor empty=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
        groups.push_back(wgpuDeviceCreateBindGroupLayout(device,&empty));
        groups.push_back(uniform_layout(variant.sparse?3u:1u));
        if(ps)groups.push_back(uniform_layout(1u));
        if(tex)groups.push_back(texture_layout(variant.uber?255u:gx::used_texmap_mask(key)));
        WGPUPipelineLayoutDescriptor d=WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;d.bindGroupLayoutCount=groups.size();d.bindGroupLayouts=groups.data();
        auto layout=wgpuDeviceCreatePipelineLayout(device,&d);for(auto group:groups)wgpuBindGroupLayoutRelease(group);CHECK(layout);return layout;
    }
''' +s[start:]
replace('        auto b = buffer(bytes,size,WGPUBufferUsage_Uniform); auto layout = wgpuRenderPipelineGetBindGroupLayout(pipe,group);',
        '        std::vector<unsigned char> guarded(256u+size+256u,0xD3);std::memcpy(guarded.data()+256u,bytes,size);\n'
        '        auto b = buffer(guarded.data(),guarded.size(),WGPUBufferUsage_Uniform); auto layout = wgpuRenderPipelineGetBindGroupLayout(pipe,group);')
replace('auto bg = wgpuDeviceCreateBindGroup(device,&d); wgpuRenderPassEncoderSetBindGroup(pass,group,bg,0,nullptr);',
        'auto bg = wgpuDeviceCreateBindGroup(device,&d);const uint32_t offset=256u;wgpuRenderPassEncoderSetBindGroup(pass,group,bg,1,&offset);')
replace('const gx::ShaderKey& key) {\n        auto layout=',
        'const gx::ShaderKey& key,bool uber=false) {\n        auto layout=')
replace('const unsigned mask=gx::used_texmap_mask(key);const bool multiple=gx::texmap_popcount(mask)>1;',
        'const unsigned mask=uber?255u:gx::used_texmap_mask(key);const bool multiple=uber||gx::texmap_popcount(mask)>1;')

start=s.index('    Image render(')
s=s[:start]+r'''
    void vertex_uniforms(WGPURenderPassEncoder pass,WGPURenderPipeline pipe,
                         const gx::DrawPlan& p,const Variant& variant,const std::string& label) {
#if defined(K7_UNIFORMS_CANDIDATE)
        if(variant.sparse) {
            const auto use=variant.uber?gx::VertexUniformUse{gx::kVertexBlockBytes,gx::kVertexMatrixBytes,gx::kVertexLightBytes}:
                                       gx::vertex_uniform_use(p.pipeline.shader,true);
            const unsigned prefixes[]{use.block,use.matrices,use.lights};
            const unsigned starts[]{0u,gx::kVertexMatrixOffset,gx::kVertexLightOffset};
            const unsigned capacities[]{gx::kVertexBlockBytes,gx::kVertexMatrixBytes,gx::kVertexLightBytes};
            auto layout=wgpuRenderPipelineGetBindGroupLayout(pipe,1);
            std::vector<WGPUBindGroupEntry> entries;std::vector<WGPUBuffer> owned;
            for(unsigned part=0;part<3;++part) {
                CHECK(prefixes[part]<=capacities[part]);
                // Production binds the complete bank while only its consumed
                // prefix was staged. Poison the rest and both guard regions.
                std::vector<unsigned char> bytes(256u+capacities[part]+256u,0xD3);
                std::memcpy(bytes.data()+256u,reinterpret_cast<const unsigned char*>(&p.constants)+starts[part],prefixes[part]);
                auto b=buffer(bytes.data(),bytes.size(),WGPUBufferUsage_Uniform);owned.push_back(b);
                WGPUBindGroupEntry e=WGPU_BIND_GROUP_ENTRY_INIT;e.binding=part;e.buffer=b;e.offset=0u;e.size=capacities[part];
                entries.push_back(e);
            }
            WGPUBindGroupDescriptor d=WGPU_BIND_GROUP_DESCRIPTOR_INIT;d.layout=layout;d.entryCount=entries.size();d.entries=entries.data();
            const uint32_t offsets[]{256u,256u,256u};
            auto bg=wgpuDeviceCreateBindGroup(device,&d);CHECK(bg);wgpuRenderPassEncoderSetBindGroup(pass,1,bg,3,offsets);
            wgpuBindGroupRelease(bg);wgpuBindGroupLayoutRelease(layout);for(auto b:owned)wgpuBufferRelease(b);
            bindings<<label<<'\t'<<prefixes[0]<<'\t'<<prefixes[1]<<'\t'<<prefixes[2]<<'\n';
            return;
        }
#else
        CHECK(!variant.sparse);
#endif
        bindings<<label<<'\t'<<sizeof p.constants<<"\t0\t0\n";
        uniform(pass,pipe,1,&p.constants,sizeof p.constants);
    }
''' +s[start:]
replace('Image render(const Case& c,bool selective)', 'Image render(const Case& c,const Variant& variant)')
replace('const unsigned mask=selective?gx::selective_vertex_mask(p.pipeline.shader):0u;',
        'const unsigned mask=variant.selective&&!variant.uber?gx::selective_vertex_mask(p.pipeline.shader):0u;')
replace('auto pipe=pipeline(p,mask,draw.blend,c.name+(selective?"-selective-":"-full-")+std::to_string(ordinal++));',
        'const auto label=c.name+"-"+variant.tag+"-"+std::to_string(ordinal++);\n'
        '            auto pipe=pipeline(p,mask,draw.blend,label,variant);')
replace('            uniform(pass,pipe,1,&p.constants,sizeof p.constants);\n            const bool ps=p.pipeline.shader.tev_valid||p.pipeline.shader.hud_tint;\n            if(ps)uniform(pass,pipe,2,&p.pixel_constants,sizeof p.pixel_constants);\n            if(p.pipeline.shader.textured)textures(pass,pipe,ps?3:2,p.pipeline.shader);',r'''
            vertex_uniforms(pass,pipe,p,variant,label);
            if(variant.uber) {
                gx::UberPixelConstants pc{};pc.psc=p.pixel_constants;
                std::memcpy(pc.key,&p.pipeline.shader,sizeof p.pipeline.shader);
                pc.extra[0]=gx::texmap_popcount(gx::used_texmap_mask(p.pipeline.shader))>1u;
                uniform(pass,pipe,2,&pc,sizeof pc);
                textures(pass,pipe,3,p.pipeline.shader,true);
            } else {
                const bool ps=p.pipeline.shader.tev_valid||p.pipeline.shader.hud_tint;
                if(ps)uniform(pass,pipe,2,&p.pixel_constants,sizeof p.pixel_constants);
                if(p.pipeline.shader.textured)textures(pass,pipe,ps?3:2,p.pipeline.shader);
            }
''')
# Cache owns each pipeline until the device is destroyed.
replace('wgpuRenderPipelineRelease(pipe);', '/* Pipeline retained by Device::pipelines. */')
replace('static void append_selective_cases(std::vector<Case>&);',
        'static void append_selective_cases(std::vector<Case>&);\nstatic void append_uniform_cases(std::vector<Case>&);')
replace('    append_selective_cases(out);', '    append_selective_cases(out);\n    append_uniform_cases(out);')
extra=(OUT/'cases.cpp.inc').read_text(encoding='utf-8')
start=s.index('int main() {')
s=s[:start]+extra+r'''
static void primary_oracle(const Case& c,const Image& pixels) {
    if(!c.primary_oracle)return;
    for(unsigned x:{16u,40u})for(unsigned channel=0;channel<4;++channel)
        CHECK(pixels[(20u*W+x)*4u+channel]==c.primary[channel]);
}
int main() {
    Device device;const auto fixtures=cases();const auto modes=variants();unsigned accepted=0;
    std::ofstream index("images.tsv");
    for(const auto& c:fixtures) {
        std::map<std::string,Image> rendered;
        for(const auto& mode:modes) {
            auto image=device.render(c,mode);
            const auto filename=c.name+"-"+mode.tag+".rgba-depth.bin";
            std::ofstream file(filename,std::ios::binary);file.write(reinterpret_cast<const char*>(image.data()),image.size());CHECK(file.good());
            index<<c.name<<'\t'<<mode.tag<<'\t'<<filename<<'\n';
            file.flush();index.flush();primary_oracle(c,image);
            rendered.emplace(mode.tag,std::move(image));
        }
        const auto& full=rendered.at("specialized-full");
        verify(c.name+"-full-vs-selective",rendered.at("specialized-selective"),full,0);
#if defined(K7_UNIFORMS_CANDIDATE)
        verify(c.name+"-full-vs-sparse",rendered.at("sparse-full"),full,0);
        verify(c.name+"-full-vs-sparse-selective",rendered.at("sparse-selective"),full,0);
        verify(c.name+"-uber-full-vs-sparse",rendered.at("uber-sparse"),rendered.at("uber-full"),0);
#endif
#if defined(K7_INTERFACES_CANDIDATE)
        for(const char* tag:{"pruned-full","pruned-selective","pruned-sparse-full","pruned-sparse-selective"})
            verify(c.name+"-full-vs-"+tag,rendered.at(tag),full,0);
#endif
        const auto clear=clear_image();CHECK(!std::equal(clear.begin(),clear.end(),full.begin()));++accepted;
    }
    CHECK(errors==0);
    std::ofstream result("pixels.json");result<<"{\n  \"cases_passed\": "<<accepted<<",\n  \"submitted_frames\": "<<frames
        <<",\n  \"checks\": "<<checks<<",\n  \"uncaptured_errors\": "<<errors.load()
        <<",\n  \"channel_tolerance\": 0,\n  \"synthetic_only\": true,\n  \"window_or_surface\": false,\n  \"variants\": "<<modes.size()<<"\n}\n";
    std::printf("Offscreen D3D12 uniforms/interfaces: %u cases/%u frames, %u checks PASS\n",accepted,frames,checks);
}
'''
target=OUT/'uniform_pixels_test.cpp'
target.write_text(s,encoding='utf-8',newline='\n')

# Snapshot only the std-only baseline generator closure, preserving source bytes.
# System/Dawn/toolchain closure is resolved and pinned by actual -M at build time.
baseline=OUT/'baseline/GXRuntime'
records=[]
for rel in ('graphics/gxcore/include/gxruntime/gxcore/shader.hpp',
            'graphics/gxcore/src/gxcore_shader.cpp','graphics/gxcore/src/gxcore_uber.cpp'):
    source=ROOT/'ref/recompcore/GXRuntime'/rel;dest=baseline/rel
    if dest.exists():
        assert dest.read_bytes()==source.read_bytes(), 'Preserved baseline differs; create a new scaffold instead'
    else:
        dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(source.read_bytes())
    records.append({'source':str(source),'snapshot':str(dest),'sha256':sha(source)})
manifest={'status':'PREPARED_NOT_COMPILED_OR_RUN','authored_parent':{'path':str(BASE),'sha256':BASE_SHA},
          'fixture':{'path':str(target),'sha256':sha(target)},'baseline':records,
          'source_only':True,'window_surface_input_game':False,
          'separate_abi_executables':True,'candidate_must_freeze_before_build':True}
(OUT/'preparation.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print(json.dumps(manifest,indent=2))
