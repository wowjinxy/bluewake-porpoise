// SPDX-License-Identifier: GPL-3.0-or-later
#include "card_manager.h"
#include "card_import.h"
#include "gxruntime/memory_card.h"
#ifdef rename
#undef rename
#endif
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include "atomic_file.h"
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <process.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
struct BwCardManager {
    fs::path card, backups, pending;
    bool (*begin_snapshot)(const char*, void*) = nullptr;
    void (*end_snapshot)(void*) = nullptr;
    void* user = nullptr;
    std::mutex mutex;
};
namespace {
constexpr uint64_t max_card_bytes = 128ull * 16 * 8192 + 127 * 68 + 40;
std::atomic<uint64_t> sequence{0};
enum FailurePoint { BackupWrite = 1, StageFlush, StagePublish, CurrentPublish, AfterCurrentSync, ConsumePending, ConversionFlush };
#ifdef BLUEWAKE_CARD_MANAGER_TEST
std::atomic<int> injected_failure{0};
#endif
bool fail_once(int phase) {
    if (phase <= 0) return false;
#ifdef BLUEWAKE_CARD_MANAGER_TEST
    int expected = phase;
    return injected_failure.compare_exchange_strong(expected, 0);
#else
    (void)phase;
    return false;
#endif
}
std::string path_text(const fs::path& path) {
    const auto utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}
fs::path from_utf8(const std::string& value) {
#ifdef __cpp_char8_t
    return fs::path(std::u8string(value.begin(), value.end()));
#else
    return fs::u8path(value);
#endif
}
void text(char* buffer, size_t size, const std::string& value) { std::snprintf(buffer, size, "%s", value.c_str()); }
BwCardManagerOutcome& start(BwCardManagerOutcome* requested, BwCardManagerOutcome& local) {
    auto& result = requested ? *requested : local;
    result = {};
    result.code = BW_CARD_MANAGER_OK;
    return result;
}
bool error(BwCardManagerOutcome& result, BwCardManagerCode code, const char* message) {
    result.code = code;
    result.startup_ready = false;
    text(result.message, sizeof result.message, message);
    return false;
}
void recovery(BwCardManagerOutcome& result, const fs::path& path) {
    text(result.recovery_path, sizeof result.recovery_path, path_text(path));
}
std::string unique_id() {
    const auto now = std::chrono::system_clock::now();
    const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    const time_t seconds = std::chrono::system_clock::to_time_t(now);
    tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &seconds);
    const auto pid = _getpid();
#else
    gmtime_r(&seconds, &utc);
    const auto pid = getpid();
#endif
    char date[40];
    std::strftime(date, sizeof date, "%Y%m%d-%H%M%S", &utc);
    return std::string(date) + "-" + std::to_string(pid) + "-" + std::to_string(nanos) + "-" + std::to_string(++sequence);
}
fs::path adjacent(const fs::path& path, const char* purpose) {
    return from_utf8(path_text(path) + purpose + unique_id());
}
bool ensure_parent(const fs::path& path) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    return !ec && fs::is_directory(path.parent_path(), ec) && !ec;
}
// Reject leaf symlinks/non-regular files. A card lock must identify the same
// file path the backend will open; following a leaf alias defeats that lease.
bool regular(const fs::path& path, bool& present) {
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory || (!ec && status.type() == fs::file_type::not_found)) {
        present = false;
        return true;
    }
    present = !ec;
    return !ec && fs::is_regular_file(status);
}
bool validate(const fs::path& path) {
    bool present = false;
    return regular(path, present) && present && dol_card_validate(path_text(path).c_str());
}
class Lease {
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int fd = -1;
#endif
public:
    bool take(const fs::path& card, const char* suffix) {
        const fs::path path = from_utf8(path_text(card) + suffix);
#ifdef _WIN32
        handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        OVERLAPPED offset{};
        if (handle != INVALID_HANDLE_VALUE && LockFileEx(handle,
                LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &offset)) return true;
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
#else
        fd = open(path.c_str(), O_CREAT | O_RDWR, 0600);
        if (fd >= 0 && flock(fd, LOCK_EX | LOCK_NB) == 0) return true;
        if (fd >= 0) close(fd);
        fd = -1;
#endif
        return false;
    }
    ~Lease() {
#ifdef _WIN32
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
#else
        if (fd >= 0) close(fd);
#endif
    }
};
class Snapshot {
    BwCardManager& manager;
    bool active = false;
    Lease lease;
public:
    explicit Snapshot(BwCardManager& value) : manager(value) {}
    bool begin() {
        if (manager.begin_snapshot) {
            active = manager.begin_snapshot(path_text(manager.card).c_str(), manager.user);
            return active;
        }
        return lease.take(manager.card, ".lock");
    }
    ~Snapshot() { if (active) manager.end_snapshot(manager.user); }
};
FILE* read_stream(const fs::path& path) {
#ifdef _WIN32
    return _wfopen(path.c_str(), L"rb");
#else
    return std::fopen(path.c_str(), "rb");
#endif
}
FILE* exclusive_stream(const fs::path& path) {
#ifdef _WIN32
    int fd = _wopen(path.c_str(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
    if (fd < 0) return nullptr;
    FILE* file = _fdopen(fd, "wb");
    if (!file) _close(fd);
#else
    int fd = open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (fd < 0) return nullptr;
    FILE* file = fdopen(fd, "wb");
    if (!file) close(fd);
#endif
    return file;
}
bool sync_directory(const fs::path& file) {
#ifdef _WIN32
    // Every publication uses MOVEFILE_WRITE_THROUGH; file contents were _commit'd.
    (void)file;
    return true;
#else
    int fd = open(file.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) return false;
    bool ok = fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    return ok;
#endif
}
bool copy_snapshot(const fs::path& source, const fs::path& target, int injected_phase = 0) {
    bool present = false;
    if (!regular(source, present) || !present) return false;
    FILE* input = read_stream(source);
    if (!input) return false;
    FILE* output = exclusive_stream(target);
    bool ok = output != nullptr;
    unsigned char bytes[8192];
    uint64_t total = 0;
    size_t count = 0;
    while (ok && (count = std::fread(bytes, 1, sizeof bytes, input)) != 0) {
        total += count;
        ok = total <= max_card_bytes && !(injected_phase == BackupWrite && fail_once(BackupWrite)) &&
            std::fwrite(bytes, 1, count, output) == count;
    }
    if (std::ferror(input)) ok = false;
    if (std::fclose(input) != 0) ok = false;
    if (output) {
        if (ok && (!bw_atomic_flush(output) || (injected_phase == StageFlush && fail_once(StageFlush)))) ok = false;
        if (std::fclose(output) != 0) ok = false;
    }
    return ok;
}
bool read_bytes(const fs::path& path, std::vector<uint8_t>& bytes) {
    FILE* input = read_stream(path);
    if (!input) return false;
    uint8_t buffer[8192]; size_t count;
    bool ok = true;
    while ((count = std::fread(buffer, 1, sizeof buffer, input)) != 0) {
        if (bytes.size() + count > max_card_bytes) { ok = false; break; }
        bytes.insert(bytes.end(), buffer, buffer + count);
    }
    ok = ok && !std::ferror(input);
    if (std::fclose(input) != 0) ok = false;
    return ok;
}
bool write_conversion(const fs::path& path, const std::vector<uint8_t>& bytes) {
    FILE* output = exclusive_stream(path);
    if (!output) return false;
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), output) == bytes.size();
    if (ok && (!bw_atomic_flush(output) || fail_once(ConversionFlush))) ok = false;
    if (std::fclose(output) != 0) ok = false;
    return ok;
}
bool replace(const fs::path& source, const fs::path& target) {
#ifdef _WIN32
    return MoveFileExW(source.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
    return std::rename(source.c_str(), target.c_str()) == 0;
#endif
}
bool move_unique(const fs::path& source, const fs::path& target) {
#ifdef _WIN32
    return MoveFileExW(source.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
#else
    // Publish without replacement. If unlink fails, both immutable names stay
    // available and startup refuses the incomplete cleanup until the next try.
    return link(source.c_str(), target.c_str()) == 0 && unlink(source.c_str()) == 0;
#endif
}
bool same_bytes(const fs::path& a, const fs::path& b) {
    FILE* left = read_stream(a);
    FILE* right = read_stream(b);
    if (!left || !right) { if (left) std::fclose(left); if (right) std::fclose(right); return false; }
    unsigned char x[8192], y[8192];
    bool equal = true;
    size_t nx, ny;
    do {
        nx = std::fread(x, 1, sizeof x, left);
        ny = std::fread(y, 1, sizeof y, right);
        if (nx != ny || std::memcmp(x, y, nx) != 0) { equal = false; break; }
    } while (nx != 0);
    equal = equal && !std::ferror(left) && !std::ferror(right);
    if (std::fclose(left) != 0) equal = false;
    if (std::fclose(right) != 0) equal = false;
    return equal;
}
bool make_backup(BwCardManager& manager, BwCardManagerOutcome& result, bool require_valid) {
    std::error_code ec;
    fs::create_directories(manager.backups, ec);
    if (ec) return error(result, BW_CARD_MANAGER_IO_ERROR, "The backup folder could not be created; saves were not replaced.");
    const auto destination = manager.backups / from_utf8("GZLE01-" + unique_id() + ".card");
    text(result.backup_path, sizeof result.backup_path, path_text(destination));
    if (!copy_snapshot(manager.card, destination, BackupWrite) || !sync_directory(destination)) {
        recovery(result, destination);
        return error(result, BW_CARD_MANAGER_IO_ERROR, "The current card could not be backed up; its bytes and pending replacement were kept.");
    }
    result.backup_valid = validate(destination);
    if (require_valid && !result.backup_valid) {
        recovery(result, destination);
        return error(result, BW_CARD_MANAGER_INVALID, "The current card is damaged. Its original bytes were preserved, but this copy cannot be restored as a valid card.");
    }
    return true;
}
bool consume(BwCardManager& manager, BwCardManagerOutcome& result, const char* suffix) {
    const auto retained = adjacent(manager.card, suffix);
    if (fail_once(ConsumePending) || !move_unique(manager.pending, retained)) {
        recovery(result, manager.pending);
        return error(result, BW_CARD_MANAGER_INCOMPLETE, "The pending request could not be consumed. Keep the game closed and retry; the replacement is retained.");
    }
    recovery(result, retained);
    if (!sync_directory(retained))
        return error(result, BW_CARD_MANAGER_INCOMPLETE, "The completed request is retained, but directory durability could not be confirmed. Keep the game closed and retry.");
    return true;
}
bool manager_lease(BwCardManager& manager, Lease& lease, BwCardManagerOutcome& result) {
    if (!ensure_parent(manager.card)) return error(result, BW_CARD_MANAGER_IO_ERROR, "The card folder is not accessible.");
    if (!lease.take(manager.card, ".manager.lock"))
        return error(result, BW_CARD_MANAGER_BUSY, "Another save-management operation is in progress; no card was changed.");
    return true;
}
}

extern "C" BwCardManager* bluewake_card_manager_create(const BwCardManagerConfig* config, BwCardManagerOutcome* outcome) {
    BwCardManagerOutcome local;
    auto& result = start(outcome, local);
    if (!config || !config->card_path || !config->backup_directory || !*config->card_path || !*config->backup_directory ||
        std::strlen(config->card_path) > 3800 || std::strlen(config->backup_directory) > 3800 ||
        bool(config->begin_snapshot) != bool(config->end_snapshot)) {
        error(result, BW_CARD_MANAGER_INVALID, "Save management needs card and backup paths and a complete snapshot bridge.");
        return nullptr;
    }
    try {
        auto manager = std::make_unique<BwCardManager>();
        manager->card = fs::absolute(from_utf8(config->card_path)).lexically_normal();
        manager->backups = fs::absolute(from_utf8(config->backup_directory)).lexically_normal();
        manager->pending = from_utf8(path_text(manager->card) + ".pending");
        if (path_text(manager->card).size() > 3800 || path_text(manager->backups).size() > 3800) {
            error(result, BW_CARD_MANAGER_INVALID, "Resolved save-management paths are too long.");
            return nullptr;
        }
        manager->begin_snapshot = config->begin_snapshot;
        manager->end_snapshot = config->end_snapshot;
        manager->user = config->user;
        return manager.release();
    } catch (...) { error(result, BW_CARD_MANAGER_IO_ERROR, "Save-management paths could not be initialized."); return nullptr; }
}
extern "C" void bluewake_card_manager_destroy(BwCardManager* manager) { delete manager; }
extern "C" bool bluewake_card_manager_backup(BwCardManager* manager, BwCardManagerOutcome* outcome) {
    BwCardManagerOutcome local; auto& result = start(outcome, local);
    if (!manager) return error(result, BW_CARD_MANAGER_INVALID, "Save management is unavailable.");
    try {
        std::lock_guard<std::mutex> guard(manager->mutex);
        Lease lease;
        if (!manager_lease(*manager, lease, result)) return false;
        Snapshot snapshot(*manager);
        if (!snapshot.begin()) return error(result, BW_CARD_MANAGER_BUSY, "The running card could not be quiesced. No backup or card replacement was made.");
        if (!make_backup(*manager, result, true)) return false;
        text(result.message, sizeof result.message, "The actual persistent card bytes were backed up. Use the game's Save first to include its latest progress.");
        return true;
    } catch (...) { return error(result, BW_CARD_MANAGER_IO_ERROR, "Backup failed; the live card was kept."); }
}
extern "C" bool bluewake_card_manager_stage(BwCardManager* manager, const char* source, BwCardManagerOutcome* outcome) {
    BwCardManagerOutcome local; auto& result = start(outcome, local);
    if (!manager || !source || !*source || std::strlen(source) > 3800)
        return error(result, BW_CARD_MANAGER_INVALID, "Choose a valid BlueWake .card, Wind Waker .gci or GameCube .raw memory card.");
    try {
        std::lock_guard<std::mutex> guard(manager->mutex);
        Lease lease;
        if (!manager_lease(*manager, lease, result)) return false;
        auto candidate = adjacent(manager->card, ".stage-");
        recovery(result, candidate);
        if (!copy_snapshot(from_utf8(source), candidate, StageFlush))
            return error(result, BW_CARD_MANAGER_IO_ERROR, "The import could not be copied and flushed. Existing saves and any earlier pending request were kept.");
        bool converted = false;
        BwCardImportInfo import_info;
        if (!validate(candidate)) {
            std::vector<uint8_t> copied_bytes, container;
            if (!read_bytes(candidate, copied_bytes))
                return error(result, BW_CARD_MANAGER_IO_ERROR, "The copied import could not be read. Existing saves and the earlier pending request were kept.");
            std::string import_error;
            if (!bluewake_card_import_convert(copied_bytes, container, import_info, import_error))
                return error(result, BW_CARD_MANAGER_INVALID, import_error.c_str());
            const auto converted_candidate = adjacent(manager->card, ".converted-");
            recovery(result, converted_candidate);
            if (!write_conversion(converted_candidate, container) || !validate(converted_candidate))
                return error(result, BW_CARD_MANAGER_IO_ERROR, "The converted import could not be flushed and validated by the card backend. Existing saves and the earlier request were kept.");
            // Both candidates are our exclusively-created copies, never the
            // user's source. Remove the redundant original-format copy only
            // after the converted bytes have passed actual backend validation.
            std::error_code ignored;
            fs::remove(candidate, ignored);
            candidate = converted_candidate;
            converted = true;
        }
        bool exists = false;
        if (!regular(manager->pending, exists) || fail_once(StagePublish) || !replace(candidate, manager->pending))
            return error(result, BW_CARD_MANAGER_IO_ERROR, "The validated replacement could not be staged. The candidate was retained; the live card was not changed.");
        recovery(result, manager->pending);
        if (!sync_directory(manager->pending)) return error(result, BW_CARD_MANAGER_INCOMPLETE, "The staged replacement is retained, but its directory could not be synced. Retry before restarting.");
        std::string message = "Replacement staged for the next launch. The current card will be backed up before it is replaced.";
        if (converted) {
            message += " All three Wind Waker quest logs and pictures were preserved.";
            if (!import_info.backup_copy_valid) message += " The source's redundant game-save copy is damaged; the valid primary copy was kept unchanged.";
        }
        text(result.message, sizeof result.message, message);
        return true;
    } catch (...) { return error(result, BW_CARD_MANAGER_IO_ERROR, "Import staging failed; the live card was kept."); }
}
extern "C" bool bluewake_card_manager_pending(BwCardManager* manager, BwCardManagerPending* pending, BwCardManagerOutcome* outcome) {
    BwCardManagerOutcome local; auto& result = start(outcome, local);
    if (!manager || !pending) return error(result, BW_CARD_MANAGER_INVALID, "Save-management status is unavailable.");
    *pending = {};
    try {
        std::lock_guard<std::mutex> guard(manager->mutex);
        Lease lease;
        if (!manager_lease(*manager, lease, result)) return false;
        text(pending->path, sizeof pending->path, path_text(manager->pending));
        if (!regular(manager->pending, pending->present)) return error(result, BW_CARD_MANAGER_INVALID, "The pending path is not a regular card file.");
        if (!pending->present) { result.code = BW_CARD_MANAGER_NO_PENDING; return true; }
        pending->bytes = fs::file_size(manager->pending);
        pending->valid = validate(manager->pending);
        if (!pending->valid) return error(result, BW_CARD_MANAGER_INVALID, "The pending replacement is damaged. Cancel it or stage a valid card before restarting.");
        return true;
    } catch (...) { return error(result, BW_CARD_MANAGER_IO_ERROR, "The pending replacement could not be inspected."); }
}
extern "C" bool bluewake_card_manager_cancel(BwCardManager* manager, BwCardManagerOutcome* outcome) {
    BwCardManagerOutcome local; auto& result = start(outcome, local);
    if (!manager) return error(result, BW_CARD_MANAGER_INVALID, "Save management is unavailable.");
    try {
        std::lock_guard<std::mutex> guard(manager->mutex);
        Lease lease;
        if (!manager_lease(*manager, lease, result)) return false;
        bool present = false;
        if (!regular(manager->pending, present)) return error(result, BW_CARD_MANAGER_INVALID, "The pending path is not a regular file; it was kept unchanged.");
        if (!present) { result.code = BW_CARD_MANAGER_NO_PENDING; return true; }
        if (!consume(*manager, result, ".cancelled-")) return false;
        text(result.message, sizeof result.message, "Pending replacement cancelled. Its staged bytes are retained for recovery; current saves were kept.");
        return true;
    } catch (...) { return error(result, BW_CARD_MANAGER_IO_ERROR, "The pending replacement could not be cancelled."); }
}
extern "C" bool bluewake_card_manager_list_backups(BwCardManager* manager, BwCardManagerBackup* backups,
                                                   size_t capacity, size_t* total, BwCardManagerOutcome* outcome) {
    BwCardManagerOutcome local; auto& result = start(outcome, local);
    if (total) *total = 0;
    if (!manager || !total || (capacity && !backups)) return error(result, BW_CARD_MANAGER_INVALID, "Backup listing is unavailable.");
    try {
        std::lock_guard<std::mutex> guard(manager->mutex);
        Lease lease;
        if (!manager_lease(*manager, lease, result)) return false;
        std::error_code ec;
        if (!fs::exists(manager->backups, ec) && !ec) return true;
        if (ec || !fs::is_directory(manager->backups)) return error(result, BW_CARD_MANAGER_IO_ERROR, "The backup folder could not be read.");
        std::vector<fs::path> paths;
        for (const auto& entry : fs::directory_iterator(manager->backups)) {
            const auto filename = path_text(entry.path().filename());
            bool present = false;
            if (filename.rfind("GZLE01-", 0) == 0 && entry.path().extension() == ".card" &&
                regular(entry.path(), present) && present) paths.push_back(entry.path());
        }
        std::sort(paths.begin(), paths.end(), std::greater<fs::path>());
        *total = paths.size();
        for (size_t i = 0; i < std::min(capacity, paths.size()); ++i) {
            auto& metadata = backups[i]; metadata = {};
            text(metadata.path, sizeof metadata.path, path_text(paths[i]));
            metadata.bytes = fs::file_size(paths[i]);
            metadata.valid = validate(paths[i]);
            const auto modified = fs::last_write_time(paths[i]);
            const auto system = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                modified - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            metadata.modified_unix_seconds = std::chrono::system_clock::to_time_t(system);
        }
        return true;
    } catch (...) { return error(result, BW_CARD_MANAGER_IO_ERROR, "Backups could not be inspected; all files were kept."); }
}
extern "C" BwCardManagerCode bluewake_card_manager_apply_at_startup(BwCardManager* manager, BwCardManagerOutcome* outcome) {
    BwCardManagerOutcome local; auto& result = start(outcome, local);
    if (!manager) { error(result, BW_CARD_MANAGER_INVALID, "Save management is unavailable."); return result.code; }
    try {
        std::lock_guard<std::mutex> guard(manager->mutex);
        Lease manager_lock;
        if (!manager_lease(*manager, manager_lock, result)) return result.code;
        bool pending_exists = false;
        if (!regular(manager->pending, pending_exists)) {
            error(result, BW_CARD_MANAGER_INVALID, "The pending path is not a regular card file. Startup was stopped."); return result.code;
        }
        if (!pending_exists) { result.code = BW_CARD_MANAGER_NO_PENDING; result.startup_ready = true; return result.code; }
        recovery(result, manager->pending);
        Lease card_lock;
        if (!card_lock.take(manager->card, ".lock")) {
            error(result, BW_CARD_MANAGER_BUSY, "A running backend owns this card. Replacement was refused; keep that game closed before applying it."); return result.code;
        }
        if (!validate(manager->pending)) {
            error(result, BW_CARD_MANAGER_INVALID, "The staged replacement is damaged. Startup was stopped; current saves and the staged file were kept."); return result.code;
        }
        bool current_exists = false;
        if (!regular(manager->card, current_exists)) {
            error(result, BW_CARD_MANAGER_INVALID, "The current card path is not a regular file. Startup was stopped without replacing it."); return result.code;
        }
        if (current_exists && same_bytes(manager->card, manager->pending)) {
            // A previous run published but could not consume its queue. Finish
            // that transaction before permitting the backend to write again.
            if (!bw_atomic_flush_path(path_text(manager->card).c_str()) || !sync_directory(manager->card) ||
                !consume(*manager, result, ".applied-")) {
                if (result.code == BW_CARD_MANAGER_OK) error(result, BW_CARD_MANAGER_INCOMPLETE, "The previously applied replacement could not be confirmed. Keep the game closed and retry.");
                return result.code;
            }
            result.startup_ready = true;
            text(result.message, sizeof result.message, "Previously applied replacement confirmed; the pending request was consumed.");
            return result.code;
        }
        if (current_exists && !make_backup(*manager, result, false)) return result.code;
        const auto candidate = adjacent(manager->card, ".apply-");
        recovery(result, candidate);
        if (!copy_snapshot(manager->pending, candidate) || !validate(candidate)) {
            error(result, BW_CARD_MANAGER_IO_ERROR, "The staged card could not be copied and validated for publication. The current card, backup and pending request were kept."); return result.code;
        }
        if (fail_once(CurrentPublish) || !replace(candidate, manager->card)) {
            error(result, BW_CARD_MANAGER_IO_ERROR, "Atomic card replacement failed. The current card, backup and validated candidate were kept; retry with the game closed."); return result.code;
        }
        result.card_changed = true;
        recovery(result, manager->pending);
        if (fail_once(AfterCurrentSync) || !sync_directory(manager->card)) {
            error(result, BW_CARD_MANAGER_INCOMPLETE, "The replacement was published but durability could not be confirmed. Keep the game closed; current-card backup and pending replacement are retained."); return result.code;
        }
        if (!consume(*manager, result, ".applied-")) return result.code;
        result.startup_ready = true;
        text(result.message, sizeof result.message, "Card replacement applied before backend startup; the previous card was preserved and the pending request was consumed.");
        return result.code;
    } catch (...) { error(result, BW_CARD_MANAGER_IO_ERROR, "Card replacement failed. Startup was stopped; recovery files were retained."); return result.code; }
}
#ifdef BLUEWAKE_CARD_MANAGER_TEST
extern "C" void bluewake_card_manager_test_fail_once(BwCardManagerTestFailure failure) { injected_failure = failure; }
#endif
