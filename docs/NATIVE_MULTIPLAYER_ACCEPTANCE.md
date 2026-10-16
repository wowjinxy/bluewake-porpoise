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
source work. It has not produced a new executable or runtime qualification.

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
evidence only. An earlier 2026-10-16 snapshot passed 22 translation-unit semantic
checks. Remote Windows clang-cl 19.1.5 then built the seven standalone suites at
`899f0529`; schema, store, profile, progression logic, game policy and client
journal suites passed. The transport suite failed its malformed-frame fixture's
strict pump assertion after an expected connection reset. That failure is
retained in [run 38956661533](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38956661533).
The fixture correction still requires a new run; it retains full-send,
prompt-closure and no-publication requirements.

None of these checks proves native saving, recovery or multiplayer behavior.
Local builds require at least 8 GiB free on C:, which remains unavailable.
The manual CI workflow now provides separate standalone-network and source-only
Windows game compile/link scopes. Full integration compile/link and actual game
acceptance remain pending. The qualified pilot executable/tester is unchanged.

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
