"""Prepare the authored D3D12 selective-layout fixture; never builds or runs it."""
from pathlib import Path
import hashlib
import json

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[2]
BASE = ROOT / 'patches/experiments/gpu-raw-stripecross/fixtures/raw_pixels_test.cpp'
BASE_SHA = '76f5f4915d841e5a4317a9ebb153479a0e8b5a7439a9fdec5b963c6ab471aa7f'
assert hashlib.sha256(BASE.read_bytes()).hexdigest() == BASE_SHA
s = BASE.read_text(encoding='utf-8')

def replace(a, b):
    global s
    assert s.count(a) == 1, (a[:90], s.count(a))
    s = s.replace(a, b)

replace('#include <vector>', '#include <vector>\n#include <regex>\n#include <set>\n#include <cstdint>')
replace('std::array<float, gx::kVertexFloats> v{}; v[0]',
        'std::array<float, gx::kVertexFloats> v{}; gx::full_vertex_defaults(v.data()); v[0]')
replace('        v[12] = .5f; v[13] = .5f;',
        '        // Omitted UV inputs retain the decoder canonical zero defaults.')

start = s.index('    WGPURenderPipeline pipeline(')
end = s.index('    void uniform(', start)
s = s[:start] + r'''
    // The shader text is identical for both routes. Locations come from its
    // actual VertexIn declaration, not a copied renderer-demand heuristic.
    WGPURenderPipeline pipeline(const gx::DrawPlan& p, unsigned mask, bool blend,
                                const std::string& label) {
        const auto& k=p.pipeline.shader;
        const auto code=gx::generate_wgsl(k);
        std::ofstream(label+".wgsl") << code;
        WGPUShaderSourceWGSL src=WGPU_SHADER_SOURCE_WGSL_INIT;src.code={code.data(),code.size()};
        WGPUShaderModuleDescriptor md=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;md.nextInChain=&src.chain;
        auto shader=wgpuDeviceCreateShaderModule(device,&md);CHECK(shader);
        const auto begin=code.find("struct VertexIn {");CHECK(begin!=std::string::npos);
        const auto finish=code.find("};",begin);CHECK(finish!=std::string::npos);
        const auto inputs=code.substr(begin,finish-begin);
        const std::regex input("@location\\(([0-9]+)\\)");
        const WGPUVertexFormat formats[gx::kVertexLocationCount]={
            WGPUVertexFormat_Float32x3,WGPUVertexFormat_Uint32,WGPUVertexFormat_Float32x4,WGPUVertexFormat_Float32x4,
            WGPUVertexFormat_Float32x2,WGPUVertexFormat_Float32x2,WGPUVertexFormat_Float32x2,WGPUVertexFormat_Float32x2,
            WGPUVertexFormat_Float32x3,WGPUVertexFormat_Uint32,WGPUVertexFormat_Float32x3,WGPUVertexFormat_Float32x3,
            WGPUVertexFormat_Float32x2,WGPUVertexFormat_Uint32};
        std::array<std::vector<WGPUVertexAttribute>,2> attrs;
        std::set<unsigned> seen;
        for(auto it=std::sregex_iterator(inputs.begin(),inputs.end(),input);it!=std::sregex_iterator();++it){
            const unsigned location=static_cast<unsigned>(std::stoul((*it)[1].str()));
            CHECK(location<gx::kVertexLocationCount);CHECK(seen.insert(location).second);
            const bool present=gx::vertex_location_present(mask,location);
            WGPUVertexAttribute a=WGPU_VERTEX_ATTRIBUTE_INIT;a.shaderLocation=location;a.format=formats[location];
            a.offset=present?gx::vertex_location_offset(mask,location):gx::kVertexLocationOffsets[location];
            CHECK(a.offset+gx::kVertexLocationBytes[location] <= (present?gx::vertex_stride_bytes(mask):gx::kVertexStrideBytes));
            attrs[present?0:1].push_back(a);
        }
        CHECK(seen.size()>=8);
        WGPUVertexBufferLayout vl[2]={WGPU_VERTEX_BUFFER_LAYOUT_INIT,WGPU_VERTEX_BUFFER_LAYOUT_INIT};
        vl[0].arrayStride=gx::vertex_stride_bytes(mask);vl[0].stepMode=WGPUVertexStepMode_Vertex;
        vl[1].arrayStride=gx::kVertexStrideBytes;vl[1].stepMode=WGPUVertexStepMode_Instance;
        for(unsigned i=0;i<2;++i){vl[i].attributeCount=attrs[i].size();vl[i].attributes=attrs[i].data();}
        WGPUBlendState bs=WGPU_BLEND_STATE_INIT;
        bs.color.srcFactor=k.use_dst_alpha?WGPUBlendFactor_Src1Alpha:WGPUBlendFactor_SrcAlpha;
        bs.color.dstFactor=k.use_dst_alpha?WGPUBlendFactor_OneMinusSrc1Alpha:WGPUBlendFactor_OneMinusSrcAlpha;
        bs.alpha.srcFactor=k.use_dst_alpha?WGPUBlendFactor_One:WGPUBlendFactor_SrcAlpha;
        bs.alpha.dstFactor=k.use_dst_alpha?WGPUBlendFactor_Zero:WGPUBlendFactor_OneMinusSrcAlpha;
        WGPUColorTargetState target=WGPU_COLOR_TARGET_STATE_INIT;target.format=WGPUTextureFormat_RGBA8Unorm;
        target.writeMask=WGPUColorWriteMask_All;if(blend)target.blend=&bs;
        WGPUFragmentState fs=WGPU_FRAGMENT_STATE_INIT;fs.module=shader;fs.entryPoint={"fs_main",7};fs.targetCount=1;fs.targets=&target;
        WGPUDepthStencilState ds=WGPU_DEPTH_STENCIL_STATE_INIT;ds.format=WGPUTextureFormat_Depth32Float;
        ds.depthWriteEnabled=(p.pipeline.depth_test && p.pipeline.depth_update)?WGPUOptionalBool_True:WGPUOptionalBool_False;
        ds.depthCompare=p.pipeline.depth_test?WGPUCompareFunction_Greater:WGPUCompareFunction_Always;
        WGPURenderPipelineDescriptor pd=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        pd.vertex.module=shader;pd.vertex.entryPoint={"vs_main",7};pd.vertex.bufferCount=attrs[1].empty()?1u:2u;pd.vertex.buffers=vl;
        pd.primitive.topology=WGPUPrimitiveTopology_TriangleList;pd.primitive.cullMode=WGPUCullMode_None;
        pd.depthStencil=&ds;pd.fragment=&fs;
        auto pipe=wgpuDeviceCreateRenderPipeline(device,&pd);wgpuShaderModuleRelease(shader);CHECK(pipe);return pipe;
    }
''' + s[end:]

start=s.index('    void textures(')
end=s.index('    Image render(',start)
s=s[:start]+r'''
    void textures(WGPURenderPassEncoder pass,WGPURenderPipeline pipe,unsigned group,const gx::ShaderKey& key) {
        auto layout=wgpuRenderPipelineGetBindGroupLayout(pipe,group);
        const unsigned mask=gx::used_texmap_mask(key);const bool multiple=gx::texmap_popcount(mask)>1;
        std::vector<WGPUBindGroupEntry> entries;
        for(unsigned t=0;t<8;++t){
            if(multiple && !(mask&(1u<<t)))continue;
            WGPUBindGroupEntry a=WGPU_BIND_GROUP_ENTRY_INIT;a.binding=multiple?2*t:0;a.textureView=texture_view;entries.push_back(a);
            WGPUBindGroupEntry b=WGPU_BIND_GROUP_ENTRY_INIT;b.binding=multiple?2*t+1:1;b.sampler=sampler;entries.push_back(b);
            if(!multiple)break;
        }
        WGPUBindGroupDescriptor d=WGPU_BIND_GROUP_DESCRIPTOR_INIT;d.layout=layout;d.entryCount=entries.size();d.entries=entries.data();
        auto bg=wgpuDeviceCreateBindGroup(device,&d);wgpuRenderPassEncoderSetBindGroup(pass,group,bg,0,nullptr);
        wgpuBindGroupRelease(bg);wgpuBindGroupLayoutRelease(layout);
    }
''' + s[end:]

replace('Image render(const Case& c,bool raw)', 'Image render(const Case& c,bool selective)')
replace('auto tex = wgpuDeviceCreateTexture(device,&td); auto tv = wgpuTextureCreateView(tex,nullptr);',
        'auto tex = wgpuDeviceCreateTexture(device,&td); auto tv = wgpuTextureCreateView(tex,nullptr);\n'
        '        td.format=WGPUTextureFormat_Depth32Float;auto ztex=wgpuDeviceCreateTexture(device,&td);auto zview=wgpuTextureCreateView(ztex,nullptr);')
replace('WGPURenderPassDescriptor rd = WGPU_RENDER_PASS_DESCRIPTOR_INIT; rd.colorAttachmentCount = 1; rd.colorAttachments = &ca;',
        'WGPURenderPassDepthStencilAttachment za=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;za.view=zview;za.depthLoadOp=WGPULoadOp_Clear;za.depthStoreOp=WGPUStoreOp_Store;za.depthClearValue=0.f;\n'
        '        WGPURenderPassDescriptor rd = WGPU_RENDER_PASS_DESCRIPTOR_INIT; rd.colorAttachmentCount = 1; rd.colorAttachments = &ca;rd.depthStencilAttachment=&za;')
replace('auto pass = wgpuCommandEncoderBeginRenderPass(encoder,&rd); unsigned ordinal = 0;',
        'auto pass = wgpuCommandEncoderBeginRenderPass(encoder,&rd); unsigned ordinal = 0;\n'
        '        std::array<float,gx::kVertexFloats> defaults{};gx::full_vertex_defaults(defaults.data());\n'
        '        auto default_buffer=buffer(defaults.data(),gx::kVertexStrideBytes,WGPUBufferUsage_Vertex);\n'
        '        wgpuRenderPassEncoderSetVertexBuffer(pass,1,default_buffer,0,gx::kVertexStrideBytes);')
start=s.index('            auto pipe = pipeline(p.pipeline.shader,raw,')
end=s.index('            wgpuRenderPassEncoderSetViewport(',start)
s=s[:start]+r'''
            const unsigned mask=selective?gx::selective_vertex_mask(p.pipeline.shader):0u;
            CHECK(gx::vertex_layout_valid(mask));
            auto pipe=pipeline(p,mask,draw.blend,c.name+(selective?"-selective-":"-full-")+std::to_string(ordinal++));
            wgpuRenderPassEncoderSetPipeline(pass,pipe);
            gx::DrawPlan packed=p;packed.vertex_layout_mask=mask;packed.vertex_floats=gx::vertex_stride_bytes(mask)/4;
            packed.vertices.assign(p.vertex_count*packed.vertex_floats,0.f);
            for(unsigned v=0;v<p.vertex_count;++v)for(unsigned loc=0;loc<gx::kVertexLocationCount;++loc){
                if(!gx::vertex_location_present(mask,loc))continue;
                std::memcpy(reinterpret_cast<unsigned char*>(packed.vertices.data()+v*packed.vertex_floats)+gx::vertex_location_offset(mask,loc),
                            reinterpret_cast<const unsigned char*>(p.vertices.data()+v*gx::kVertexFloats)+gx::kVertexLocationOffsets[loc],gx::kVertexLocationBytes[loc]);
            }
            // Independently retain the canonical source and require exact fallback
            // including integer matrix-index words and omitted attribute defaults.
            auto restored=packed;CHECK(gx::materialize_full_vertices(restored));
            CHECK(restored.vertices.size()==p.vertices.size());
            CHECK(std::memcmp(restored.vertices.data(),p.vertices.data(),p.vertices.size()*sizeof(float))==0);
            // Exercise nonzero baseVertex and an aligned nonzero bound offset.
            const unsigned base_vertex=3;
            const size_t prefix=12u+base_vertex*gx::vertex_stride_bytes(mask);
            std::vector<unsigned char> owned(prefix,0xCD);
            const auto* vertex_bytes=reinterpret_cast<const unsigned char*>(packed.vertices.data());
            owned.insert(owned.end(),vertex_bytes,vertex_bytes+packed.vertices.size()*sizeof(float));
            auto vb=buffer(owned.data(),owned.size(),WGPUBufferUsage_Vertex);
            auto ib=buffer(p.indices.data(),p.indices.size()*sizeof(uint16_t),WGPUBufferUsage_Index);
            wgpuRenderPassEncoderSetVertexBuffer(pass,0,vb,12u,owned.size()-12u);
            wgpuRenderPassEncoderSetIndexBuffer(pass,ib,WGPUIndexFormat_Uint16,0,p.indices.size()*sizeof(uint16_t));
            uniform(pass,pipe,1,&p.constants,sizeof p.constants);
            const bool ps=p.pipeline.shader.tev_valid||p.pipeline.shader.hud_tint;
            if(ps)uniform(pass,pipe,2,&p.pixel_constants,sizeof p.pixel_constants);
            if(p.pipeline.shader.textured)textures(pass,pipe,ps?3:2,p.pipeline.shader);
''' + s[end:]
replace('wgpuRenderPassEncoderDrawIndexed(pass,static_cast<unsigned>(p.indices.size()),1,0,0,raw ? 19u + ordinal * 3u : 0u);',
        'wgpuRenderPassEncoderDrawIndexed(pass,static_cast<unsigned>(p.indices.size()),1,0,base_vertex,0);')
replace('wgpuRenderPassEncoderEnd(pass); wgpuRenderPassEncoderRelease(pass);',
        'wgpuRenderPassEncoderEnd(pass); wgpuRenderPassEncoderRelease(pass);wgpuBufferRelease(default_buffer);')
replace('bd.size = W*H*4;', 'bd.size = W*H*8;')
replace('wgpuCommandEncoderCopyTextureToBuffer(encoder,&source,&dest,&extent);',
        'wgpuCommandEncoderCopyTextureToBuffer(encoder,&source,&dest,&extent);\n'
        '        source.texture=ztex;source.aspect=WGPUTextureAspect_DepthOnly;dest.layout.offset=W*H*4;\n'
        '        wgpuCommandEncoderCopyTextureToBuffer(encoder,&source,&dest,&extent);')
replace('wgpuBufferMapAsync(read,WGPUMapMode_Read,0,W*H*4,cb)', 'wgpuBufferMapAsync(read,WGPUMapMode_Read,0,W*H*8,cb)')
replace('wgpuBufferGetConstMappedRange(read,0,W*H*4)', 'wgpuBufferGetConstMappedRange(read,0,W*H*8)')
replace('Image result(bytes,bytes+W*H*4);', 'Image result(bytes,bytes+W*H*8);')
replace('wgpuTextureViewRelease(tv); wgpuTextureRelease(tex); clean_scope(); return result;',
        'wgpuTextureViewRelease(tv); wgpuTextureRelease(tex);wgpuTextureViewRelease(zview);wgpuTextureRelease(ztex); clean_scope(); return result;')
replace('static void verify(const std::string& name,const Image& actual,const Image& expected,unsigned tolerance=1)',
        'static void verify(const std::string& name,const Image& actual,const Image& expected,unsigned tolerance=0)')

# The original raw fixture only used color0/cached normals. Repair its canonical
# defaults before adding tests which deliberately consume every input location.
replace('for(const auto& point:points){std::array<float,gx::kVertexFloats> v{};v[0]',
        'for(const auto& point:points){std::array<float,gx::kVertexFloats> v{};gx::full_vertex_defaults(v.data());v[0]')
replace('    return out;\n}\nint main()', '    append_selective_cases(out);\n    return out;\n}\nint main()')
replace('static std::vector<Case> cases() {', 'static void append_selective_cases(std::vector<Case>&);\nstatic std::vector<Case> cases() {')
replace('const auto pulled=device.render(c,true);', 'const auto pulled=device.render(c,true);')
replace('verify(c.name+"-gpu-vs-decoded",pulled,legacy,0);', 'verify(c.name+"-selective-vs-full-color-and-depth",pulled,legacy,0);')
replace('CHECK(legacy != clear_image());++accepted;',
        'const auto clear=clear_image();CHECK(!std::equal(clear.begin(),clear.end(),legacy.begin()));++accepted;')
replace('"Offscreen D3D12 GPU BE-f32 pull: %u cases/%u frames, %u checks PASS\\n"',
        '"Offscreen D3D12 selective vertices: %u cases/%u frames, %u checks PASS\\n"')

extra=(OUT/'cases.cpp.inc').read_text(encoding='utf-8')
s=s.replace('int main() {', extra+'\nint main() {')
target=OUT/'compact_pixels_test.cpp'
target.write_text(s,encoding='utf-8',newline='\n')
manifest={
    'status':'PREPARED_NOT_COMPILED_OR_RUN',
    'authored_parent':{'path':str(BASE),'sha256':BASE_SHA},
    'fixture':{'path':str(target),'sha256':hashlib.sha256(target.read_bytes()).hexdigest()},
    'source_only':True,'game_window_surface_or_input':False,
    'comparison':'same generated WGSL; canonical/full vs selective+default vertex streams; exact RGBA8 and Depth32Float bytes',
    'limits':['manual synthetic canonical vertices use production layout helpers; real decoder/renderer integration requires separate qualification',
              'pipeline input demand is parsed from actual generated shader; backend factory is separately source-audited',
              'no performance conclusions from fixture throughput'],
}
(OUT/'preparation.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print(json.dumps(manifest,indent=2))
