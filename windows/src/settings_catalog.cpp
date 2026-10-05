// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_catalog.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace {
bool hud_binding(const std::string& id,unsigned& group,std::string& field) {
    static const char* names[]={"hearts","magic","buttons","rupees","keys"};
    for(unsigned i=0;i<BW_HUD_GROUP_COUNT;++i){const std::string prefix=std::string("hud.")+names[i]+".";
        if(id.compare(0,prefix.size(),prefix)==0){group=i;field=id.substr(prefix.size());return true;}}
    return false;
}
struct BoolBinding { const char* id; bool Settings::* field; };
const BoolBinding kBools[] = {
    {"fullscreen", &Settings::fullscreen}, {"smooth_motion", &Settings::smooth_motion},
    {"show_fps", &Settings::show_fps}, {"pause_unfocused", &Settings::pause_unfocused},
    {"compile_shaders_first", &Settings::shaders_first}, {"mouse_camera", &Settings::mouse_camera},
    {"mouse_invert_y", &Settings::mouse_invert_y}, {"controller_swap_ab", &Settings::controller_swap_ab},
    {"controller_swap_xy", &Settings::controller_swap_xy}, {"controller_invert_x", &Settings::pad_invert_x},
    {"controller_invert_y", &Settings::pad_invert_y}, {"stick_camera", &Settings::stick_camera},
    {"climb", &Settings::climb}, {"quick_items", &Settings::quick_items},
    {"faster_wind", &Settings::faster_wind}, {"faster_boots", &Settings::faster_boots},
    {"autosave", &Settings::autosave}, {"haptics_triggers", &Settings::haptics_triggers},
    {"audio_muted", &Settings::audio_muted},
    {"keep_aspect", &Settings::keep_aspect}, {"betterww", &Settings::betterww},
    {"option_defaults_off", &Settings::option_defaults_off}, {"hd_textures", &Settings::hd_textures},
    {"lle_audio", &Settings::lle_audio}, {"movement_extras", &Settings::movement_extras},
    {"fast_transitions", &Settings::fast_transitions}, {"quick_doors", &Settings::quick_doors}
};
struct IntBinding { const char* id; int Settings::* field; };
const IntBinding kInts[] = {
    {"damage_rate_q8", &Settings::damage_rate_q8}, {"healing_rate_q8", &Settings::healing_rate_q8},
    {"menu_size", &Settings::menu_size},
    {"audio_master", &Settings::audio_master},
    {"audio_music", &Settings::audio_music}, {"audio_sfx", &Settings::audio_sfx},
    {"window_w", &Settings::window_w}, {"window_h", &Settings::window_h},
    {"window_x", &Settings::window_x}, {"window_y", &Settings::window_y},
    {"render_scale", &Settings::render_scale}, {"anisotropy", &Settings::anisotropy},
    {"stick_camera_speed", &Settings::stick_speed}, {"stick_aim_speed", &Settings::stick_aim_speed},
    {"climb_stamina", &Settings::climb_stamina}, {"haptics_strength", &Settings::haptics_strength},
    {"autosave_interval", &Settings::autosave_interval}
};
std::string number(double value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(17) << value;
    return output.str();
}
bool fail(std::string* error, const std::string& text) {
    if (error) *error = text;
    return false;
}
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
}

std::string bw_setting_value(const Settings& settings, const BwSettingDefinition& definition) {
    const std::string id = definition.id;
    if(id=="hud.enabled")return settings.hud_enabled?"1":"0";
    unsigned hg=0;std::string hf;
    if(hud_binding(id,hg,hf)) {
        const auto& g=settings.hud.groups[hg];
        if(hf=="offset_x")return number(g.offset_x);
        if(hf=="offset_y")return number(g.offset_y);
        if(hf=="scale")return number(g.scale);
        if(hf=="opacity")return number(g.opacity);
        if(hf=="anchor_x")return number(g.anchor_x);
        if(hf=="anchor_y")return number(g.anchor_y);
        if(hf=="visible")return g.visible?"1":"0";
        if(hf=="tint_r")return std::to_string(g.tint[0]);
        if(hf=="tint_g")return std::to_string(g.tint[1]);
        if(hf=="tint_b")return std::to_string(g.tint[2]);
        if(hf=="tint_a")return std::to_string(g.tint[3]);
    }

    if (definition.option_name)
        return bw_settings_option_value(settings, definition.option_name, std::string(definition.default_value) == "1") ? "1" : "0";
    for (const auto& binding : kBools) if (id == binding.id) return settings.*(binding.field) ? "1" : "0";
    for (const auto& binding : kInts) if (id == binding.id) return std::to_string(settings.*(binding.field));
    if (id == "mouse_sensitivity") return number(settings.mouse_sensitivity);
    if (id == "dialogue_speed") return number(settings.dialogue_speed);
    if (id == "smooth_motion_fps") return settings.smooth_steps == -1 ? "display" : settings.smooth_steps >= 3 ? "120" : "60";
    if (id == "haptics") return settings.haptics == 0 ? "off" : settings.haptics == 1 ? "classic" : "enhanced";
    if (id == "sprint_keyboard_mode") return settings.sprint_keyboard_mode == BW_SPRINT_TOGGLE ? "toggle" : "hold";
    if (id == "sprint_controller_mode") return settings.sprint_controller_mode == BW_SPRINT_HOLD ? "hold" : "toggle";
    if (id == "aspect") return settings.aspect;
    return {};
}

bool bw_setting_validate(const BwSettingDefinition& definition, const std::string& value,
                         std::string* canonical, std::string* error) {
    if (error) error->clear();
    if (value.empty() || value.size() > 128) return fail(error, "Empty or oversized value for " + std::string(definition.id));
    std::string normalized = value;
    if (definition.type == BW_SETTING_BOOL) {
        if (value != "0" && value != "1") return fail(error, "Expected 0 or 1 for " + std::string(definition.id));
    } else if (definition.type == BW_SETTING_CHOICE) {
        bool found = false;
        for (size_t i = 0; i < definition.choice_count; ++i) found |= value == definition.choices[i].value;
        if (!found) return fail(error, "Unknown choice for " + std::string(definition.id));
    } else if (definition.type == BW_SETTING_INT) {
        int parsed = 0;
        const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
            return fail(error, "Expected an integer for " + std::string(definition.id));
        if (parsed < definition.minimum || parsed > definition.maximum)
            return fail(error, "Value outside bounds for " + std::string(definition.id));
        normalized = std::to_string(parsed);
    } else {
        // The Windows standard library accepts hexadecimal floating-point
        // text that other platforms reject. Presets use decimal text only.
        if (value.find_first_not_of("0123456789eE+.-") != std::string::npos)
            return fail(error, "Expected a decimal number for " + std::string(definition.id));
        // Parse real values in the classic locale and reject whitespace,
        // trailing text, nonfinite numbers, and values outside bounds.
        for (unsigned char c : value) if (std::isspace(c)) return fail(error, "Whitespace in numeric value for " + std::string(definition.id));
        std::istringstream input(value);
        input.imbue(std::locale::classic());
        double parsed = 0;
        if (!(input >> parsed) || !input.eof() || !std::isfinite(parsed)) return fail(error, "Invalid number for " + std::string(definition.id));
        if (parsed < definition.minimum || parsed > definition.maximum) return fail(error, "Value outside bounds for " + std::string(definition.id));
        normalized = number(parsed);
    }
    if (canonical) *canonical = normalized;
    return true;
}

bool bw_setting_assign(Settings& settings, const std::string& id, const std::string& value, std::string* error) {
    const auto* definition = bw_setting_find(id.c_str());
    if (!definition) return fail(error, "Unknown setting: " + id);
    std::string canonical;
    if (!bw_setting_validate(*definition, value, &canonical, error)) return false;
    if(id=="hud.enabled"){settings.hud_enabled=canonical=="1";return true;}
    unsigned hg=0;std::string hf;
    if(hud_binding(id,hg,hf)) {
        BwHudConfig next=settings.hud;auto& g=next.groups[hg];
        float f=0;std::istringstream input(canonical);input.imbue(std::locale::classic());input>>f;
        if(hf=="offset_x")g.offset_x=f;
        else if(hf=="offset_y")g.offset_y=f;
        else if(hf=="scale")g.scale=f;
        else if(hf=="opacity")g.opacity=f;
        else if(hf=="anchor_x")g.anchor_x=f;
        else if(hf=="anchor_y")g.anchor_y=f;
        else if(hf=="visible")g.visible=canonical=="1";
        else if(hf=="tint_r")g.tint[0]=static_cast<uint8_t>(std::stoi(canonical));
        else if(hf=="tint_g")g.tint[1]=static_cast<uint8_t>(std::stoi(canonical));
        else if(hf=="tint_b")g.tint[2]=static_cast<uint8_t>(std::stoi(canonical));
        else if(hf=="tint_a")g.tint[3]=static_cast<uint8_t>(std::stoi(canonical));
        else return fail(error,"Unknown HUD field");
        if(!bw_hud_config_valid(&next))return fail(error,"Invalid HUD preferences");
        settings.hud=next;return true;
    }

    if (definition->option_name) { settings.options[definition->option_name] = canonical == "1"; return true; }
    for (const auto& binding : kBools) if (id == binding.id) { settings.*(binding.field) = canonical == "1"; return true; }
    for (const auto& binding : kInts) if (id == binding.id) { settings.*(binding.field) = std::stoi(canonical); return true; }
    if (id == "mouse_sensitivity") {
        std::istringstream input(canonical); input.imbue(std::locale::classic()); input >> settings.mouse_sensitivity;
    } else if (id == "dialogue_speed") {
        std::istringstream input(canonical); input.imbue(std::locale::classic()); input >> settings.dialogue_speed;
    } else if (id == "smooth_motion_fps") settings.smooth_steps = canonical == "display" ? -1 : canonical == "120" ? 3 : 1;
    else if (id == "haptics") settings.haptics = canonical == "off" ? 0 : canonical == "classic" ? 1 : 2;
    else if (id == "sprint_keyboard_mode") settings.sprint_keyboard_mode = canonical == "toggle" ? BW_SPRINT_TOGGLE : BW_SPRINT_HOLD;
    else if (id == "sprint_controller_mode") settings.sprint_controller_mode = canonical == "hold" ? BW_SPRINT_HOLD : BW_SPRINT_TOGGLE;
    else if (id == "aspect") settings.aspect = canonical;
    else return fail(error, "No platform adapter for " + id);
    return true;
}

bool bw_setting_enabled(const Settings& settings, const BwSettingDefinition& definition) {
    if (!definition.dependency_id) return true;
    const auto* dependency = bw_setting_find(definition.dependency_id);
    return dependency && bw_setting_value(settings, *dependency) == definition.dependency_value;
}

std::vector<const BwSettingDefinition*> bw_settings_search(const std::string& query) {
    std::istringstream words(lower(query));
    std::vector<std::string> terms;
    std::string term;
    while (words >> term) terms.push_back(term);
    size_t count = 0;
    const auto* definitions = bw_setting_definitions(&count);
    std::vector<const BwSettingDefinition*> results;
    for (size_t i = 0; i < count; ++i) {
        const auto& definition = definitions[i];
        if (definition.flags & BW_SETTING_HIDDEN) continue;
        const auto haystack = lower(std::string(definition.id) + " " + definition.label + " " + definition.help + " " + bw_setting_page_name(definition.page));
        if (std::all_of(terms.begin(), terms.end(), [&](const std::string& word) { return haystack.find(word) != std::string::npos; }))
            results.push_back(&definition);
    }
    return results;
}
