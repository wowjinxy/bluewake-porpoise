// SPDX-License-Identifier: GPL-3.0-or-later
// Native directory/GCI implementation. Configuration principles and SHA-256
// are adapted from the preserved BlueWake network_preferences.cpp/digest;
// see ../network/PROVENANCE.md. No guest CARD/RAM representation is used.
#include "pc/pc_network_profile.h"
#include "../network/network_digest.h"
#include "../network/atomic_file.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <memory>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <fcntl.h>
#ifndef _WIN32
#include <sys/file.h>
#endif

namespace {
namespace fs = std::filesystem;
constexpr size_t kMaxPreferences = 16384;
void message(char* out, unsigned capacity, const char* text) {
    if (out != nullptr && capacity != 0) std::snprintf(out, capacity, "%s", text);
}
bool fail(char* out, unsigned capacity, const char* text) { message(out, capacity, text); return false; }
std::string utf8(const fs::path& path) { const auto s = path.u8string(); return {s.begin(), s.end()}; }
fs::path ioPath(const fs::path& path) {
#ifdef _WIN32
    // Filesystem metadata, handle leases and CRT writes must all address the
    // same long path; using UTF-16 alone still leaves MAX_PATH restrictions.
    std::unique_ptr<wchar_t, decltype(&std::free)> wide(
        pc_net_atomic_wide_path(utf8(path).c_str()), std::free);
    if (!wide) throw std::runtime_error("Invalid native storage path");
    return fs::path(wide.get());
#else
    return path;
#endif
}
bool text(const char* value, size_t capacity) { return value && std::memchr(value, 0, capacity); }
std::string randomId() {
    std::random_device random;
    const char* digits = "0123456789abcdef";
    std::string value(32, '0');
    for (char& c : value) c = digits[random() & 15];
    return value;
}
bool pathValue(const char* value, fs::path& path) {
    if (value == nullptr || *value == 0 || std::strlen(value) > 1024) return false;
    path = fs::u8path(value);
    for (const auto& part : path.relative_path()) {
        const auto s = utf8(part);
        if (s.empty() || s == ".") continue;
        if (s == ".." || s.back() == ' ' || s.back() == '.' ||
            s.find_first_of(":\r\n") != std::string::npos) return false;
    }
    path = fs::absolute(path).lexically_normal();
    return !path.empty() && utf8(path).size() < 1024;
}
bool safeNode(const fs::path& path, bool directory) {
    std::error_code ec;
    const auto status = fs::symlink_status(ioPath(path), ec);
    if (ec || fs::is_symlink(status)) return false;
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(ioPath(path).c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
#endif
    if (directory) return fs::is_directory(status);
    return fs::is_regular_file(status) && fs::hard_link_count(ioPath(path), ec) == 1 && !ec;
}
struct DirectoryPins {
#ifdef _WIN32
    std::vector<HANDLE> handles;
    ~DirectoryPins() { for (HANDLE h : handles) CloseHandle(h); }
    bool add(const fs::path& path) {
        HANDLE handle = CreateFileW(ioPath(path).c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return false;
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(handle, &info) ||
            !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) { CloseHandle(handle); return false; }
        handles.push_back(handle); // no delete sharing: ancestors cannot be replaced while mounted
        return true;
    }
#else
    std::vector<int> handles;
    ~DirectoryPins() { for (int fd : handles) ::close(fd); }
    bool add(const fs::path& path) {
        int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (fd < 0) return false;
        handles.push_back(fd);
        return true;
    }
#endif
};
bool directories(const fs::path& path, bool create, DirectoryPins* pins = nullptr) {
    fs::path at = path.root_path();
    if (!safeNode(at, true)) return false;
    for (const auto& piece : path.relative_path()) {
        at /= piece;
        std::error_code ec;
        const auto status = fs::symlink_status(ioPath(at), ec);
        if (ec && ec != std::errc::no_such_file_or_directory) return false;
        if (!fs::exists(status)) {
            if (!create || !fs::create_directory(ioPath(at), ec) || ec) return false;
        }
        if (!safeNode(at, true) || (pins && !pins->add(at))) return false;
    }
    return true;
}
bool absent(const fs::path& path) {
    std::error_code ec;
    const auto status = fs::symlink_status(ioPath(path), ec);
    return (!ec || ec == std::errc::no_such_file_or_directory) && !fs::exists(status);
}
bool readBounded(const fs::path& path, std::string& value) {
    if (!safeNode(path, false)) return false;
    std::error_code ec;
    const auto size = fs::file_size(ioPath(path), ec);
    if (ec || size > kMaxPreferences) return false;
    FILE* file = pc_net_atomic_open(utf8(path).c_str(), "rb");
    if (!file) return false;
    value.resize(static_cast<size_t>(size));
    bool ok = std::fread(value.data(), 1, value.size(), file) == value.size() &&
        std::fgetc(file) == EOF && !std::ferror(file);
    return std::fclose(file) == 0 && ok && value.find('\0') == std::string::npos;
}
bool syncDirectory(const fs::path& directory) {
#ifdef _WIN32
    (void)directory;
    return true; // MoveFileExW WRITE_THROUGH is used below.
#else
    const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return false;
    const bool ok = fsync(fd) == 0;
    return ::close(fd) == 0 && ok;
#endif
}
bool writeAtomic(const fs::path& target, const std::string& value, bool replace) {
    if (value.size() > kMaxPreferences || (!absent(target) && (!replace || !safeNode(target, false)))) return false;
    DirectoryPins parents;
    if (!directories(target.parent_path(), false, &parents)) return false;
    const fs::path pending = target.parent_path() / (target.filename().string() + ".tmp-" + randomId());
#ifdef _WIN32
    int fd = _wopen(ioPath(pending).c_str(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
    FILE* file = fd < 0 ? nullptr : _fdopen(fd, "wb");
    if (!file && fd >= 0) _close(fd);
#else
    int fd = ::open(pending.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC | O_NOFOLLOW, 0600);
    FILE* file = fd < 0 ? nullptr : fdopen(fd, "wb");
    if (!file && fd >= 0) ::close(fd);
#endif
    if (!file) return false;
    bool ok = std::fwrite(value.data(), 1, value.size(), file) == value.size() && pc_net_atomic_flush(file);
    if (std::fclose(file) != 0) ok = false;
    if (ok && !absent(target) && (!replace || !safeNode(target, false))) ok = false;
    if (ok) {
#ifdef _WIN32
        ok = pc_net_atomic_move(utf8(pending).c_str(), utf8(target).c_str(), replace) == 0;
#else
        if (replace) ok = ::rename(pending.c_str(), target.c_str()) == 0;
        else ok = ::link(pending.c_str(), target.c_str()) == 0;
#endif
    }
    std::error_code ignored;
    fs::remove(ioPath(pending), ignored); // only our exclusively created pending path
    return ok && syncDirectory(target.parent_path());
}
void frame(std::string& data, const std::string& value) {
    data += std::to_string(value.size()); data += ':'; data += value;
}
bool canonical(const PcNetworkProfile* source, PcNetworkProfile& copy) {
    if (!source || !text(source->bind, sizeof source->bind) ||
        !pc_network_config_valid(&source->config, nullptr, 0)) return false;
    copy = *source;
    if (!pc_network_normalize_endpoint(source->config.server, copy.config.server, sizeof copy.config.server) ||
        !pc_network_normalize_endpoint(source->bind, copy.bind, sizeof copy.bind)) return false;
    return std::strchr(copy.bind, ':') == nullptr; // native host is explicitly IPv4
}
std::string routeIdentity(const PcNetworkProfile& p) {
    std::string data;
    const auto& c = p.config;
    frame(data, PC_NETWORK_NAMESPACE); frame(data, std::to_string(PC_NETWORK_STORAGE_PROTOCOL));
    frame(data, "directory-gci-route-v1"); frame(data, c.compatibility.game_id);
    frame(data, std::to_string(c.compatibility.disc_revision)); frame(data, c.compatibility.build_id);
    frame(data, std::to_string(c.compatibility.progression_schema));
    frame(data, c.compatibility.semantic_digest); frame(data, c.compatibility.rules_digest);
    frame(data, c.server); frame(data, std::to_string(c.port)); frame(data, c.room); frame(data, c.player_id);
    return pc_net::digest(data);
}
bool output(const std::string& value, char* out, unsigned capacity) {
    if (!out || value.size() >= capacity) return false;
    std::memcpy(out, value.c_str(), value.size() + 1);
    return true;
}
std::string encode(const PcNetworkProfile& p) {
    const auto& c = p.config;
    return std::string("version=2\nenabled=") + (p.enabled ? "1" : "0") +
        "\nhost=" + (p.host ? "1" : "0") + "\nbind=" + p.bind + "\nserver=" + c.server +
        "\nport=" + std::to_string(c.port) + "\nroom=" + c.room + "\nplayer_id=" + c.player_id +
        "\nplayer_name=" + c.player_name + "\nroom_password=" + c.room_password +
        "\ncreate_room=" + (c.create_room ? "1" : "0") + "\ngame_id=" + c.compatibility.game_id +
        "\nbuild_id=" + c.compatibility.build_id + "\nsemantic_digest=" + c.compatibility.semantic_digest +
        "\nrules_digest=" + c.compatibility.rules_digest +
        "\nprogression_schema=" + std::to_string(c.compatibility.progression_schema) +
        "\ndisc_revision=" + std::to_string(c.compatibility.disc_revision) + "\n";
}
bool number(const std::string& text, unsigned max, unsigned& result) {
    if (text.empty() || text.size() > 10) return false;
    uint64_t value = 0;
    for (char c : text) { if (c < '0' || c > '9') return false; value = value * 10 + c - '0'; if (value > max) return false; }
    result = static_cast<unsigned>(value); return true;
}
bool decode(const std::string& input, PcNetworkProfile& p) {
    p = {};
    std::set<std::string> seen;
    size_t at = 0;
    while (at < input.size()) {
        const auto end = input.find('\n', at);
        if (end == std::string::npos || end - at > 1024) return false;
        auto line = input.substr(at, end - at); at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto split = line.find('='); if (split == std::string::npos) return false;
        const auto key = line.substr(0, split), value = line.substr(split + 1);
        if (!seen.insert(key).second) return false;
        auto string = [&](char* out, size_t capacity) { if (value.size() >= capacity) return false; std::memcpy(out, value.c_str(), value.size() + 1); return true; };
        unsigned n = 0;
        if (key == "version") { if (!number(value, 2, n) || n != 2) return false; }
        else if (key == "enabled") { if (!number(value, 1, n)) return false; p.enabled = n != 0; }
        else if (key == "host") { if (!number(value, 1, n)) return false; p.host = n != 0; }
        else if (key == "port") { if (!number(value, 65535, n)) return false; p.config.port = static_cast<uint16_t>(n); }
        else if (key == "create_room") { if (!number(value, 1, n)) return false; p.config.create_room = n != 0; }
        else if (key == "progression_schema") { if (!number(value, UINT32_MAX, n)) return false; p.config.compatibility.progression_schema = n; }
        else if (key == "disc_revision") { if (!number(value, UINT32_MAX, n)) return false; p.config.compatibility.disc_revision = n; }
        else if (key == "bind") { if (!string(p.bind, sizeof p.bind)) return false; }
        else if (key == "server") { if (!string(p.config.server, sizeof p.config.server)) return false; }
        else if (key == "room") { if (!string(p.config.room, sizeof p.config.room)) return false; }
        else if (key == "player_id") { if (!string(p.config.player_id, sizeof p.config.player_id)) return false; }
        else if (key == "player_name") { if (!string(p.config.player_name, sizeof p.config.player_name)) return false; }
        else if (key == "room_password") { if (!string(p.config.room_password, sizeof p.config.room_password)) return false; }
        else if (key == "game_id") { if (!string(p.config.compatibility.game_id, sizeof p.config.compatibility.game_id)) return false; }
        else if (key == "build_id") { if (!string(p.config.compatibility.build_id, sizeof p.config.compatibility.build_id)) return false; }
        else if (key == "semantic_digest") { if (!string(p.config.compatibility.semantic_digest, sizeof p.config.compatibility.semantic_digest)) return false; }
        else if (key == "rules_digest") { if (!string(p.config.compatibility.rules_digest, sizeof p.config.compatibility.rules_digest)) return false; }
        else return false;
    }
    PcNetworkProfile normalized{};
    if (seen.size() != 17 || !canonical(&p, normalized)) return false;
    p = normalized;
    return true;
}
bool within(const fs::path& child, const fs::path& parent) {
    auto a = child.begin(), b = parent.begin();
    for (; b != parent.end(); ++a, ++b) {
        if (a == child.end()) return false;
#ifdef _WIN32
        if (_wcsicmp(a->c_str(), b->c_str()) != 0) return false;
#else
        if (*a != *b) return false;
#endif
    }
    return true;
}
bool safeTree(const fs::path& root) {
    size_t count = 0;
    for (fs::recursive_directory_iterator i(ioPath(root)), end; i != end; ++i) {
        if (++count > 1024 || i.depth() > 8) return false;
        std::error_code ec;
        const auto status = i->symlink_status(ec);
        if (ec || fs::is_symlink(status) || !safeNode(i->path(), fs::is_directory(status))) return false;
    }
    return true;
}
} // namespace

struct PcNetworkProfileMount {
    PcNetworkProfile profile{};
    std::string base, identity;
    DirectoryPins pins;
#ifdef _WIN32
    HANDLE lease = INVALID_HANDLE_VALUE;
    ~PcNetworkProfileMount() { if (lease != INVALID_HANDLE_VALUE) CloseHandle(lease); }
#else
    int lease = -1;
    ~PcNetworkProfileMount() { if (lease >= 0) ::close(lease); }
#endif
};
namespace { std::unique_ptr<PcNetworkProfileMount> sMounted; }

extern "C" {
void pc_network_profile_defaults(PcNetworkProfile* p) {
    if (!p) return;
    *p = {};
    pc_network_native_compatibility(&p->config.compatibility);
    std::strcpy(p->bind, "127.0.0.1"); std::strcpy(p->config.server, "127.0.0.1");
    p->config.port = 49383;
    std::strcpy(p->config.room, "bluewake"); std::strcpy(p->config.player_name, "Link");
    try { std::strcpy(p->config.player_id, randomId().c_str()); } catch (...) { p->config.player_id[0] = 0; }
}
bool pc_network_profile_valid(const PcNetworkProfile* p, char* error, unsigned capacity) {
    PcNetworkProfile normalized{};
    if (!canonical(p, normalized)) return fail(error, capacity, "Invalid native network profile or compatibility");
    message(error, capacity, ""); return true;
}
bool pc_network_profile_load(const char* file, bool required, PcNetworkProfile* out, char* error, unsigned capacity) {
    if (!out) return fail(error, capacity, "Missing profile output");
    try {
        fs::path path;
        if (!pathValue(file, path)) return fail(error, capacity, "Invalid native profile path");
        if (absent(path)) {
            // Reject a linked parent even if the final file does not exist.
            fs::path ancestor = path.parent_path();
            while (absent(ancestor) && ancestor != ancestor.parent_path()) ancestor = ancestor.parent_path();
            if (required || !directories(ancestor, false)) return fail(error, capacity, "Explicit profile missing or unsafe");
            pc_network_profile_defaults(out);
            return pc_network_profile_valid(out, error, capacity);
        }
        DirectoryPins pins;
        if (!directories(path.parent_path(), false, &pins)) return fail(error, capacity, "Unsafe native profile parent");
        std::string bytes;
        PcNetworkProfile profile{};
        if (!readBounded(path, bytes) || !decode(bytes, profile)) return fail(error, capacity, "Malformed or unsafe native profile; personal fallback refused");
        *out = profile; message(error, capacity, ""); return true;
    } catch (...) { return fail(error, capacity, "Cannot read native profile; personal fallback refused"); }
}
bool pc_network_profile_save(const char* file, const PcNetworkProfile* p, char* error, unsigned capacity) {
    try {
        PcNetworkProfile profile{}; fs::path path;
        if (!canonical(p, profile) || !pathValue(file, path)) return fail(error, capacity, "Invalid profile or destination");
        if (!writeAtomic(path, encode(profile), true)) return fail(error, capacity, "Cannot publish native profile; previous settings retained");
        message(error, capacity, ""); return true;
    } catch (...) { return fail(error, capacity, "Cannot publish native profile; previous settings retained"); }
}
bool pc_network_profile_route(const PcNetworkProfile* p, const char* root, const char* personal,
    char* base, unsigned baseCapacity, char* identity, unsigned identityCapacity, char* error, unsigned errorCapacity) {
    if (base && baseCapacity) base[0] = 0;
    if (identity && identityCapacity) identity[0] = 0;
    try {
        PcNetworkProfile profile{}; fs::path personalPath, selected, roomRoot;
        if (!canonical(p, profile) || !pathValue(personal, personalPath)) return fail(error, errorCapacity, "Invalid native storage identity");
        std::string key = "personal";
        selected = personalPath;
        if (profile.enabled) {
            if (!pathValue(root, roomRoot)) return fail(error, errorCapacity, "Invalid room storage root");
            key = routeIdentity(profile); selected = roomRoot / key;
            if (within(personalPath, selected) || within(roomRoot, personalPath / "USA"))
                return fail(error, errorCapacity, "Room and personal card directories overlap");
        }
        const auto selectedText = utf8(selected) + "/";
        if (!base || selectedText.size() >= baseCapacity || !identity || key.size() >= identityCapacity)
            return fail(error, errorCapacity, "Native storage route buffer too small");
        output(selectedText, base, baseCapacity); output(key, identity, identityCapacity);
        message(error, errorCapacity, ""); return true;
    } catch (...) { return fail(error, errorCapacity, "Cannot resolve native storage route"); }
}
PcNetworkProfileMount* pc_network_profile_mount_create(const PcNetworkProfile* p, const char* root,
    const char* personal, char* error, unsigned capacity) {
    try {
        auto mount = std::make_unique<PcNetworkProfileMount>();
        char base[1100], identity[65];
        if (!canonical(p, mount->profile)) { fail(error, capacity, "Invalid native room profile"); return nullptr; }
        if (!pc_network_profile_route(p, root, personal, base, sizeof base,
            identity, sizeof identity, error, capacity)) return nullptr;
        mount->base = base; mount->identity = identity;
        if (mount->profile.enabled) {
            const fs::path path = fs::u8path(base);
            if (!directories(path, true, &mount->pins) || !safeTree(path)) {
                fail(error, capacity, "Room storage contains a link, alias or invalid path"); return nullptr;
            }
            const auto marker = path / "profile.identity";
            const std::string markerBytes = "BlueWake.Native.GCI.Profile.v1\n" + mount->identity + "\n";
            if (!absent(marker)) {
                std::string found;
                if (!readBounded(marker, found) || found != markerBytes) {
                    fail(error, capacity, "Room storage identity mismatch"); return nullptr;
                }
            } else {
                for (const auto& entry : fs::directory_iterator(ioPath(path))) {
                    if (entry.path().filename() != "profile.lock") {
                        fail(error, capacity, "Unidentified room storage is not empty"); return nullptr;
                    }
                }
            }
            const auto lock = path / "profile.lock";
#ifdef _WIN32
            mount->lease = CreateFileW(ioPath(lock).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            BY_HANDLE_FILE_INFORMATION info{};
            if (mount->lease == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(mount->lease, &info) ||
                (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) || info.nNumberOfLinks != 1) {
#else
            mount->lease = ::open(lock.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
            struct stat info{};
            if (mount->lease < 0 || fstat(mount->lease, &info) || !S_ISREG(info.st_mode) || info.st_nlink != 1 ||
                flock(mount->lease, LOCK_EX | LOCK_NB)) {
#endif
                fail(error, capacity, "Room storage is already in use or unsafe"); return nullptr;
            }
            if (absent(marker) && !writeAtomic(marker, markerBytes, false)) {
                fail(error, capacity, "Cannot publish room storage identity"); return nullptr;
            }
            if (!directories(path / "USA" / "Card A", true, &mount->pins) ||
                !directories(path / "USA" / "Card B", true, &mount->pins)) {
                fail(error, capacity, "Cannot prepare isolated native card directories"); return nullptr;
            }
        }
        message(error, capacity, ""); return mount.release();
    } catch (...) { fail(error, capacity, "Cannot prepare native room storage; personal fallback refused"); return nullptr; }
}
void pc_network_profile_mount_destroy(PcNetworkProfileMount* mount) { delete mount; }
const char* pc_network_profile_mount_card_base(const PcNetworkProfileMount* mount) { return mount ? mount->base.c_str() : nullptr; }
const char* pc_network_profile_mount_identity(const PcNetworkProfileMount* mount) { return mount ? mount->identity.c_str() : nullptr; }
bool pc_network_profile_prepare(const PcNetworkProfile* p, const char* root, const char* personal, char* error, unsigned capacity) {
    if (sMounted) return fail(error, capacity, "Native card profile is immutable; restart required");
    sMounted.reset(pc_network_profile_mount_create(p, root, personal, error, capacity));
    return sMounted != nullptr;
}
const char* pc_network_profile_mounted_card_base(void) { return pc_network_profile_mount_card_base(sMounted.get()); }
const char* pc_network_profile_mounted_identity(void) { return pc_network_profile_mount_identity(sMounted.get()); }
bool pc_network_profile_mounted(PcNetworkProfile* copy) {
    if (!sMounted || !copy) return false;
    *copy = sMounted->profile; return true;
}
} // extern "C"
