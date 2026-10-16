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

The extraction intentionally changes the wire magic, namespace, protocol to 4,
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

The permanent progression format deliberately rejects protocol2/schema1 pilot rooms and store1
files; there is no silent migration. Store2 receipts refer to the immutable
producer ledger to retain the existing512KiB bound. A complete ledger is sent
in ordered pages and published with facts atomically before Connected; partial,
duplicate, reordered or inconsistent pages never become game-visible state.

Wire4 adds a separate transient, render-only Link pose channel. Durable storage
headers and length-framed CARD routes explicitly retain storage protocol3,
schema2, the same semantic/rules digests and profile INI version2. Existing
protocol3/schema2 room stores, client journals, pending operations and isolated
CARD routes are reopened in place; there is no data migration, import or silent
fallback. Protocol3 clients cannot join a protocol4 socket. This separation does
not authorize reusing persistent identities after future permanent-rule changes.

The worker owns copied configuration, protocol bytes, facts and poses. Native
state is observed/applied only by the game thread outside this module. Poses
never enter facts, durable client sequences, ACKs, revisions, producer ledgers,
journals or CARD saves. There is no remote simulation authority, scene/cutscene
timing, collision, resource, item-selection or reward-suppression synchronization.

Each pose contains copied stage/room/layer identity, a local quest generation,
root/base scale, exactly42 final world-affine joint matrices and native scale
flags for the GZLE01 rev0 Link hierarchy. A hidden pose is a canonical one-byte
zero; stale or nonfinite inactive fields never prevent hiding. Visible data is
finite and bounded. No animation index, actor/model pointer or consumable crosses
the channel. A future renderer must own its model/materials and must not apply
the root twice or execute a second player actor.

The server stamps origin and a fresh connection epoch from its joined roster;
clients never choose a forwarded origin. Per-socket transient sequence numbers
only increase and are unrelated to reliable progression sequences. Senders and
relays coalesce to at most10Hz. Eight replace-latest wire slots plus at most one
already-partial frame are separate from the256 reliable-frame queue. Four
complete reliable frames receive priority before one bounded transient frame;
partial TCP frames finish without interleaving bytes. New peers receive all
Welcome/ledger pages before any pose. A high-rate publisher replaces unsent
poses instead of filling reliable queues or forcing a backpressure disconnect.

Leave/reconnect drops unsent poses and copied remote rows; game code must publish
freshly after Connected. Changed rosters retire old epochs, hidden packets clear
rows, and receiver workers/snapshot copies expire rows after1000ms without a
new sample. Server relays also drop expired latest samples. This is local
receipt-based freshness, not a synchronized network-clock or latency guarantee.
Local quest/area/hidden changes clear visible copied rows; snapshots filter to
the locally published stage/room/layer. Remote quest/scene/epoch changes replace
the complete row. A future interpolator must reset its history on those tokens.

Bounds remain 8 connected sockets, 16 rooms, 128 stable client identities per
room, 128 pending operations/replay receipts, 4096 authored rewards per room,
4 KiB message bodies, 64 KiB receive
buffers, 256 reliable queued wire frames and 512 KiB durable room files. Transient
frames retain the same4KiB body bound. Reconnect retains
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
The separate presence checks use real loopback sockets, bounded backpressure,
durable receipts and fixed independently serialized protocol3 persistence
fixtures. They do not prove remote Link visuals, animation, interpolation,
performance or gameplay; no native render adapter is part of this module change.
