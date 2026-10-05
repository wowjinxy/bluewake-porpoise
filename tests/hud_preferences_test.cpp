#include "settings_presets.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);std::exit(1);}}while(0)
int main(int argc,char** argv) {
    CHECK(argc==2);Settings settings;CHECK(!settings.hud_enabled&&bw_hud_config_is_identity(&settings.hud));
    CHECK(BW_PAGE_DEVELOPER==6&&BW_PAGE_HUD==7);size_t count=0;const auto* defs=bw_setting_definitions(&count);
    unsigned hud_count=0;std::set<std::string> ids;
    for(size_t i=0;i<count;++i){CHECK(ids.insert(defs[i].id).second);if(defs[i].page==BW_PAGE_HUD){++hud_count;
        CHECK(bw_setting_value(settings,defs[i])==defs[i].default_value);CHECK(defs[i].apply==BW_SETTING_LIVE);}}
    CHECK(hud_count==56);std::string error;
    CHECK(bw_setting_assign(settings,"hud.enabled","1"));
    CHECK(bw_setting_assign(settings,"hud.hearts.offset_x","125.25"));
    CHECK(bw_setting_assign(settings,"hud.hearts.opacity","0.375"));
    CHECK(bw_setting_assign(settings,"hud.hearts.tint_r","91"));
    CHECK(bw_setting_assign(settings,"hud.rupees.visible","0"));
    const Settings before_bad=settings;
    for(const auto& value:{"nan","inf","2049","0x1p4","1 2"})CHECK(!bw_setting_assign(settings,"hud.hearts.offset_x",value,&error));
    CHECK(settings.hud.groups[0].offset_x==before_bad.hud.groups[0].offset_x);
    CHECK(!bw_setting_assign(settings,"hud.hearts.scale","0.249"));
    CHECK(!bw_setting_assign(settings,"hud.magic.tint_a","256"));
    CHECK(!bw_setting_assign(settings,"hud.keys.visible","true"));
    CHECK(bw_settings_search("hearts opacity").size()==1);
    auto preset=bw_settings_capture_preset("Native HUD",settings,1u<<BW_PAGE_HUD);
    preset.unknown_fields["hud.future.extra"]="kept";preset.unknown_sections.push_back("future_hud");
    auto text=bw_settings_preset_serialize(preset);BwSettingsPreset parsed;CHECK(bw_settings_preset_parse(text,parsed,&error));
    CHECK(parsed.unknown_fields==preset.unknown_fields&&parsed.unknown_sections==preset.unknown_sections);
    Settings original;BwPresetPreview preview;CHECK(bw_settings_preset_preview(parsed,original,1u<<BW_PAGE_HUD,preview,&error));
    CHECK(preview.changes.size()==5);CHECK(bw_settings_preset_apply(parsed,original,1u<<BW_PAGE_HUD,&error));
    CHECK(original.hud_enabled&&original.hud.groups[0].offset_x==125.25f&&original.hud.groups[3].visible==false);
    CHECK(original.hud.groups[0].tint[0]==91);CHECK(bw_hud_config_valid(&original.hud));
    auto path=(std::filesystem::path(argv[1])/"hud-preset.txt").string();
    std::filesystem::create_directories(argv[1]);CHECK(bw_settings_preset_write(path,parsed,&error));
    CHECK(bw_settings_preset_read(path,parsed,&error));CHECK(bw_settings_preset_serialize(parsed)==text);
    CHECK(bw_settings_preset_write(path,parsed,&error));CHECK(!std::filesystem::exists(path+".tmp"));
    /* Existing launch overrides are untouched by editing one HUD component. */
    Settings saved;Settings session=saved;session.betterww=true;session.audio_master=13;
    session.hud.groups[1].opacity=.25f;session.options["swift_sail"]=false;Settings before=session;
    session.hud.groups[0].offset_y=31;bw_settings_keep_edits(saved,before,session);
    CHECK(!saved.betterww&&saved.audio_master==100&&saved.options.empty());
    CHECK(saved.hud.groups[0].offset_y==31&&saved.hud.groups[1].opacity==1);
    auto builtins=bw_settings_builtin_presets();CHECK(builtins.size()==3);
    CHECK(builtins[0].values.at("hud.enabled")=="0");CHECK(builtins[0].values.at("hud.hearts.opacity")=="1");
    CHECK(!builtins[1].values.count("hud.enabled")&&!builtins[2].values.count("hud.enabled"));
    std::printf("HUD typed catalog/presets/atomic persistence and edited-diff: %u checks PASS\n",checks);
}
