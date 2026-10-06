// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bluewake::randomizer::storage {
using Bytes = std::vector<std::uint8_t>;
struct Generation {
    std::string profile, origin_card, card_digest, ledger_digest, record_digest;
    std::uint64_t number = 0;
    Bytes card, ledger;
};
// File storage only. These copied receipts do not authorize guest reads, item
// awards or native save success. The game owner must validate those separately.
class CommitReceipt {
public:
    CommitReceipt() = default; // Invalid, never an acknowledgement.
    const std::string& profile() const { return profile_; }
    const std::string& origin_card() const { return origin_card_; }
    const std::string& card_digest() const { return card_digest_; }
    const std::string& ledger_digest() const { return ledger_digest_; }
    const std::string& record_digest() const { return record_digest_; }
    std::uint64_t number() const { return number_; }
private:
    friend class Store;
    CommitReceipt(const Generation& g) : profile_(g.profile),origin_card_(g.origin_card),
        card_digest_(g.card_digest),ledger_digest_(g.ledger_digest),record_digest_(g.record_digest),number_(g.number) {}
    std::string profile_, origin_card_, card_digest_, ledger_digest_, record_digest_;
    std::uint64_t number_ = 0;
};
class Store {
public:
    static constexpr std::size_t MaxCardBytes = 2u * 1024u * 1024u;
    static constexpr std::size_t MaxLedgerBytes = 256u * 1024u;
    Store();
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    // One owner thread and a dedicated seed directory, with an exclusive process
    // lease for its lifetime. Destroy only after the native backend is closed.
    // The caller's canonical profile SHA-256 must match every retained record.
    bool open(const char* directory, const std::string& profile, std::string& error);
    bool load(Generation& out, bool& found, std::string& error,
              CommitReceipt* receipt = nullptr) const;
    // Whole CARD and ledger publish in a single flushed, checksummed envelope.
    // expected_generation is zero only for the first complete pair. The previous
    // complete pair is retained before replacement. Failure never returns a receipt.
    bool commit(std::uint64_t expected_generation, const Bytes& card,
                const Bytes& ledger, CommitReceipt& out, std::string& error);
    // Before the native backend opens: replace only this store's working.card
    // from its complete pair, recovering uncommitted native multi-write changes.
    // Requires a stored generation. Never accepts an arbitrary destination path.
    bool restore_working_card(std::string& error);
    std::string working_card_path() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
#ifdef BLUEWAKE_RANDOMIZER_STORE_TEST
enum class Fault { None, BeforePublish, AfterPublish };
void fail_next_commit(Fault);
#endif
} // namespace bluewake::randomizer::storage
