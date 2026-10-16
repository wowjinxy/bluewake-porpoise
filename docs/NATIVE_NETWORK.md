# Native LAN shared-progress pilot

Status at 2026-10-16 03:43 UTC: five standalone Windows foundation cases, the
actual two-game Telescope reward/save case, both fresh offline native reloads,
and paused receive after persistent server restart pass. The Network menu's Host
action reached Connected and its captured image was inspected. This qualifies
only the experimental Telescope pilot described below, not full multiplayer.
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
stands in for the other. Actual receive, the native save-screen procedures and
fresh offline room reload passed in isolated diagnostic runs; this does not
qualify a durable client reward ledger or guarded autosave.

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

## Qualification

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

The integrated executable was linked at 2026-10-16 03:27:31 UTC and is 13,761,536
bytes, SHA-256 `ef84bbf98064b9d1a237d54e854b71d36be267b912594977e73248fd3ef7b833`.
Its implementation is committed as `532ae6d18f9a8ed457473a0cf213fcd21a1dd720`.
Later documentation commits do not change that binary. All game cases below use
this exact executable or a hash-verified copy with the same eight runtime files.

| Native case | Actual observed result | Private evidence beneath the migration directory |
| --- | --- | --- |
| Two independent games | Simultaneous Outset `sea:44:206` and Windfall `sea:11:9` processes, two peers. Only the Outset fixture sets opening event `2A80`; neither starts with granted equipment. Ordinary B input completes authored `awake`, `omedeto`, `get_telescope` and follow-up dialogue. | `two-games3/results.json` |
| Genuine local reward | Aryll completion emitted once, invocation 1/epoch 5; local awards 1, remote applies 0, server revision 1. | `two-games3/runtime-gift/` |
| Remote effect | Telescope absent at baseline, then local awards 0/remote applies 1. XYZ assignments and resource counts match the captured baseline; local scene/player identity is retained. | `two-games3/runtime-receive/` |
| Native saves | Both processes completed the original save-screen steps and compared the written GCI contents with packed native quest state. File 1 passed at frames 4837 and 3262, including actual paths longer than MAX_PATH. These are automated native saves, not physical menu-input acceptance. | Both preceding runtime directories |
| Fresh offline reloads | Separate new processes loaded each original room GCI through native file select and entered `sea`, room 44, point 128. Telescope remained owned, local awards and remote applies both stayed zero. Native loaded state matched file 1 except save date; both GCI hashes were unchanged. No server or automatic connection was active. | `post-532ae6d1-20261015/loads2/` |
| Durable host restart and paused receive | A new host process reopened the original persistent room store. A third private player joined with the native pause menu open; canonical progress stayed pending for 12 paused frames without a grant, then applied exactly once after closing and passed a native save. Original sender/receiver GCI hashes were unchanged. | `paused-rejoin1/` |
| Network Host menu | Actual scripted menu action created the persistent room, mounted both isolated card slots, and reached Connected with PostC online. Frame-600 image inspected; Telescope-only and independent-scene limitations were visible. | `post-532ae6d1-20261015/menus/runtime-network-menu-host/`, `network-menu-inspection.json` |
| Controls regression and reload | Existing menu fixture navigated past the appended Network tab, changed both camera axes and equipment shortcuts, then passed at frame 660. A separate process restored those settings at frame 360 without changing the settings file hash. | `post-532ae6d1-20261015/menus/runtime-controls-live/`, `runtime-controls-reload/` |
| Fresh personal defaults | With target-FPS and explicit-network-profile environment overrides absent and no default network.ini, the game retained 30 FPS, shortcuts/camera inversion off, overlay off and no room connection. Existing settings/default assertions passed at frame 360. | `post-532ae6d1-20261015/menus/runtime-fresh-defaults/` |
| Startup refusal | Malformed explicit profile and enabled room plus explicit personal-card override each exited 2 before CARD initialization, with the expected specific refusal. Private sentinels and card trees stayed unchanged. | `post-532ae6d1-20261015/startup/` |

Successful native children exited zero and their owned process jobs drained;
the two intended startup refusals exited 2 and drained. The two-game driver's
before/after check found no personal card changes. Production personal cards
were never copied into these fixtures. Transport tests use real local sockets;
native game acceptance also uses same-machine processes. External-machine LAN,
packet loss and extended gameplay remain unqualified.

Earlier diagnostic failures remain intact. `two-games1` incorrectly treated
creation of a temporary camera actor as replacement of the actual player; the
driver now validates the actual player/scene IDs. In `two-games2` the receiver
passed but the sender driver stopped ordinary B input before follow-up dialogue
finished and timed out. The final driver continues native dialogue input; it
does not skip original event timing or modify reward state. The first `loads`
attempt refused the private wrapper's disk preflight before either game started.
Its fresh `loads2` retry passed. Windows had expanded its system-managed
pagefile; subsequent small runtime/package operations preserve 2 GiB free,
while the separate build runner retains the required 8 GiB build reserve.

## Remaining limits and follow-up

This whitelist contains Telescope only. Other gifts, chests, songs, equipment,
capacities and story/dungeon facts need their own producer, effect and save
qualification. Reward-suppressing flags, a durable client reward ledger,
transactional save/receipt recovery, autosave, save import/restore, remote Link
rendering and public relays are not implemented. Server durability is qualified
separately from each player's native GCI; it is not a combined transaction.

Standalone tests cover replay and reconnect. A native already-loaded owning
quest rejoining without a duplicate effect, mid-save termination, forced native
read/write failures and every unsafe cutscene/transition still need focused
game cases. A paused receive is evidence for that specific guard only. Existing
normal-new-game, performance, audio and physical-controller acceptance retain
their original executable identities; they are not rerun or implied here.

Read-only review identified two additional native integration edges outside the
qualified default configuration: a failed save-menu CARD read can leave sharing
unauthorized across a successful retry, and a host's reconnect after quest load
does not repeat the initial local-bind endpoint override. For the latter, a host
with a configured public endpoint different from its listener can reconnect to
the wrong address. Restart/reload the quest after a card-read failure; use the
explicit Host action again after loading when hosting with such an endpoint.
These require fixes and failure-path/native reconnect qualification in a later
source checkpoint. The menu also needs clearer canonical pending counts and
removal of a stale Connecting hint alongside Connected; its lower help hint can
clip at 960x720. None of these results expands the pilot into general shared
progress or full-game stability acceptance.
