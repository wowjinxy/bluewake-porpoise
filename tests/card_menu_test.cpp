// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "card_menu.h"
#include "card_menu_test_api.h"
#include "card_import_test_support.h"
#include "atomic_file.h"
#include "gxruntime/memory_card.h"
#include <imgui.h>
#include <imgui_internal.h>
#ifdef rename
#undef rename
#endif
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace {
struct Item { ImRect bounds; std::string label; };
std::map<ImGuiID, Item> items;
std::string rendered, runtime_path;
unsigned begun, ended;
bool held, allow_snapshot = true;

void check(bool yes, const std::string& message) { if (!yes) throw std::runtime_error(message); }
std::string utf8(const fs::path& path) {
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
std::string bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    check(input.good(), "Missing fixture file: " + utf8(path));
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void write(const fs::path& path, const std::string& data) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    check(output.good(), "Synthetic card fixture write failed");
}
using Card = std::unique_ptr<DolMemoryCard, decltype(&dol_card_close)>;
Card open_card(const fs::path& path) {
    const auto name = utf8(path);
    DolMemoryCardConfig config{};
    config.path = name.c_str(); config.size_mbits = 4;
    std::memcpy(config.game_code, "GZLE", 4); std::memcpy(config.company, "01", 2);
    Card card(dol_card_open(&config), dol_card_close);
    check(card != nullptr && dol_card_mount(card.get()) == DOL_CARD_RESULT_READY,
        "Actual memory-card serializer could not mount the synthetic card");
    return card;
}
void put_value(DolMemoryCard* card, unsigned char value) {
    s32 file = -1;
    if (dol_card_open_file(card, "synthetic", &file, nullptr) == DOL_CARD_RESULT_NO_FILE)
        check(dol_card_create_file(card, "synthetic", 8192, &file) == DOL_CARD_RESULT_READY, "Card file creation failed");
    std::vector<unsigned char> data(8192, value);
    check(dol_card_write_file(card, file, 0, data.data(), static_cast<u32>(data.size())) == DOL_CARD_RESULT_READY,
        "Actual card serializer write failed");
}
void make_card(const fs::path& path, unsigned char value) { auto card = open_card(path); put_value(card.get(), value); }
unsigned char read_value(DolMemoryCard* card) {
    s32 file = -1; unsigned char value = 0;
    check(dol_card_open_file(card, "synthetic", &file, nullptr) == DOL_CARD_RESULT_READY &&
        dol_card_read_file(card, file, 0, &value, 1) == DOL_CARD_RESULT_READY, "Actual card serializer read failed");
    return value;
}
std::vector<fs::path> backups(const fs::path& folder) {
    std::vector<fs::path> found;
    if (fs::exists(folder)) for (const auto& entry : fs::directory_iterator(folder))
        if (entry.is_regular_file() && entry.path().extension() == ".card") found.push_back(entry.path());
    return found;
}
bool contains_backup(const fs::path& folder, const std::string& data) {
    for (const auto& path : backups(folder)) if (bytes(path) == data) return true;
    return false;
}
void frame() {
    items.clear();
    ImGui::NewFrame();
    ImGui::LogToBuffer(0);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1200, 950), ImGuiCond_Always);
    ImGui::Begin("Card menu fixture", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    bw_card_menu_draw();
    ImGui::End();
    rendered.assign(GImGui->LogBuffer.begin(), GImGui->LogBuffer.end());
    ImGui::LogFinish();
    ImGui::Render();
    check(ImGui::GetDrawData() && ImGui::GetDrawData()->TotalVtxCount > 0, "Actual card menu emitted no draw geometry");
}
bool has_item(const std::string& text, bool prefix = false) {
    for (const auto& [id, item] : items)
        if (prefix ? item.label.rfind(text, 0) == 0 : item.label == text) return true;
    return false;
}
void click(const std::string& text, bool prefix = false) {
    bool found = false; ImVec2 position;
    for (const auto& [id, item] : items) {
        if (prefix ? item.label.rfind(text, 0) == 0 : item.label == text) {
            check(!found, "UI widget lookup was ambiguous: " + text);
            found = true; position = item.bounds.GetCenter();
        }
    }
    check(found, "Actual card menu did not expose widget: " + text);
    auto& io = ImGui::GetIO();
    // These events belong solely to this in-memory ImGui context. They do not
    // use any backend, native window, cursor, message queue or desktop input.
    io.AddMousePosEvent(position.x, position.y); frame();
    io.AddMouseButtonEvent(0, true); frame();
    io.AddMouseButtonEvent(0, false); frame(); frame();
}
void assert_current(const fs::path& card, const std::string& expected) {
    check(!held && begun == ended, "Snapshot bridge remained held");
    check(bytes(card) == expected, "Card-menu action changed the running persistent card");
}
void canceled_copy(const fs::path& folder, const std::string& expected) {
    for (const auto& entry : fs::directory_iterator(folder))
        if (utf8(entry.path().filename()).rfind("GZLE01.card.cancelled-", 0) == 0 && bytes(entry.path()) == expected) return;
    throw std::runtime_error("Cancel failed to retain staged bytes for recovery");
}
}

extern "C" const char* bluewake_card_runtime_path(void) { return runtime_path.empty() ? nullptr : runtime_path.c_str(); }
extern "C" bool bluewake_card_runtime_begin_snapshot(const char* expected) {
    check(!held, "Nested runtime snapshot bridge");
    check(expected && runtime_path == expected, "Menu did not pass the copied RAW runtime path to snapshot begin");
    if (!allow_snapshot) return false;
    held = true; ++begun; return true;
}
extern "C" void bluewake_card_runtime_end_snapshot(void) {
    check(held, "Unmatched runtime snapshot end"); held = false; ++ended;
}

void ImGuiTestEngineHook_ItemAdd(ImGuiContext*, ImGuiID id, const ImRect& bounds, const ImGuiLastItemData*) {
    if (id) items[id].bounds = bounds;
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID id, const char* label, ImGuiItemStatusFlags) {
    items[id].label = label ? label : "";
}
void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID id) {
    const auto at = items.find(id); return at == items.end() ? nullptr : at->second.label.c_str();
}
// Aurora's ImGui configuration replaces file functions. This fixture uses
// ordinary files in its own temporary folder and has no SDL dependency.
ImFileHandle ImFileOpen(const char* path, const char* mode) { return reinterpret_cast<ImFileHandle>(std::fopen(path, mode)); }
bool ImFileClose(ImFileHandle file) { return !file || std::fclose(reinterpret_cast<FILE*>(file)) == 0; }
ImU64 ImFileGetSize(ImFileHandle file) {
    auto* stream = reinterpret_cast<FILE*>(file); if (!stream) return static_cast<ImU64>(-1);
    const auto at = std::ftell(stream); std::fseek(stream, 0, SEEK_END); const auto size = std::ftell(stream);
    std::fseek(stream, at, SEEK_SET); return size < 0 ? static_cast<ImU64>(-1) : static_cast<ImU64>(size);
}
ImU64 ImFileRead(void* data, ImU64 size, ImU64 count, ImFileHandle file) {
    return std::fread(data, static_cast<size_t>(size), static_cast<size_t>(count), reinterpret_cast<FILE*>(file));
}
ImU64 ImFileWrite(const void* data, ImU64 size, ImU64 count, ImFileHandle file) {
    return std::fwrite(data, static_cast<size_t>(size), static_cast<size_t>(count), reinterpret_cast<FILE*>(file));
}

int main(int argc, char** argv) {
    fs::path folder;
    try {
        const auto parent = fs::absolute(argc > 1 ? fs::u8path(argv[1]) : fs::temp_directory_path()).lexically_normal();
        folder = parent / ("card-menu-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directories(folder), "Could not create isolated fixture directory");
        const auto card_directory = folder / fs::u8path("saves-\xE2\x98\x83");
        check(fs::create_directories(card_directory), "Could not create Unicode active-card directory");
        const auto current = card_directory / "GZLE01.card", pending = card_directory / "GZLE01.card.pending";
        const auto source = folder / fs::u8path("import-\xE2\x98\x83.card"), other = folder / "other.card";
        const auto backup_folder = card_directory / "Backups" / "GZLE01";
        make_card(current, 0x11); make_card(source, 0x44); make_card(other, 0x55);
        const auto original = bytes(current), imported = bytes(source);
        const auto name = utf8(current);
        check(dol_card_validate(name.c_str()) && dol_card_validate(utf8(source).c_str()),
            "Backend validator did not preserve Unicode active/source path identity");
        check(bw_atomic_flush_path(name.c_str()), "Atomic helper could not flush Unicode card path");
        const auto migrated = card_directory / fs::u8path("migrated-\xE2\x98\x83.card");
        const auto migrated_name = utf8(migrated);
        check(bw_atomic_copy_if_missing(utf8(source).c_str(), migrated_name.c_str()) && bytes(migrated) == imported,
            "Atomic startup migration did not copy exact Unicode source/target bytes");
        check(bw_atomic_copy_if_missing(name.c_str(), migrated_name.c_str()) && bytes(migrated) == imported,
            "Atomic migration replaced an existing Unicode target");
        check(bw_card_menu_prepare(name.c_str()), bw_card_menu_error());
        check(backups(backup_folder).empty(), "Ordinary startup unexpectedly created a backup");
        auto live = open_card(current);
        DolMemoryCardConfig lock_probe{};
        lock_probe.path = name.c_str(); lock_probe.size_mbits = 4;
        Card blocked(dol_card_open(&lock_probe), dol_card_close);
        check(!blocked, "Unicode card path did not retain the actual backend's exclusive OS lock");
        runtime_path = name;
        // Distinct slash and ASCII case spelling must still identify this
        // Windows file, while begin receives exactly this raw spelling.
        for (char& c : runtime_path) {
            if (c == '\\') c = '/';
            else if (static_cast<unsigned char>(c) < 128) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        IMGUI_CHECKVERSION(); ImGui::CreateContext();
        auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(1280, 1024); io.DeltaTime = 1.f / 60; io.ConfigInputTrickleEventQueue = false;
        io.Fonts->AddFontDefault(); unsigned char* pixels = nullptr; int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        check(pixels && width > 0 && height > 0, "In-memory ImGui font build failed");
        GImGui->TestEngineHookItems = true; frame(); frame();
        check(rendered.find("Use the game Save first") != std::string::npos &&
            rendered.find("all three quest logs and pictures") != std::string::npos, "Required card-format/save guidance is missing");
        click("Backup card now");
        auto found = backups(backup_folder);
        check(found.size() == 1 && bytes(found[0]) == original && begun == 1 && ended == 1,
            "Actual Backup handler did not copy exact persistent bytes through runtime snapshot bridge");
        const auto first_backup = found[0];
        assert_current(current, original);
        const auto correct_runtime = runtime_path; runtime_path = utf8(other);
        click("Backup card now");
        check(begun == 1 && backups(backup_folder).size() == 1 && *bw_card_menu_error(),
            "Snapshot bridge accepted the wrong active runtime card");
        runtime_path = correct_runtime;

        const auto external = backup_folder / "GZLE01-external.card";
        const auto damaged = backup_folder / "GZLE01-damaged.card";
        fs::copy_file(other, external); write(damaged, "damaged synthetic backup");
        for (int i = 0; i < 12; ++i) frame();
        check(!has_item("GZLE01-external.card", true) && !has_item("GZLE01-damaged.card", true),
            "Unchanged card-menu frames scanned the filesystem instead of using the cached backup list");
        click("Refresh backups");
        check(has_item("GZLE01-external.card", true) && has_item("GZLE01-damaged.card", true), "Refresh did not discover backup metadata");
        click(utf8(first_backup.filename()), true);
        put_value(live.get(), 0x22); const auto latest_save = bytes(current);
        check(bytes(fs::u8path(name + ".bak")) == original,
            "Backend rotated backup lost exact prior bytes under Unicode active path");
        click("Restore selected backup on next launch");
        check(fs::exists(pending) && bytes(pending) == original && bw_card_menu_restart_needed(),
            "Restore handler did not queue selected validated backup and mark restart needed");
        assert_current(current, latest_save);
        check(read_value(live.get()) == 0x22, "Restore changed the open serializer's current card");
        click("Cancel queued replacement");
        check(!fs::exists(pending) && !bw_card_menu_restart_needed(), "Cancel did not dequeue and clear restart status");
        canceled_copy(card_directory, original); assert_current(current, latest_save);

        const auto native_gci = folder / fs::u8path("native-\xE2\x98\x83.gci");
        const auto gci_bytes = card_import_fixture::gci();
        write(native_gci, std::string(reinterpret_cast<const char*>(gci_bytes.data()), gci_bytes.size()));
        bw_card_menu_test_set_import_path(utf8(native_gci).c_str()); click("Import save...");
        check(fs::exists(pending) && dol_card_validate(utf8(pending).c_str()) && bw_card_menu_restart_needed(),
            "Actual Import handler did not convert/validate original GCI");
        auto converted = open_card(pending); int32_t native_file = -1; uint32_t native_length = 0;
        check(dol_card_open_file(converted.get(), "gczelda", &native_file, &native_length) == 0 && native_length == 0x18000,
            "Converted menu import lost the native game file");
        std::vector<unsigned char> native_payload(native_length);
        check(dol_card_read_file(converted.get(), native_file, 0, native_payload.data(), native_length) == 0 &&
            std::equal(native_payload.begin(), native_payload.end(), gci_bytes.begin() + 64), "Menu import altered quest logs or pictures");
        converted.reset(); assert_current(current, latest_save);
        click("Cancel queued replacement");
        check(!fs::exists(pending), "Converted GCI could not be cancelled");
        bw_card_menu_test_set_import_path(utf8(source).c_str()); click("Import save...");
        check(bytes(pending) == imported && bw_card_menu_restart_needed(), "Import button did not stage actual synthetic .card bytes");
        assert_current(current, latest_save);
        bw_card_menu_test_set_import_path(""); click("Import save...");
        check(bytes(pending) == imported && bw_card_menu_restart_needed(), "Cancelled picker changed queued replacement");
        const auto invalid = folder / "invalid.card"; write(invalid, "not a card");
        bw_card_menu_test_set_import_path(utf8(invalid).c_str()); click("Import save...");
        check(bytes(pending) == imported && *bw_card_menu_error() && bw_card_menu_restart_needed(),
            "Invalid import replaced an earlier valid queued request");
        allow_snapshot = false; click("Backup card now"); allow_snapshot = true;
        check(begun == ended && backups(backup_folder).size() == 3 && *bw_card_menu_error(), "Denied runtime snapshot created a backup or leaked its bridge");
        assert_current(current, latest_save);

        check(!bw_card_menu_prepare(name.c_str()) && *bw_card_menu_error() && bytes(pending) == imported,
            "Startup apply ignored an open backend or failed to retain its fatal message");
        assert_current(current, latest_save);
        live.reset(); runtime_path.clear(); bw_card_menu_shutdown();
        check(bw_card_menu_prepare(name.c_str()), bw_card_menu_error());
        check(bytes(current) == imported && !fs::exists(pending) && contains_backup(backup_folder, latest_save),
            "Prepare did not preserve latest card, apply queued import and consume it before runtime startup");
        check(begun == 1 && ended == 1, "Startup apply incorrectly used the live snapshot bridge");
        frame(); frame(); check(!bw_card_menu_restart_needed(), "Applied queue left restart flag set");
        live = open_card(current); check(read_value(live.get()) == 0x44, "Actual serializer could not read startup-applied import");
        put_value(live.get(), 0x66); const auto later_save = bytes(current); live.reset();
        const auto count_before = backups(backup_folder).size(); bw_card_menu_shutdown();
        check(bw_card_menu_prepare(name.c_str()), bw_card_menu_error());
        check(bytes(current) == later_save && backups(backup_folder).size() == count_before,
            "A later launch replayed an applied import or created an ordinary-launch backup");
        frame(); frame();
        write(pending, "damaged pending card"); click("Refresh backups");
        check(!bw_card_menu_restart_needed() && rendered.find("invalid; startup will stop") != std::string::npos,
            "Damaged pending card was not displayed as invalid");
        click("Cancel queued replacement");
        check(!fs::exists(pending) && !bw_card_menu_restart_needed() && bytes(current) == later_save,
            "Invalid queued replacement could not be cancelled without changing saves");
        bw_card_menu_shutdown(); ImGui::DestroyContext();
        // Cleanup is restricted to the fresh, direct child this fixture made.
        check(folder.parent_path() == parent && utf8(folder.filename()).rfind("card-menu-", 0) == 0,
            "Fixture cleanup escaped its explicit parent");
        fs::remove_all(folder);
        std::cout << "Actual offscreen card menu backup/cache/restore/import/cancel/startup regression passed\n";
        return 0;
    } catch (const std::exception& error) {
        bw_card_menu_shutdown(); if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
        std::cerr << error.what() << "\nRetained isolated fixture: " << utf8(folder) << '\n';
        return 1;
    }
}
