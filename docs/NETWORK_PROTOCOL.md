# Wind Waker shared progress protocol

BlueWake uses its own system-socket TCP protocol and dedicated server. Clients
explore independently: CPU state, camera, scene timing and current controls are
local. The server relays a roster and copied scene/position presence; remote
Link models are not implemented by this core.

The architecture follows [Shipwright Anchor's game-thread packet queue](https://github.com/HarbourMasters/Shipwright/blob/cb71e22a79bc5d1f688fa881795bbd93094895fc/soh/soh/Network/Anchor/Anchor.cpp).
The wire namespace is `BlueWake.WindWaker.GZLE01.Progression`. This server is not
an Anchor server and must not be used with the public Shipwright service.

## Current progression schema

Schema 1 uses stable integer keys, not guest addresses. Every received value
must satisfy its key's whitelist before it can enter a room snapshot or game
memory. Primary native layout and behavior are audited against the local
`tww` source: `include/d/d_save.h`, `d_com_inf_game.h`, `d_stage.h`,
`src/d/d_save.cpp`, `d_item.cpp`, `d_s_play.cpp`, and `d_meter.cpp`.

| Keys | Shared facts | Merge |
| --- | --- | --- |
| 0–20, excluding 14–17 | Permanent tool ownership; canonical inventory slots, including camera/bow upgrades | Highest audited rank |
| 32–35 | Wallet tier, maximum magic, arrow capacity, bomb capacity | Maximum allowed tier/capacity |
| 67–68, 73–75 | Pirate/charm ownership, songs, Triforce shards, pearls | Audited ownership bits |
| 192–207 | Map, compass, boss key, boss defeat and boss-demo marker for sixteen saved stages | Five audited bits, mask `0x2F` |
| 244, 255 | Named placed-pearl flags and Grandma healed | Named bits only |

All collected sword/shield/bracelet flags are excluded until their native
derived equipment path is qualified; an ownership bit alone does not equip a
usable weapon. Native Forsaken Fortress/barrel story also removes the basic
Hero's Sword flag. Native `GTower` removes the bow slot temporarily and restores
it from permanent obtained bits on exit. Incoming bow ownership preserves that
hidden slot in `GTower`. A received bow/bomb bag derives its minimum permanent
capacity, and a received Deku Leaf derives minimum maximum magic. Current
arrows, bombs and magic are never refilled by network apply. Deku Leaf is
inventory slot 6; slot 5 is Boomerang and never grants maximum magic. Native
recollection (its active byte or authored `Xboss0`–`Xboss3` stages) defers both
capture and apply, preserving the temporary loadout and its later restoration.

The schema does not sync any chest markers: opening markers can suppress native
small keys, bottles or hearts whose rewards have not been reconciled. It also
excludes `STAGE_LIFE`, which suppresses a native boss heart container.
The schema does not sync resource counts, bottles or their contents, bag
contents, arbitrary event registers, switches, temporary scene flags, position
or equipment selections. Heart-piece/heart-container reward reconciliation,
full quest/event coverage and consumable quest deliveries need explicit
reward identities and further native qualification. Refresh of already cached
X/Y/Z equipped tool data also requires native qualification. Already selected
bow/camera upgrades may need the player to reselect the tool or reload the
native room save; this core leaves the cached selection unchanged. This is
an experimental audited subset, not complete story co-op acceptance.

## Compatibility and room saves

A handshake includes the exact game ID, protocol namespace/version, progression
schema, semantic build digest, translated module digest and normalized
progression-affecting options digest. Incompatible clients are rejected before
any progression exchange. The room has a separate optional credential; the
credential never enters a file path or log. Numeric IPv4/IPv6 addresses and
`localhost` avoid an unbounded DNS operation during game-thread shutdown.
The current server listens on an explicitly selected numeric IPv4 address.

Local audio/display controls, dialogue speed, equipment shortcuts and the two
equipment animation rates are independent client preferences. They do not
enter the permanent-progress options digest or select a different room card.
Wind selection, item awards, ownership and native equipment transitions keep
their normal semantics. Rules that change rewards, progression or seed identity
must instead participate in compatibility before they can be shared.

Startup selects the save route before CARD management or the backend opens:

```
data/Network/<SHA256 of complete compatibility + endpoint + room identity>/<player-id>/GZLE01.card
```

The player ID is a stable 32-character lowercase hexadecimal value. Room names
are bounded ASCII slugs. No unvalidated server address is a path component;
the entire manifest/endpoint/room identity is length-framed before hashing.
Path resolution rejects traversal, escaping links and a card equivalent to
the personal card. This short route fits ordinary Windows data paths.
`network.ini` is bounded, duplicate-checked and atomically persisted. A routing
or preferences error fails startup rather than falling back to personal CARD.

Changing room, player, endpoint or compatibility requires restart. Join/host/
leave controls only act within the immutable mounted room. Leaving pauses
sharing and stops an owned local server; it does not switch to personal saves.
No personal save is automatically copied or imported into a room.

Each client persists its room progression through the normal native CARD path.
The dedicated server's room state is currently in memory. A server restart
gets a new room generation; authorized clients re-submit their native room
whitelists. Cross-room merge and personal export remain separate future work.

## Queue, lifecycle and replay rules

The socket worker never receives a CPU or RAM pointer. A game-thread bridge
subscribes to genuine GameEvents and consumes bounded copied updates after its
retrace. Only an explicitly mounted isolated room can export or apply progress.
Native CARD load authorizes a room snapshot. A clean first new quest can have
no CARD load: native opening scene initialization plus a completed native
player update with controls ready authorizes the one startup cold-boot token.
Every later state load, machine reset, module reload or replaced-memory event
revokes that token until a genuine native CARD load.

Apply checks the current native player, scene, transition, menu pause, event and
demo fields again before writing. Incoming deltas remain pending through unsafe
controls/cutscenes. Writes use GXRuntime's normal memory-write journal. The
game-thread known state advances only after outgoing queue admission succeeds;
queue refusal retries from the room RAM whitelist. Remote apply updates the
known state to avoid an echo. Room-authoritative permanent values remain after
the incoming queue is consumed and reconcile again after native lower-tier
assignments, preserving higher camera/bow/wallet/capacity tiers. Reconciliation
keeps the same native readiness, recollection and temporary bow guards. State
loads cannot export stale RAM or reapply canonical values until genuine native
CARD load reauthorizes that mounted room.

The frame header is 20 bytes, big endian: `BWPN`, `u16 version`, `u16 type`,
`u32 body length`, `u64 client sequence`. Bodies are at most 4096 bytes. Strings
are bounded `u16 length + bytes`, without embedded NUL. Delta bodies are
`u16 key + u32 value`. Welcome contains a complete validated room snapshot;
commit contains origin, client sequence, server revision and canonical value.

Limits are eight connected clients, sixteen rooms, 128 stable identities per
room, 128 queued incoming/outgoing progression entries, and 128 replay receipts
per identity. TCP fragments are assembled before decoding. Exact sequence
replays return an acknowledgement without another commit; changed replay
payloads, expired replay receipts and future sequence gaps are rejected.
Unacknowledged outgoing sequences survive automatic reconnect. A fresh room
identity rebases pending sequences; a revision gap or inbound queue saturation
reconnects for a complete room snapshot.

## Standalone verification

`scripts/network/CMakeLists.txt` builds the server and source-only fixtures
without game assets or a translated game module. The tests cover whitelist
accessors, native temporary-equipment guards, compatibility/password rejection,
replay/gap validation, fragmented/oversized frames, actual socket reconnect,
room save isolation, lifecycle cancellation and transport backpressure.

After building, run:

```
ctest --test-dir <network-build> --output-on-failure
python scripts/network/loopback_test.py --build <network-build> --receipt <receipt.json>
```

The loopback test launches a dedicated server and two independent clients,
observes exactly two live commits, then verifies a reconnect snapshot. On
Windows it uses hidden child processes and never uses desktop input. These
synthetic tests do not substitute for the separate two-game/native CARD
qualification. `BLUEWAKE_NETWORK_TRACE=1` logs lifecycle/capture/apply facts for
that qualification; it is off by default and never logs credentials.

## Native qualification scope

The Windows candidate was also tested with three hidden native game processes,
using separate isolated room CARDs and the player's matching disc/module. One
player activated the authored Tingle release button in Pnezumi, completed the
original dialogue and received Tingle Tuner from the native present-item actor.
The observer recorded exactly one Tuner award. A second player stayed at sea
and received exactly one ownership update for slot 7, with no local Tuner award
and unchanged resources, item selectors and equipped gear. Personal CARDs and
the genuine source CARD remained unchanged. The Pnezumi route used a diagnostic
native stage warp followed by normal controller inputs.

After the first host exited, a fresh host/server with the original unowned room
CARD started. The surviving player reconnected and reseeded its earned Tuner
ownership. The fresh host applied it once after native CARD authorization and
native controls readiness, without a native Tuner award. Its first snapshot
was already after apply; the receipt retains that late observation and uses
explicit log ordering and the exact original unowned CARD baseline instead.
All three processes kept optimized direct calls enabled and used no machine
state load, guest inventory edits or desktop input.

Another two-game run enabled native autosave in both isolated room profiles.
The sender earned Tuner once; the sea player applied that ownership once
without a local award. Both completed native saves, and the receiver's second
save retained the received Tuner. A fresh offline native launch loaded that
receiver card with slot 7 still owned, no award replay, no network session and
autosave disabled. The original personal cards, other two quest logs, photo
region and card serial were preserved; native checksums and redundant copies
passed. This was an explicit private qualification copy, not automatic room-to-
personal migration.

This qualifies native Tuner sharing, real server restart/reseed and persistence
of that received reward across a native card reload. Other whitelist entries
remain source/fixture qualified. Complete story co-op, chest reward ledgers,
native derived equipment refresh and remote Links are separate acceptance work.
