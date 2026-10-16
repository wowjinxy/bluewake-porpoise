# Native permanent-progress transport

The session, wire framing, SHA-256 digest, atomic file helpers and durable room
store are adapted from the preserved BlueWake recompilation checkout's
`runtime/host/src/network_session.{h,cpp}`, `network_wire.h`,
`network_store.{h,cpp}`, `network_digest.h`, `atomic_file.h`, and bounded
configuration validation from `network_preferences.cpp`. The donor session's
last committed change was `11553ac63fcc9689978e66f60adf6ac37d189619`.
The original checkout and its uncommitted work remain untouched.

These files retain their donor GPL-3.0-or-later license, with the complete license
in `COPYING`. Native adapters outside this module retain their own licenses.

The extraction intentionally changes the wire magic, namespace, protocol to 3,
store magic, compatibility model and whitelist. It cannot connect to legacy
BlueWake or a public Anchor service. Canonical compatibility names USA GZLE01
revision 0, the native semantic implementation, schema 2, and the current rules.
Cosmetic settings and source commits do not change room identity; changes to the
meaning of permanent facts require an explicit rules/schema change.

The game-free `pc/permanent_facts.h` defines sparse stable IDs and value domains
for permanent tools, songs, capacities, pearls and shards. Max merge retains
higher fixed ranks. Wallet, arrow and bomb upgrades are server-authoritative
STEP operations: a typed, stable authored producer advances the room ladder
once. Submitting an observed local capacity as a raw fact is rejected, preventing
the same fairy from repeatedly increasing canonical progress. The immutable
producer ledger rejects changed operations and acknowledges exact duplicates
without another step. IDs never contain actor process IDs or memory addresses.

This revision deliberately rejects protocol2/schema1 pilot rooms and store1
files; there is no silent migration. Store2 receipts refer to the immutable
producer ledger to retain the existing512KiB bound. A complete ledger is sent
in ordered pages and published with facts atomically before Connected; partial,
duplicate, reordered or inconsistent pages never become game-visible state.

The worker owns copied
configuration, protocol bytes and facts. Native game state is observed/applied
only by the game thread outside this module. There is no position, scene timing,
resource, item-selection or reward-suppression synchronization.

Bounds remain 8 connected sockets, 16 rooms, 128 stable client identities per
room, 128 pending operations/replay receipts, 4096 authored rewards per room,
4 KiB message bodies, 64 KiB receive
buffers, 256 queued wire frames and 512 KiB durable room files. Reconnect retains
unacknowledged local sequences. Saturation/gaps reconnect for a full snapshot;
submission failure leaves the fact with its caller. Explicit Leave discards work.
Room creation and each replay receipt are flushed/published before ACK; store
failure disables durable acknowledgement with no ephemeral fallback. Stored
credentials are salted digests; the protocol remains an owned local/LAN service,
not encrypted Internet authentication. Credentials are never placed in logs or
filenames. One store writer holds an exclusive process lease.

The native extraction additionally limits each server-peer turn to eight frames
and yields after each durable delta, so it pumps acknowledgements between real
filesystem flushes and services other peers rather than withholding an entire
128-entry replay backlog behind synchronous disk writes.

The ephemeral server API exists for focused standalone tests. Actual native
gameplay must opt into an immutable room CARD profile before starting a durable
host/client and must not seed personal inventory automatically. These module
tests alone do not prove real reward, native save/load or multiplayer gameplay.
