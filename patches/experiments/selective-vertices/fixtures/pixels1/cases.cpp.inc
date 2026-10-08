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
