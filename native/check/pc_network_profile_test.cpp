// SPDX-License-Identifier: GPL-3.0-or-later
// Real filesystem qualification in a caller-selected NEW private directory.
// All .gci names below contain tiny synthetic text, never a production save.
#include "pc/pc_network_profile.h"
#include "../src/pc/network/atomic_file.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#include <cstddef>
#endif

namespace fs = std::filesystem;
namespace {
int errors = 0;
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition); ++errors; } } while (0)
std::string utf8(const fs::path& p) { const auto s = p.u8string(); return {s.begin(), s.end()}; }
fs::path ioPath(const fs::path& path) {
#ifdef _WIN32
    std::unique_ptr<wchar_t, decltype(&std::free)> wide(
        pc_net_atomic_wide_path(utf8(path).c_str()), std::free);
    if (!wide) throw std::runtime_error("Private fixture path conversion failed");
    return fs::path(wide.get());
#else
    return path;
#endif
}
std::string read(const fs::path& path) { std::ifstream f(ioPath(path), std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
void write(const fs::path& path, const std::string& value) { std::ofstream f(ioPath(path), std::ios::binary); f << value; CHECK(bool(f)); }
using Mount = std::unique_ptr<PcNetworkProfileMount, decltype(&pc_network_profile_mount_destroy)>;
Mount mount(const PcNetworkProfile& p, const fs::path& root, const fs::path& personal) {
    char error[256]{};
    return Mount(pc_network_profile_mount_create(&p, utf8(root).c_str(), utf8(personal).c_str(), error, sizeof error),
        pc_network_profile_mount_destroy);
}
std::string route(const PcNetworkProfile& p, const fs::path& root, const fs::path& personal, std::string* id = nullptr) {
    char base[1100]{}, identity[65]{}, error[256]{};
    const bool ok = pc_network_profile_route(&p, utf8(root).c_str(), utf8(personal).c_str(),
        base, sizeof base, identity, sizeof identity, error, sizeof error);
    CHECK(ok);
    if (id) *id = identity;
    return base;
}
bool directoryLink(const fs::path& link, const fs::path& target) {
#ifdef _WIN32
    if (CreateSymbolicLinkW(link.c_str(), target.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY | 2)) return true;
    // Junction fallback needs no Developer Mode symlink privilege. Both paths
    // are private fixtures; the target is retained, and nothing is deleted.
    if (!CreateDirectoryW(link.c_str(), nullptr)) return false;
    HANDLE h = CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    const std::wstring substitute = L"\\??\\" + target.native();
    const std::wstring print = target.native();
    struct Junction {
        DWORD tag; WORD bytes, reserved;
        WORD substituteOffset, substituteLength, printOffset, printLength;
        wchar_t paths[2048];
    } data{};
    static_assert(offsetof(Junction, paths) == 16);
    if (substitute.size() + print.size() + 2 >= 2048) { CloseHandle(h); return false; }
    data.tag = IO_REPARSE_TAG_MOUNT_POINT;
    data.substituteLength = static_cast<WORD>(substitute.size() * sizeof(wchar_t));
    data.printOffset = static_cast<WORD>((substitute.size() + 1) * sizeof(wchar_t));
    data.printLength = static_cast<WORD>(print.size() * sizeof(wchar_t));
    std::memcpy(data.paths, substitute.c_str(), data.substituteLength + sizeof(wchar_t));
    std::memcpy(reinterpret_cast<char*>(data.paths) + data.printOffset, print.c_str(), data.printLength + sizeof(wchar_t));
    data.bytes = static_cast<WORD>(8 + data.printOffset + data.printLength + sizeof(wchar_t));
    DWORD returned = 0;
    const bool ok = DeviceIoControl(h, FSCTL_SET_REPARSE_POINT, &data, data.bytes + 8,
        nullptr, 0, &returned, nullptr) != FALSE;
    CloseHandle(h);
    return ok;
#else
    std::error_code ec;
    fs::create_directory_symlink(target, link, ec);
    return !ec;
#endif
}
void identities(const fs::path& root, PcNetworkProfile p) {
    const auto cards = root / "NetworkNative", personal = root / "personal";
    std::string id;
    const auto a = route(p, cards, personal, &id);
    CHECK(id.size() == 64 && id.find_first_not_of("0123456789abcdef") == std::string::npos);
    CHECK(id.find(p.config.player_id) == std::string::npos && id.find(p.config.room) == std::string::npos);
    auto changed = p;
    std::strcpy(changed.config.server, "localhost");
    CHECK(route(changed, cards, personal) == a);
    changed = p; std::strcpy(changed.config.room, "room-B"); CHECK(route(changed, cards, personal) != a);
    changed = p; changed.config.player_id[0] = 'b'; CHECK(route(changed, cards, personal) != a);
    changed = p; ++changed.config.port; CHECK(route(changed, cards, personal) != a);
    changed = p; std::strcpy(changed.config.server, "127.0.0.2"); CHECK(route(changed, cards, personal) != a);
    changed = p; changed.host = !changed.host; changed.config.create_room = !changed.config.create_room;
    std::strcpy(changed.config.room_password, "private-test-credential");
    std::strcpy(changed.config.player_name, "Renamed");
    CHECK(route(changed, cards, personal) == a);
    CHECK(id.find(changed.config.room_password) == std::string::npos);
    changed = p; changed.enabled = false; CHECK(route(changed, cards, personal, &id) != a && id == "personal");
    char error[256]{}, base[1100]{}, identity[65]{};
    changed = p; ++changed.config.compatibility.disc_revision;
    CHECK(!pc_network_profile_valid(&changed, error, sizeof error));
    CHECK(!pc_network_profile_route(&changed, utf8(cards).c_str(), utf8(personal).c_str(), base, sizeof base,
        identity, sizeof identity, error, sizeof error) && base[0] == 0 && identity[0] == 0);
    changed = p; changed.config.compatibility.progression_schema = 2;
    CHECK(!pc_network_profile_valid(&changed, error, sizeof error));
    CHECK(!pc_network_profile_route(&changed, utf8(cards).c_str(), utf8(personal).c_str(), base, sizeof base,
        identity, sizeof identity, error, sizeof error) && base[0] == 0 && identity[0] == 0);
    changed = p; changed.config.compatibility.rules_digest[0] ^= 1;
    CHECK(!pc_network_profile_valid(&changed, error, sizeof error));
    CHECK(!pc_network_profile_route(&p, utf8(root / ".." / "escape").c_str(), utf8(personal).c_str(),
        base, sizeof base, identity, sizeof identity, error, sizeof error));
    CHECK(!pc_network_profile_route(&p, utf8(personal / "USA" / "Card A").c_str(), utf8(personal).c_str(),
        base, sizeof base, identity, sizeof identity, error, sizeof error));
    CHECK(!pc_network_profile_route(&p, utf8(cards).c_str(), utf8(personal).c_str(), base, sizeof base,
        identity, 2, error, sizeof error) && base[0] == 0 && identity[0] == 0);
}
void preferences(const fs::path& root, const PcNetworkProfile& p) {
    char error[256]{};
    PcNetworkProfile loaded{};
    const auto file = root / "network.ini";
    CHECK(pc_network_profile_load(utf8(file).c_str(), false, &loaded, error, sizeof error) && !loaded.enabled);
    CHECK(!fs::exists(file));
    CHECK(!pc_network_profile_load(utf8(file).c_str(), true, &loaded, error, sizeof error));
    CHECK(pc_network_profile_save(utf8(file).c_str(), &p, error, sizeof error));
    CHECK(pc_network_profile_load(utf8(file).c_str(), true, &loaded, error, sizeof error));
    CHECK(loaded.enabled && std::strcmp(loaded.config.player_id, p.config.player_id) == 0);
    const auto original = read(file);
    auto schema2=original;const auto schemaAt=schema2.find("progression_schema=3\n");
    CHECK(schemaAt!=std::string::npos);
    if(schemaAt!=std::string::npos) {
        schema2.replace(schemaAt,std::strlen("progression_schema=3\n"),"progression_schema=2\n");
        const auto prior=root/"schema2.ini";write(prior,schema2);
        CHECK(!pc_network_profile_load(utf8(prior).c_str(),true,&loaded,error,sizeof error));
        CHECK(read(prior)==schema2); // refusal preserves existing preferences
    }
    auto bad = p; std::strcpy(bad.config.room, "../personal");
    CHECK(!pc_network_profile_save(utf8(file).c_str(), &bad, error, sizeof error));
    CHECK(read(file) == original);
    std::vector<std::string> malformed = {original + "version=2\n", original + "unknown=1\n",
        original.substr(0, original.size() - 1), "version=1\n", std::string(16385, 'x'), original + std::string("x\0y\n", 4)};
    for (unsigned i = 0; i < malformed.size(); ++i) {
        const auto path = root / ("malformed-" + std::to_string(i) + ".ini"); write(path, malformed[i]);
        CHECK(!pc_network_profile_load(utf8(path).c_str(), true, &loaded, error, sizeof error));
    }
    const auto alias = root / "hardlink.ini";
    fs::create_hard_link(file, alias);
    CHECK(!pc_network_profile_load(utf8(alias).c_str(), true, &loaded, error, sizeof error));
    CHECK(!pc_network_profile_save(utf8(alias).c_str(), &p, error, sizeof error));
    CHECK(read(file) == original);
    // Drop only our link so the original can be used by the failure test.
    CHECK(fs::remove(alias));
    const auto blocker = root / "not-a-directory"; write(blocker, "fixture");
    CHECK(!pc_network_profile_save(utf8(blocker / "network.ini").c_str(), &p, error, sizeof error));
    CHECK(read(blocker) == "fixture" && read(file) == original);
#ifdef _WIN32
    CHECK(SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_READONLY));
    auto changed = p; std::strcpy(changed.config.player_name, "Changed");
    CHECK(!pc_network_profile_save(utf8(file).c_str(), &changed, error, sizeof error));
    CHECK(read(file) == original);
    CHECK(SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_NORMAL));
#endif
    const auto target = root / "linked-target"; fs::create_directory(target);
    write(target / "network.ini", original);
    const auto link = root / "linked-parent";
    CHECK(directoryLink(link, target));
    CHECK(!pc_network_profile_load(utf8(link / "network.ini").c_str(), true, &loaded, error, sizeof error));
    CHECK(!pc_network_profile_save(utf8(link / "network.ini").c_str(), &p, error, sizeof error));
    CHECK(read(target / "network.ini") == original);
}
void mounting(const fs::path& root, PcNetworkProfile p) {
    const auto personal = root / "personal"; fs::create_directory(personal);
    write(personal / "fixture.gci", "PERSONAL-DUMMY");
    const auto storage = root / "NetworkNative";
    auto a = mount(p, storage, personal); CHECK(bool(a)); if (!a) return;
    const auto aBase = fs::u8path(pc_network_profile_mount_card_base(a.get()));
    CHECK(fs::is_directory(aBase / "USA" / "Card A") && fs::is_directory(aBase / "USA" / "Card B"));
    CHECK(!mount(p, storage, personal)); // second writer is refused, never personal
    auto roomB = p; std::strcpy(roomB.config.room, "room-B");
    auto b = mount(roomB, storage, personal); CHECK(bool(b)); if (!b) return;
    const auto bBase = fs::u8path(pc_network_profile_mount_card_base(b.get()));
    CHECK(aBase != bBase);
    write(aBase / "USA" / "Card A" / "fixture.gci", "ROOM-A-DUMMY");
    CHECK(!fs::exists(bBase / "USA" / "Card A" / "fixture.gci"));
    CHECK(read(personal / "fixture.gci") == "PERSONAL-DUMMY");
    a.reset(); CHECK(bool(mount(p, storage, personal))); // same identity reopens after lease release
    auto aliasRoom = p; std::strcpy(aliasRoom.config.room, "alias");
    auto aliasMount = mount(aliasRoom, storage, personal); CHECK(bool(aliasMount));
    if (aliasMount) {
        const auto base = fs::u8path(pc_network_profile_mount_card_base(aliasMount.get()));
        aliasMount.reset();
        fs::create_hard_link(personal / "fixture.gci", base / "USA" / "Card A" / "fixture.gci");
        CHECK(!mount(aliasRoom, storage, personal));
        CHECK(read(personal / "fixture.gci") == "PERSONAL-DUMMY");
    }
    auto linkRoom = p; std::strcpy(linkRoom.config.room, "escape");
    const fs::path linkRoute = fs::u8path(route(linkRoom, storage, personal)).parent_path();
    CHECK(directoryLink(linkRoute, personal));
    CHECK(!mount(linkRoom, storage, personal));
    auto unknownRoom = p; std::strcpy(unknownRoom.config.room, "unknown");
    const fs::path unknown = fs::u8path(route(unknownRoom, storage, personal));
    fs::create_directories(unknown); write(unknown / "unclaimed.gci", "NO-AUTO-IMPORT");
    CHECK(!mount(unknownRoom, storage, personal));
    auto markerRoom = p; std::strcpy(markerRoom.config.room, "marker");
    auto marked = mount(markerRoom, storage, personal); CHECK(bool(marked));
    if (marked) {
        const auto base = fs::u8path(pc_network_profile_mount_card_base(marked.get())); marked.reset();
        write(base / "profile.identity", "wrong identity\n");
        CHECK(!mount(markerRoom, storage, personal));
    }
    char error[256]{};
    auto bad = p; ++bad.config.compatibility.progression_schema;
    CHECK(pc_network_profile_mounted_identity() == nullptr);
    CHECK(!pc_network_profile_prepare(&bad, utf8(storage).c_str(), utf8(personal).c_str(), error, sizeof error));
    CHECK(pc_network_profile_mounted_card_base() == nullptr);
    auto personalProfile = p; personalProfile.enabled = false;
    CHECK(pc_network_profile_prepare(&personalProfile, utf8(storage).c_str(), utf8(personal).c_str(), error, sizeof error));
    CHECK(std::strcmp(pc_network_profile_mounted_identity(), "personal") == 0);
    CHECK(!pc_network_profile_prepare(&p, utf8(storage).c_str(), utf8(personal).c_str(), error, sizeof error));
    CHECK(std::strcmp(pc_network_profile_mounted_identity(), "personal") == 0);
    CHECK(read(personal / "fixture.gci") == "PERSONAL-DUMMY");
}
void longPaths(const fs::path& root, const PcNetworkProfile& p) {
    // Keep the real path long: a short fixture root would hide CRT/MAX_PATH
    // failures in settings publication, marker creation and the writer lease.
    fs::path deep = root / "deep-path";
    for (unsigned i = 0; utf8(deep).size() < 320; ++i)
        deep /= "private-component-" + std::to_string(i) + "-12345678901234567890";
    fs::create_directories(ioPath(deep));
    CHECK(utf8(deep).size() > 300);
    const auto file = deep / "network.ini";
    char error[256]{};
    PcNetworkProfile loaded{};
    CHECK(pc_network_profile_save(utf8(file).c_str(), &p, error, sizeof error));
    CHECK(pc_network_profile_load(utf8(file).c_str(), true, &loaded, error, sizeof error));
    CHECK(std::strcmp(loaded.config.player_id, p.config.player_id) == 0);
    auto changed = p; std::strcpy(changed.config.player_name, "DeepPath");
    CHECK(pc_network_profile_save(utf8(file).c_str(), &changed, error, sizeof error));
    CHECK(pc_network_profile_load(utf8(file).c_str(), true, &loaded, error, sizeof error));
    CHECK(std::strcmp(loaded.config.player_name, "DeepPath") == 0);
    const auto personal = deep / "personal", storage = deep / "NetworkNative";
    fs::create_directory(ioPath(personal));
    write(personal / "fixture.gci", "DEEP-PERSONAL-DUMMY");
    auto first = mount(p, storage, personal);
    CHECK(bool(first));
    if (!first) return;
    const auto base = fs::u8path(pc_network_profile_mount_card_base(first.get()));
    CHECK(utf8(base).size() > 380);
    CHECK(fs::is_directory(ioPath(base / "USA" / "Card A")));
    CHECK(fs::is_directory(ioPath(base / "USA" / "Card B")));
    const auto fixture = base / "USA" / "Card A" / "fixture.gci";
    write(fixture, "DEEP-ROOM-DUMMY");
    const std::string identity = pc_network_profile_mount_identity(first.get());
    CHECK(!mount(p, storage, personal));
    first.reset();
    auto reopened = mount(p, storage, personal);
    CHECK(bool(reopened));
    if (reopened) {
        CHECK(identity == pc_network_profile_mount_identity(reopened.get()));
        CHECK(base == fs::u8path(pc_network_profile_mount_card_base(reopened.get())));
        CHECK(read(fixture) == "DEEP-ROOM-DUMMY");
    }
    CHECK(read(personal / "fixture.gci") == "DEEP-PERSONAL-DUMMY");
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: cos_network_profile_tests NEW_PRIVATE_FIXTURE_DIRECTORY\n"); return 2; }
    try {
        const fs::path root = fs::absolute(fs::u8path(argv[1])).lexically_normal();
        if (fs::exists(root)) { std::fprintf(stderr, "Fixture directory must be new; nothing was removed\n"); return 2; }
        fs::create_directories(root);
        PcNetworkProfile profile{}; pc_network_profile_defaults(&profile);
        profile.enabled = true; std::strcpy(profile.config.room, "room-A");
        std::memset(profile.config.player_id, 'a', 32); profile.config.player_id[32] = 0;
        identities(root, profile); preferences(root, profile); longPaths(root, profile); mounting(root, profile);
    } catch (const std::exception& e) { std::fprintf(stderr, "Private fixture failure: %s\n", e.what()); return 1; }
    if (errors != 0) return 1;
    std::puts("network-profile: PASS normalized identities, disjoint GCI directories, links/aliases, exclusive writer, atomic failure preservation, long-path publication/reopen and restart-only mount; synthetic private fixtures retained");
    return 0;
}
