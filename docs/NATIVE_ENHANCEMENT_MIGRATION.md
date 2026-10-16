# Native PC enhancement and shared-progress migration

Recorded on 2026-10-15 (America/Chicago), after native Windows bring-up. The direct C++ GameCube port is the primary implementation. The earlier BlueWake recompilation checkout and its diagnostics remain preserved as reference material. HD feature migration follows a stable PC game; it does not block controls, saves, or the shared-progress foundation.

This is an implementation plan, not a claim that the old enhancement or multiplayer implementation already runs in this port. [Windows bring-up](WINDOWS_PC_BRINGUP.md) records the qualified executable, normal new-game-to-Outset run, measured windows, package, and remaining acceptance. Git branch/remotes and source promotion are handled separately from this document.

## Accepted behavior

- Co-op means **independent exploration with shared permanent progress**, following Shipwright Anchor's queue-and-apply model. Each player owns their simulation, controls, camera, scene, cutscene timing, and ordinary resource counts. This is not the archived multiple-Links-in-one-simulation design.
- Use an owned system-socket TCP service and room saves. No Steam transport is required. This project's protocol is not public Anchor-compatible and must not connect to the Shipwright public service.
- The first enhancement milestone remains controls, menu, and core gameplay conveniences. The agent implements and runs the relevant checks; the user's play session provides additional acceptance rather than replacing engineering tests. Tests must not take over the user's mouse.
- Preserve the agreed shortcuts: hold **LB** on a modern controller, or **Tab** on keyboard, then **Up: play the Wind Waker; Left: deploy the cannon at sea; Right: deploy the Grappling Hook crane at sea**. Require earned equipment and the native safe action context. Plain D-pad retains map behavior; Down retains its native action, including with the modifier. X/Y/Z assignments remain unchanged.
- Keep correct left/right conducting directions. Camera inversion applies only to the camera; it must not invert baton directions, songs, grappling controls, or item selection. The first controls slice checks actual left/right conducting with both camera inversion axes off and on; hardware acceptance remains separate.
- Experimental wall climbing must eventually handle Wind Waker's sloped and faceted walls. Re-qualify it through native collision and player procedures later; do not copy the old guest-memory controller.
- **Tingle rescue wait-skip stays OFF.** Preserve its existing work and diagnostic evidence. It is not a gate for this roadmap, a tester package, or unrelated gameplay work. Tingle chest access is a different feature and must not silently enable that wait-skip.

## Current native inventory and gaps

The inventory below comes from current `native/src/pc`, its headers, game source, and the bring-up record. “Present” means a source implementation exists; only the evidence in the last column establishes local qualification.

| Area | Present in this native port | Missing or awaiting separate qualification |
| --- | --- | --- |
| PC foundation | Windows build/launcher, disc picker, D3D12 renderer, scripted virtual PAD, original-rate pacing, native card backend and explicit `COS_CARD_DIR` | Normal title and real prologue-to-controllable-Outset are qualified. Broader stability, physical controllers, audible quality, and an in-game save/reload round trip remain open. |
| Menu/settings | `PcSettingDesc` registry; Graphics, Performance, Debug, Controls and Network tabs; persistent scalar settings, restart-only room profiles, F1/controller menu input and pause/input blocking | Further player-facing tabs, search, named presets, binding/profile editor, input viewer, device reconnect/deadzone qualification. Controls camera/shortcut settings have live and fresh-process reload checks; Network gameplay/menu qualification is tracked separately in the LAN pilot record. |
| Controls | SDL-backed PAD; camera X/Y inversion; keyboard/controller menu navigation; optional native owned-equipment LB/Tab shortcuts and release/profile-conflict guards | Persistent action rebinding and full device-profile editing remain unmigrated. Scripted-modifier acceptance and physical-controller qualification are distinct. |
| Display/performance | Aspect, fullscreen, internal scale, optional interpolation, overlays, screenshots, pipeline controls and diagnostics | Fresh native settings consistently select 30 FPS after the menu default correction; an isolated options smoke with `COS_TARGET_FPS` absent confirmed that value and passed at frame 360. Keep this profile for regression and qualify optional modes individually. Existing HD-labelled controls do not establish Wii U feature parity. Detailed verification belongs in the Windows bring-up record. |
| Assets | Optional Dolphin-format HD texture replacement and size caps, off by default | The old ordered pack catalog, conflict UI, manifests and shareable asset presets are not migrated. Model/audio packs remain later work. |
| Gameplay hooks | Copied native lifecycle and CARD request/result/quest-load events, actor epochs, and an explicitly correlated Aryll Telescope producer; guarded idempotent native apply | The first adapter is deliberately narrow. Other reward producers, general enhancement subscriptions, native autosave and a recoverable save/reward ledger remain unimplemented. Process-local save generations are not durable reward receipts. |
| Shared progress | Extracted bounded native TCP client/server and durable room store; strict new native compatibility; both-slot GCI isolation and restart-only room configuration; Telescope-only adapter and Network controls | Standalone schema, store, reconnect, routing and correlation checks pass. Actual two-game reward/save/reload, menu and unsafe-state outcomes are recorded in [the LAN pilot](NATIVE_NETWORK.md), separately from source presence. Wider whitelists and remote Links are not implemented. The Switch debug server remains unrelated. |
| Other enhancements | Original native gameplay and selected SwitchWaker correctness fixes | Old text-speed, equipment conveniences, HUD customization, separated audio controls, accessibility, challenge modes and randomizer foundations require explicit native adaptation and acceptance. |

The old enhancement roadmap and protocol are preserved at:

```text
E:\GPT5\WindWakerRecomp\bluewake-porpoise\docs\ENHANCEMENT_ROADMAP.md
E:\GPT5\WindWakerRecomp\bluewake-porpoise\docs\NETWORK_PROTOCOL.md
```

Their completed controls fixtures, real reward/save checks, Tingle Tuner sharing, reconnect experiments and durable-store tests are **historical evidence from the recompilation host**. Their descriptions sometimes call those runs “native”; that does not make them qualification of this direct C++ port. Preserve failures and partial results alongside successes. For example, old cannon use, complete reward reconciliation, durable-host gameplay acceptance, and remote Links were not all complete.

## Phase 1: controls, menu, and equipment shortcuts

The first slice implements the Controls tab, retains camera keys/defaults, and adds `COS_QUICK_ITEMS`
(desktop only, default off). Enable it through Controls > Equipment shortcuts. Native player/ship
paths handle conducting, cannon deployment/firing and crane deployment/lowering/retraction/stow;
no guest-memory controller, X/Y/Z overlay or fabricated item-button press is used. The input filter
runs before JUT button-edge conversion. Claimed directions remain suppressed until released,
and enable/menu/focus/device/profile/scene-player changes require neutral input before reuse.
Custom physical or keyboard mappings that conflict with shortcut sources retain native controls
and decline those shortcuts. The Switch adapter remains disabled and its menu row is hidden.
The implemented subset and its actual test outcomes are recorded in the
[Windows controls qualification](WINDOWS_PC_BRINGUP.md#first-native-controls-migration).
Persistent binding editing, its save/retry UI and live hardware reconnect acceptance remain open;
the following phase gate includes that later work as well as the completed equipment slice.

Extend the existing registry and SDL/PAD integration. Keep scalar settings/presets separate from persistent device bindings, so applying a display or gameplay preset cannot overwrite a controller profile. Support keyboard and controller rebinding, deadzones, hotplug, an input viewer, and reset-to-default behavior in small reviewable increments. Reuse the old editor's behavior and test cases where useful; adapt its host input boundary to Aurora instead of introducing another competing PAD provider.

The implemented Quick Items modifier requests the appropriate **native player or boat procedure** on a fresh supported direction press. Continue to preserve this boundary while adding profile editing: do not rewrite selected inventory buttons, grant equipment, fabricate an item award, or synthesize an item pickup. Consume only the shortcut directions that the feature owns. Multiple/conflicting directions, unavailable equipment, loading, conversations, menus and unsafe player procedures must not trigger an action. Cancel pending actions across scene/player changes and require release after menu capture, device reconnect or profile reload. Ordinary D-pad and X/Y/Z remain usable.

Native source entry points to inspect include `d_a_player_tact.inc`, `d_a_player_main.cpp`, `d_a_ship.cpp`, and `m_Do_controller_pad.cpp`; determine the correct callable procedure and its prerequisites from their source. The old `quick_items.c` contains guest PC/register writes and audited addresses: it is a behavior reference, not an implementation to transplant.

**Gate:** meaningful input/profile tests plus isolated game checks. Cover restarts and reconnects preserving bindings; held-key suppression on menu open/close; a failed profile save retaining a visible unsaved state and retry; presets preserving bindings; plain D-pad/Down; and all three shortcuts with and without owned equipment. Actually conduct and cancel, deploy/fire the cannon at sea, and lower/raise/stow the crane, with unchanged X/Y/Z before and after. Verify left/right baton directions with camera inversion both on and off. Virtual PAD proves the scripted path; physical-controller coverage is recorded separately.

Existing native harness cases provide concrete starting points: `COS_SMOKE=pad-echo` for PAD interpretation; `options-menu` with its `#expect` assertions for actual pause/settings persistence; `camera-invert` for camera-only inversion; `sailing` for boat controls; and `COS_MILESTONE=outset-real` with `new-game.txt` for normal progression and control. They do not cover the new shortcuts or binding profiles until extended with their actual outcomes. Review current menu row order before reusing old scripts; keep state assertions rather than accepting a completed frame count.

Target player-facing organization is Display, Controls, Enhancements, Mods, Network, Sound & Saves, Developer. Add sections when they contain implemented behavior. A hidden development experiment or clearly unavailable status is acceptable; a multiplayer-looking page is not evidence of co-op. Core conveniences such as text speed and selected equipment animations follow one at a time, retaining native waits, choices, cancellation and cleanup.

## Phase 2: typed gameplay and lifecycle events

Create a small native event boundary for player updates, scene changes, real item awards, whitelisted permanent progression, and native save/load transactions. Use named enums and copied values, session/save-slot identity, an epoch and scene/player generation, and stable invocation/reward identifiers. A worker receives neither a game pointer nor a CPU/RAM interface. Native actor pointers may be resolved transiently on the game thread; never retain them across destruction or scene changes without revalidation.

Emit award completion from the actual accepted native reward path, rather than inferring it from a flag appearing in a snapshot. `execItemGet` is not a universal success-returning transaction: audit each producer's lifecycle. Ordinary chest items, gifts, songs, equipment, capacity upgrades, repeatable consumables and temporary borrowed gear are different cases. Save-loaded inventory and received network facts are not newly earned local awards. Network apply must not feed the local outgoing-award path.

Scene/player destruction, a new quest, card reload, reset and changed save identity revoke queued actions and observations. Save events distinguish requested, actually committed, failed, and actually loaded. Do not label a queued write, machine snapshot, or structurally inspected GCI as a completed gameplay save/reload transaction.

**Gate:** optimized native code reaches the hooks, real transactions emit once, scene changes invalidate old handles, cancellation emits no award, and reload does not replay one. Exercise an ordinary chest and the genuine Telescope, Wind's Requiem, Orca sword and Medli Grappling Hook routes using copied test cards. Borrowed training equipment must not become permanent. Combine focused source tests with actual game runs; snapshots alone cannot establish the correct award producer.

The native `event-sweep` case can drive selected authored demos, but its exit code alone is insufficient: it also exits zero for several incomplete outcomes. Inspect the event artifact for the requested event actually starting and completing, reject missing-demo/stuck/scene-lost outcomes, and correlate the genuine award/save records. Likewise, the existing `save` smoke's format checks are not an in-game reward-to-save-to-reload transaction.

## Phase 3: real saves, backups, and isolated room profiles

First qualify an ordinary manual save and fresh-process in-game reload using the current backend. Add backups, explicit import/restore and guarded autosave around native save procedures. Preserve the other quest slots, photos, card identity and unrelated files; test populated data separately. Autosave waits for a safe native context and actual reward completion, retaining pending work through busy scenes and write failures.

Select personal or room storage **before any CARD backend opens**. Build on the now explicit `COS_CARD_DIR` override, using the native backend's actual directory/GCI representation; the old host's single `.card` path is not a format conversion. A room profile is determined by compatible game/schema/rules, endpoint/room identity, and stable player identity. Hash length-framed validated identity into a bounded route. Credentials never appear in paths or logs. A routing error refuses startup, rather than falling back to the personal card. No automatic personal-card import or room-to-personal export.

The mounted room/profile is immutable for the process. Changing room, identity, endpoint or progression-affecting compatibility requires restart. Client settings that only affect display, sound or shortcut input do not split progress compatibility. Back up before any explicit import/restore; publish a save plus reward-receipt sidecar through a recoverable generation/journal design rather than assuming two separate file replacements are one atomic operation.

**Gate:** real native save/reload after a real earned item; backups restore the expected quest; room A, room B and personal mode never open each other's files; malformed routes and escaping links fail safely; failed writes do not report success; interrupted publication recovers a consistent generation. All source cards remain byte-preserved. No network feature is enabled until this isolation gate passes.

## Phase 4: transport extraction and native permanent-progress adapter

Reuse the old bounded TCP/session/storage mechanisms selectively, after separating their copied-value types from the guest bridge. Follow Anchor's model: the socket worker validates and queues immutable messages; the native game thread checks the current lifecycle and applies eligible permanent facts. Keep independently exploring scenes and local player timing authoritative. The protocol is this project's service, with an explicit native compatibility identity; it is not an Anchor client.

The source audit found these boundaries:

| Donor source under old `runtime/host/src/` | Intended treatment |
| --- | --- |
| `network_wire.h`, `network_session.cpp` | Candidate framing, nonblocking sockets, queue bounds, replay and reconnect logic. Preserve limits and failure behavior; extract host-independent types first. Windows uses Winsock/`ws2_32`. |
| `network_store.cpp`, `network_digest.h`, `atomic_file.h` | Candidate durable room records, receipt validation, exclusive store ownership, hashing and flushed publication. Re-qualify native Windows build and filesystem failures. |
| `network_preferences.cpp` | Reuse validated configuration principles; adapt the storage route to native directory/GCI cards. |
| `network_session.h` / `progression_sync.h` | Currently pull in `game_events.h` and guest `CPUState` types. Split the wire DTOs and pure schema/merge rules into headers with no game-memory dependency. |
| `network_game.cpp`, guest accessors in `progression_sync.c`, `game_events.c` | Replace with native typed adapters and lifecycle subscription. Do not copy guest addresses, RAM offsets, register conventions, module leases or GXRuntime write journals. |
| `scripts/network/CMakeLists.txt` and fixtures | Preserve as donor evidence. Its apparent standalone build still includes guest bridges/GXRuntime headers and `standalone_memory.c`; it is not already a clean native transport library. Preserve attribution and source licenses when extracting actual reusable code. |

Replace the translated-module digest with an intentional native compatibility contract: exact supported disc/revision, wire version, native progression schema and semantic build/rules digest, including randomizer seed identity if later supported. Bump/reject incompatible legacy manifests; never claim compatibility merely because field sizes match. Serialize stable integer facts, not raw native C++ structs, pointers, padding or save offsets.

Begin with a very small permanent whitelist whose **native effects** are qualified. Candidate facts are permanent tools, songs, capacities, pearls/shards and selected named story/dungeon facts. Local arrows, bombs, magic, health, rupees, bottles and contents, equipment selection, temporary switches, position, camera and cutscene state stay local. Do not grant capacity upgrades by refilling consumables. Defer temporary recollection inventories and story-specific equipment removal/restoration. Derived sword/shield/bracelet equipment and cached X/Y/Z tool upgrades require their native refresh path before sharing.

Do not synchronize opened-chest markers, boss-heart suppression (`STAGE_LIFE`), small-key state or other reward-suppressing flags until their actual rewards have a reconciled identity. A chest flag arriving ahead of its item can permanently remove an unclaimed reward. Add a room-scoped reward ledger for once-only permanent rewards, with explicit rules for repeatable consumables and excluded local rewards.

**Gate:** newly adapted standalone transport/schema tests, then two independent native game processes in different scenes. One earns a genuine whitelisted reward; the other receives its permanent effect once without replaying a local award, refilling resources or changing selections. Unsafe cutscenes/load transitions defer apply. Actual receive then native save, fresh offline reload, rejoin and absence of duplicate grant must all pass. Only this scoped result may be described as experimental shared-progress co-op.

## Phase 5: reconnect, durable receipts, and usable room controls

Keep bounded framing, queue limits, monotonic server revisions and stable client sequence receipts. Exact replay returns an acknowledgement without another commit; a changed payload at an existing sequence is rejected. Queue saturation or a revision gap requests a validated full snapshot/reconnect instead of silently dropping progress. Preserve unacknowledged outgoing work through automatic reconnect. A fresh room identity is not the previous room's receipt history.

The server acknowledges only after durable state and its replay receipt have been flushed and published. A storage failure never degrades silently to an in-memory acknowledgement. On the client, associate received reward receipts with the confirmed native save generation, recover an interrupted save/receipt publication, and reconcile after genuine native load. Incoming canonical facts and outgoing locally earned facts have separate origins, preventing echo loops and duplicate fanfares or rewards.

Retain the old bounded-room/client/history approach as the starting contract, not an unbounded ledger. Expired receipts and divergent generations need explicit recovery behavior. A room credential is not encrypted Internet authentication; the initial implementation keeps the documented local/LAN service scope and does not advertise a public relay.

Once the previous gates pass, expose honest room controls: configure a restart-required profile, Host/Join, compatibility rejection, roster, connection/reconnect status, pending progress, save/receipt failures, Leave/Rejoin. Show the current whitelist and limitations. Presence may report a player's scene/status; it does not imply a rendered remote Link or scene synchronization.

**Gate:** owned server termination/restart restores room identity, progress, revisions and receipts before client reseeding; lost acknowledgements do not duplicate a reward; fragmented/oversized frames, wrong credentials/compatibility, future/gapped/replayed sequences, two store writers and real filesystem failures have asserted outcomes. Re-run the real two-game reward scenario, save both room profiles, restart, and verify convergence with personal saves untouched. Standalone tests, loader success, and an exit code of zero do not substitute for these gameplay conditions.

## Later enhancement work

After the foundation, migrate text speed and selected equipment conveniences; mod-pack management; HUD layout/colors; genuinely separated music/SFX controls and replacement audio; accessibility; challenge profiles; and randomizer catalog/seed/reward systems in independently tested increments. The old roadmap records which parts were fixture-only or incomplete. Reuse native implementations already present rather than duplicating them, and review later SwitchWaker correctness/QoL commits one at a time.

During optional HD-texture menu qualification, make periodic texture statistics explicitly opt-in in both the menu descriptor and loader fallback. Both currently default to 300 frames on desktop when HD textures are enabled. HD textures are off on a fresh install, so the baseline checks do not establish this optional path's logging behavior.

Remote Link models, animation and interpolation are a separate phase after permanent-progress correctness. Their presence data is cosmetic and room-scoped, expires on disconnect/scene change, and never drives another player's simulation. Uneven-wall climbing also remains an optional later feature, validated on several real sloped/faceted surfaces with stable grab, corner traversal, falling/cancellation and original ladders/ivy preserved.

Each completed feature records source revision, test inputs, original/candidate hashes where relevant, isolated data paths, actual native outcomes and remaining limits. Keep the current native baseline playable while these phases advance. Optional HD visuals, climbing, remote models and Tingle rescue wait-skip do not gate unrelated milestones or tester builds.
