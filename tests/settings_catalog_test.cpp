// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../windows/src/settings_catalog.h"
#include "../windows/src/settings_presets.h"
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

namespace {
const BwSettingDefinition& definition(const char* id) {
    const auto* result = bw_setting_find(id);
    assert(result);
    return *result;
}
std::string file_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void catalog_defaults_and_adapters() {
    Settings defaults;
    size_t count = 0;
    const auto* definitions = bw_setting_definitions(&count);
    constexpr size_t prior_count = 61, hud_count = 1 + 5 * 11;
    assert(count == prior_count + hud_count + 1 + 2 + 2); // Menu size, Sprint modes, health rates.
    std::set<std::string> ids;
    unsigned options = 0, default_options = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto& item = definitions[i];
        assert(ids.insert(item.id).second);
        assert(item.label && item.label[0] && item.help && item.help[0]);
        assert(item.page >= BW_PAGE_DISPLAY && item.page < BW_PAGE_COUNT);
        assert(bw_setting_find(item.id) == &item);
        assert(bw_setting_value(defaults, item) == item.default_value);
        std::string canonical, error;
        assert(bw_setting_validate(item, item.default_value, &canonical, &error));
        assert(error.empty() && canonical == item.default_value);
        Settings changed;
        assert(bw_setting_assign(changed, item.id, item.default_value, &error));
        assert(bw_setting_value(changed, item) == canonical);
        if (item.dependency_id) assert(bw_setting_find(item.dependency_id));
        if (item.type == BW_SETTING_CHOICE) {
            assert(item.choices && item.choice_count > 0);
            for (size_t choice = 0; choice < item.choice_count; ++choice) {
                assert(bw_setting_assign(changed, item.id, item.choices[choice].value));
                assert(bw_setting_value(changed, item) == item.choices[choice].value);
            }
        }
        if (item.option_name) { ++options; default_options += canonical == "1"; assert(item.apply == BW_SETTING_RESTART); }
    }
    assert(options == 15 && default_options == 10);
    assert(!bw_setting_find("unknown") && !bw_setting_find(nullptr));
    assert(definition("aspect").page == BW_PAGE_DISPLAY);
    assert(definition("menu_size").page == BW_PAGE_DISPLAY &&
           definition("menu_size").apply == BW_SETTING_LIVE);
    assert(definition("sprint_keyboard_mode").apply == BW_SETTING_LIVE &&
           definition("sprint_controller_mode").page == BW_PAGE_CONTROLS);
    for(const auto& p:bw_settings_builtin_presets()) {
        assert(!p.values.count("sprint_keyboard_mode") && !p.values.count("sprint_controller_mode"));
    }
    assert(definition("haptics").page == BW_PAGE_CONTROLS);
    assert(definition("climb").apply == BW_SETTING_LIVE);
    assert(definition("option_defaults_off").flags & BW_SETTING_SESSION_ONLY);
    assert(BW_PAGE_DEVELOPER == 6 && BW_PAGE_HUD == 7 && BW_PAGE_COUNT == 8);
    assert(bw_setting_all_sections() == 255u);
    assert(std::string(bw_setting_page_name(BW_PAGE_SOUND_SAVES)) == "Sound & Saves");
    assert(std::string(bw_setting_page_name(BW_PAGE_HUD)) == "HUD");
    assert(std::string(bw_setting_page_id(BW_PAGE_HUD)) == "hud");
    assert(definition("hud.enabled").page == BW_PAGE_HUD && !definition("hud.enabled").dependency_id);
    unsigned hud_definitions = 0;
    for (size_t i = 0; i < count; ++i) if (definitions[i].page == BW_PAGE_HUD) {
        ++hud_definitions;
        assert(definitions[i].apply == BW_SETTING_LIVE);
        if (std::string(definitions[i].id) != "hud.enabled") {
            assert(std::string(definitions[i].dependency_id) == "hud.enabled");
            assert(std::string(definitions[i].dependency_value) == "1");
        }
    }
    assert(hud_definitions == hud_count);
}
void validation_and_search() {
    Settings settings;
    std::string error, canonical;
    const char* invalid_reals[] = {"", "nan", "inf", "-inf", "1e309", "1garbage", " 1", "1 ", "0.09", "10.01", "0x1p0"};
    for (const auto* value : invalid_reals) {
        assert(!bw_setting_assign(settings, "mouse_sensitivity", value, &error));
        assert(settings.mouse_sensitivity == 1.0 && !error.empty());
    }
    assert(!bw_setting_assign(settings, "climb_stamina", "4.5"));
    assert(!bw_setting_assign(settings, "climb_stamina", "4.0000000000000001"));
    assert(!bw_setting_assign(settings, "climb_stamina", "4e0"));
    assert(!bw_setting_assign(settings, "climb_stamina", "31"));
    assert(!bw_setting_assign(settings, "climb_stamina", "3"));
    assert(bw_setting_assign(settings, "climb_stamina", "4"));
    assert(settings.climb_stamina == 4);
    assert(bw_setting_assign(settings, "climb_stamina", "30"));
    assert(settings.climb_stamina == 30);
    assert(!bw_setting_assign(settings, "climb", "true"));
    assert(!bw_setting_assign(settings, "climb", "2"));
    assert(!bw_setting_assign(settings, "aspect", "21:9"));
    assert(!bw_setting_assign(settings, "window_x", "2147483648"));
    assert(bw_setting_assign(settings, "window_x", "-2147483648") && settings.window_x == INT_MIN);
    assert(bw_setting_assign(settings, "window_x", "2147483647") && settings.window_x == INT_MAX);
    assert(!bw_setting_assign(settings, "not-a-setting", "0"));
    assert(!bw_setting_enabled(settings, definition("climb_stamina")));
    assert(bw_setting_assign(settings, "climb", "1"));
    assert(bw_setting_enabled(settings, definition("climb_stamina")));
    assert(!bw_setting_enabled(settings, definition("option.instant_text")));
    assert(bw_setting_assign(settings, "betterww", "1"));
    assert(bw_setting_enabled(settings, definition("option.instant_text")));
    assert(bw_setting_enabled(settings, definition("haptics_strength")));
    assert(bw_setting_assign(settings, "haptics", "classic"));
    assert(!bw_setting_enabled(settings, definition("haptics_strength")));
    auto results = bw_settings_search("CLIMB stamina");
    assert(results.size() == 2); // The enable switch and stamina both explain it.
    for (auto* result : results) assert(std::string(result->id) == "climb" || std::string(result->id) == "climb_stamina");
    assert(bw_settings_search("window placement").empty());
    assert(bw_settings_search("not present anywhere").empty());
    assert(bw_settings_search("").size() == 56 + 56 + 1 + 2 + 2); // Prior visible catalog, HUD, menu, Sprint, health.
    auto hud_results = bw_settings_search("Hearts opacity");
    assert(hud_results.size() == 1 && std::string(hud_results[0]->id) == "hud.hearts.opacity");
    assert(!settings.hud_enabled && !bw_setting_enabled(settings, *hud_results[0]));
    assert(bw_setting_assign(settings, "hud.enabled", "1") && bw_setting_enabled(settings, *hud_results[0]));
    assert(bw_setting_assign(settings, "hud.enabled", "0") && !bw_setting_enabled(settings, *hud_results[0]));
    assert(!settings.quick_items);
    assert(bw_setting_assign(settings,"quick_items","1")&&settings.quick_items);
    assert(definition("quick_items").apply==BW_SETTING_LIVE);
    assert(bw_settings_search("cannon").size()==1);
    assert(!bw_setting_find("quick_item_up")); // Fixed contextual shortcuts, no arbitrary persistent X assignments.
    for(const auto* invalid:{"nan","inf","0.99","10.01","2x"})assert(!bw_setting_assign(settings,"dialogue_speed",invalid));
    assert(bw_setting_assign(settings,"dialogue_speed","2.5")&&settings.dialogue_speed==2.5);
    assert(definition("dialogue_speed").apply==BW_SETTING_LIVE);
    assert(!settings.faster_wind && !settings.faster_boots);
    assert(bw_setting_assign(settings,"faster_wind","1") && settings.faster_wind && !settings.faster_boots);
    assert(bw_setting_assign(settings,"faster_boots","1") && settings.faster_wind && settings.faster_boots);
    assert(definition("faster_wind").apply==BW_SETTING_LIVE && definition("faster_boots").apply==BW_SETTING_LIVE);
    assert(definition("faster_wind").page==BW_PAGE_ENHANCEMENTS && definition("faster_boots").page==BW_PAGE_ENHANCEMENTS);
    assert(bw_settings_search("Iron Boots").size()==1);
    assert(settings.audio_master == 100 && !settings.audio_muted);
    assert(!bw_setting_assign(settings,"audio_master","101") && !bw_setting_assign(settings,"audio_master","-1"));
    assert(bw_setting_assign(settings,"audio_master","35") && settings.audio_master==35);
    assert(bw_setting_assign(settings,"audio_muted","1") && settings.audio_muted);
    assert(definition("audio_master").apply==BW_SETTING_LIVE && definition("audio_muted").apply==BW_SETTING_LIVE);
    assert(settings.audio_music == 100 && settings.audio_sfx == 100);
    for (const auto* id : {"audio_music", "audio_sfx"}) {
        assert(!bw_setting_assign(settings, id, "101") && !bw_setting_assign(settings, id, "-1"));
        assert(!bw_setting_assign(settings, id, "2.5"));
        assert(bw_setting_assign(settings, id, "0"));
        assert(definition(id).apply == BW_SETTING_LIVE && definition(id).page == BW_PAGE_SOUND_SAVES);
        assert(bw_setting_enabled(settings, definition(id)));
        settings.lle_audio = true;
        assert(!bw_setting_enabled(settings, definition(id)));
        settings.lle_audio = false;
    }
    assert(settings.audio_music == 0 && settings.audio_sfx == 0 && settings.audio_master == 35 && settings.audio_muted);
}
void presets_and_session_overrides() {
    const auto builtins = bw_settings_builtin_presets();
    assert(builtins.size() == 3);
    for (const auto& preset : builtins) {
        assert(preset.values.count("menu_size") == 0); // Preserve accessibility preference.
        BwSettingsPreset parsed;
        const auto text = bw_settings_preset_serialize(preset);
        assert(!text.empty() && bw_settings_preset_parse(text, parsed));
        assert(parsed.name == preset.name && parsed.values == preset.values && parsed.sections == preset.sections);
    }
    Settings saved;
    Settings session = saved;
    // Launch-only overrides remain separate from saved preferences.
    session.fullscreen = true;
    session.render_scale = 4;
    session.lle_audio = true;
    session.option_defaults_off = true;
    session.options["future_cli_option"] = true;
    session.audio_master = 40; session.audio_music = 75; session.audio_sfx = 65; session.audio_muted = true;
    const Settings before = session;
    BwPresetPreview preview;
    const auto display = 1u << BW_PAGE_DISPLAY;
    assert(bw_settings_preset_preview(builtins[2], session, display, preview));
    assert(preview.candidate.render_scale == 1 && preview.candidate.lle_audio);
    assert(preview.candidate.fullscreen && preview.candidate.option_defaults_off);
    assert(preview.candidate.options.at("future_cli_option"));
    assert(session.render_scale == 4); // Preview is pure.
    assert(bw_settings_preset_apply(builtins[2], session, display));
    bw_settings_keep_edits(saved, before, session);
    assert(saved.render_scale == 1 && !saved.fullscreen && !saved.lle_audio);
    assert(!saved.option_defaults_off && saved.options.count("future_cli_option") == 0);
    const Settings before_qol = session;
    assert(bw_settings_preset_apply(builtins[1], session, 1u << BW_PAGE_ENHANCEMENTS));
    assert(session.betterww && session.quick_doors && session.fast_transitions);
    assert(!session.climb && !session.movement_extras); // Optional movement stays unchanged.
    assert(!session.faster_wind && !session.faster_boots); // Experimental equipment stays opt-in.
    assert(session.option_defaults_off && bw_settings_option_value(session, "instant_text", true));
    bw_settings_keep_edits(saved, before_qol, session);
    assert(saved.betterww && saved.options.at("instant_text"));
    assert(!saved.option_defaults_off && saved.options.count("future_cli_option") == 0);
    const auto captured = bw_settings_capture_preset("My settings", session, bw_setting_all_sections());
    assert(captured.values.count("window_x") == 0 && captured.values.count("option_defaults_off") == 0);
    assert(captured.values.count("option.future_cli_option") == 0);
    assert(captured.values.at("option.instant_text") == "1");
    assert(captured.values.at("option.brisk_sail") == "0");
    assert(captured.values.at("audio_master") == "40" && captured.values.at("audio_music") == "75" &&
           captured.values.at("audio_sfx") == "65" && captured.values.at("audio_muted") == "1");
    BwSettingsPreset audio_roundtrip;
    assert(bw_settings_preset_parse(bw_settings_preset_serialize(captured), audio_roundtrip));
    Settings audio_restored;
    assert(bw_settings_preset_apply(audio_roundtrip, audio_restored, 1u << BW_PAGE_SOUND_SAVES));
    assert(audio_restored.audio_master == 40 && audio_restored.audio_music == 75 &&
           audio_restored.audio_sfx == 65 && audio_restored.audio_muted);
    assert(audio_restored.render_scale == 0 && !audio_restored.betterww);
    auto bad = builtins[2]; bad.values["render_scale"] = "bad";
    assert(!bw_settings_preset_apply(bad, session, bw_setting_all_sections()));
    assert(session.render_scale == 1 && session.betterww); // Failed apply is atomic.
    assert(!bw_settings_preset_preview(builtins[2], session, 1u << 30, preview));
    assert(bw_settings_preset_preview(builtins[2], session, 0, preview) && preview.changes.empty());
}
void import_forward_compatibility_and_errors() {
    const std::string header = "bluewake_preset=1\nname=Example\nsections=enhancements,future_section\n";
    BwSettingsPreset preset;
    std::string error;
    const std::string text = header + "value.betterww=0\nvalue.option.instant_text=1\nvalue.future_option=hello\\nworld\\\\end\nfuture.meta=a=b\n";
    assert(bw_settings_preset_parse(text, preset, &error));
    assert(error.empty() && preset.unknown_sections.size() == 1);
    assert(preset.unknown_fields.at("value.future_option") == "hello\nworld\\end");
    assert(preset.unknown_fields.at("future.meta") == "a=b");
    BwSettingsPreset roundtrip;
    assert(bw_settings_preset_parse(bw_settings_preset_serialize(preset), roundtrip));
    assert(roundtrip.values == preset.values && roundtrip.unknown_fields == preset.unknown_fields);
    assert(roundtrip.unknown_sections == preset.unknown_sections);
    BwPresetPreview preview;
    assert(bw_settings_preset_preview(preset, Settings{}, bw_setting_all_sections(), preview));
    assert(!preview.candidate.betterww && preview.warnings.size() >= 1);
    assert(preview.candidate.options.count("future_option") == 0);
    const auto preserved = preset.values;
    const std::string invalid[] = {
        "bluewake_preset=2\nname=Wrong\nsections=display\n",
        header + "name=Duplicate\n", header + "value.betterww=2\n",
        header + "value.betterww=0\nvalue.betterww=1\n",
        header + "value.option_defaults_off=1\n", header + "value.window_x=0\n",
        header + "value.render_scale=2\n", header + "value.future_option=bad\\q\n",
        "bluewake_preset=1\nname=Bad\\nname\nsections=display\n",
        "bluewake_preset=1\nname=Empty section\nsections=display,\n",
        "bluewake_preset=1\nname=Duplicate section\nsections=display,display\n",
        std::string(256u * 1024u + 1u, 'x'), header + std::string("x=\0", 3)
    };
    for (const auto& bad : invalid) {
        assert(!bw_settings_preset_parse(bad, preset, &error));
        assert(!error.empty() && preset.values == preserved && preset.name == "Example");
    }
}
void atomic_persistence() {
    const auto folder = std::filesystem::temp_directory_path() / ("bluewake-settings-catalog-" + std::to_string(std::rand()));
    assert(std::filesystem::create_directory(folder));
    const auto path = folder / "portable.bwpreset";
    const auto builtins = bw_settings_builtin_presets();
    std::string error;
    assert(bw_settings_preset_write(path.string(), builtins[0], &error));
    BwSettingsPreset loaded;
    assert(bw_settings_preset_read(path.string(), loaded, &error) && loaded.name == "Original");
    assert(bw_settings_preset_write(path.string(), builtins[1], &error)); // Replace existing atomically.
    assert(bw_settings_preset_read(path.string(), loaded, &error) && loaded.name == "Quality of Life");
    const auto good_bytes = file_text(path);
    auto invalid = builtins[2]; invalid.values["render_scale"] = "999";
    assert(!bw_settings_preset_write(path.string(), invalid, &error));
    assert(file_text(path) == good_bytes);
    assert(!bw_settings_preset_write(folder.string(), builtins[2], &error)); // Rename cannot replace a directory.
    assert(file_text(path) == good_bytes);
    for (const auto& item : std::filesystem::directory_iterator(folder)) assert(item.path() == path);
    assert(!bw_settings_preset_read((folder / "missing").string(), loaded, &error));
    assert(loaded.name == "Quality of Life");
    assert(std::filesystem::remove(path));
    assert(std::filesystem::remove(folder));
}
}

int main() {
    catalog_defaults_and_adapters();
    validation_and_search();
    { Settings s;assert(s.damage_rate_q8==256&&s.healing_rate_q8==256);
      assert(bw_setting_assign(s,"damage_rate_q8","512"));assert(bw_setting_assign(s,"healing_rate_q8","0"));
      assert(!bw_setting_assign(s,"damage_rate_q8","257"));assert(s.damage_rate_q8==512);
      auto rows=bw_settings_search("ordinary damage");assert(rows.size()==1&&std::string(rows[0]->id)=="damage_rate_q8");
      Settings saved,before;s.quick_items=true;bw_settings_keep_edits(saved,before,s);
      assert(saved.damage_rate_q8==512&&saved.healing_rate_q8==0&&saved.quick_items);
      auto preset=bw_settings_capture_preset("Challenge",s,1u<<BW_PAGE_ENHANCEMENTS);
      auto text=bw_settings_preset_serialize(preset);BwSettingsPreset restored;
      assert(bw_settings_preset_parse(text,restored));assert(restored.values.at("damage_rate_q8")=="512"); }

    presets_and_session_overrides();
    import_forward_compatibility_and_errors();
    atomic_persistence();
    std::puts("Settings catalog: metadata, validation, search, scoped presets, CLI separation and atomic files passed");
}
