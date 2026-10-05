// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_presets.h"
#include <cerrno>
#include <cstdio>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
// A standalone catalog test does not force-include BlueWake's POSIX shim.
// Give the shared atomic helper the same replace-existing rename semantics.
#ifndef rename
static int bw_settings_atomic_rename(const char* from, const char* to) {
    if (MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return 0;
    errno = EACCES;
    return -1;
}
#define rename(from, to) bw_settings_atomic_rename((from), (to))
#define BW_SETTINGS_LOCAL_ATOMIC_RENAME
#endif
#endif
#include "../../runtime/host/src/atomic_file.h"
#ifdef BW_SETTINGS_LOCAL_ATOMIC_RENAME
#undef rename
#undef BW_SETTINGS_LOCAL_ATOMIC_RENAME
#endif
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <sstream>

namespace {
constexpr size_t kMaxPresetBytes = 256u * 1024u;
constexpr size_t kMaxLineBytes = 4096u;
bool fail(std::string* error, const std::string& text) { if (error) *error = text; return false; }
bool key_valid(const std::string& key) {
    if (key.empty() || key.size() > 128) return false;
    for (unsigned char c : key)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
    return true;
}
bool name_valid(const std::string& name) {
    if (name.empty() || name.size() > 96) return false;
    for (unsigned char c : name) if (c < 0x20 || c == 0x7f) return false;
    return true;
}
std::string escape(const std::string& value) {
    std::string result;
    for (char c : value) {
        if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else result += c;
    }
    return result;
}
bool unescape(const std::string& value, std::string& result) {
    result.clear();
    for (size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if (c == '\0' || static_cast<unsigned char>(c) < 0x20) return false;
        if (c == '\\') {
            if (++i == value.size()) return false;
            c = value[i];
            if (c == 'n') c = '\n';
            else if (c == 'r') c = '\r';
            else if (c == 't') c = '\t';
            else if (c != '\\') return false;
        }
        result += c;
    }
    return true;
}
bool preset_valid(const BwSettingsPreset& preset, std::string* error) {
    if (!name_valid(preset.name)) return fail(error, "Preset name must contain 1 through 96 bytes without control characters.");
    if ((preset.sections & ~bw_setting_all_sections()) != 0) return fail(error, "Invalid preset section mask.");
    for (const auto& [id, value] : preset.values) {
        const auto* definition = bw_setting_find(id.c_str());
        if (!definition) return fail(error, "Unknown value must be stored as an unknown field: " + id);
        if (definition->flags & BW_SETTING_NO_PRESET) return fail(error, "Setting cannot be included in a portable preset: " + id);
        if (!(preset.sections & (1u << definition->page))) return fail(error, "Setting is outside the preset's selected sections: " + id);
        if (!bw_setting_validate(*definition, value, nullptr, error)) return false;
    }
    std::set<std::string> sections;
    size_t section_bytes = 9;
    for (unsigned i = 0; i < BW_PAGE_COUNT; ++i)
        if (preset.sections & (1u << i)) section_bytes += std::string(bw_setting_page_id(static_cast<BwSettingPage>(i))).size() + 1;
    for (const auto& section : preset.unknown_sections) {
        if (!key_valid(section) || !sections.insert(section).second) return fail(error, "Invalid or duplicate unknown section.");
        section_bytes += section.size() + 1;
        for (unsigned i = 0; i < BW_PAGE_COUNT; ++i)
            if (section == bw_setting_page_id(static_cast<BwSettingPage>(i))) return fail(error, "Known section listed as unknown.");
    }
    if (section_bytes > kMaxLineBytes) return fail(error, "Preset section list is too long.");
    for (const auto& [key, value] : preset.unknown_fields) {
        if (!key_valid(key) || key == "bluewake_preset" || key == "name" || key == "sections" || escape(value).size() + key.size() + 1 > kMaxLineBytes)
            return fail(error, "Invalid unknown preset field.");
        if (key.rfind("value.", 0) == 0 && bw_setting_find(key.substr(6).c_str())) return fail(error, "Known setting listed as unknown: " + key);
        for (unsigned char c : value)
            if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') return fail(error, "Invalid control byte in unknown preset field.");
    }
    return true;
}
}

BwSettingsPreset bw_settings_capture_preset(const std::string& name, const Settings& settings, uint32_t sections) {
    BwSettingsPreset preset;
    preset.name = name;
    preset.sections = sections & bw_setting_all_sections();
    size_t count = 0;
    const auto* definitions = bw_setting_definitions(&count);
    for (size_t i = 0; i < count; ++i) {
        const auto& definition = definitions[i];
        if (!(definition.flags & BW_SETTING_NO_PRESET) && (preset.sections & (1u << definition.page)))
            preset.values[definition.id] = bw_setting_value(settings, definition);
    }
    return preset;
}

std::vector<BwSettingsPreset> bw_settings_builtin_presets() {
    const uint32_t original_sections = (1u << BW_PAGE_DISPLAY) | (1u << BW_PAGE_CONTROLS) |
        (1u << BW_PAGE_ENHANCEMENTS) | (1u << BW_PAGE_MODS) | (1u << BW_PAGE_SOUND_SAVES) | (1u << BW_PAGE_HUD);
    Settings original;
    original.mouse_camera = false;
    original.stick_camera = false;
    original.haptics = 1;
    BwSettingsPreset original_preset = bw_settings_capture_preset("Original", original, original_sections);
    original_preset.values.erase("menu_size"); // Gameplay/display presets preserve accessibility text size.
    original_preset.values.erase("sprint_keyboard_mode");
    original_preset.values.erase("sprint_controller_mode");
    for (auto& [id, value] : original_preset.values) if (id.rfind("option.", 0) == 0) value = "0";
    BwSettingsPreset qol;
    qol.name = "Quality of Life";
    qol.sections = 1u << BW_PAGE_ENHANCEMENTS;
    qol.values = {{"betterww", "1"}, {"fast_transitions", "1"}, {"quick_doors", "1"}};
    size_t count = 0;
    const auto* definitions = bw_setting_definitions(&count);
    for (size_t i = 0; i < count; ++i)
        if (definitions[i].option_name) qol.values[definitions[i].id] = definitions[i].default_value;
    BwSettingsPreset performance;
    performance.name = "Performance";
    performance.sections = (1u << BW_PAGE_DISPLAY) | (1u << BW_PAGE_SOUND_SAVES);
    performance.values = {{"render_scale", "1"}, {"anisotropy", "1"}, {"smooth_motion", "0"}, {"lle_audio", "0"}};
    return {original_preset, qol, performance};
}

bool bw_settings_preset_parse(const std::string& text, BwSettingsPreset& preset, std::string* error) {
    if (error) error->clear();
    if (text.size() > kMaxPresetBytes || text.find('\0') != std::string::npos) return fail(error, "Preset is oversized or contains a NUL byte.");
    BwSettingsPreset candidate;
    std::map<std::string, std::string> fields;
    std::istringstream input(text);
    std::string line;
    unsigned line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() > kMaxLineBytes) return fail(error, "Preset line is too long.");
        if (line.empty() || line[0] == '#') continue;
        const size_t equals = line.find('=');
        if (equals == std::string::npos) return fail(error, "Missing '=' at line " + std::to_string(line_number));
        const std::string key = line.substr(0, equals);
        std::string value;
        if (!key_valid(key) || !unescape(line.substr(equals + 1), value)) return fail(error, "Invalid field at line " + std::to_string(line_number));
        if (!fields.emplace(key, value).second) return fail(error, "Duplicate field: " + key);
    }
    if (fields["bluewake_preset"] != "1") return fail(error, "Unsupported or missing BlueWake preset version.");
    const auto name = fields.find("name"), sections = fields.find("sections");
    if (name == fields.end() || sections == fields.end()) return fail(error, "Preset name and sections are required.");
    candidate.name = name->second;
    if (!sections->second.empty()) {
        std::istringstream section_input(sections->second);
        std::string section;
        std::set<std::string> seen;
        while (std::getline(section_input, section, ',')) {
            if (!key_valid(section) || !seen.insert(section).second) return fail(error, "Invalid or duplicate section: " + section);
            bool known = false;
            for (unsigned i = 0; i < BW_PAGE_COUNT; ++i) {
                if (section == bw_setting_page_id(static_cast<BwSettingPage>(i))) { candidate.sections |= 1u << i; known = true; break; }
            }
            if (!known) candidate.unknown_sections.push_back(section);
        }
        if (sections->second.back() == ',') return fail(error, "Empty trailing section.");
    }
    for (const auto& [key, value] : fields) {
        if (key == "bluewake_preset" || key == "name" || key == "sections") continue;
        if (key.rfind("value.", 0) == 0) {
            const std::string id = key.substr(6);
            const auto* definition = bw_setting_find(id.c_str());
            if (definition) {
                std::string canonical;
                if (!bw_setting_validate(*definition, value, &canonical, error)) return false;
                candidate.values[id] = canonical;
                continue;
            }
        }
        candidate.unknown_fields[key] = value;
    }
    if (!preset_valid(candidate, error)) return false;
    preset = std::move(candidate);
    return true;
}

std::string bw_settings_preset_serialize(const BwSettingsPreset& preset) {
    if (!preset_valid(preset, nullptr)) return {};
    std::ostringstream output;
    output << "# BlueWake portable settings preset\nbluewake_preset=1\nname=" << escape(preset.name) << "\nsections=";
    bool comma = false;
    for (unsigned i = 0; i < BW_PAGE_COUNT; ++i) if (preset.sections & (1u << i)) {
        if (comma) output << ',';
        output << bw_setting_page_id(static_cast<BwSettingPage>(i)); comma = true;
    }
    for (const auto& section : preset.unknown_sections) { if (comma) output << ','; output << section; comma = true; }
    output << '\n';
    for (const auto& [id, value] : preset.values) {
        std::string canonical; const auto* definition = bw_setting_find(id.c_str());
        bw_setting_validate(*definition, value, &canonical);
        output << "value." << id << '=' << escape(canonical) << '\n';
    }
    for (const auto& [key, value] : preset.unknown_fields) output << key << '=' << escape(value) << '\n';
    const auto text = output.str();
    return text.size() <= kMaxPresetBytes ? text : std::string{};
}

bool bw_settings_preset_read(const std::string& path, BwSettingsPreset& preset, std::string* error) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return fail(error, "Could not open preset file.");
    std::string text;
    char buffer[4096];
    size_t count;
    bool ok = true;
    while ((count = std::fread(buffer, 1, sizeof buffer, file)) != 0) {
        if (text.size() + count > kMaxPresetBytes) { ok = false; break; }
        text.append(buffer, count);
    }
    if (std::ferror(file)) ok = false;
    if (std::fclose(file) != 0) ok = false;
    if (!ok) return fail(error, "Could not read preset, or file exceeded 256 KiB.");
    return bw_settings_preset_parse(text, preset, error);
}

bool bw_settings_preset_write(const std::string& path, const BwSettingsPreset& preset, std::string* error) {
    if (error) error->clear();
    if (!preset_valid(preset, error)) return false;
    const std::string text = bw_settings_preset_serialize(preset);
    if (text.empty()) return fail(error, "Preset exceeds the portable file size limit.");
    char* pending = bw_atomic_path(path.c_str());
    FILE* file = pending ? std::fopen(pending, "wb") : nullptr;
    if (!file) { std::free(pending); return fail(error, "Could not create pending preset file."); }
    const bool written = std::fwrite(text.data(), 1, text.size(), file) == text.size();
    const bool ok = bw_atomic_finish(file, pending, path.c_str(), written);
    std::free(pending);
    return ok || fail(error, "Could not save preset; the previous file was retained.");
}

bool bw_settings_preset_preview(const BwSettingsPreset& preset, const Settings& current, uint32_t selected_sections,
                                BwPresetPreview& preview, std::string* error) {
    if (error) error->clear();
    if (!preset_valid(preset, error)) return false;
    if (selected_sections & ~bw_setting_all_sections()) return fail(error, "Invalid selected section mask.");
    BwPresetPreview candidate;
    candidate.candidate = current;
    const uint32_t included = preset.sections & selected_sections;
    for (const auto& [id, value] : preset.values) {
        const auto* definition = bw_setting_find(id.c_str());
        if (!(included & (1u << definition->page))) continue;
        const std::string before = bw_setting_value(current, *definition);
        std::string canonical;
        if (!bw_setting_validate(*definition, value, &canonical, error)) return false;
        // Do not turn a launch-only effective value into a stored override
        // when applying the preset would not change that value.
        if (before == canonical) continue;
        if (!bw_setting_assign(candidate.candidate, id, value, error)) return false;
        const std::string after = bw_setting_value(candidate.candidate, *definition);
        if (before != after) candidate.changes.push_back({definition, before, after});
    }
    if (!preset.unknown_fields.empty() || !preset.unknown_sections.empty())
        candidate.warnings.push_back("Unknown fields or sections are retained in the preset file and will not be applied.");
    for (const auto& change : candidate.changes)
        if (!bw_setting_enabled(candidate.candidate, *change.definition))
            candidate.warnings.push_back(std::string(change.definition->label) + " is stored but inactive until its dependency is enabled.");
    preview = std::move(candidate);
    return true;
}

bool bw_settings_preset_apply(const BwSettingsPreset& preset, Settings& session, uint32_t selected_sections, std::string* error) {
    BwPresetPreview preview;
    if (!bw_settings_preset_preview(preset, session, selected_sections, preview, error)) return false;
    session = std::move(preview.candidate);
    return true;
}
