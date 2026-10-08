// Synthetic rectangles/textures only. Pure Dawn C API: no SDL/window/surface/game.
#include <dawn/webgpu.h>
#include <gxruntime/gxcore/shader.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <regex>
#include <set>
#include <cstdint>

namespace gx = gxruntime::gxcore;
static constexpr unsigned W = 64, H = 64;
using Pixel = std::array<unsigned char, 4>;
using Image = std::vector<unsigned char>;
static const Pixel background{16, 32, 48, 64};
static std::atomic<unsigned> errors{0};
static unsigned checks = 0, frames = 0;
static void fail(const char* m) { std::fprintf(stderr, "FAIL: %s\n", m); std::exit(1); }
#define CHECK(x) do { ++checks; if (!(x)) fail(#x); } while (0)
static std::string view(WGPUStringView s) {
    return s.data ? std::string(s.data, s.length == WGPU_STRLEN ? std::strlen(s.data) : s.length) : std::string();
}
static void wait(WGPUInstance i, WGPUFuture f) {
    WGPUFutureWaitInfo item = WGPU_FUTURE_WAIT_INFO_INIT; item.future = f;
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!item.completed) {
        auto s = wgpuInstanceWaitAny(i, 1, &item, 0);
        CHECK(s == WGPUWaitStatus_Success || s == WGPUWaitStatus_TimedOut);
        if (std::chrono::steady_clock::now() > until) fail("Dawn callback timeout");
        if (!item.completed) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
static Image clear_image() {
    Image p(W * H * 4);
    for (size_t n = 0; n < p.size(); n += 4) std::copy(background.begin(), background.end(), p.begin() + n);
    return p;
}
static void paint(Image& p, int x0, int y0, int x1, int y1, Pixel c) {
    for (int y = std::max(y0, 0); y < std::min(y1, int(H)); ++y)
        for (int x = std::max(x0, 0); x < std::min(x1, int(W)); ++x)
            std::copy(c.begin(), c.end(), p.begin() + (y * W + x) * 4);
}
static Pixel multiply(Pixel c, const std::array<float, 4>& m) {
    for (unsigned i = 0; i < 4; ++i) c[i] = static_cast<unsigned char>(std::lround(c[i] * m[i]));
    return c;
}
static Pixel blended(Pixel c, Pixel dst, bool forced_alpha) {
    const float alpha = c[3] / 255.f;
    Pixel r;
    for (unsigned i = 0; i < 3; ++i) r[i] = static_cast<unsigned char>(std::lround(c[i] * alpha + dst[i] * (1.f - alpha)));
    r[3] = forced_alpha ? 91 : static_cast<unsigned char>(std::lround(c[3] * alpha + dst[3] * (1.f - alpha)));
    return r;
}
static gx::DrawPlan plan(int x0 = 8, int y0 = 12, int x1 = 24, int y1 = 28, Pixel color = {200,100,50,192}) {
    gx::DrawPlan p; p.ok = true; p.constants_id = 735;
    p.pipeline.shader.has_color0 = 1; p.pipeline.shader.num_color_chans = 1;
    for (unsigned i = 0; i < 3; ++i) p.constants.posnormalmatrix[i][i] = 1;
    p.constants.projection[0][0] = 2.f / W; p.constants.projection[0][3] = -1;
    p.constants.projection[1][1] = -2.f / H; p.constants.projection[1][3] = 1;
    p.constants.projection[2][2] = 1; p.constants.projection[3][3] = 1;
    p.viewport_valid = true; p.viewport[0] = W / 2.f; p.viewport[1] = -static_cast<float>(H) / 2.f;
    p.viewport[2] = 16777215; p.viewport[3] = 342 + W / 2.f; p.viewport[4] = 342 + H / 2.f; p.viewport[5] = 16777215;
    p.scissor_valid = true; p.scissor_width = W; p.scissor_height = H;
    p.vertex_count = 4;
    for (auto point : {std::array<float,2>{float(x0),float(y0)}, {float(x1),float(y0)}, {float(x1),float(y1)}, {float(x0),float(y1)}}) {
        std::array<float, gx::kVertexFloats> v{}; gx::full_vertex_defaults(v.data()); v[0] = point[0]; v[1] = point[1]; v[2] = -.5f;
        for (unsigned i = 0; i < 4; ++i) v[4+i] = color[i] / 255.f;
        // Omitted UV inputs retain the decoder canonical zero defaults.
        p.vertices.insert(p.vertices.end(), v.begin(), v.end());
    }
    p.indices = {0,1,2,2,3,0};
    return p;
}
static void tev_color(gx::DrawPlan& p, Pixel c) {
    auto& k = p.pipeline.shader; k.tev_valid = 1; k.num_tev_stages = 1;
    auto& s = k.tev_stages[0]; s.cc_a = s.cc_b = s.cc_c = 15; s.cc_d = 2;
    s.ac_a = s.ac_b = s.ac_c = 7; s.ac_d = 1; s.cc_clamp = s.ac_clamp = 1;
    for (unsigned i = 0; i < 4; ++i) p.pixel_constants.colors[1][i] = c[i];
}
static void textured(gx::DrawPlan& p, bool tev) {
    auto& k = p.pipeline.shader; k.textured = 1; k.num_tex_gens = 1; k.uv_mask = 1;
    k.tex_gens[0].enabled = 1; k.tex_gens[0].sourcerow = static_cast<unsigned char>(gx::TexSourceRow::Tex0);
    p.constants.texmatrices[0][0] = 1; p.constants.texmatrices[1][1] = 1;
    p.has_texture = true; p.tex_width = p.tex_height = 1;
    if (tev) {
        tev_color(p, {0,0,0,0}); auto& s = k.tev_stages[0];
        s.cc_d = 8; s.ac_d = 4; s.tevorders_enable = 1;
        for (unsigned i = 0; i < 4; ++i) s.tex_swap[i] = s.ras_swap[i] = static_cast<unsigned char>(i);
    }
}
struct Draw { gx::DrawPlan p; bool blend = false; bool suppressed = false; };
struct Case { std::string name; std::vector<Draw> draws; Image expected; };

struct Device {
    WGPUInstance instance{}; WGPUAdapter adapter{}; WGPUDevice device{}; WGPUQueue queue{};
    WGPUTexture texture{}; WGPUTextureView texture_view{}; WGPUSampler sampler{};
    Device() {
        WGPUInstanceDescriptor id = WGPU_INSTANCE_DESCRIPTOR_INIT; instance = wgpuCreateInstance(&id); CHECK(instance);
        WGPURequestAdapterOptions o = WGPU_REQUEST_ADAPTER_OPTIONS_INIT; o.backendType = WGPUBackendType_D3D12;
        o.powerPreference = WGPUPowerPreference_LowPower;
        WGPURequestAdapterCallbackInfo cb = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT; cb.mode = WGPUCallbackMode_WaitAnyOnly; cb.userdata1 = &adapter;
        cb.callback = [](WGPURequestAdapterStatus status,WGPUAdapter a,WGPUStringView m,void* v,void*) {
            if (status != WGPURequestAdapterStatus_Success) { std::fprintf(stderr,"Adapter: %s\n",view(m).c_str()); return; }
            *static_cast<WGPUAdapter*>(v) = a;
        };
        wait(instance,wgpuInstanceRequestAdapter(instance,&o,cb)); CHECK(adapter);
        WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT; CHECK(wgpuAdapterGetInfo(adapter,&info) == WGPUStatus_Success);
        std::printf("D3D12 adapter: %s / %s, type=%u\n",view(info.device).c_str(),view(info.description).c_str(),info.adapterType);
        wgpuAdapterInfoFreeMembers(info);
        WGPUFeatureName dual = WGPUFeatureName_DualSourceBlending; CHECK(wgpuAdapterHasFeature(adapter,dual));
        WGPUDeviceDescriptor dd = WGPU_DEVICE_DESCRIPTOR_INIT; dd.requiredFeatureCount = 1; dd.requiredFeatures = &dual;
        dd.uncapturedErrorCallbackInfo.callback = [](WGPUDevice const*,WGPUErrorType,WGPUStringView m,void*,void*) { ++errors; std::fprintf(stderr,"Dawn: %s\n",view(m).c_str()); };
        device = wgpuAdapterCreateDevice(adapter,&dd); CHECK(device); queue = wgpuDeviceGetQueue(device);
        WGPUTextureDescriptor td = WGPU_TEXTURE_DESCRIPTOR_INIT; td.size = {2,2,1}; td.format = WGPUTextureFormat_RGBA8Unorm;
        td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        texture = wgpuDeviceCreateTexture(device,&td); texture_view = wgpuTextureCreateView(texture,nullptr);
        const std::array<unsigned char,16> sample{120,180,240,160, 255,0,0,255, 0,255,0,255, 0,0,255,255}; WGPUTexelCopyTextureInfo dest = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT; dest.texture = texture;
        WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT; layout.bytesPerRow = 8; layout.rowsPerImage = 2;
        WGPUExtent3D extent{2,2,1}; wgpuQueueWriteTexture(queue,&dest,sample.data(),sample.size(),&layout,&extent);
        WGPUSamplerDescriptor sd = WGPU_SAMPLER_DESCRIPTOR_INIT; sd.magFilter = sd.minFilter = WGPUFilterMode_Nearest;
        sampler = wgpuDeviceCreateSampler(device,&sd);
    }
    ~Device() {
        wgpuSamplerRelease(sampler); wgpuTextureViewRelease(texture_view); wgpuTextureRelease(texture); wgpuQueueRelease(queue);
        wgpuDeviceDestroy(device); wgpuDeviceRelease(device); wgpuAdapterRelease(adapter); wgpuInstanceRelease(instance);
    }
    void clean_scope() {
        bool good = false; WGPUPopErrorScopeCallbackInfo cb = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT; cb.mode = WGPUCallbackMode_WaitAnyOnly; cb.userdata1 = &good;
        cb.callback = [](WGPUPopErrorScopeStatus s,WGPUErrorType t,WGPUStringView m,void* p,void*) {
            *static_cast<bool*>(p) = s == WGPUPopErrorScopeStatus_Success && t == WGPUErrorType_NoError;
            if (!*static_cast<bool*>(p)) std::fprintf(stderr,"Validation: %s\n",view(m).c_str());
        };
        wait(instance,wgpuDevicePopErrorScope(device,cb)); CHECK(good); CHECK(errors == 0);
    }
    WGPUBuffer buffer(const void* bytes,size_t size,WGPUBufferUsage usage) {
        WGPUBufferDescriptor d = WGPU_BUFFER_DESCRIPTOR_INIT; d.size = (size+3)&~size_t(3); d.usage = usage | WGPUBufferUsage_CopyDst;
        auto b = wgpuDeviceCreateBuffer(device,&d); CHECK(b); wgpuQueueWriteBuffer(queue,b,0,bytes,size); return b;
    }

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
    void uniform(WGPURenderPassEncoder pass,WGPURenderPipeline pipe,unsigned group,const void* bytes,size_t size) {
        auto b = buffer(bytes,size,WGPUBufferUsage_Uniform); auto layout = wgpuRenderPipelineGetBindGroupLayout(pipe,group);
        WGPUBindGroupEntry e = WGPU_BIND_GROUP_ENTRY_INIT; e.binding = 0; e.buffer = b; e.size = size;
        WGPUBindGroupDescriptor d = WGPU_BIND_GROUP_DESCRIPTOR_INIT; d.layout = layout; d.entryCount = 1; d.entries = &e;
        auto bg = wgpuDeviceCreateBindGroup(device,&d); wgpuRenderPassEncoderSetBindGroup(pass,group,bg,0,nullptr);
        wgpuBindGroupRelease(bg); wgpuBindGroupLayoutRelease(layout); wgpuBufferRelease(b);
    }

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
    Image render(const Case& c,bool selective) {
        wgpuDevicePushErrorScope(device,WGPUErrorFilter_Validation);
        WGPUTextureDescriptor td = WGPU_TEXTURE_DESCRIPTOR_INIT; td.size = {W,H,1}; td.format = WGPUTextureFormat_RGBA8Unorm;
        td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
        auto tex = wgpuDeviceCreateTexture(device,&td); auto tv = wgpuTextureCreateView(tex,nullptr);
        td.format=WGPUTextureFormat_Depth32Float;auto ztex=wgpuDeviceCreateTexture(device,&td);auto zview=wgpuTextureCreateView(ztex,nullptr);
        WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT; auto encoder = wgpuDeviceCreateCommandEncoder(device,&ed);
        WGPURenderPassColorAttachment ca = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT; ca.view = tv; ca.loadOp = WGPULoadOp_Clear; ca.storeOp = WGPUStoreOp_Store;
        ca.clearValue = {background[0]/255.,background[1]/255.,background[2]/255.,background[3]/255.};
        WGPURenderPassDepthStencilAttachment za=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;za.view=zview;za.depthLoadOp=WGPULoadOp_Clear;za.depthStoreOp=WGPUStoreOp_Store;za.depthClearValue=0.f;
        WGPURenderPassDescriptor rd = WGPU_RENDER_PASS_DESCRIPTOR_INIT; rd.colorAttachmentCount = 1; rd.colorAttachments = &ca;rd.depthStencilAttachment=&za;
        auto pass = wgpuCommandEncoderBeginRenderPass(encoder,&rd); unsigned ordinal = 0;
        std::array<float,gx::kVertexFloats> defaults{};gx::full_vertex_defaults(defaults.data());
        auto default_buffer=buffer(defaults.data(),gx::kVertexStrideBytes,WGPUBufferUsage_Vertex);
        wgpuRenderPassEncoderSetVertexBuffer(pass,1,default_buffer,0,gx::kVertexStrideBytes);
        for(const auto& draw:c.draws) {
            const auto& p = draw.p; if(draw.suppressed)continue;

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
            wgpuRenderPassEncoderSetViewport(pass,0,0,W,H,0,1);
            CHECK(p.scissor_x >= 0 && p.scissor_y >= 0 && p.scissor_x+p.scissor_width <= int(W) && p.scissor_y+p.scissor_height <= int(H));
            wgpuRenderPassEncoderSetScissorRect(pass,p.scissor_x,p.scissor_y,p.scissor_width,p.scissor_height);
            wgpuRenderPassEncoderDrawIndexed(pass,static_cast<unsigned>(p.indices.size()),1,0,base_vertex,0);
            wgpuBufferRelease(vb); wgpuBufferRelease(ib); wgpuRenderPipelineRelease(pipe);
        }
        wgpuRenderPassEncoderEnd(pass); wgpuRenderPassEncoderRelease(pass);wgpuBufferRelease(default_buffer);
        WGPUBufferDescriptor bd = WGPU_BUFFER_DESCRIPTOR_INIT; bd.size = W*H*8; bd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
        auto read = wgpuDeviceCreateBuffer(device,&bd);
        WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT; source.texture = tex;
        WGPUTexelCopyBufferInfo dest = WGPU_TEXEL_COPY_BUFFER_INFO_INIT; dest.buffer = read; dest.layout.bytesPerRow = W*4; dest.layout.rowsPerImage = H;
        WGPUExtent3D extent{W,H,1}; wgpuCommandEncoderCopyTextureToBuffer(encoder,&source,&dest,&extent);
        source.texture=ztex;source.aspect=WGPUTextureAspect_DepthOnly;dest.layout.offset=W*H*4;
        wgpuCommandEncoderCopyTextureToBuffer(encoder,&source,&dest,&extent);
        WGPUCommandBufferDescriptor cd = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT; auto command = wgpuCommandEncoderFinish(encoder,&cd);
        wgpuQueueSubmit(queue,1,&command); ++frames;
        bool mapped = false; WGPUBufferMapCallbackInfo cb = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT; cb.mode = WGPUCallbackMode_WaitAnyOnly; cb.userdata1 = &mapped;
        cb.callback = [](WGPUMapAsyncStatus s,WGPUStringView m,void* v,void*) { *static_cast<bool*>(v)=s==WGPUMapAsyncStatus_Success; if(s!=WGPUMapAsyncStatus_Success)std::fprintf(stderr,"Map: %s\n",view(m).c_str()); };
        wait(instance,wgpuBufferMapAsync(read,WGPUMapMode_Read,0,W*H*8,cb)); CHECK(mapped);
        auto bytes = static_cast<const unsigned char*>(wgpuBufferGetConstMappedRange(read,0,W*H*8)); CHECK(bytes);
        Image result(bytes,bytes+W*H*8); wgpuBufferUnmap(read);
        wgpuCommandBufferRelease(command); wgpuCommandEncoderRelease(encoder); wgpuBufferRelease(read);
        wgpuTextureViewRelease(tv); wgpuTextureRelease(tex);wgpuTextureViewRelease(zview);wgpuTextureRelease(ztex); clean_scope(); return result;
    }
};
static void verify(const std::string& name,const Image& actual,const Image& expected,unsigned tolerance=0) {
    CHECK(actual.size()==expected.size());
    std::ofstream file(name+".rgba",std::ios::binary); file.write(reinterpret_cast<const char*>(actual.data()),actual.size()); file.close();
    unsigned mismatches = 0, max_error = 0;
    for(size_t i=0;i<actual.size();++i) {
        ++checks; unsigned delta = static_cast<unsigned>(std::abs(int(actual[i])-int(expected[i]))); max_error = std::max(max_error,delta);
        if(delta>tolerance) { if(mismatches<4)std::fprintf(stderr,"%s x=%zu y=%zu c=%zu got=%u expected=%u\n",name.c_str(),(i/4)%W,(i/4)/W,i%4,actual[i],expected[i]); ++mismatches; }
    }
    std::printf("%s: max-channel-error=%u mismatches=%u\n",name.c_str(),max_error,mismatches); std::fflush(stdout);
    CHECK(mismatches==0);
}

static gx::DrawPlan raw_plan(int x0=8,int y0=12,int x1=24,int y1=28) {
    auto p=plan(x0,y0,x1,y1,{255,255,255,255});
    p.pipeline.shader.has_color0=0;
    p.constants.cached_normal[0]=.25f;p.constants.cached_normal[1]=.5f;p.constants.cached_normal[2]=1.f;
    p.constants.cached_tangent[0]=1.f;p.constants.cached_binormal[1]=1.f;
    return p;
}
static void texture_uvs(gx::DrawPlan& p) {
    const float uv[4][2]={{-.125f,-.25f},{1.125f,-.25f},{1.125f,1.25f},{-.125f,1.25f}};
    for(unsigned v=0;v<p.vertex_count;++v) {
        p.vertices[v*gx::kVertexFloats+12]=uv[v%4][0];
        p.vertices[v*gx::kVertexFloats+13]=uv[v%4][1];
    }
}
static void append_selective_cases(std::vector<Case>&);
static std::vector<Case> cases() {
    std::vector<Case> out;
    auto add=[&](std::string name,gx::DrawPlan p,bool blend=false){out.push_back({name,{{p,blend,false}}, {}});};
    auto p=raw_plan();add("quad-white",p);
    p=raw_plan();tev_color(p,{200,100,50,192});add("quad-register-TEV",p);
    add("quad-TEV-alpha-blend",p,true);
    p.pipeline.shader.use_dst_alpha=1;p.pipeline.shader.dst_alpha=91;add("quad-destination-alpha",p,true);
    p=raw_plan();textured(p,false);texture_uvs(p);add("quad-variable-UV",p);
    p=raw_plan();textured(p,true);texture_uvs(p);add("quad-variable-UV-TEV",p);
    p=raw_plan();p.scissor_x=10;p.scissor_y=14;p.scissor_width=10;p.scissor_height=10;add("bounded-scissor",p);
    p=raw_plan();p.constants.posnormalmatrix[0][3]=7.125f;p.constants.posnormalmatrix[1][3]=-3.25f;add("fractional-transform",p);
    p=raw_plan();p.constants.projection[3][2]=-.125f;add("perspective-transform",p);
    p=raw_plan();textured(p,false);texture_uvs(p);
    p.pipeline.shader.tex_gens[0].sourcerow=static_cast<unsigned char>(gx::TexSourceRow::Geom);
    p.constants.texmatrices[0][0]=.05f;p.constants.texmatrices[1][1]=.05f;add("geometry-texcoord",p);
    p=raw_plan();textured(p,false);texture_uvs(p);
    p.pipeline.shader.tex_gens[0].sourcerow=static_cast<unsigned char>(gx::TexSourceRow::Normal);
    p.constants.texmatrices[0][0]=1;p.constants.texmatrices[1][1]=1;add("cached-normal-texcoord",p);
    // Six original strip vertices with the renderer's exact alternating winding.
    p=raw_plan();p.vertices.clear();p.vertex_count=6;
    const float points[6][2]={{7.25f,9.5f},{10.5f,25.25f},{23.75f,7.25f},{25.25f,28.5f},{43.5f,12.25f},{46.75f,32.75f}};
    for(const auto& point:points){std::array<float,gx::kVertexFloats> v{};gx::full_vertex_defaults(v.data());v[0]=point[0];v[1]=point[1];v[2]=-.5f;for(unsigned c=4;c<12;++c)v[c]=1;v[12]=point[0]/64;v[13]=point[1]/64;p.vertices.insert(p.vertices.end(),v.begin(),v.end());}
    p.indices={0,1,2,2,1,3,2,3,4,4,3,5};textured(p,true);add("six-vertex-stripe",p);
    auto a=raw_plan();tev_color(a,{220,80,40,180});auto b=raw_plan(18,20,42,44);textured(b,true);texture_uvs(b);
    out.push_back({"ordered-state-and-overlap",{{a,true,false},{b,true,false}}, {}});
    append_selective_cases(out);
    return out;
}
static float* field(gx::DrawPlan& p,unsigned vertex,unsigned location) {
    CHECK(vertex<p.vertex_count && location<gx::kVertexLocationCount);
    return p.vertices.data()+vertex*gx::kVertexFloats+gx::kVertexLocationOffsets[location]/4;
}
static void words(gx::DrawPlan& p,unsigned location,uint32_t value) {
    for(unsigned v=0;v<p.vertex_count;++v)std::memcpy(field(p,v,location),&value,4);
}
static void init_normal_matrix(gx::DrawPlan& p) {
    for(unsigned i=0;i<3;++i)p.constants.posnormalmatrix[3+i][i]=1;
    p.constants.cached_normal[2]=1;p.constants.cached_tangent[0]=1;p.constants.cached_binormal[1]=1;
}
static void vertex_lighting(gx::DrawPlan& p,bool normal) {
    auto& k=p.pipeline.shader;k.lit_valid=1;k.has_vertex_normal=normal;k.chan_captured_mask=5;
    k.litchan[0].enablelighting=1;k.litchan[0].matsource=1;k.litchan[0].ambsource=0;
    k.litchan[0].light_mask=1;k.litchan[0].diffusefunc=2;k.litchan[0].attnfunc=2;
    k.litchan[2].matsource=1;
    for(unsigned i=0;i<4;++i){p.constants.materials[0][i]=16;p.constants.lights[0].color[i]=int(220-20*i);}
    p.constants.lights[0].dir[2]=1;p.constants.lights[0].pos[2]=20;
    init_normal_matrix(p);
    if(normal)for(unsigned v=0;v<p.vertex_count;++v){auto* n=field(p,v,8);n[0]=.15f*float(v);n[1]=.1f;n[2]=1;}
}
static void all_texgens(gx::DrawPlan& p,unsigned slot) {
    auto& k=p.pipeline.shader; k.num_tex_gens=5;k.uv_mask=static_cast<uint8_t>(1u<<slot);
    k.textured=1;k.tev_valid=1;k.num_tev_stages=1;
    auto& st=k.tev_stages[0];st.cc_a=st.cc_b=st.cc_c=15;st.cc_d=8;st.ac_a=st.ac_b=st.ac_c=7;st.ac_d=4;
    st.cc_clamp=st.ac_clamp=1;st.tevorders_enable=1;st.tevorders_texcoord=static_cast<uint8_t>(slot);
    for(unsigned c=0;c<4;++c)st.tex_swap[c]=st.ras_swap[c]=static_cast<uint8_t>(c);
    for(unsigned i=0;i<5;++i){
        k.tex_gens[i].enabled=1;k.tex_gens[i].sourcerow=static_cast<uint8_t>(gx::TexSourceRow::Tex0)+static_cast<uint8_t>(i);
        p.constants.texmatrices[3*i][0]=1;p.constants.texmatrices[3*i+1][1]=1;
    }
    const float uv[4][2]={{-.125f,-.25f},{1.125f,-.25f},{1.125f,1.25f},{-.125f,1.25f}};
    const unsigned location=slot==4?12:4+slot;
    for(unsigned v=0;v<p.vertex_count;++v){auto* f=field(p,v,location);f[0]=uv[v%4][0];f[1]=uv[v%4][1];}
}
static void append_selective_cases(std::vector<Case>& out) {
    auto add=[&](std::string name,gx::DrawPlan p,bool blend=false){out.push_back({name,{{p,blend,false}}, {}});};
    auto p=plan();
    const float colors[4][4]={{1,.2f,.1f,.25f},{.1f,1,.2f,.5f},{.2f,.1f,1,.75f},{1,1,.1f,1}};
    for(unsigned v=0;v<p.vertex_count;++v)std::copy(colors[v],colors[v]+4,field(p,v,2));
    add("interpolated-color0-and-alpha",p,true);

    p=plan();p.pipeline.shader.has_color0=0;p.pipeline.shader.has_color1=1;
    for(unsigned v=0;v<p.vertex_count;++v){std::fill(field(p,v,2),field(p,v,2)+4,1.f);std::copy(colors[v],colors[v]+4,field(p,v,3));}
    add("color1-only-fallback-to-channel0",p,true);
    p=plan();p.pipeline.shader.has_color1=1;p.pipeline.shader.num_color_chans=2;
    for(unsigned v=0;v<p.vertex_count;++v)std::copy(colors[v],colors[v]+4,field(p,v,3));
    tev_color(p,{0,0,0,0});p.pipeline.shader.tev_stages[0].cc_d=10;p.pipeline.shader.tev_stages[0].ac_d=5;
    p.pipeline.shader.tev_stages[0].tevorders_colorchan=1;add("both-colors-TEV-raster1",p,true);

    for(unsigned uv=0;uv<5;++uv){p=raw_plan();all_texgens(p,uv);add("isolated-uv"+std::to_string(uv),p);}
    p=raw_plan();all_texgens(p,4);p.pipeline.shader.tex_gens[4].projection=1;
    p.constants.texmatrices[14][2]=.75f;add("uv4-STQ-projective",p);

    p=plan();vertex_lighting(p,true);add("vertex-normal-lighting",p);
    p=plan();vertex_lighting(p,false);add("cached-normal-lighting",p);
    p=raw_plan();textured(p,true);p.pipeline.shader.has_vertex_normal=1;
    p.pipeline.shader.tex_gens[0].sourcerow=static_cast<uint8_t>(gx::TexSourceRow::Normal);
    p.constants.texmatrices[0][0]=1;p.constants.texmatrices[1][1]=1;
    for(unsigned v=0;v<p.vertex_count;++v){auto* f=field(p,v,8);f[0]=.2f+.15f*float(v);f[1]=.6f;f[2]=1;}
    add("vertex-normal-texgen",p);

    p=plan();p.pipeline.shader.has_pos_mtx_idx=1;
    for(unsigned base: {0u,3u})for(unsigned c=0;c<3;++c)p.constants.transformmatrices[base+c][c]=1;
    p.constants.transformmatrices[3][3]=4.5f;
    for(unsigned v=0;v<p.vertex_count;++v){uint32_t row=v&1?3u:0u;std::memcpy(field(p,v,1),&row,4);}
    add("varying-position-matrix-rows",p);

    p=raw_plan();all_texgens(p,0);p.pipeline.shader.has_tex_mtx_idx=1;p.pipeline.shader.tex_mtx_idx_mask=0x0f;
    words(p,9,0x120c0600u);words(p,13,0x241e1803u);
    for(unsigned base: {0u,6u,12u,18u}){p.constants.transformmatrices[base][0]=1;p.constants.transformmatrices[base+1][1]=1;}
    add("packed-low-matrix-and-undeclared-high-word",p);
    p=raw_plan();all_texgens(p,4);p.pipeline.shader.has_tex_mtx_idx=1;p.pipeline.shader.tex_mtx_idx_mask=0xf0;
    // TEX5..7 bytes are retained even though the five-texgen shader consumes only TEX4.
    words(p,9,0x120c0600u);words(p,13,0x241e1803u);
    p.constants.transformmatrices[3][0]=1;p.constants.transformmatrices[4][1]=1;
    add("packed-high-texture-matrix-word-and-unused-upper-bytes",p);

    // Physical TEXMTXIDX5..7 survive canonical materialization even though
    // none of the five supported texgens uses a per-vertex matrix index.
    for(unsigned physical=5;physical<8;++physical){
        p=plan();p.pipeline.shader.has_tex_mtx_idx=1;p.pipeline.shader.tex_mtx_idx_mask=0;
        words(p,13,(6u*(physical-4u))<<(8u*(physical-4u)));
        add("physical-TEXMTXIDX"+std::to_string(physical)+"-only-metadata",p);
    }

    for(bool nbt:{false,true}) {
        p=raw_plan();textured(p,true);texture_uvs(p);init_normal_matrix(p);
        p.pipeline.shader.num_tex_gens=2;p.pipeline.shader.tex_gens[1].enabled=1;
        p.pipeline.shader.tex_gens[1].texgentype=static_cast<uint8_t>(gx::TexGenType::EmbossMap);
        p.pipeline.shader.tex_gens[1].embosssourceshift=0;p.pipeline.shader.tex_gens[1].embosslightshift=0;
        p.pipeline.shader.tev_stages[0].tevorders_texcoord=1;
        p.constants.lights[0].pos[0]=20;p.constants.lights[0].pos[1]=14;p.constants.lights[0].pos[2]=10;
        if(nbt){
            p.pipeline.shader.has_vertex_normal=1;p.pipeline.shader.has_vertex_binormal=1;p.pipeline.shader.has_vertex_tangent=1;
            for(unsigned v=0;v<p.vertex_count;++v){field(p,v,8)[2]=1;field(p,v,10)[1]=1;field(p,v,11)[0]=1;}
        }
        add(nbt?"vertex-NBT-emboss":"cached-NBT-emboss",p);
    }

    p=raw_plan();textured(p,true);texture_uvs(p);p.pipeline.shader.num_tev_stages=3;
    for(unsigned i=1;i<3;++i)p.pipeline.shader.tev_stages[i]=p.pipeline.shader.tev_stages[0];
    p.pipeline.shader.tev_stages[1].tevorders_texmap=3;p.pipeline.shader.tev_stages[2].tevorders_texmap=6;
    add("multi-texture-TEV-sparse-bindings-0-3-6",p);

    p=plan();tev_color(p,{230,80,120,220});p.pipeline.shader.alpha_comp0=static_cast<uint8_t>(gx::CompareMode::Greater);
    p.pipeline.shader.alpha_comp1=static_cast<uint8_t>(gx::CompareMode::Always);p.pixel_constants.alpha_ref[0]=120;
    add("TEV-alpha-test-pass",p);
    auto under=plan(6,7,40,43,{70,190,230,180});auto rejected=p;
    rejected.pixel_constants.colors[1][3]=80;
    for(unsigned v=0;v<under.vertex_count;++v){field(under,v,0)[2]=-.25f;field(rejected,v,0)[2]=-.75f;}
    for(auto* q:{&under,&rejected}){q->pipeline.depth_test=1;q->pipeline.depth_update=1;q->pipeline.depth_func=static_cast<uint8_t>(gx::CompareMode::Greater);}
    out.push_back({"TEV-alpha-rejection-preserves-underlay-and-depth",{{under,false,false},{rejected,false,false}}, {}});
    p.pipeline.shader.hud_tint=1;p.pixel_constants.hud_multiplier[0]=.5f;p.pixel_constants.hud_multiplier[2]=.25f;
    add("TEV-native-hud-multiplier",p,true);
    p.pipeline.shader.hud_tint=0;p.pipeline.shader.fog_fsel=static_cast<uint8_t>(gx::FogType::Linear);
    p.pipeline.shader.fog_proj=static_cast<uint8_t>(gx::FogProjection::Orthographic);
    p.pixel_constants.fogcolor[0]=12;p.pixel_constants.fogcolor[1]=220;p.pixel_constants.fogcolor[2]=88;
    p.pixel_constants.fogf[0]=1;p.pixel_constants.fogf[1]=.1f;add("TEV-linear-fog",p);

    auto far=plan(6,7,40,43,{220,80,40,200});auto near=plan(15,16,52,54,{50,210,90,160});
    for(unsigned v=0;v<far.vertex_count;++v){field(far,v,0)[2]=-.25f;field(near,v,0)[2]=-.75f;}
    for(auto* q:{&far,&near}){q->pipeline.depth_test=1;q->pipeline.depth_update=1;q->pipeline.depth_func=static_cast<uint8_t>(gx::CompareMode::Greater);}
    out.push_back({"reversed-Z-overlap",{{far,false,false},{near,false,false},{far,false,false}}, {}});
    near.scissor_x=20;near.scissor_y=24;near.scissor_width=18;near.scissor_height=17;
    out.push_back({"reversed-Z-scissor-alpha",{{far,true,false},{near,true,false}}, {}});

    p=raw_plan();textured(p,true);texture_uvs(p);p.pipeline.shader.ztex_op=1;p.pipeline.shader.ztex_type=0;
    p.pixel_constants.zbias[3]=100;p.pipeline.depth_test=1;p.pipeline.depth_update=1;add("late-Z-texture",p);

    // Every location present normalizes to the full-layout sentinel; this must
    // keep the original one-stream input and fallback behavior available.
    p=plan();all_texgens(p,4);p.pipeline.shader.has_color1=1;p.pipeline.shader.uv_mask=31;
    p.pipeline.shader.has_tex_mtx_idx=1;p.pipeline.shader.tex_mtx_idx_mask=31;
    p.pipeline.shader.has_vertex_normal=1;p.pipeline.shader.has_vertex_binormal=1;p.pipeline.shader.has_vertex_tangent=1;
    p.pipeline.shader.num_color_chans=2;
    for(unsigned v=0;v<p.vertex_count;++v){field(p,v,8)[2]=1;field(p,v,10)[1]=1;field(p,v,11)[0]=1;}
    p.constants.transformmatrices[0][0]=1;p.constants.transformmatrices[1][1]=1;
    CHECK(gx::selective_vertex_mask(p.pipeline.shader)==0);add("all-inputs-full-layout-sentinel",p);

    auto minimal=raw_plan(18,20,42,44);auto uv=raw_plan(23,9,51,39);textured(uv,true);texture_uvs(uv);
    out.push_back({"mixed-selective-full-selective-input-streams",{{minimal,true,false},{p,true,false},{uv,true,false}}, {}});
}

int main() {
    Device device;const auto fixtures=cases();unsigned accepted=0;
    for(const auto& c:fixtures) {
        const auto legacy=device.render(c,false);
        const auto pulled=device.render(c,true);
        verify(c.name+"-selective-vs-full-color-and-depth",pulled,legacy,0);
        const auto clear=clear_image();CHECK(!std::equal(clear.begin(),clear.end(),legacy.begin()));++accepted;
    }
    CHECK(errors==0);
    std::ofstream result("pixels.json");result<<"{\n  \"cases_passed\": "<<accepted<<",\n  \"submitted_frames\": "<<frames
        <<",\n  \"checks\": "<<checks<<",\n  \"uncaptured_errors\": "<<errors.load()
        <<",\n  \"channel_tolerance\": 0,\n  \"synthetic_only\": true,\n  \"window_or_surface\": false\n}\n";
    std::printf("Offscreen D3D12 selective vertices: %u cases/%u frames, %u checks PASS\n",accepted,frames,checks);
}
