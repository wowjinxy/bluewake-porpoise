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
        std::array<float, gx::kVertexFloats> v{}; v[0] = point[0]; v[1] = point[1]; v[2] = -.5f;
        for (unsigned i = 0; i < 4; ++i) v[4+i] = color[i] / 255.f;
        v[12] = .5f; v[13] = .5f;
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
    WGPURenderPipeline pipeline(const gx::ShaderKey& k,bool raw,bool blend,const std::string& label) {
        auto code = raw ? gx::generate_raw_pos_uv_wgsl(k) : gx::generate_wgsl(k);
        std::ofstream(label + ".wgsl") << code;
        WGPUShaderSourceWGSL src = WGPU_SHADER_SOURCE_WGSL_INIT; src.code = {code.data(),code.size()};
        WGPUShaderModuleDescriptor md = WGPU_SHADER_MODULE_DESCRIPTOR_INIT; md.nextInChain = &src.chain;
        auto shader = wgpuDeviceCreateShaderModule(device,&md); CHECK(shader);
        WGPUVertexAttribute attrs[14]; const WGPUVertexFormat fmt[14] = {
            WGPUVertexFormat_Float32x3,WGPUVertexFormat_Uint32,WGPUVertexFormat_Float32x4,WGPUVertexFormat_Float32x4,
            WGPUVertexFormat_Float32x2,WGPUVertexFormat_Float32x2,WGPUVertexFormat_Float32x2,WGPUVertexFormat_Float32x2,
            WGPUVertexFormat_Float32x3,WGPUVertexFormat_Uint32,WGPUVertexFormat_Float32x3,WGPUVertexFormat_Float32x3,
            WGPUVertexFormat_Float32x2,WGPUVertexFormat_Uint32};
        const uint64_t off[14] = {0,12,16,32,48,56,64,72,80,92,96,108,120,128};
        for(unsigned i=0;i<14;++i){attrs[i]=WGPU_VERTEX_ATTRIBUTE_INIT;attrs[i].shaderLocation=i;attrs[i].format=fmt[i];attrs[i].offset=off[i];}
        WGPUVertexBufferLayout vl = WGPU_VERTEX_BUFFER_LAYOUT_INIT; vl.arrayStride = gx::kVertexStrideBytes; vl.attributeCount = 14; vl.attributes = attrs;
        WGPUBlendState bs = WGPU_BLEND_STATE_INIT;
        bs.color.srcFactor = k.use_dst_alpha ? WGPUBlendFactor_Src1Alpha : WGPUBlendFactor_SrcAlpha;
        bs.color.dstFactor = k.use_dst_alpha ? WGPUBlendFactor_OneMinusSrc1Alpha : WGPUBlendFactor_OneMinusSrcAlpha;
        bs.alpha.srcFactor = k.use_dst_alpha ? WGPUBlendFactor_One : WGPUBlendFactor_SrcAlpha;
        bs.alpha.dstFactor = k.use_dst_alpha ? WGPUBlendFactor_Zero : WGPUBlendFactor_OneMinusSrcAlpha;
        WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT; target.format = WGPUTextureFormat_RGBA8Unorm;
        target.writeMask = WGPUColorWriteMask_All; if(blend) target.blend = &bs;
        WGPUFragmentState fs = WGPU_FRAGMENT_STATE_INIT; fs.module = shader; fs.entryPoint = {"fs_main",7}; fs.targetCount = 1; fs.targets = &target;
        WGPURenderPipelineDescriptor pd = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        pd.vertex.module = shader; pd.vertex.entryPoint = {"vs_main",7}; pd.vertex.bufferCount = raw ? 0u : 1u; pd.vertex.buffers = raw ? nullptr : &vl;
        pd.primitive.topology = WGPUPrimitiveTopology_TriangleList; pd.primitive.cullMode = WGPUCullMode_None;
        pd.fragment = &fs; auto pipe = wgpuDeviceCreateRenderPipeline(device,&pd); wgpuShaderModuleRelease(shader); CHECK(pipe); return pipe;
    }
    void uniform(WGPURenderPassEncoder pass,WGPURenderPipeline pipe,unsigned group,const void* bytes,size_t size) {
        auto b = buffer(bytes,size,WGPUBufferUsage_Uniform); auto layout = wgpuRenderPipelineGetBindGroupLayout(pipe,group);
        WGPUBindGroupEntry e = WGPU_BIND_GROUP_ENTRY_INIT; e.binding = 0; e.buffer = b; e.size = size;
        WGPUBindGroupDescriptor d = WGPU_BIND_GROUP_DESCRIPTOR_INIT; d.layout = layout; d.entryCount = 1; d.entries = &e;
        auto bg = wgpuDeviceCreateBindGroup(device,&d); wgpuRenderPassEncoderSetBindGroup(pass,group,bg,0,nullptr);
        wgpuBindGroupRelease(bg); wgpuBindGroupLayoutRelease(layout); wgpuBufferRelease(b);
    }
    void textures(WGPURenderPassEncoder pass,WGPURenderPipeline pipe,unsigned group,unsigned count) {
        auto layout = wgpuRenderPipelineGetBindGroupLayout(pipe,group); WGPUBindGroupEntry e[16];
        for(unsigned i=0;i<count;++i){e[i*2]=WGPU_BIND_GROUP_ENTRY_INIT;e[i*2].binding=i*2;e[i*2].textureView=texture_view;
            e[i*2+1]=WGPU_BIND_GROUP_ENTRY_INIT;e[i*2+1].binding=i*2+1;e[i*2+1].sampler=sampler;}
        WGPUBindGroupDescriptor d = WGPU_BIND_GROUP_DESCRIPTOR_INIT; d.layout = layout; d.entryCount = count*2; d.entries = e;
        auto bg = wgpuDeviceCreateBindGroup(device,&d); wgpuRenderPassEncoderSetBindGroup(pass,group,bg,0,nullptr);
        wgpuBindGroupRelease(bg); wgpuBindGroupLayoutRelease(layout);
    }
    Image render(const Case& c,bool raw) {
        wgpuDevicePushErrorScope(device,WGPUErrorFilter_Validation);
        WGPUTextureDescriptor td = WGPU_TEXTURE_DESCRIPTOR_INIT; td.size = {W,H,1}; td.format = WGPUTextureFormat_RGBA8Unorm;
        td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
        auto tex = wgpuDeviceCreateTexture(device,&td); auto tv = wgpuTextureCreateView(tex,nullptr);
        WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT; auto encoder = wgpuDeviceCreateCommandEncoder(device,&ed);
        WGPURenderPassColorAttachment ca = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT; ca.view = tv; ca.loadOp = WGPULoadOp_Clear; ca.storeOp = WGPUStoreOp_Store;
        ca.clearValue = {background[0]/255.,background[1]/255.,background[2]/255.,background[3]/255.};
        WGPURenderPassDescriptor rd = WGPU_RENDER_PASS_DESCRIPTOR_INIT; rd.colorAttachmentCount = 1; rd.colorAttachments = &ca;
        auto pass = wgpuCommandEncoderBeginRenderPass(encoder,&rd); unsigned ordinal = 0;
        for(const auto& draw:c.draws) {
            const auto& p = draw.p; if(draw.suppressed)continue;
            auto pipe = pipeline(p.pipeline.shader,raw,draw.blend,c.name+(raw?"-gpu-raw-":"-decoded-")+std::to_string(ordinal++));
            wgpuRenderPassEncoderSetPipeline(pass,pipe);
            auto vb = buffer(p.vertices.data(),p.vertices.size()*sizeof(float),WGPUBufferUsage_Vertex);
            auto ib = buffer(p.indices.data(),p.indices.size()*sizeof(uint16_t),WGPUBufferUsage_Index);
            wgpuRenderPassEncoderSetVertexBuffer(pass,0,vb,0,p.vertices.size()*sizeof(float));
            wgpuRenderPassEncoderSetIndexBuffer(pass,ib,WGPUIndexFormat_Uint16,0,p.indices.size()*sizeof(uint16_t));
            uniform(pass,pipe,1,&p.constants,sizeof p.constants);
            const bool ps = p.pipeline.shader.tev_valid || p.pipeline.shader.hud_tint;
            if(ps)uniform(pass,pipe,2,&p.pixel_constants,sizeof p.pixel_constants);
            if(p.pipeline.shader.textured)textures(pass,pipe,ps?3:2,1);
            if (raw) {
                // Deliberately nonzero base, not aligned to the 5-word stride.
                const unsigned base = 19u + ordinal * 3u;
                std::vector<unsigned char> owned(base * 4u, 0xA5);
                for (unsigned v=0;v<p.vertex_count;++v) {
                    const float* f=p.vertices.data()+v*gx::kVertexFloats;
                    for (unsigned c: {0u,1u,2u,12u,13u}) {
                        unsigned word=0;std::memcpy(&word,f+c,4);
                        owned.push_back(static_cast<unsigned char>(word>>24));
                        owned.push_back(static_cast<unsigned char>(word>>16));
                        owned.push_back(static_cast<unsigned char>(word>>8));
                        owned.push_back(static_cast<unsigned char>(word));
                    }
                }
                auto storage_buffer=buffer(owned.data(),owned.size(),WGPUBufferUsage_Storage);
                auto layout=wgpuRenderPipelineGetBindGroupLayout(pipe,0);
                WGPUBindGroupEntry e=WGPU_BIND_GROUP_ENTRY_INIT;e.binding=0;e.buffer=storage_buffer;e.size=owned.size();
                WGPUBindGroupDescriptor d=WGPU_BIND_GROUP_DESCRIPTOR_INIT;d.layout=layout;d.entryCount=1;d.entries=&e;
                auto bg=wgpuDeviceCreateBindGroup(device,&d);wgpuRenderPassEncoderSetBindGroup(pass,0,bg,0,nullptr);
                wgpuBindGroupRelease(bg);wgpuBindGroupLayoutRelease(layout);wgpuBufferRelease(storage_buffer);
            }
            wgpuRenderPassEncoderSetViewport(pass,0,0,W,H,0,1);
            CHECK(p.scissor_x >= 0 && p.scissor_y >= 0 && p.scissor_x+p.scissor_width <= int(W) && p.scissor_y+p.scissor_height <= int(H));
            wgpuRenderPassEncoderSetScissorRect(pass,p.scissor_x,p.scissor_y,p.scissor_width,p.scissor_height);
            wgpuRenderPassEncoderDrawIndexed(pass,static_cast<unsigned>(p.indices.size()),1,0,0,raw ? 19u + ordinal * 3u : 0u);
            wgpuBufferRelease(vb); wgpuBufferRelease(ib); wgpuRenderPipelineRelease(pipe);
        }
        wgpuRenderPassEncoderEnd(pass); wgpuRenderPassEncoderRelease(pass);
        WGPUBufferDescriptor bd = WGPU_BUFFER_DESCRIPTOR_INIT; bd.size = W*H*4; bd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
        auto read = wgpuDeviceCreateBuffer(device,&bd);
        WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT; source.texture = tex;
        WGPUTexelCopyBufferInfo dest = WGPU_TEXEL_COPY_BUFFER_INFO_INIT; dest.buffer = read; dest.layout.bytesPerRow = W*4; dest.layout.rowsPerImage = H;
        WGPUExtent3D extent{W,H,1}; wgpuCommandEncoderCopyTextureToBuffer(encoder,&source,&dest,&extent);
        WGPUCommandBufferDescriptor cd = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT; auto command = wgpuCommandEncoderFinish(encoder,&cd);
        wgpuQueueSubmit(queue,1,&command); ++frames;
        bool mapped = false; WGPUBufferMapCallbackInfo cb = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT; cb.mode = WGPUCallbackMode_WaitAnyOnly; cb.userdata1 = &mapped;
        cb.callback = [](WGPUMapAsyncStatus s,WGPUStringView m,void* v,void*) { *static_cast<bool*>(v)=s==WGPUMapAsyncStatus_Success; if(s!=WGPUMapAsyncStatus_Success)std::fprintf(stderr,"Map: %s\n",view(m).c_str()); };
        wait(instance,wgpuBufferMapAsync(read,WGPUMapMode_Read,0,W*H*4,cb)); CHECK(mapped);
        auto bytes = static_cast<const unsigned char*>(wgpuBufferGetConstMappedRange(read,0,W*H*4)); CHECK(bytes);
        Image result(bytes,bytes+W*H*4); wgpuBufferUnmap(read);
        wgpuCommandBufferRelease(command); wgpuCommandEncoderRelease(encoder); wgpuBufferRelease(read);
        wgpuTextureViewRelease(tv); wgpuTextureRelease(tex); clean_scope(); return result;
    }
};
static void verify(const std::string& name,const Image& actual,const Image& expected,unsigned tolerance=1) {
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
    for(const auto& point:points){std::array<float,gx::kVertexFloats> v{};v[0]=point[0];v[1]=point[1];v[2]=-.5f;for(unsigned c=4;c<12;++c)v[c]=1;v[12]=point[0]/64;v[13]=point[1]/64;p.vertices.insert(p.vertices.end(),v.begin(),v.end());}
    p.indices={0,1,2,2,1,3,2,3,4,4,3,5};textured(p,true);add("six-vertex-stripe",p);
    auto a=raw_plan();tev_color(a,{220,80,40,180});auto b=raw_plan(18,20,42,44);textured(b,true);texture_uvs(b);
    out.push_back({"ordered-state-and-overlap",{{a,true,false},{b,true,false}}, {}});
    return out;
}
int main() {
    Device device;const auto fixtures=cases();unsigned accepted=0;
    for(const auto& c:fixtures) {
        const auto legacy=device.render(c,false);
        const auto pulled=device.render(c,true);
        verify(c.name+"-gpu-vs-decoded",pulled,legacy,0);
        CHECK(legacy != clear_image());++accepted;
    }
    CHECK(errors==0);
    std::ofstream result("pixels.json");result<<"{\n  \"cases_passed\": "<<accepted<<",\n  \"submitted_frames\": "<<frames
        <<",\n  \"checks\": "<<checks<<",\n  \"uncaptured_errors\": "<<errors.load()
        <<",\n  \"channel_tolerance\": 0,\n  \"synthetic_only\": true,\n  \"window_or_surface\": false\n}\n";
    std::printf("Offscreen D3D12 GPU BE-f32 pull: %u cases/%u frames, %u checks PASS\n",accepted,frames,checks);
}
