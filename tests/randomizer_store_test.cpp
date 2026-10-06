// SPDX-License-Identifier: GPL-3.0-or-later
// All CARD/ledger bytes below are synthetic opaque storage payloads. This test
// neither validates a native card nor supplies native award/save evidence.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "randomizer_store.h"
#include "network_digest.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <crtdbg.h>
#endif

namespace fs = std::filesystem;
using namespace bluewake::randomizer::storage;
static unsigned checks;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #x); std::abort(); } ++checks; } while (0)

static std::string hash(const Bytes& b) { return bw_net::digest(std::string(b.begin(), b.end())); }
static Bytes payload(std::size_t n, unsigned salt) {
    Bytes b(n); for (std::size_t i = 0; i < n; ++i) b[i] = static_cast<std::uint8_t>((i * 37 + salt * 19) & 255); return b;
}
static Bytes read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary); CHECK(bool(f));
    Bytes b(std::istreambuf_iterator<char>(f), {}); CHECK(!f.bad()); return b;
}
static void write_file(const fs::path& p, const Bytes& b) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc); CHECK(bool(f));
    f.write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size())); CHECK(bool(f));
    f.close(); CHECK(!f.fail());
}
static bool open(Store& s, const fs::path& p, const std::string& profile, std::string& error) {
    return s.open(p.u8string().c_str(), profile, error);
}
static void invalid(const CommitReceipt& r) {
    CHECK(r.number() == 0 && r.profile().empty() && r.origin_card().empty() &&
          r.card_digest().empty() && r.ledger_digest().empty() && r.record_digest().empty());
}
static void receipt(const CommitReceipt& r, const Generation& g) {
    CHECK(r.number() == g.number && r.profile() == g.profile && r.origin_card() == g.origin_card &&
          r.card_digest() == g.card_digest && r.ledger_digest() == g.ledger_digest && r.record_digest() == g.record_digest);
}
static Generation loaded(Store& s, const std::string& profile, std::uint64_t number,
                         const Bytes& card, const Bytes& ledger, const std::string& origin) {
    Generation g; bool found = false; std::string error; CommitReceipt r;
    CHECK(s.load(g, found, error, &r) && found && error.empty());
    CHECK(g.profile == profile && g.number == number && g.card == card && g.ledger == ledger);
    CHECK(g.card_digest == hash(card) && g.ledger_digest == hash(ledger) && g.origin_card == origin);
    receipt(r, g); return g;
}
static std::vector<fs::path> pending(const fs::path& folder, const std::string& stem) {
    std::vector<fs::path> out;
    for (const auto& e : fs::directory_iterator(folder))
        if (e.path().filename().u8string().rfind(stem + ".pending-", 0) == 0) out.push_back(e.path());
    return out;
}
// Only the public version-1 framing is used to create malformed envelopes;
// successful recovery assertions compare original caller-owned payload bytes.
static void checksum(Bytes& b) {
    CHECK(b.size() >= 64); const auto h = bw_net::digest(std::string(b.begin(), b.end() - 64));
    std::copy(h.begin(), h.end(), b.end() - 64);
}
static void number(Bytes& b, std::uint64_t n) {
    for (unsigned i = 0; i < 8; ++i) b[8 + i] = std::uint8_t(n >> (56 - i * 8));
}
static void rejected_open(const fs::path& p, const std::string& profile) {
    Store s; std::string error; CHECK(!open(s, p, profile, error) && !error.empty());
    CHECK(s.working_card_path().empty());
    Generation g; bool found = true; CommitReceipt r;
    CHECK(!s.load(g, found, error, &r) && !found && g.number == 0); invalid(r);
}
static void malformed(const fs::path& base, unsigned id, const std::string& profile,
                      const Bytes& current, const Bytes* previous = nullptr) {
    const auto p = base / ("malformed-" + std::to_string(id)); fs::create_directory(p);
    if (!current.empty()) write_file(p / "current.bwseed", current);
    if (previous) write_file(p / "previous.bwseed", *previous);
    rejected_open(p, profile);
    if (!current.empty()) CHECK(read_file(p / "current.bwseed") == current);
    if (previous) CHECK(read_file(p / "previous.bwseed") == *previous);
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _CALL_REPORTFAULT); _set_error_mode(_OUT_TO_STDERR);
#endif
    CHECK(argc <= 2);
    const auto parent = fs::absolute(argc == 2 ? fs::u8path(argv[1]) : fs::temp_directory_path()).lexically_normal();
    CHECK(fs::is_directory(parent));
    const auto base = parent / ("BlueWake-seed-store-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(fs::equivalent(base.parent_path(), parent) && !fs::exists(base)); fs::create_directory(base);
    const std::string profile = bw_net::digest("synthetic seed profile A"), other_profile = bw_net::digest("synthetic seed profile B");
    CHECK(bw_net::digest("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const Bytes card_a = payload(513, 1), ledger_a = payload(71, 2), card_b = payload(769, 3), ledger_b = payload(99, 4);
    const std::string origin = hash(card_a); Bytes first, second;
    const auto ordinary = base / fs::u8path(u8"ordinary-\u2603");
    {
        Store s; std::string error; CommitReceipt r; invalid(r);
        CHECK(!s.restore_working_card(error) && s.working_card_path().empty());
        CHECK(!s.open(nullptr, profile, error)); CHECK(!s.open("", profile, error));
        CHECK(!open(s, ordinary, "INVALID", error));
        CHECK(open(s, ordinary, profile, error) && error.empty());
        CHECK(fs::u8path(s.working_card_path()) == fs::absolute(ordinary / "working.card"));
        CHECK(!open(s, base / "wrong-second-open", profile, error));
        Generation g; bool found = true; CHECK(s.load(g, found, error, &r) && !found && g.number == 0); invalid(r);
        CHECK(!s.restore_working_card(error) && !fs::exists(ordinary / "working.card"));
        Store competing; CHECK(!open(competing, ordinary, profile, error));
        Store independent; CHECK(open(independent, base / "independent", other_profile, error));
        CHECK(independent.commit(0, Bytes{1}, {}, r, error));
        CHECK(s.commit(0, card_a, ledger_a, r, error));
        g = loaded(s, profile, 1, card_a, ledger_a, origin); receipt(r, g);
        first = read_file(ordinary / "current.bwseed"); CHECK(g.record_digest == hash(first));
        CHECK(!fs::exists(ordinary / "previous.bwseed"));
        for (std::uint64_t stale : {std::uint64_t(0), std::uint64_t(2), std::numeric_limits<std::uint64_t>::max()}) {
            CHECK(!s.commit(stale, card_b, ledger_b, r, error)); invalid(r);
            CHECK(read_file(ordinary / "current.bwseed") == first);
        }
        CHECK(!s.commit(1, {}, ledger_a, r, error)); invalid(r);
        CHECK(!s.commit(1, payload(Store::MaxCardBytes + 1, 1), ledger_a, r, error)); invalid(r);
        CHECK(!s.commit(1, card_a, payload(Store::MaxLedgerBytes + 1, 1), r, error)); invalid(r);
        CHECK(pending(ordinary, "current.bwseed").empty() && !fs::exists(ordinary / "previous.bwseed"));
        bool foreign_load = true, foreign_commit = true, foreign_restore = true;
        std::thread foreign([&] { Generation x; bool f = true; std::string e; CommitReceipt cr;
            foreign_load = s.load(x, f, e, &cr); foreign_commit = s.commit(1, card_b, ledger_b, cr, e);
            foreign_restore = s.restore_working_card(e); }); foreign.join();
        CHECK(!foreign_load && !foreign_commit && !foreign_restore && read_file(ordinary / "current.bwseed") == first);
        CHECK(s.commit(1, card_b, ledger_b, r, error));
        g = loaded(s, profile, 2, card_b, ledger_b, origin); receipt(r, g);
        second = read_file(ordinary / "current.bwseed"); CHECK(g.record_digest == hash(second));
        CHECK(read_file(ordinary / "previous.bwseed") == first);
        CHECK(s.restore_working_card(error) && read_file(ordinary / "working.card") == card_b);
        // An uncommitted native multi-write is repaired from the stored pair.
        write_file(ordinary / "working.card", Bytes{0, 255, 1});
    }
    {
        Store s; std::string error; CHECK(open(s, ordinary, profile, error));
        loaded(s, profile, 2, card_b, ledger_b, origin);
        CHECK(s.restore_working_card(error) && read_file(ordinary / "working.card") == card_b);
    }
    rejected_open(ordinary, other_profile);
    CHECK(read_file(ordinary / "current.bwseed") == second && read_file(ordinary / "previous.bwseed") == first);

    for (const bool previous : {false, true}) {
        const auto folder = base / (previous ? "changed-previous" : "changed-current"); fs::create_directory(folder);
        write_file(folder / "current.bwseed", second); write_file(folder / "previous.bwseed", first);
        Store s; std::string error; CHECK(open(s, folder, profile, error));
        const auto changed_path = folder / (previous ? "previous.bwseed" : "current.bwseed");
        auto changed = previous ? first : second; changed[152] ^= 1; checksum(changed); write_file(changed_path, changed);
        Generation g; bool found = true; CommitReceipt r;
        CHECK(!s.load(g, found, error, &r) && !found && g.number == 0); invalid(r);
        CHECK(!s.commit(2, card_a, ledger_a, r, error)); invalid(r);
        CHECK(!s.restore_working_card(error)); CHECK(read_file(changed_path) == changed);
    }
    unsigned n = 0; Bytes bad;
    bad = second; bad[0] ^= 1; checksum(bad); malformed(base, ++n, profile, bad);
    bad = second; bad[7] = 2; checksum(bad); malformed(base, ++n, profile, bad);
    bad = second; number(bad, 0); checksum(bad); malformed(base, ++n, profile, bad);
    bad = second; bad[16] = 'G'; checksum(bad); malformed(base, ++n, profile, bad);
    bad = second; bad[80] = 'G'; checksum(bad); malformed(base, ++n, profile, bad);
    bad = second; std::copy(other_profile.begin(), other_profile.end(), bad.begin() + 16); checksum(bad); malformed(base, ++n, profile, bad);
    bad = second; bad[144] = 255; checksum(bad); malformed(base, ++n, profile, bad);
    bad = second; bad[148] = 255; checksum(bad); malformed(base, ++n, profile, bad);
    bad = second; bad[152] ^= 1; malformed(base, ++n, profile, bad);
    bad = second; bad.back() = 'G'; malformed(base, ++n, profile, bad);
    bad = second; bad.resize(bad.size() - 1); malformed(base, ++n, profile, bad);
    bad = second; bad.resize(152); malformed(base, ++n, profile, bad);
    bad = second; bad.push_back(0); malformed(base, ++n, profile, bad);
    bad.assign(152 + Store::MaxCardBytes + Store::MaxLedgerBytes + 65, 0); malformed(base, ++n, profile, bad);
    bad = first; bad.back() ^= 1; malformed(base, ++n, profile, second, &bad);
    bad = first; bad.resize(153); malformed(base, ++n, profile, second, &bad);
    bad = first; number(bad, 4); checksum(bad); malformed(base, ++n, profile, second, &bad);
    bad = first; number(bad, 2); checksum(bad); malformed(base, ++n, profile, second, &bad); // Same number requires identical pair.
    bad = first; bad[80] = bad[80] == '1' ? '2' : '1'; checksum(bad); malformed(base, ++n, profile, second, &bad);
    bad = first; std::copy(other_profile.begin(), other_profile.end(), bad.begin() + 16); checksum(bad); malformed(base, ++n, profile, second, &bad);
    malformed(base, ++n, profile, {}, &first); // Previous cannot become current implicitly.

    {
        const auto folder = base / "generation-exhausted"; fs::create_directory(folder);
        auto maximum = first; number(maximum, std::numeric_limits<std::uint64_t>::max()); checksum(maximum);
        write_file(folder / "current.bwseed", maximum); Store s; std::string error; CommitReceipt r;
        CHECK(open(s, folder, profile, error));
        loaded(s, profile, std::numeric_limits<std::uint64_t>::max(), card_a, ledger_a, origin);
        CHECK(!s.commit(std::numeric_limits<std::uint64_t>::max(), card_b, ledger_b, r, error)); invalid(r);
        CHECK(read_file(folder / "current.bwseed") == maximum && !fs::exists(folder / "previous.bwseed"));
    }

    {
        const auto folder = base / "maximum"; Store s; std::string error; CommitReceipt r;
        CHECK(open(s, folder, profile, error));
        const auto card = payload(Store::MaxCardBytes, 9), ledger = payload(Store::MaxLedgerBytes, 10);
        CHECK(s.commit(0, card, ledger, r, error)); loaded(s, profile, 1, card, ledger, hash(card));
        CHECK(s.restore_working_card(error) && read_file(folder / "working.card") == card);
    }
    for (const Fault fault : {Fault::BeforePublish, Fault::AfterPublish}) {
        const auto folder = base / (fault == Fault::BeforePublish ? "fault-before" : "fault-after"); Bytes old;
        {
            Store s; std::string error; CommitReceipt r; CHECK(open(s, folder, profile, error));
            CHECK(s.commit(0, card_a, ledger_a, r, error)); old = read_file(folder / "current.bwseed");
            CHECK(s.restore_working_card(error)); fail_next_commit(fault);
            CHECK(!s.commit(1, card_b, ledger_b, r, error) && !error.empty()); invalid(r);
            CHECK(read_file(folder / "previous.bwseed") == old && read_file(folder / "working.card") == card_a);
            if (fault == Fault::BeforePublish) {
                CHECK(read_file(folder / "current.bwseed") == old); loaded(s, profile, 1, card_a, ledger_a, origin);
            } else {
                Generation g; bool found = true; CHECK(!s.load(g, found, error, &r) && !found); invalid(r);
                CHECK(!s.restore_working_card(error));
            }
        }
        Store reopened; std::string error; CHECK(open(reopened, folder, profile, error));
        const bool published = fault == Fault::AfterPublish;
        loaded(reopened, profile, published ? 2 : 1, published ? card_b : card_a, published ? ledger_b : ledger_a, origin);
        CHECK(reopened.restore_working_card(error)); CHECK(read_file(folder / "working.card") == (published ? card_b : card_a));
    }
    {
        const auto folder = base / "candidate-not-replayed"; fs::create_directory(folder);
        write_file(folder / "current.bwseed", first);
        write_file(folder / "current.bwseed.pending-1-2-3", second);
        write_file(folder / "working.card.pending-1-2-3", Bytes{3, 2, 1});
        Store s; std::string error; CHECK(open(s, folder, profile, error)); loaded(s, profile, 1, card_a, ledger_a, origin);
        CHECK(s.restore_working_card(error) && read_file(folder / "working.card") == card_a);
        CHECK(read_file(folder / "current.bwseed.pending-1-2-3") == second);
        CHECK(read_file(folder / "working.card.pending-1-2-3") == Bytes({3, 2, 1}));
    }
    {
        const auto folder = base / "hard-linked"; fs::create_directory(folder); write_file(folder / "current.bwseed", first);
        fs::create_hard_link(folder / "current.bwseed", base / "outside-alias"); rejected_open(folder, profile);
        CHECK(read_file(base / "outside-alias") == first);
    }
    {
        const auto folder = base / "unknown"; fs::create_directory(folder); write_file(folder / "unrelated.card", card_a);
        rejected_open(folder, profile); CHECK(read_file(folder / "unrelated.card") == card_a);
    }
    {
        const auto folder = base / "too-many-candidates"; fs::create_directory(folder);
        for (unsigned i = 0; i < 32; ++i) write_file(folder / ("current.bwseed.pending-1-2-" + std::to_string(i)), Bytes{0});
        rejected_open(folder, profile); // seed.lock becomes entry 33, never prune others.
        CHECK(std::distance(fs::directory_iterator(folder), fs::directory_iterator()) == 33);
    }
#ifdef _WIN32
    {
        const auto folder = base / fs::u8path(u8"deny-delete-\u2603"); Bytes old; fs::path candidate;
        {
            Store s; std::string error; CommitReceipt r; CHECK(open(s, folder, profile, error));
            CHECK(s.commit(0, card_a, ledger_a, r, error)); old = read_file(folder / "current.bwseed");
            CHECK(s.restore_working_card(error));
            // Real OS sharing contract: retaining READ/WRITE sharing but denying
            // DELETE prevents replacement of the current file, after candidate flush.
            HANDLE held = CreateFileW((folder / "current.bwseed").c_str(), GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            CHECK(held != INVALID_HANDLE_VALUE);
            CHECK(!s.commit(1, card_b, ledger_b, r, error) && !error.empty()); invalid(r);
            CHECK(read_file(folder / "current.bwseed") == old && read_file(folder / "previous.bwseed") == old);
            const auto candidates = pending(folder, "current.bwseed"); CHECK(candidates.size() == 1); candidate = candidates.front();
            CHECK(read_file(candidate) == second); // Exact failed complete pair, not partial payload.
            Generation g; bool found = true; CHECK(!s.load(g, found, error, &r) && !found); invalid(r);
            CHECK(!s.restore_working_card(error) && read_file(folder / "working.card") == card_a);
            CHECK(CloseHandle(held));
        }
        Store reopened; std::string error; CHECK(open(reopened, folder, profile, error));
        loaded(reopened, profile, 1, card_a, ledger_a, origin); CHECK(reopened.restore_working_card(error));
        CHECK(read_file(folder / "working.card") == card_a && read_file(candidate) == second);
    }
    {
        const auto folder = base / "deny-working-delete"; Store s; std::string error; CommitReceipt r;
        CHECK(open(s, folder, profile, error) && s.commit(0, card_a, ledger_a, r, error) && s.restore_working_card(error));
        HANDLE held = CreateFileW((folder / "working.card").c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        CHECK(held != INVALID_HANDLE_VALUE); CHECK(!s.restore_working_card(error)); CHECK(CloseHandle(held));
        Generation g; bool found = true; CHECK(!s.load(g, found, error, &r) && !found); invalid(r);
        CHECK(read_file(folder / "working.card") == card_a && pending(folder, "working.card").size() == 1);
    }
#endif
    CHECK(fs::equivalent(fs::canonical(base).parent_path(), parent)); fs::remove_all(base);
    std::printf("Randomizer opaque storage checks=%u; synthetic files only, no native CARD/save/award evidence\n", checks);
    return 0;
}
