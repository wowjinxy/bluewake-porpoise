// SPDX-License-Identifier: GPL-3.0-or-later
#include "card_menu.h"
#include "card_manager.h"
#if defined(BLUEWAKE_CARD_MENU_TEST)
#include "card_menu_test_api.h"
#endif
#ifdef rename
#undef rename
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// Keep the UI independent of guest CPU types in card_runtime.h. The runtime
// holds its card mutex from a successful begin until the matching end.
extern "C" {
const char* bluewake_card_runtime_path(void);
bool bluewake_card_runtime_begin_snapshot(const char* expected_raw_path);
void bluewake_card_runtime_end_snapshot(void);
}

namespace {
namespace fs = std::filesystem;
constexpr size_t max_listed_backups = 512;
BwCardManager* g_manager;
BwCardManagerPending g_pending{};
std::vector<BwCardManagerBackup> g_backups;
std::string g_card_path, g_backup_path;
std::string g_status, g_error, g_list_error, g_pending_error;
size_t g_backup_total;
int g_selected = -1;
bool g_loaded;
#if defined(BLUEWAKE_CARD_MENU_TEST)
std::string g_test_import_path;
#endif

fs::path from_utf8(const char* value) {
#ifdef __cpp_char8_t
    const std::string text(value);
    return fs::path(std::u8string(text.begin(), text.end()));
#else
    return fs::u8path(value);
#endif
}

std::string path_text(const fs::path& path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

std::wstring normalized_path(const char* value) {
    auto result = fs::absolute(from_utf8(value)).lexically_normal().wstring();
    std::replace(result.begin(), result.end(), L'/', L'\\');
    return result;
}

bool begin_snapshot(const char* expected, void*) {
    try {
        const char* runtime = bluewake_card_runtime_path();
        if (!expected || !*expected || !runtime || !*runtime) return false;
        // The manager uses an absolute normalized path; the runtime may retain
        // the user's relative/slash/case spelling. Compare canonical spelling
        // here, then let begin verify the exact copied runtime spelling under
        // its mutex. Never pass the manager spelling to that raw-path check.
        const std::string raw(runtime);
        const auto requested = normalized_path(expected);
        const auto active = normalized_path(raw.c_str());
        if (CompareStringOrdinal(requested.c_str(), static_cast<int>(requested.size()),
                active.c_str(), static_cast<int>(active.size()), TRUE) != CSTR_EQUAL) return false;
        return bluewake_card_runtime_begin_snapshot(raw.c_str());
    } catch (...) {
        return false;
    }
}

void end_snapshot(void*) { bluewake_card_runtime_end_snapshot(); }

std::string outcome_text(const BwCardManagerOutcome& outcome, const char* fallback) {
    std::string text = *outcome.message ? outcome.message : fallback;
    if (outcome.backup_valid && *outcome.backup_path)
        text += std::string("\nBackup: ") + outcome.backup_path;
    if (*outcome.recovery_path)
        text += std::string("\nRecovery: ") + outcome.recovery_path;
    return text;
}

void report(bool success, const BwCardManagerOutcome& outcome, const char* fallback) {
    const auto text = outcome_text(outcome, fallback);
    if (success) { g_status = text; g_error.clear(); }
    else { g_error = text; g_status.clear(); }
}

void refresh_pending() {
    BwCardManagerOutcome outcome{};
    g_pending = {};
    if (!bluewake_card_manager_pending(g_manager, &g_pending, &outcome))
        g_pending_error = outcome_text(outcome, "The queued replacement could not be checked.");
    else g_pending_error.clear();
}

void refresh_list() {
    std::string selected;
    if (g_selected >= 0 && static_cast<size_t>(g_selected) < g_backups.size())
        selected = g_backups[static_cast<size_t>(g_selected)].path;
    g_selected = -1;
    g_backups.clear();
    g_backup_total = 0;
    BwCardManagerOutcome outcome{};
    size_t total = 0;
    if (!bluewake_card_manager_list_backups(g_manager, nullptr, 0, &total, &outcome)) {
        g_list_error = outcome_text(outcome, "The backup list could not be read.");
        return;
    }
    std::vector<BwCardManagerBackup> found(std::min(total, max_listed_backups));
    if (!found.empty() && !bluewake_card_manager_list_backups(g_manager, found.data(),
            found.size(), &total, &outcome)) {
        g_list_error = outcome_text(outcome, "The backup list could not be read.");
        return;
    }
    found.resize(std::min(found.size(), total));
    g_backups = std::move(found);
    g_backup_total = total;
    g_list_error.clear();
    for (size_t i = 0; i < g_backups.size(); ++i)
        if (g_backups[i].valid && selected == g_backups[i].path) g_selected = static_cast<int>(i);
}

void refresh() {
    refresh_pending();
    refresh_list();
    g_loaded = true;
}

std::string import_file() {
#if defined(BLUEWAKE_CARD_MENU_TEST)
    // The offscreen fixture clicks the real Import widget. Its file picker is
    // compiled out, so it can never open a dialog or affect desktop input.
    return std::exchange(g_test_import_path, std::string{});
#else
    wchar_t file[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof dialog;
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrTitle = L"Import Wind Waker saves for the next launch";
    dialog.lpstrFilter = L"Supported memory-card saves (*.card;*.gci;*.raw)\0*.card;*.gci;*.raw\0BlueWake cards (*.card)\0*.card\0Wind Waker GCI (*.gci)\0*.gci\0GameCube raw cards (*.raw)\0*.raw\0\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = static_cast<DWORD>(sizeof file / sizeof *file);
    dialog.lpstrDefExt = L"card";
    dialog.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameW(&dialog)) {
        if (CommDlgExtendedError() != 0) g_error = "The memory-card file picker could not open.";
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, file, -1,
        nullptr, 0, nullptr, nullptr);
    if (length <= 1) { g_error = "The selected path could not be converted to UTF-8."; return {}; }
    std::string result(static_cast<size_t>(length), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, file, -1, result.data(), length,
            nullptr, nullptr) != length) {
        g_error = "The selected path could not be converted to UTF-8.";
        return {};
    }
    result.pop_back();
    return result;
#endif
}

std::string filename(const char* path) {
    const std::string text(path);
    const auto separator = text.find_last_of("/\\");
    return separator == std::string::npos ? text : text.substr(separator + 1);
}

std::string backup_label(const BwCardManagerBackup& backup) {
    char date[40]{};
    const auto seconds = static_cast<time_t>(backup.modified_unix_seconds);
    tm local{};
    if (localtime_s(&local, &seconds) == 0) std::strftime(date, sizeof date, "%Y-%m-%d %H:%M", &local);
    char bytes[64];
    std::snprintf(bytes, sizeof bytes, " (%llu bytes)", static_cast<unsigned long long>(backup.bytes));
    std::string label = filename(backup.path);
    if (*date) label += std::string("  ") + date;
    label += bytes;
    if (!backup.valid) label += " [damaged]";
    return label;
}
}

#if defined(BLUEWAKE_CARD_MENU_TEST)
extern "C" void bw_card_menu_test_set_import_path(const char* path) {
    g_test_import_path = path ? path : "";
}
#endif

extern "C" bool bw_card_menu_prepare(const char* card_path) {
    bw_card_menu_shutdown();
    try {
        if (!card_path || !*card_path) { g_error = "The memory-card path is missing."; return false; }
        const auto card = fs::absolute(from_utf8(card_path)).lexically_normal();
        g_card_path = path_text(card);
        g_backup_path = path_text(card.parent_path() / "Backups" / card.stem());
        BwCardManagerConfig config{};
        config.card_path = g_card_path.c_str();
        config.backup_directory = g_backup_path.c_str();
        config.begin_snapshot = begin_snapshot;
        config.end_snapshot = end_snapshot;
        BwCardManagerOutcome outcome{};
        g_manager = bluewake_card_manager_create(&config, &outcome);
        if (!g_manager) { report(false, outcome, "Memory-card management could not start."); return false; }
        bluewake_card_manager_apply_at_startup(g_manager, &outcome);
        if (!outcome.startup_ready) {
            report(false, outcome, "The queued card could not be applied. Startup was stopped.");
            return false;
        }
        report(true, outcome, "No memory-card replacement is queued.");
        return true;
    } catch (...) {
        g_error = "The memory-card management paths could not be initialized.";
        return false;
    }
}

extern "C" const char* bw_card_menu_error(void) { return g_error.c_str(); }

extern "C" bool bw_card_menu_restart_needed(void) { return g_pending.present && g_pending.valid; }

extern "C" void bw_card_menu_draw(void) {
    ImGui::TextUnformatted("Memory card");
    if (!g_manager) {
        ImGui::TextWrapped("%s", g_error.empty() ? "Memory-card management is unavailable." : g_error.c_str());
        return;
    }
    if (!g_loaded) refresh();
    ImGui::TextWrapped("Use the game Save first. Backup copies the persistent card, including all of its save slots.");
    ImGui::TextWrapped("Card: %s", g_card_path.c_str());
    if (ImGui::Button("Backup card now")) {
        BwCardManagerOutcome outcome{};
        report(bluewake_card_manager_backup(g_manager, &outcome), outcome, "Memory card backed up.");
        refresh();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh backups")) refresh();
    ImGui::SameLine();
    if (ImGui::Button("Import save...")) {
        const auto source = import_file();
        if (!source.empty()) {
            BwCardManagerOutcome outcome{};
            report(bluewake_card_manager_stage(g_manager, source.c_str(), &outcome), outcome,
                "Import queued for the next launch.");
            refresh_pending();
        }
    }
    ImGui::TextWrapped("Import .card, GZLE01 Wind Waker .gci, or GameCube .raw. Original-format imports include all three quest logs and pictures; other games in a raw card are excluded.");
    ImGui::TextWrapped("Backups: %s", g_backup_path.c_str());
    if (!g_list_error.empty()) ImGui::TextWrapped("%s", g_list_error.c_str());
    if (g_backups.empty()) ImGui::TextDisabled("No backups listed.");
    else if (ImGui::BeginListBox("##card-backups", ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 7))) {
        for (size_t i = 0; i < g_backups.size(); ++i) {
            const auto& backup = g_backups[i];
            const auto label = backup_label(backup);
            ImGui::PushID(static_cast<int>(i));
            ImGui::BeginDisabled(!backup.valid);
            if (ImGui::Selectable(label.c_str(), g_selected == static_cast<int>(i))) g_selected = static_cast<int>(i);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", backup.path);
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
    if (g_backup_total > g_backups.size())
        ImGui::TextDisabled("Showing the newest %u of %llu backups.", static_cast<unsigned>(g_backups.size()),
            static_cast<unsigned long long>(g_backup_total));
    const bool selected_valid = g_selected >= 0 && static_cast<size_t>(g_selected) < g_backups.size() &&
        g_backups[static_cast<size_t>(g_selected)].valid;
    ImGui::BeginDisabled(!selected_valid);
    if (ImGui::Button("Restore selected backup on next launch") && selected_valid) {
        BwCardManagerOutcome outcome{};
        const auto& backup = g_backups[static_cast<size_t>(g_selected)];
        report(bluewake_card_manager_stage(g_manager, backup.path, &outcome), outcome,
            "Restore queued for the next launch.");
        refresh_pending();
    }
    ImGui::EndDisabled();
    if (g_pending.present) {
        ImGui::TextWrapped("Queued replacement: %s", g_pending.valid ? "valid; applies on next launch" : "invalid; startup will stop until it is cancelled or replaced");
        ImGui::TextWrapped("%s", g_pending.path);
        if (ImGui::Button("Cancel queued replacement")) {
            BwCardManagerOutcome outcome{};
            report(bluewake_card_manager_cancel(g_manager, &outcome), outcome, "Queued replacement cancelled.");
            refresh_pending();
        }
    } else ImGui::TextDisabled("No replacement queued.");
    if (!g_pending_error.empty()) ImGui::TextWrapped("%s", g_pending_error.c_str());
    if (!g_status.empty()) ImGui::TextWrapped("%s", g_status.c_str());
    if (!g_error.empty()) ImGui::TextWrapped("%s", g_error.c_str());
    ImGui::TextWrapped("Import and restore leave the running card untouched. On the next launch, the current card is backed up before replacement.");
}

extern "C" void bw_card_menu_shutdown(void) {
    if (g_manager) bluewake_card_manager_destroy(g_manager);
    g_manager = nullptr;
    g_pending = {};
    g_backups.clear();
    g_card_path.clear();
    g_backup_path.clear();
    g_status.clear();
    g_error.clear();
    g_list_error.clear();
    g_pending_error.clear();
    g_backup_total = 0;
    g_selected = -1;
    g_loaded = false;
#if defined(BLUEWAKE_CARD_MENU_TEST)
    g_test_import_path.clear();
#endif
}
