// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_NETWORK_STORE_H
#define BLUEWAKE_NETWORK_STORE_H
#include "network_session.h"
#include <deque>
#include <map>
#include <memory>
#include <string>
namespace bw_net {
struct RoomReceipt { uint64_t sequence; BwProgressionDelta delta; };
struct RoomClientHistory { uint64_t sequence=0; std::deque<RoomReceipt> recent; };
struct StoredRoom {
    BwNetworkCompatibility compatibility{};
    std::string name,id,salt,credential;
    BwProgressionState progress{};
    uint64_t revision=0;
    std::map<std::string,RoomClientHistory> clients;
};
/* File schema is independent of the unchanged wire protocol. Only validated
 * permanent facts and replay receipts enter it; never CARD/guest memory.
 * Single server-worker owner, protected by an exclusive cross-process lease.
 * Each accepted mutation is flushed and atomically published before its ACK.
 * A failed publication stops further durable acknowledgements. */
class RoomStore {
public:
    static constexpr unsigned version=1, max_rooms=16, max_clients=128;
    static constexpr size_t max_file_bytes=512u*1024u;
    RoomStore();
    ~RoomStore();
    RoomStore(const RoomStore&)=delete;
    RoomStore& operator=(const RoomStore&)=delete;
    bool open(const char* directory,std::string& error);
    bool load(const BwNetworkCompatibility&,const char* room,StoredRoom&,bool& found,std::string& error) const;
    bool save(const StoredRoom&,std::string& error);
    unsigned rooms() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
std::string room_storage_key(const BwNetworkCompatibility&,const char* room);
void room_set_credential(StoredRoom&,const char* password);
bool room_credential_matches(const StoredRoom&,const char* password);
}
#endif
