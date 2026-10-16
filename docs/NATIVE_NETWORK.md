# Native LAN shared-progress pilot

Status at 2026-10-16 03:18 UTC: the native networking foundation passes its five
standalone Windows cases. Native two-game reward, save/reload and menu acceptance
are still pending. The first game harness attempt stopped at an incorrect
readiness check; it does not establish a game regression or a multiplayer pass.
See [Windows bring-up](WINDOWS_PC_BRINGUP.md) for the playable baseline and
[the migration plan](NATIVE_ENHANCEMENT_MIGRATION.md) for the remaining gates.

This is an experimental owned TCP service for local/LAN play. It does not use
Steam and cannot connect to public Shipwright Anchor services or the archived
BlueWake recompilation protocol. The current native compatibility is USA
`GZLE01`, disc revision 0, progression schema 1, protocol 2 and the fixed native
semantic/rules manifest below. A source commit or cosmetic setting does not
change that manifest; changing the meaning of shared facts must do so explicitly.

## What is shared

The only accepted permanent fact is **Telescope ownership**, key 1, value 1.
Players run independent scenes and simulations. Positions, camera, controls,
cutscene timing, consumable counts, X/Y/Z selections and reward-suppressing event
flags are not synchronized. There is no rendered remote Link in this pilot.

The local producer correlates Aryll's real Telescope gift with its native item
message, native ownership effect and event completion. A cancelled or deleted
producer, a different lifecycle, a save snapshot or an applied remote effect is
not a newly earned local reward. A loaded isolated room quest may reseed its
already owned permanent fact after native load authorization; personal inventory
is never imported automatically.

Incoming facts are copied from the worker and applied by the game thread through
the native Telescope effect. Unsafe menus, cutscenes, loading/scene transitions,
missing players, saves and other guarded states defer application. Remote apply
does not replay Aryll's reward or refill resources. Native lifecycle changes
invalidate stale actor observations and in-flight quest work.

Receiving a fact changes the active quest's memory. **Use the game's normal save
screen to keep it for an offline room reload.** The Network tab shows received
progress as unsaved until a matching prepared native quest save commits. A room
server's durable acknowledgement is independent of the player's GCI save; neither
stands in for the other. Real receive, manual save and fresh offline reload are
qualification gates still pending at the status recorded above.

## Configure a room

The Windows menu exposes a **Network** tab. Open the options menu with its usual
F1/controller control and select that tab. Profile text fields currently use
keyboard entry.

1. Enable **Use isolated room saves**.
2. Set **Server IP / localhost**, **Port**, **Room**, **Player name** and
   **Room credential**. Use the host's numeric IP, or `localhost` for processes
   on the same machine. Room names use letters, digits, `_` or `-`.
3. On the host, set **Host bind address** to its local IPv4 address, or
   `0.0.0.0` to listen on its IPv4 interfaces. The default `127.0.0.1` listens
   only locally. The server IP must address the host from that client.
4. Keep **Stable player identity**. It is a generated 32-character lowercase
   hexadecimal ID shown read-only in the menu. Different players need different
   identities; changing an identity selects a different isolated save route.
5. Select **Save room settings for next start**, then exit and restart the game.
   The current card mount never changes in place. Host/Join refuse a profile
   awaiting restart.
6. In the Network tab, the owner selects **Host room (LAN)**. Other players select
   **Join / reconnect** with the same endpoint, port, room and credential. Starting
   the game alone does not create a server or connect a client.
7. **Leave and stop owned host** disconnects this client and stops a server owned
   by this process. Its isolated room saves stay mounted. Rejoin uses that same
   mounted profile; returning to personal saves requires saving a disabled room
   profile and restarting.

The tab displays connection/reconnect status, peer names, pending queues, errors
and the unsaved received-progress warning. Room/endpoint/player/compatibility
changes require restart. Display, sound and shortcut preferences do not split
progress compatibility. The stored room credential is checked by this owned
service; its wire protocol is not encrypted Internet authentication.

## Save isolation and host storage

The default preferences file is `<user>/network.ini`, where `<user>` is the
native port's executable-relative user directory. A missing default file means
personal mode. A malformed existing file, or a missing explicitly selected file,
refuses startup instead of selecting personal saves silently.

An enabled profile resolves a card route before the first CARD backend opens:

```text
<user>/NetworkNative/<64-hex-route>/USA/Card A/
<user>/NetworkNative/<64-hex-route>/USA/Card B/
```

The route hashes length-framed native compatibility, normalized endpoint/port,
room and stable player identity. `localhost` normalizes to `127.0.0.1`. Display
name, credential, bind address and Host/Join role do not appear in the path or
split the route. Credentials and raw room/player names are not logged as route
identifiers. Both native CARD channels are redirected, so Card B cannot inherit
the personal default.

Each mounted room directory has a validated identity marker and an exclusive
writer lease. Existing links/reparse points, hardlinked files, escaping paths,
identity mismatches and nonempty unmarked directories refuse the mount. Profile
publication uses an exclusive temporary file, flush and atomic replacement;
Windows filesystem/CRT operations use extended UTF-16 paths. There is no
automatic personal-card import, room-to-personal export or fallback after a
route error. Personal mode retains the existing native card behavior and does
not start networking workers.

An owned host stores room state beneath `<network-root>/host-state`. Room
creation and accepted sequence receipts are flushed and atomically published
before acknowledgement. One process leases that host store exclusively. Server
restart restores the room identity, permanent state, revision and replay history;
storage failure does not enable an ephemeral gameplay fallback. Explicit Leave
discards the client's queued transport work. Automatic reconnect retains
unacknowledged sequences and requests an authoritative snapshot.

These are separate persistence boundaries: server receipts and a native player
GCI are not a combined two-file save transaction. Backups/import/restore UI and
broader reward sharing remain future work.

## Profile format and diagnostic overrides

Ordinary players can use the menu. The following complete example is the strict
v2 format for an isolated local fixture; save it as UTF-8/ASCII with a terminal
newline. Every key is required exactly once. Unknown or duplicate keys, malformed
values and incompatible manifests fail validation.

```ini
version=2
enabled=1
host=0
bind=127.0.0.1
server=127.0.0.1
port=49383
room=native-test
player_id=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
player_name=LinkA
room_password=private-fixture
create_room=0
game_id=GZLE01
build_id=bluewake-native-gzle01-r0
semantic_digest=8ec123d195b1d50ccc097df3f3931b7459664762756ce4f435cb7c314fbb3313
rules_digest=383ee6ed762a3e8faa75777d307ff4133f7b5826f5ca1efc4a34b96677660489
progression_schema=1
disc_revision=0
```

For a second test player use a different stable ID, such as 32 `b` characters,
and a different display name. Preserve that player's ID across later restarts.
The serialized `host` and `create_room` fields do not auto-connect at boot; the
explicit Host/Join action selects whether to create a room. Bind is numeric IPv4;
client endpoints accept normalized numeric IPv4/IPv6 or `localhost`. Port is
1–65535, room/name at most 32 characters, player ID exactly 32 lowercase hex
characters, and credential at most 64 printable ASCII characters. The actual
native host currently listens on IPv4.

| Override | Meaning |
| --- | --- |
| `COS_NETWORK_PROFILE` | Explicit preferences file. A nonempty override is required to exist and validate; it never falls back to the default file. |
| `COS_NETWORK_ROOT` | Private network storage root for diagnostics; defaults to `<user>/NetworkNative`. Keep each independent test client's root private. |
| `COS_CARD_DIR` | Existing personal-card override. An enabled room profile rejects a nonempty value, preventing an ambiguous or bypassed room route. Unset it for room runs. |

Use private settings/run directories as well when driving automated native
checks. These overrides are startup configuration, not live remount controls.
Do not point qualification runs at a player's production card. The fixture INI
above contains an artificial identity and credential, not a production profile.

## Bounds and implementation ownership

Current source bounds are 8 connected sockets, 16 rooms, 128 stable client
identities per room, 128 pending facts/replay receipts, 4 KiB message bodies,
64 KiB receive buffers, 256 queued wire frames and 512 KiB durable room files.
The native event queue is separately bounded at 64 entries; lost event history
stops sharing. Saturation/gapped sequences reconnect for a full snapshot, and
failed submission retains caller ownership. Server work yields between durable
receipts and limits frames processed per peer turn, so disk flushes do not hide
an entire replay backlog from acknowledgements and other peers.

Relevant source contracts:

- [Public copied session DTOs](../native/include/pc/network/network_session.h),
  [wire bounds](../native/src/pc/network/network_wire.h) and
  [durable store](../native/src/pc/network/network_store.h).
- [Room profile routing](../native/src/pc/features/pc_network_profile.cpp) and
  [native integration](../native/src/pc/features/pc_network_game.cpp).
- [Native lifecycle/reward adapter](../native/src/pc/game_hooks/pc_progression_game.cpp).

The transport, digest, atomic helpers and store are adapted from the preserved
BlueWake implementation. Their GPL-3.0-or-later notices, donor reference and full
license remain in [PROVENANCE.md](../native/src/pc/network/PROVENANCE.md) and
[COPYING](../native/src/pc/network/COPYING). Guest CPU state, RAM offsets and
recompilation reward adapters are not part of this native integration.

## Qualification recorded so far

The completed Windows foundation run at 2026-10-16 03:12 UTC passed all five
cases with exit code zero and their asserted success markers:

| Case | Actual result | Scope |
| --- | --- | --- |
| Schema/framing | 309 checks passed | Whitelist, compatibility and bounded framing. |
| Durable store | 91 checks passed | Actual private filesystem publication, persistence, replay state and failure cases. |
| Transport/replay/durable reconnect | 264 checks passed | Actual sockets, server restart, retained unacknowledged work and durable replay. |
| Room profiles | PASS | Disjoint synthetic GCI directories, normalized identities, links/aliases, exclusive writers, failed publication preservation, paths over 320 characters, reopen and immutable startup mount. |
| Native progression correlation | 51 checks passed | Game-free producer/lifecycle/apply rules; no gameplay or save claim. |

Private, ignored evidence is under
`build/pc-bringup-20261015/migration-network-20261015/`, with
`foundation-results3.json` and the five `runtime-*3` result/log directories.
Earlier failures are retained there: Windows MAX_PATH prevented pending-file
publication; the transport also needed to pump acknowledgements between durable
replay writes. The final foundation run includes those corrections. Profile
fixtures contain only small artificial text files, not copied player saves.

**Native game qualification remains pending.** The next evidence must establish
two real game processes exploring different scenes; a genuine Aryll award;
exactly one remote permanent effect without refill/selection changes; deferral
through unsafe transitions; actual native manual save and fresh offline room
reload; rejoin without duplicate grant; and no personal/other-room card access.
Network menu behavior and unchanged default personal gameplay must also be
checked on the final candidate. A compiled game, connected socket, synthetic
profile fixture or successful foundation test does not complete these gates.
