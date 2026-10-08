// SPDX-License-Identifier: GPL-3.0-or-later
// Independent primary-light oracle adapted from tests/renderer_lighting_pixels_test.cpp.
static void raster_channel0(gx::DrawPlan& p) {
    tev_color(p,{0,0,0,0});auto& s=p.pipeline.shader.tev_stages[0];s.cc_d=10;s.ac_d=5;
    for(unsigned i=0;i<4;++i)s.ras_swap[i]=static_cast<uint8_t>(i);
}
static void append_uniform_cases(std::vector<Case>& out) {
    auto add=[&](std::string name,gx::DrawPlan p){out.push_back({name,{{p,false,false}},{}});};
    for(unsigned light=0;light<8;++light)for(unsigned attenuation=0;attenuation<4;++attenuation) {
        Pixel primary{0,0,0,255};primary[light%3]=255;
        auto left=plan(8,12,24,28,{255,255,255,255});auto right=plan(32,12,48,28,{255,255,255,255});
        for(auto* p:{&left,&right}) {
            auto& k=p->pipeline.shader;k.lit_valid=1;k.chan_captured_mask=5;
            init_normal_matrix(*p);
            for(unsigned channel:{0u,2u}) {
                auto& c=k.litchan[channel];c.enablelighting=1;c.matsource=1;c.ambsource=0;
                c.light_mask=static_cast<uint8_t>(1u<<light);c.attnfunc=static_cast<uint8_t>(attenuation);c.diffusefunc=0;
            }
            auto& l=p->constants.lights[light];for(unsigned c=0;c<4;++c)l.color[c]=primary[c];
            l.cosatt[0]=1;l.distatt[0]=1;l.dir[2]=1;l.pos[0]=2;
        }
        raster_channel0(right);
        out.push_back({"lighting-oracle-L"+std::to_string(light)+"-A"+std::to_string(attenuation),
                       {{left,false,false},{right,false,false}},{},true,primary});
    }
    auto p=plan();p.pipeline.shader.chan_captured_mask=5;
    for(unsigned i=0;i<4;++i)p.constants.materials[2][i]=static_cast<int>(55+45*i);
    add("captured-unlit-register-material",p);
    raster_channel0(p);add("TEV-captured-unlit-register-material",p);

    p=plan();vertex_lighting(p,false);p.pipeline.shader.litchan[0].ambsource=1;
    p.pipeline.shader.litchan[2].matsource=0;p.constants.materials[2][3]=93;
    raster_channel0(p);add("mixed-register-alpha-and-vertex-ambient",p);

    for(bool vertex:{false,true}) {
        p=plan();vertex_lighting(p,vertex);p.pipeline.shader.has_pos_mtx_idx=1;words(p,1,27);
        for(unsigned c=0;c<3;++c){p.constants.transformmatrices[27+c][c]=1;p.constants.normalmatrices[27+c][c]=1;}
        p.constants.normalmatrices[27][1]=.125f;p.constants.normalmatrices[28][0]=-.125f;
        raster_channel0(p);add(vertex?"PN-normal-bank-vertex-normal":"PN-normal-bank-cached-normal",p);
    }
    for(unsigned color=0;color<2;++color) {
        p=plan();textured(p,true);p.pipeline.shader.has_color1=1;p.pipeline.shader.num_color_chans=2;
        p.pipeline.shader.tex_gens[0].texgentype=static_cast<uint8_t>(color?gx::TexGenType::Color1:gx::TexGenType::Color0);
        for(unsigned v=0;v<p.vertex_count;++v){field(p,v,3)[0]=.2f+.2f*v;field(p,v,3)[1]=.3f;}
        add(color?"Color1-texgen-without-fragment-raster1":"Color0-texgen-without-fragment-raster0",p);
    }
    p=raw_plan();all_texgens(p,0);p.pipeline.shader.has_tex_mtx_idx=1;p.pipeline.shader.tex_mtx_idx_mask=1;
    words(p,9,60);words(p,13,0);p.constants.transformmatrices[60][0]=1;p.constants.transformmatrices[61][1]=1;
    p.pipeline.shader.tex_gens[0].projection=1;p.constants.transformmatrices[62][2]=1;
    add("indexed-texture-matrix-near-XF-bank-end",p);

    p=raw_plan();textured(p,true);texture_uvs(p);init_normal_matrix(p);p.pipeline.shader.num_tex_gens=3;
    for(unsigned slot=1;slot<3;++slot) {
        auto& t=p.pipeline.shader.tex_gens[slot];t.enabled=1;t.texgentype=static_cast<uint8_t>(gx::TexGenType::EmbossMap);
        t.embosssourceshift=static_cast<uint8_t>(slot-1);t.embosslightshift=7;
    }
    p.constants.lights[7].pos[0]=40;p.constants.lights[7].pos[1]=30;p.constants.lights[7].pos[2]=10;
    p.pipeline.shader.tev_stages[0].tevorders_texcoord=2;
    add("emboss-intermediate-UV-local-only",p);
    p.pipeline.shader.tex_gens[1].embosssourceshift=2;
    add("emboss-forward-source-zero-initialization",p);

    p=raw_plan();textured(p,true);texture_uvs(p);p.pipeline.shader.num_tex_gens=2;
    p.pipeline.shader.tex_gens[1]=p.pipeline.shader.tex_gens[0];
    p.pipeline.shader.num_ind_stages=1;p.pipeline.shader.ind_stages[0].texcoord=1;
    auto& s=p.pipeline.shader.tev_stages[0];s.tevorders_enable=0;s.ind_stage=0;s.ind_bump_alpha=1;
    s.tevorders_colorchan=5;s.cc_d=10;s.ac_d=5;for(unsigned i=0;i<4;++i)s.ras_swap[i]=static_cast<uint8_t>(i);
    add("indirect-bump-with-direct-sampling-disabled",p);
    p.pipeline.shader.ind_stages[0].texcoord=7;add("indirect-invalid-coordinate-falls-back-UV0",p);
    s.tevorders_enable=1;s.tevorders_texcoord=7;add("direct-and-indirect-invalid-coordinates-fall-back-UV0",p);

    p=plan();p.pipeline.shader.has_color1=1;p.pipeline.shader.num_color_chans=2;
    for(unsigned v=0;v<p.vertex_count;++v){field(p,v,3)[0]=.2f;field(p,v,3)[1]=.8f;field(p,v,3)[2]=.4f;}
    raster_channel0(p);p.pipeline.shader.num_tev_stages=2;
    p.pipeline.shader.tev_stages[1]=p.pipeline.shader.tev_stages[0];p.pipeline.shader.tev_stages[1].tevorders_colorchan=1;
    add("two-TEV-stages-consume-both-raster-colors",p);
}
