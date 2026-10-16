# Native standard multiplayer acceptance

Active objective, recorded 2026-10-16: **keep working until full standard
multiplayer is implemented and verified**. The Telescope pilot is a preserved
baseline, not completion of that objective. This checklist records missing
evidence as missing; implementation, standalone tests and real game acceptance
remain separate columns.

The accepted model is independent exploration with shared permanent progress
over an owned TCP service. Each player keeps local movement, camera, cutscenes
and ordinary consumable counts. Steam and public Anchor services are not used.
Visible players in a shared area are included in the working scope, pending the
user's optional clarification. Presence must not control another simulation.

## Requirements and current evidence

| Requirement | Current authoritative evidence | Remaining work |
| --- | --- | --- |
| Room setup, host/join/leave, stable identities and roster | Pilot `532ae6d1`, qualified as `8636102f`; actual Host menu and two local game processes | External-machine LAN, usable errors, lifecycle/restart/leave/rejoin regression on the expanded implementation |
| Independent scenes and timing | Pilot uses Outset and Windfall simultaneously with unchanged local player identity, selections and resources | Extended play and simultaneous rewards/scene transitions in the expanded implementation |
| Shared permanent inventory | Pilot qualifies genuine Aryll Telescope only | Real producers and native effects for tools, weapons/derived equipment, bags, songs, capacities, pearls/shards and other permanent progression |
| Once-only progressive rewards | No qualified pilot implementation | Stable authored reward identities; durable host deduplication; fairy, heart and other progressive rewards cannot increment twice when two players claim the same source |
| Shared world/dungeon progression | No qualified pilot implementation | Explicit reward identities and reconciliation before synchronizing chest/boss-heart or other reward-suppressing flags; coherent key/dungeon and story progression policies; no stranded rewards |
| Safe game-thread application | Pilot qualifies one Telescope receive and 12 paused native frames of deferral | Broader effects, menus/cutscenes/loading/player replacement, borrowed/recollection inventory, cancellation and stale work |
| Durable player saves and receipts | Pilot qualifies native saves and fresh offline loads of both Telescope room cards; server durability is separate | Persistent outgoing claims, client receipt journal, interrupted save/receipt recovery, native read/write failures and retry; offline persistence after host loss |
| Reconnect and host restoration | Pilot standalone transport/replay and restarted host with a fresh native receiver | Native owning-quest reconnect, retained unsent rewards, late join, duplicate delivery, host termination, explicit rejoin and server state reconciliation |
| Save/room isolation | Pilot routes both CARD slots before initialization and rejects unsafe routes/configuration | Expanded manifest/store compatibility, populated multi-slot/photo preservation, personal/other-room isolation across failure and recovery |
| Remote player presence | No native implementation or game proof | Bounded transient presence, scene/room filtering, expiry/disconnect, rendering/animation/interpolation and actual two-game visual acceptance; no remote authority over local actions |
| Player-facing multiplayer UI | Pilot Network tab; Telescope-only status and room setup | Broader shared-progress status, canonical pending effects vs transport queues, save/recovery errors, presence and honest compatibility messages |
| Build, regression and tester provenance | Qualified EF84 pilot and 34-entry tester at `8636102f` | Build expanded source with named Windows targets, run relevant standalone and native cases, preserve failures, verify packaged title and archive contents before replacing the tester |

The detailed pilot evidence and known edges remain in [Native LAN pilot](NATIVE_NETWORK.md).
Its executable SHA-256 is
`ef84bbf98064b9d1a237d54e854b71d36be267b912594977e73248fd3ef7b833`.
The previous tester, source commits, original saves and private results are
retained. None of that evidence qualifies newly edited source automatically.

## Current implementation work

The feature branch `multiplayer/native-standard-20261016` contains the following
source work. It has compiled and linked in Windows CI; native runtime
qualification remains pending.

- Sparse permanent facts cover tools/bags, camera and bow ranks, six songs,
  wallet/magic/ammo capacities, pearls and shards. Native authored chest, fairy
  and named event hooks correlate presentation, actual effect and completion.
  Unknown producers remain unsupported. Frozen actual quest-init/load/save
  facts can reconcile fixed ownership; observed live ranks cannot earn rewards.
- Protocol 3/schema 2/store 2 add a bounded immutable authored reward ledger.
  Fixed facts merge by maximum; wallet/ammo upgrades advance once per authored
  source. A complete paged Welcome publishes facts and receipts together.
- A separate per-profile client journal flushes completed outgoing operations
  before transport and incoming canonical state before native application.
  Exact completed claims can transfer to journal ownership across quest reload.
- Native save intent is flushed before an actual armed CARD write. A receipt
  requires actual successful native I/O and an actual GCI hash. Interrupted
  intent recovery requires a matching accepted read, successful native result,
  one-time quest-load provenance and unchanged source hash. A proven older
  snapshot can retire only its selected slot's missing intent; ambiguous or
  failed evidence retains it. Other-slot unresolved intents still block saves
  conservatively and identify the slot requiring recovery.
- Offline replay derives a separate provisional target from acknowledged facts
  plus deduplicated durable outgoing operations. It uses safe game-thread
  application after genuine quest activation, without a new award, host ACK or
  outbox removal. The game save gate independently checks that projection.
  A completed local presentation can release its native correlation after an
  exact durable operation and fresh projection are verified; repeated offline
  sources reconcile to that deduplicated target while their outbox operations
  remain unconfirmed. Native safety deferral retains the correlation.
- Native failed-read retries preserve prior authorization without authorizing
  an unloaded quest; old results and old hashes cannot restore it. Initial Host
  and post-load rejoin both use the owned listener's configured bind address.
- Failure diagnostics exercise actual isolated native CARD read/replace
  failures and retry, plus actual owning-host native quest load. They remain
  off by default and have not been run on the expanded source.

Source review and no-output clang-cl semantic checks are useful intermediate
evidence only. At `bea6a4a0`, all 22 changed translation units passed no-output
clang-cl semantic checks; protected build metadata across 8,509 files stayed
identical and no temporary outputs remained. The same source passed all seven
standalone suites on Windows with clang-cl 19.1.5 in
[run 38958492489](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38958492489):
schema, transport, store, profile, progression logic, game policy and client
journal. Actual CTest elapsed time was 15.68 seconds.

Earlier failed runs remain available:
[38956661533](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38956661533)
exposed the malformed-frame fixture's strict pump assertion after an expected
connection reset; [38957726940](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38957726940)
then exposed queued reconnects contaminating a shared fake-server listener.
The corrected fixtures require full send, prompt closure, exact Hello identity
and no publication. Each case owns a listener; all eight peer identities satisfy
the hexadecimal contract. Production rejection requirements were retained.

None of these checks proves native saving, recovery or multiplayer behavior.
Local builds require at least 8 GiB free on C:, which remains unavailable.
The manual CI workflow provides separate standalone-network and source-only
Windows game compile/link scopes. The full Windows job at `80227854` passed in
[run 38957731060](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38957731060),
building the named `switchwaker` target plus `cos_pc_tests` and `cos_sdk_smoke`.
Both headless helpers exited successfully; their documented POSIX fork/wait
death/exit checks were skipped on Windows. The linked executable was 13,897,216
bytes, SHA-256
`f04ae930f5cd4b5eaa568c6dcfdf8beb835057a7be22372758bc158e60c1018c`.
The later `bea6a4a0` changes affect transport fixtures only; the native game code
is identical to this linked source.

That first job retained logs and an output hash, not a downloadable build.
Native runtime, real CARD recovery and expanded two-game acceptance remain
pending. The qualified pilot executable/tester is unchanged.

## Next implementation boundaries

These are source-backed design constraints, not implemented or qualified
features. Their fact/operation formats require an intentional compatibility
change before use.

- **Permanent gear:** sword ranks, shield ranks and bracelets need native collect
  bits plus selected equipment. Defer held-sword, guarding and lifting changes
  until the native models can refresh safely. Preserve recollection/minigame
  guards and the authored first-Forsaken-Fortress stolen-sword sequence. Master
  Sword cutscenes call `execItemGet` directly; catalogue entries alone miss those
  genuine producers. The dropped recovery sword is not a newly earned reward.
- **Health:** once-only authored heart pieces add one quarter unit; containers
  add four, with capacity capped at 80. Observed maximum life cannot identify
  overlapping earned sources. Native item functions also heal and their queued
  effects settle later in the meter. A permanent-only remote effect needs a HUD
  refresh that preserves current health and pending damage/heal queues. Bind
  first-drop and respawn boss hearts to one boss save identity, including the
  sea/Forsaken-Fortress alias. Salvage and NPC rewards need their originating
  authored identity; position and item-box parameters are insufficient.
- **World rewards:** a chest sets its opened flag before award completion, and
  boss defeat differs from heart collection. Persist reward/dependency bundles
  atomically; apply the reward entitlement before its suppressing flag. Local
  cancelled presentations also need reconciliation before saving opened flags.
  Defer live actors/events to safe boundaries. Stage-aware setters must update
  saved data and the active bank; never replace the active stage bank merely to
  update another dungeon.
- **Dungeon keys:** keep unique key-source and unlocked-door ledgers. A new door
  consumes one canonical key atomically; the same door consumes none on replay.
  Two different doors competing for one key need host authorization or reserved
  key tokens. An initial policy can authorize new unlocks online while retaining
  already unlocked doors offline; key sharing remains in scope. Raw key counts
  cannot merge by maximum or bitwise OR.
- **Story:** whitelist named milestones and their prerequisites. Save switches
  include reversible/resettable states, and event bytes include multibit
  registers. Neither persistence nor a set bit alone establishes monotonic
  shared progress.
- **Visible players:** use a render-only capsule with private archive bytes,
  model data/materials and heap; a second player actor would replace local
  singleton state. Final 42-joint body poses fit the bounded transient packet,
  but skinning, equipment/hand/face rendering, interpolation, expiry, scene/room/
  layer filtering and safe render-heap retirement need actual qualification.
  Presence must remain separate from durable rewards and local simulation
  authority.

Protocol/schema/store changes must intentionally reject incompatible pilot
rooms. Keep the previous build playable while preparing a new checkpoint.
Client recovery qualification, weapons/derived equipment and health, the
remaining authored producers, world/dungeon/key policies and reward suppression,
remote players, external-machine LAN and a newly qualified tester remain part
of the active objective. None are removed from scope by these foundations.

## Completion rule

For each row above, inspect the actual final source and the evidence that covers
that row's full scope. Record the final executable identity, inputs, native
outcomes and remaining uncertainty. A green standalone test, compiled target,
connected socket, snapshot, frame count or structurally valid GCI cannot stand
in for a required game transaction or visual result. Until all required rows
are implemented and verified, the full multiplayer objective remains active.

Tingle rescue wait-skip stays disabled. Optional HD visuals and unrelated
enhancement work do not gate multiplayer or a qualified tester checkpoint.
