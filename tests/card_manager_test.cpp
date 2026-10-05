// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "card_manager.h"
#include "gxruntime/memory_card.h"
#ifdef rename
#undef rename
#endif
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace {
void check(bool yes, const std::string& message) { if (!yes) throw std::runtime_error(message); }
std::string utf8(const fs::path& path) {
    const auto encoded = path.u8string();
    return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}
std::string bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    check(input.good(), "Missing fixture artifact: " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void write(const fs::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    check(output.good(), "Unable to write synthetic fixture data");
}
DolMemoryCardConfig config(const fs::path& path, std::string& storage) {
    storage = utf8(path);
    DolMemoryCardConfig result{};
    result.path = storage.c_str();
    result.size_mbits = 4;
    std::memcpy(result.game_code, "GZLE", 4);
    std::memcpy(result.company, "01", 2);
    return result;
}
using Card = std::unique_ptr<DolMemoryCard, decltype(&dol_card_close)>;
Card open_card(const fs::path& path) {
    std::string storage;
    auto options = config(path, storage);
    Card card(dol_card_open(&options), dol_card_close);
    check(card != nullptr && dol_card_mount(card.get()) == DOL_CARD_RESULT_READY, "Actual GXRuntime card could not be opened/mounted");
    return card;
}
void put_value(DolMemoryCard* card, unsigned char value) {
    s32 file = -1;
    if (dol_card_open_file(card, "synthetic", &file, nullptr) == DOL_CARD_RESULT_NO_FILE)
        check(dol_card_create_file(card, "synthetic", 8192, &file) == DOL_CARD_RESULT_READY, "Synthetic file create failed");
    std::vector<unsigned char> data(8192, value);
    check(dol_card_write_file(card, file, 0, data.data(), static_cast<u32>(data.size())) == DOL_CARD_RESULT_READY,
          "Actual card write failed");
}
unsigned char value(const fs::path& path) {
    auto card = open_card(path);
    s32 file = -1;
    check(dol_card_open_file(card.get(), "synthetic", &file, nullptr) == DOL_CARD_RESULT_READY, "Synthetic file not preserved");
    unsigned char result = 0;
    check(dol_card_read_file(card.get(), file, 0, &result, 1) == DOL_CARD_RESULT_READY, "Restored bytes could not be read");
    return result;
}
void make_card(const fs::path& path, unsigned char content) { auto card = open_card(path); put_value(card.get(), content); }
struct SnapshotBridge {
    std::string expected;
    unsigned begun = 0, ended = 0;
    bool suspended = false, allow = true;
};
bool begin_snapshot(const char* path, void* user) {
    auto& bridge = *static_cast<SnapshotBridge*>(user);
    if (!bridge.allow || bridge.expected != path) return false;
    check(!bridge.suspended, "Nested snapshot suspension");
    bridge.suspended = true;
    ++bridge.begun;
    return true;
}
void end_snapshot(void* user) {
    auto& bridge = *static_cast<SnapshotBridge*>(user);
    bridge.suspended = false;
    ++bridge.ended;
}
using Manager = std::unique_ptr<BwCardManager, decltype(&bluewake_card_manager_destroy)>;
Manager manager(const fs::path& card, const fs::path& backups, SnapshotBridge* bridge = nullptr) {
    const auto card_name = utf8(card), backup_name = utf8(backups);
    BwCardManagerConfig options{};
    options.card_path = card_name.c_str();
    options.backup_directory = backup_name.c_str();
    if (bridge) { options.begin_snapshot = begin_snapshot; options.end_snapshot = end_snapshot; options.user = bridge; }
    BwCardManagerOutcome result{};
    Manager handle(bluewake_card_manager_create(&options, &result), bluewake_card_manager_destroy);
    check(handle != nullptr, result.message);
    return handle;
}
void stage(BwCardManager* manager, const fs::path& source) {
    BwCardManagerOutcome result{};
    const auto name = utf8(source);
    check(bluewake_card_manager_stage(manager, name.c_str(), &result), result.message);
    check(!result.card_changed && !result.startup_ready, "Staging claimed to replace a live card");
}
void pending(BwCardManager* manager, bool wanted) {
    BwCardManagerPending metadata{};
    BwCardManagerOutcome result{};
    check(bluewake_card_manager_pending(manager, &metadata, &result), result.message);
    check(metadata.present == wanted && (!wanted || (metadata.valid && metadata.bytes > 40)), "Incorrect pending status");
}
std::vector<BwCardManagerBackup> backups(BwCardManager* manager) {
    size_t total = 0;
    BwCardManagerOutcome result{};
    check(bluewake_card_manager_list_backups(manager, nullptr, 0, &total, &result), result.message);
    std::vector<BwCardManagerBackup> found(total);
    check(bluewake_card_manager_list_backups(manager, found.data(), found.size(), &total, &result), result.message);
    check(total == found.size(), "Backup metadata count changed without an operation");
    for (const auto& entry : found) check(entry.modified_unix_seconds > 0, "Backup metadata omitted its timestamp");
    return found;
}
}

int main(int argc, char** argv) {
    fs::path root;
    try {
        const auto parent = argc > 1 ? fs::absolute(argv[1]) : fs::temp_directory_path();
        root = parent / ("card-manager-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directories(root), "Unable to create isolated card fixture");
        const auto current = root / "GZLE01.card";
        const auto queued = root / "GZLE01.card.pending";
        const auto source = root / "source.card";
        const auto other = root / "other.card";
        make_card(current, 0x11);
        make_card(source, 0x44);
        make_card(other, 0x55);
        const auto original = bytes(current);
        const auto imported = bytes(source);
        SnapshotBridge bridge{utf8(current)};
        auto live_manager = manager(current, root / "Backups", &bridge);
        auto offline_manager = manager(current, root / "Backups");
        auto live = open_card(current);
        BwCardManagerOutcome result{};

        check(!bluewake_card_manager_backup(offline_manager.get(), &result) && result.code == BW_CARD_MANAGER_BUSY,
              "Backup without a quiesce bridge ignored the live backend lock");
        check(bluewake_card_manager_backup(live_manager.get(), &result), result.message);
        check(result.backup_valid && bytes(fs::u8path(result.backup_path)) == original,
              "Backup was not an exact copy of persistent GXRuntime bytes");
        const std::string first_backup = result.backup_path;
        check(bridge.begun == 1 && bridge.ended == 1 && !bridge.suspended, "Successful backup did not resume its bridge");
        check(bluewake_card_manager_backup(live_manager.get(), &result) && first_backup != result.backup_path,
              "Repeated backups reused a filename");
        bridge.allow = false;
        check(!bluewake_card_manager_backup(live_manager.get(), &result) && result.code == BW_CARD_MANAGER_BUSY,
              "Failed live-card quiesce still copied a backup");
        bridge.allow = true;
        bluewake_card_manager_test_fail_once(BW_CARD_TEST_BACKUP_WRITE);
        check(!bluewake_card_manager_backup(live_manager.get(), &result), "Injected live backup write failure was ignored");
        check(bridge.begun == bridge.ended && !bridge.suspended && bytes(current) == original,
              "Backup failure left writes suspended or changed the card");
        stage(live_manager.get(), source);
        pending(live_manager.get(), true);
        check(bytes(current) == original, "Staging changed persistent live card bytes");
        check(bluewake_card_manager_apply_at_startup(live_manager.get(), &result) == BW_CARD_MANAGER_BUSY &&
              !result.startup_ready && !result.card_changed && bytes(queued) == imported,
              "Startup apply replaced a card while its backend was open");
        // A normal save after staging must be the bytes automatically backed
        // up on next launch. The same live card handle remains usable.
        put_value(live.get(), 0x22);
        const auto latest_save = bytes(current);
        live.reset();
        check(bluewake_card_manager_apply_at_startup(live_manager.get(), &result) == BW_CARD_MANAGER_OK &&
              result.startup_ready && result.card_changed && result.backup_valid, result.message);
        check(bytes(fs::u8path(result.backup_path)) == latest_save && bytes(current) == imported,
              "Startup did not preserve the latest save and publish the imported bytes");
        check(bytes(fs::u8path(result.recovery_path)) == imported, "Applied request bytes were not retained");
        pending(live_manager.get(), false);
        check(value(current) == 0x44, "Actual GXRuntime did not read imported card data after startup apply");
        check(backups(live_manager.get()).size() == 4, "Manual/failure/automatic backups missing from metadata");

        // Invalid candidates and failed staging must retain an earlier valid
        // queued request, alongside the original source and live card bytes.
        stage(live_manager.get(), source);
        const auto bad = root / "invalid.gci";
        write(bad, "Not a DOLCARD1 container");
        auto bad_name = utf8(bad);
        check(!bluewake_card_manager_stage(live_manager.get(), bad_name.c_str(), &result) &&
              result.code == BW_CARD_MANAGER_INVALID && bytes(queued) == imported && bytes(current) == imported,
              "Invalid import discarded a valid pending request or changed saves");
        for (auto failure : {BW_CARD_TEST_STAGE_FLUSH, BW_CARD_TEST_STAGE_PUBLISH}) {
            bluewake_card_manager_test_fail_once(failure);
            const auto name = utf8(other);
            check(!bluewake_card_manager_stage(live_manager.get(), name.c_str(), &result) &&
                  bytes(queued) == imported && bytes(current) == imported && fs::exists(fs::u8path(result.recovery_path)),
                  "Staging failure lost current/queued/recovery bytes");
        }
        check(bluewake_card_manager_cancel(live_manager.get(), &result) && bytes(fs::u8path(result.recovery_path)) == imported,
              "Cancel did not retain the staged bytes");
        pending(live_manager.get(), false);
        check(bytes(current) == imported, "Cancel changed the current card");

        stage(live_manager.get(), other);
        bluewake_card_manager_test_fail_once(BW_CARD_TEST_BACKUP_WRITE);
        check(bluewake_card_manager_apply_at_startup(live_manager.get(), &result) == BW_CARD_MANAGER_IO_ERROR &&
              !result.startup_ready && !result.card_changed && bytes(current) == imported,
              "Failed automatic backup did not stop replacement");
        pending(live_manager.get(), true);
        bluewake_card_manager_test_fail_once(BW_CARD_TEST_CURRENT_PUBLISH);
        check(bluewake_card_manager_apply_at_startup(live_manager.get(), &result) == BW_CARD_MANAGER_IO_ERROR &&
              !result.startup_ready && !result.card_changed && result.backup_valid &&
              bytes(fs::u8path(result.backup_path)) == imported && bytes(fs::u8path(result.recovery_path)) == bytes(other),
              "Failed atomic replacement did not retain previous card/backup/validated candidate");
        bluewake_card_manager_test_fail_once(BW_CARD_TEST_AFTER_CURRENT_SYNC);
        check(bluewake_card_manager_apply_at_startup(live_manager.get(), &result) == BW_CARD_MANAGER_INCOMPLETE &&
              result.card_changed && !result.startup_ready && bytes(current) == bytes(other),
              "Publication durability failure incorrectly allowed game startup");
        pending(live_manager.get(), true);
        const auto backup_count = backups(live_manager.get()).size();
        check(bluewake_card_manager_apply_at_startup(live_manager.get(), &result) == BW_CARD_MANAGER_OK &&
              result.startup_ready && !result.card_changed && backups(live_manager.get()).size() == backup_count,
              "Retry did not finish a published transaction idempotently");
        pending(live_manager.get(), false);

        stage(live_manager.get(), source);
        bluewake_card_manager_test_fail_once(BW_CARD_TEST_CONSUME_PENDING);
        check(bluewake_card_manager_apply_at_startup(live_manager.get(), &result) == BW_CARD_MANAGER_INCOMPLETE &&
              result.card_changed && !result.startup_ready, "Queue-consumption failure incorrectly allowed game startup");
        pending(live_manager.get(), true);
        check(bluewake_card_manager_apply_at_startup(live_manager.get(), &result) == BW_CARD_MANAGER_OK && result.startup_ready,
              "Queue-consumption retry failed");
        live = open_card(current);
        put_value(live.get(), 0x77);
        live.reset();
        const auto later_save = bytes(current);
        check(bluewake_card_manager_apply_at_startup(live_manager.get(), &result) == BW_CARD_MANAGER_NO_PENDING &&
              result.startup_ready && bytes(current) == later_save && value(current) == 0x77,
              "A completed request replayed over a later gameplay save");

        stage(live_manager.get(), source);
        write(queued, "damaged pending request");
        const auto damaged_queue = bytes(queued);
        check(bluewake_card_manager_apply_at_startup(live_manager.get(), &result) == BW_CARD_MANAGER_INVALID &&
              !result.startup_ready && bytes(current) == later_save && bytes(queued) == damaged_queue,
              "A damaged queue was applied or discarded");
        BwCardManagerPending damaged{};
        check(!bluewake_card_manager_pending(live_manager.get(), &damaged, &result) && damaged.present && !damaged.valid,
              "Status failed to report a damaged pending request");
        check(bluewake_card_manager_cancel(live_manager.get(), &result) && bytes(fs::u8path(result.recovery_path)) == damaged_queue,
              "Cancelling damaged queue lost its recovery bytes");

        // Restoration must also preserve a broken current card verbatim so a
        // user can recover it externally, rather than overwriting the evidence.
        write(current, "damaged current card");
        const auto damaged_current = bytes(current);
        stage(live_manager.get(), source);
        check(bluewake_card_manager_apply_at_startup(live_manager.get(), &result) == BW_CARD_MANAGER_OK &&
              result.startup_ready && !result.backup_valid && bytes(fs::u8path(result.backup_path)) == damaged_current &&
              bytes(current) == imported, "Restoring a damaged card did not keep its original bytes");

        const auto new_card = root / "new-user" / "GZLE01.card";
        auto fresh = manager(new_card, root / "new-user" / "Backups");
        stage(fresh.get(), source);
        check(bluewake_card_manager_apply_at_startup(fresh.get(), &result) == BW_CARD_MANAGER_OK && result.startup_ready &&
              result.card_changed && result.backup_path[0] == '\0' && bytes(new_card) == imported,
              "First-launch import created an unrelated empty card or a bogus backup");
        check(value(new_card) == 0x44, "First-launch imported data was not usable by GXRuntime");
        check(bridge.begun == bridge.ended && !bridge.suspended, "Snapshot bridge did not balance on all failure paths");

        std::ofstream(root / "result.txt") << "PASS: exact persistent card backups, unique metadata, live-lock refusal, "
            "stage/cancel, most recent persistent bytes backed up at apply, original malformed-byte recovery, atomic publication, "
            "flush/backup/publication/durability/cleanup failures, idempotent restart recovery and no request replay. "
            "Actual GXRuntime serializer/validator used; synthetic files only. Original formats are covered by the separate card-import fixture.\n";
        std::cout << "PASS card manager; evidence: " << root.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Card manager regression failed: " << error.what() << "\nEvidence: " << root.string() << '\n';
        return 1;
    }
}
