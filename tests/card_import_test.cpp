// SPDX-License-Identifier: GPL-3.0-or-later
#include "card_import.h"
#include "card_import_test_support.h"
#include "card_manager.h"
#include "gxruntime/memory_card.h"
#ifdef rename
#undef rename
#endif
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace card_import_fixture;
namespace fs = std::filesystem;
namespace {
void check(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); }
std::string utf8(const fs::path& path) { const auto s = path.u8string(); return {reinterpret_cast<const char*>(s.data()), s.size()}; }
void write(const fs::path& path, const Bytes& data) {
    std::ofstream output(path, std::ios::binary); output.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
    check(output.good(), "Fixture write failed");
}
Bytes read(const fs::path& path) { std::ifstream in(path, std::ios::binary); check(in.good(), "Fixture read failed"); return {std::istreambuf_iterator<char>(in), {}}; }
Bytes convert(const Bytes& source, BwCardImportInfo* requested = nullptr) {
    Bytes result{1, 2, 3}; BwCardImportInfo local; std::string error;
    check(bluewake_card_import_convert(source, result, requested ? *requested : local, error), error); return result;
}
void invalid(const Bytes& source, const char* why) {
    Bytes output{1, 2, 3}; BwCardImportInfo info; std::string error;
    check(!bluewake_card_import_convert(source, output, info, error) && output == Bytes({1, 2, 3}) && !error.empty(), why);
}
void backend(const fs::path& path, const Bytes& expected, uint16_t mbits) {
    const auto name = utf8(path); check(dol_card_validate(name.c_str()), "Actual GXRuntime rejected converted container");
    DolMemoryCardConfig options{}; options.path = name.c_str(); std::memcpy(options.game_code, "GZLE", 4); std::memcpy(options.company, "01", 2);
    std::unique_ptr<DolMemoryCard, decltype(&dol_card_close)> card(dol_card_open(&options), dol_card_close);
    check(card && dol_card_mount(card.get()) == DOL_CARD_RESULT_READY, "Actual converted backend mount failed");
    uint16_t size = 0; uint32_t sector = 0; check(dol_card_probe(card.get(), &size, &sector) == 0 && size == mbits && sector == 8192, "Geometry lost");
    int32_t file = -1; uint32_t length = 0; check(dol_card_open_file(card.get(), "gczelda", &file, &length) == 0 && length == 0x18000, "Native file lookup failed");
    Bytes restored(length); check(dol_card_read_file(card.get(), file, 0, restored.data(), length) == 0 && restored == expected, "All three logs/pictures were not preserved exactly");
    DolMemoryCardStat stat{}; check(dol_card_get_status(card.get(), file, &stat) == 0 && stat.time == 0x13579BDF && stat.banner_format == 1 &&
        stat.icon_address == 0 && stat.icon_format == 1 && stat.icon_speed == 3 && stat.comment_address == 0x1C00 && stat.permission == 4, "Native metadata lost");
    int32_t unused = -1; check(dol_card_open_file(card.get(), "another-game", &unused, nullptr) == DOL_CARD_RESULT_NO_FILE, "Foreign raw-card title leaked into converted container");
}
}
int main(int argc, char** argv) {
    fs::path folder;
    try {
        folder = fs::absolute(argc > 1 ? fs::u8path(argv[1]) : fs::temp_directory_path()) /
            fs::u8path("card-import-\xE2\x98\x83-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directories(folder), "Cannot create isolated fixture");
        const auto original = gci(); const Bytes payload(original.begin() + 64, original.end());
        BwCardImportInfo info;
        const auto gci_card = convert(original, &info);
        check(info.format == BwCardImportInfo::Format::Gci && info.backup_copy_valid && convert(original) == gci_card, "GCI conversion was not stable or native checksums failed");
        write(folder / "gci.card", gci_card); backend(folder / "gci.card", payload, 4);
        auto damaged = original; damaged[64 + 2 * 8192 + 100] ^= 1;
        const auto primary_only = convert(damaged, &info); check(!info.backup_copy_valid, "Damaged redundant game copy was reported as valid");
        check(Bytes(primary_only.begin() + 108, primary_only.end()) == Bytes(damaged.begin() + 64, damaged.end()), "Converter repaired original backup payload");
        for (size_t offset : {size_t(0), size_t(3), size_t(4), size_t(8), size_t(15), size_t(7), size_t(0x30), size_t(0x32), size_t(0x38), size_t(0x3C), size_t(64 + 8192 + 0x1FFC)}) {
            auto bad = original; bad[offset] ^= 1; invalid(bad, "Invalid GCI identity/metadata/checksum was accepted");
        }
        for (size_t slot = 0; slot < 3; ++slot) {
            auto bad = original; bad[64 + 8192 + 8 + slot * 0x770 + 7] ^= 1;
            checksum(bad.data() + 64 + 8192, 0x1FFC, bad.data() + 64 + 8192 + 0x1FFC, false);
            invalid(bad, "A corrupt primary quest log was accepted despite valid aggregate checksum");
        }
        auto bad = original; p32(bad.data() + 64 + 8192 + 4, 1); game_checksums(bad.data() + 64 + 8192); invalid(bad, "Unsupported game-data version accepted");
        bad = original; bad.pop_back(); invalid(bad, "Truncated GCI accepted"); bad = original; bad.push_back(0); invalid(bad, "Trailing GCI bytes accepted");
        const auto raw_source = raw(original), raw_card = convert(raw_source, &info);
        check(info.format == BwCardImportInfo::Format::Raw && !info.redundant_filesystem_copy_used, "Raw format/copy metadata incorrect");
        write(folder / "raw.card", raw_card); backend(folder / "raw.card", payload, 4);
        uint64_t serial = 0; for (size_t i = 0; i < 32; i += 8) { uint64_t word = 0; for (size_t j = 0; j < 8; ++j) word = word * 256 + raw_source[i + j]; serial ^= word; }
        uint64_t stored = 0; for (size_t i = 20; i < 28; ++i) stored = stored * 256 + raw_card[i]; check(stored == serial, "Raw native card serial lost");
        for (size_t b : {size_t(1), size_t(2), size_t(3), size_t(4)}) {
            bad = raw_source; bad[b * 8192] ^= 1; check(convert(bad, &info) == raw_card && info.redundant_filesystem_copy_used, "One corrupt filesystem copy did not select redundant valid metadata");
        }
        bad = raw_source; bad[8192] ^= 1; bad[3 * 8192] ^= 1; invalid(bad, "Two corrupt metadata copies were silently rescued");
        bad = raw_source; bad[1] ^= 1; invalid(bad, "Raw header checksum ignored");
        bad = raw_source; p16(bad.data() + 0x22, 8); raw_checksums(bad); invalid(bad, "Mismatched physical geometry accepted");
        bad = raw_source; p16(bad.data() + 0x24, 2); raw_checksums(bad); invalid(bad, "Unsupported raw encoding accepted");
        bad = raw_source; bad.resize(bad.size() - 8192); invalid(bad, "Truncated raw card accepted");
        const auto bat = size_t(3 * 8192), ww = size_t(8192 + 37 * 64);
        bad = raw_source; p16(bad.data() + bat + 10, 5); raw_checksums(bad); invalid(bad, "Newest looping BAT fell back to old coherent copy");
        bad = raw_source; p16(bad.data() + ww + 0x36, 17); raw_checksums(bad); invalid(bad, "Overlapping native/foreign chains accepted");
        bad = raw_source; p16(bad.data() + ww + 0x38, 11); raw_checksums(bad); invalid(bad, "Wrong native Wind Waker block count accepted");
        bad = raw_source; p16(bad.data() + bat + 10 + (19 - 5) * 2, 0xFFFF); p16(bad.data() + bat + 6, 44); raw_checksums(bad); invalid(bad, "Orphan allocation accepted");
        bad = raw_source; p16(bad.data() + ww + 0x36, 19);
        std::copy_n(bad.data() + 5 * 8192, 8192, bad.data() + 19 * 8192); bad[19 * 8192] ^= 1;
        p16(bad.data() + bat + 10, 0); p16(bad.data() + bat + 10 + (19 - 5) * 2, 9);
        p16(bad.data() + bat + 8, 0xFFFF); p16(bad.data() + bat + 10 + (64 - 5) * 2, 0x1234);
        raw_checksums(bad); const auto native_selected = convert(bad);
        check(native_selected[108] == uint8_t(payload[0] ^ 1), "Noncritical unused BAT bytes/lastAllocated changed BIOS active-copy selection");
        bad = raw_source; p16(bad.data() + bat + 10 + (18 - 5) * 2, 64); raw_checksums(bad); invalid(bad, "Malformed foreign file chain was ignored");
        bad = raw_source; bad[ww + 3] = 'P'; raw_checksums(bad); invalid(bad, "Foreign-region Wind Waker raw card accepted");
        bad = raw_source; std::copy_n(bad.data() + ww, 64, bad.data() + 8192 + 38 * 64); raw_checksums(bad); invalid(bad, "Duplicate Wind Waker identity accepted");
        bad = raw_source; p16(bad.data() + 8192 + 0x1FFA, 0x8000); p16(bad.data() + 2 * 8192 + 0x1FFA, 0x7FFF);
        bad[ww + 3] = 'P'; raw_checksums(bad); check(convert(bad) == raw_card, "BIOS signed directory-counter selection changed");
        // Full production stage/validation/publication path, including UTF-8
        // names, fault retention and unchanged original/source/current bytes.
        const auto current = folder / "current.card", pending = fs::u8path(utf8(current) + ".pending"), source = folder / fs::u8path("original-\xE2\x98\x83.gci");
        write(current, raw_card); write(source, original);
        const auto current_name = utf8(current), backup_name = utf8(folder / "Backups"), source_name = utf8(source);
        BwCardManagerConfig config{}; config.card_path = current_name.c_str(); config.backup_directory = backup_name.c_str();
        BwCardManagerOutcome outcome{};
        std::unique_ptr<BwCardManager, decltype(&bluewake_card_manager_destroy)> manager(bluewake_card_manager_create(&config, &outcome), bluewake_card_manager_destroy);
        check(manager && bluewake_card_manager_stage(manager.get(), source_name.c_str(), &outcome) && read(pending) == gci_card && read(current) == raw_card && read(source) == original,
            "Production GCI stage changed current/source or emitted wrong converted container");
        write(source, raw_source); check(bluewake_card_manager_stage(manager.get(), source_name.c_str(), &outcome) && read(pending) == raw_card && read(source) == raw_source, "Format detection relied on extension or raw stage failed");
        write(source, original); bluewake_card_manager_test_fail_once(BW_CARD_TEST_CONVERSION_FLUSH);
        check(!bluewake_card_manager_stage(manager.get(), source_name.c_str(), &outcome) && read(pending) == raw_card && read(current) == raw_card && fs::exists(fs::u8path(outcome.recovery_path)), "Conversion flush failure lost pending/current/recovery data");
        check(bluewake_card_manager_stage(manager.get(), source_name.c_str(), &outcome), outcome.message);
        check(bluewake_card_manager_apply_at_startup(manager.get(), &outcome) == BW_CARD_MANAGER_OK && outcome.startup_ready && outcome.card_changed && !fs::exists(pending) && read(current) == gci_card,
            "Converted card did not publish through existing backup/startup transaction");
        check(read(fs::u8path(outcome.backup_path)) == raw_card && read(source) == original, "Startup replaced original/current before backup"); backend(current, payload, 4);
        std::ofstream(folder / "result.txt") << "PASS: native gczelda metadata, every quest-log checksum, whole unchanged game payload, stable GCI identity, raw geometry/header/dual filesystem/BIOS counter/entire allocation graph, native serial, UTF-8 staging, backend validation, conversion-flush recovery and startup backup/publication. Synthetic data only.\n";
        std::cout << "PASS original card import; fixture: " << utf8(folder) << '\n'; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << "\nFixture: " << utf8(folder) << '\n'; return 1; }
}
