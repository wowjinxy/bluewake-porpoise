# Native standard multiplayer acceptance

Active objective, recorded 2026-10-16: **keep working until full standard
multiplayer is implemented and verified**. The Telescope pilot is a preserved
baseline, not completion of that objective. This checklist records missing
evidence as missing; implementation, standalone tests and real game acceptance
remain separate columns.

The latest source adds Power Bracelets sharing with progression schema 3.
It rejects schema-2 campaigns without rewriting their files. The retained
schema-2 binaries and their native results below remain separate checkpoints;
they do not qualify the new equipment behavior.

The accepted model is independent exploration with shared permanent progress
over an owned TCP service. Each player keeps local movement, camera, cutscenes
and ordinary consumable counts. Steam and public Anchor services are not used.
Visible players in a shared area are included in the working scope. Presence
must not control another simulation.

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
| Remote player presence | Feature branch has copied-pose transport source; no native rendering or game proof | Bounded transient presence, scene/room filtering, expiry/disconnect, rendering/animation/interpolation and actual two-game visual acceptance; no remote authority over local actions |
| Player-facing multiplayer UI | Pilot Network tab; Telescope-only status and room setup | Broader shared-progress status, canonical pending effects vs transport queues, save/recovery errors, presence and honest compatibility messages |
| Build, regression and tester provenance | Qualified EF84 pilot and 34-entry tester at `8636102f` | Build expanded source with named Windows targets, run relevant standalone and native cases, preserve failures, verify packaged title and archive contents before replacing the tester |

The detailed pilot evidence and known edges remain in [Native LAN pilot](NATIVE_NETWORK.md).
Its executable SHA-256 is
`ef84bbf98064b9d1a237d54e854b71d36be267b912594977e73248fd3ef7b833`.
The previous tester, source commits, original saves and private results are
retained. None of that evidence qualifies newly edited source automatically.

## Current implementation work

The feature branch `multiplayer/native-standard-20261016` contains the following
progression foundation, which has compiled and linked in Windows CI. Native
results and newer uncompiled work are identified separately below.

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
  off by default. Host loading passed; the first CARD retry attempt failed and
  its write-fault fixture/error handling are corrected below, pending rerun.

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
The qualified pilot executable/tester is unchanged. The separately retained
`80489913` build subsequently passed the focused native evidence below; broad
reward and full multiplayer acceptance remain pending.

## Expanded native checkpoint, 2026-10-16

[Windows run 38961066089](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38961066089)
compiled/linked `80489913`, ran both headless helpers and retained the source-only
build. The downloaded archive and all 32 manifest records were verified. Its
executable is 13,897,216 bytes, SHA-256
`faf582496f841dfa40231e5b5f6c9f1dea2551bfc392fd68113650c58ab050ac`.

Two actual native processes in different scenes completed Aryll's genuine gift,
one remote Telescope apply and 12 paused frames of deferral. XYZ selections and
local resources stayed unchanged. Both native quest saves passed SaveSync and
comparison with both GCI copies. Two fresh offline processes then selected the
saved files normally, retained ownership, emitted zero awards/applies and passed
the packed-state comparison. Source and copied GCI hashes stayed unchanged.

The initial save harness exits after actual CARD completion before the next
network-frame receipt flush. Read-only decoding confirmed both donor journals
still had strict slot-0 intents and no receipts. The fresh native loads retired
those intents and produced recovered receipts with matching actual GCI hashes,
unchanged canonical revision 1/fact 1 and zero outgoing operations. This
qualifies the post-CARD/pre-journal-flush recovery window for this reward; it
does not establish all crash windows or ordinary receipt commit behavior.

An earlier deep-path attempt preserved genuine gift/receive evidence but failed
its first CARDCreate with SaveSync 2. No GCI was created. The final path was 238
characters; Aurora's temporary suffix can extend it to 263. The executable
lacked a long-path manifest. The same binary passed in fresh shorter paths;
the exact native OS error was not logged, so the path-limit cause remains an
inference. That failed run and both drained child records are retained.

A Windows-only embedded long-path manifest and a real unprefixed Win32
create/flush/replace/reopen helper passed the full Windows build in
[run 38964914726](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38964914726)
at `d9025e95`. The helper actually passed with a 366-character Unicode target
and a 391-character temporary path. The downloaded source-only game executable
is 13,936,640 bytes, SHA-256
`48a05704e37f66cc0873870686612b2cd22d188158fb38a4684e899cff4b278a`;
all 32 retained manifest records were verified. An isolated personal-card native
save subsequently passed actual SaveSync and both-copy comparison. A passive
directory observer recorded nine native atomic temporary paths of 275–277
characters. A fresh process loaded a byte-identical private card through the
title/file select and passed native region comparison and return-place play.
The 98,368-byte donor and clone both retained SHA-256
`6841b45b4b1da46e1a0cd4cffa3f795c95c31f21d2d1524de996d32fff17cf45`;
the source ISO was unchanged and both owned process jobs drained. The final
GCI path was 252 characters, so this proves the longer temporary write path,
not a read beyond 260 characters. It does not qualify network rewards. The
helper explicitly reports a skip when Windows' existing long-path policy is
off; no system policy is changed.

The owned-host case also passed actual file selection and native quest loading
with bind `127.0.0.2` distinct from public endpoint `127.0.0.1`. It reconnected
to the same owned room, retained its unchanged GCI and emitted no new award or
remote effect.

The first controlled CARD retry attempt restored authorization after a real
LoadSync 2/read denial and successful retry. Its write probe then blocked SDL's
`r+b` readiness open rather than atomic replacement: a retained read handle is
incompatible with SDL's exclusive writer. Aurora returned NOFILE; the native
non-PAL controller did not map that terminal result and kept SaveSync busy.
The write probe now uses a zero-access pin without delete sharing, verifies its
exact file through a separate reader, then closes that reader before native
writing. Three affected translation units passed no-output compiler checks;
actual retry acceptance remains pending. An intermediate PC NOFILE-to-missing
mapping at `aeec07ec` was found unsafe during source review: an inaccessible
existing file could route the save screen toward creating three blank slots.
That source was kept off the qualified pilot and has not run with player saves.
The correction reports readiness failures for registered backing files as
I/O errors, keeps PC operation-level NOFILE out of the missing-file state,
and preserves classified errors after failed LoadSync. Genuine empty-card
attachment still enables normal new-file creation. A refused prepared save
has a separate transient result: SaveSync reports refusal and restores ready
status without queuing CARD I/O or offering file creation. Its distinct
pending-progress event is retained. Actual blocked-save menu retry remains
unqualified.

The diagnostic now requires actual terminal I/O-error status before consuming
failed read/write results. After a failed write it performs a real Load/LoadSync
into a separate buffer, verifies the active unsaved quest, selectors/resources
and original GCI are unchanged, then repacks the current quest for a fresh
armed STORE. It never unpacks the older controller-read buffer into the quest.
An owned scratch test using the retained SDL library confirmed that the
zero-access pin permits `r+b` readiness but denies replacement; releasing it
permits the same SDL rename and correct readback. This is fixture evidence;
the repaired native retry remains pending.

The final PC controller translation unit and both changed diagnostic translation
units passed scoped no-output clang-cl checks. Protected build metadata across
8,509 files stayed unchanged. The Aurora classification patch applies to the
pinned source, but its compilation and native error-path acceptance still
require the next full build and isolated retry run.

The unfaulted sender in that attempt separately failed while writing its second
blank-save copy. Its short path rules out the earlier deep-path explanation;
its exact backend error was not logged. A diagnostic-only Aurora patch now
records failure phase, result, offset/length and copied last SDL error. It does
not change I/O, callbacks or retries. The sender's cause remains unresolved.

The corrected source at `36f17db9` compiled and linked in
[Windows run 38967772328](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38967772328),
including both SDK helpers and the actual long-path helper. All 32 downloaded
manifest records matched. Its 13,947,904-byte executable SHA-256 is
`f8d1f689131d7b0abec57a60399aee6542e1f971b6f2f49bc1d2b7ec62d205fa`.
The fresh CARD case preserved the genuine remote award and native blank-file
creation, then failed its strict terminal-status check after a real read denial.
Aurora logged IOERROR -5 at offset 8192/length 8192, establishing the corrected
backend classification. Native `restore()` had discarded that returned error
and passed the earlier successful CARDOpen result to `setCardState` instead.
The PC source now propagates the actual restore result; the strict check is
unchanged and reports the observed status on failure. The corrected controller
and both diagnostics passed scoped no-output clang-cl checks; the final fairy
diagnostic revision passed separately afterward. All 2,523 input pins matched,
8,509 protected build files stayed unchanged and temporary outputs were empty.
That checkpoint still needed linking and another native retry; the later `7d`
results below provide those outcomes. The sender was drained when the receiver
failed, before its save, so this failed run does not explain the earlier
unfaulted second-copy write failure. Both negative process records and the
unchanged source ISO are retained.

## Dedicated progressive-reward native case

The off-by-default `network-fairy` harness now provides two genuine wallet-fairy
transactions in `Fairy01`. The receiver first obtains wallet tier 1 remotely,
then claims its own same authored fairy, whose native choice is the tier-2 item.
Acceptance requires one authored receipt, unchanged room revision, final tier 1,
real event/presentation completion, actual native quest saves with both GCI
copies compared, and fresh offline file-select loads of both players. Private
peer markers coordinate timing; each process must independently qualify.

Select `COS_SMOKE=network-fairy`, `COS_NETWORK_FAIRY_CASE=gift|duplicate`,
`COS_BOOT_STAGE=Fairy01:0:0:-1` and `COS_TARGET_FPS=30`; omit other boot grants
and network diagnostics. Both profiles and all writable paths must be under an
explicit `COS_NETWORK_FAIRY_PRIVATE_ROOT` with opt-in marker
`network-fairy.allow` containing exactly `BlueWake.Native.Fairy.Private.v1\n`.
For offline verification use `COS_SMOKE=save-load` with
`COS_NETWORK_FAIRY_CASE=observe` and no boot stage.

All four affected translation units passed actual no-output clang-cl checks on
2026-10-16. Protected build metadata and 2,522 source/header pins stayed
unchanged. The full Windows build including this harness passed in
[run 38962853385](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38962853385)
at `9fefbc9e`; the downloaded executable is 13,935,616 bytes, SHA-256
`ad823408e8c1c600a2eef93793c55f5d16b59e3e1155605e03734c61b2870838`.
Its first native fairy attempt exited at the private-path guard before
reward gameplay: equivalent Windows directory spellings were compared as raw
strings. Separator and trailing-directory normalization are now corrected,
with mismatched spellings logged, and passed the compiler check above. The
retry remains pending. The fresh wallet fixture has zero rupees and does not
qualify over-capacity resource corrections or ammo fairies.

The next fairy run, using the `36f17db9` executable above, passed the corrected
private-path guard and naturally entered `BIGELF_ARRIVAL`. It did not qualify a
reward: the harness expected a retained Pt2 player pointer, but the actor orders
the event with `NOPARTNER`, and native event acceptance deliberately clears Pt2
after commanding both actors. The harness consequently remained in its approach
phase until that phase's bound expired. The source correction requires the
actual fairy in Pt1/PtT, null Pt2, both native demo commands, the exact event name
and the unchanged player/quest identity. Existing approach/event bounds and all
item-effect/completion/ledger/save requirements remain unchanged. That correction
and its improved event observations passed the compiler checks above and need
a full build and fresh native run;
the failed host and receiver evidence is preserved.

The corrected source at `7d418a51` compiled and linked successfully in
[Windows run 38971340449](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38971340449),
including the SDK and long-path helpers. Fresh isolated native tests of that
exact executable now pass the CARD-retry and fairy transactions. CARD read and
replacement denial each produced real status 8 and failed sync result 2 without
changing the original GCI. A genuine controller read restored authorization;
the retry committed both native save copies without a duplicate application.
The receiver's ordinary durable receipt hashes the final GCI. The sender's
immediate diagnostic exit retained a post-CARD intent. A fresh private-copy
file-select load then recovered that exact intent, retained the original GCI
and room ledger, and added no reward. The receiver's ordinary receipt and GCI
also stayed unchanged during its fresh load. Sender recovery is a recovered
commit, separately classified from ordinary receipt publication.

Both fairy participants completed the real event. The receiver first gained
wallet tier 1 remotely, then the same fairy's actual tier-2 item was reconciled
back to tier 1. The external ledger remained revision 1 with exactly one
authored reward. Both native saves and ordinary durable receipts passed. Fresh
offline native file-select loads then retained tier 1 for both players without
changing their GCIs. These outcomes do not cover
over-capacity resources or ammo fairies. Earlier negative evidence and the
playable pilot remain preserved.

## Ordinary Save-menu recovery source

The PC Save menu now spends one explicit Yes confirmation on a real controller
read when an existing card has an I/O failure. Busy does not consume that
confirmation. Another failed read retains its classified error and returns to
acknowledgement/back; it cannot fall through to creating or formatting a file.
A successful read preserves all three buffered slots, then repacks the current
unsaved quest. The recovery read does not activate its older quest snapshot.

Each accepted Save call has a process-local admission ticket. The menu can
therefore distinguish that exact request's pre-CARD progression refusal from a
real write failure. Refusal retains the previous controller classification and
uses the ordinary cannot-save/back flow; it queues no CARD write.

Temporary recollection inventory is exchanged only when the native operation
has a real nonzero buffer stage. Success, failure, cancellation, reinitialization
and menu deletion consume at most one rollback token. It binds the initialized
native buffer lifetime and copied quest identity. FPC removes child actors from
search before their Delete callbacks, so rollback deliberately does not require
a still-executing player or refresh teardown player pointers. Native buffer
setup/reversal invalidates its generation; ordinary save exchange does not.

The final menu/controller, buffer-lifetime and diagnostic translation units
passed no-output clang-cl checks on 2026-10-16. Protected build metadata and
2,535 source/header pins stayed unchanged. The complete `494407af` candidate
compiled and linked in
[Windows run 38977449016](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38977449016).
Genuine ordinary PAD navigation, failure/retry and
nonzero recollection teardown/cancellation acceptance remain pending. Console
paths are preserved under the TARGET_PC guards.

The off-by-default `network-save-ui` diagnostic drives native PAD navigation
through the ordinary collection/Save screens. Copied observations expose the
UI's own read/write results without consuming them again. The case requires
a genuine incoming Telescope, paused deferral, exact-ticket refusal, real OS
read/replacement denial, explicit recovery reads, a native commit and both GCI
copies. It checks an existing-card identity before attempting the save. Its
private fixture needs a persistent owned service and actual native-created
baseline; those marker files cannot establish native provenance by themselves.
The first other-slot checks cover blank slots, not populated three-slot saves.
The first native attempt created its genuine baseline and passed existing-GCI
admission, then stopped before menu navigation because the persistent service
had no pre-created room. Join correctly refused that missing room. The fixture
needs explicit empty-room creation; complete native UI qualification is pending.

## Transient player-pose transport

The feature branch adds a copied 42-joint pose channel with explicit finite and
range checks. The server assigns each joined player's origin and connection
epoch. Per-origin sequences reject old samples; roster changes, local context
changes, hidden poses, disconnect and one-second expiry remove visible rows.
The receiving API filters by stage, room and layer. No game pointer or movement
authority enters the transport.

A newly ready peer receives a still-fresh existing sample even when its source
has not published again. The replay retains the original sequence/connection
epoch and obeys the same relay period. A loopback regression covers this
late-join boundary.

Each worker publishes at most ten poses per second. Bounded replace-latest
slots and one partial TCP frame remain separate from reliable progression
queues, acknowledgements, room revisions, journals and storage flushes. Initial
Welcome, reward-ledger pages and roster must drain before transient delivery.

Wire protocol 4 deliberately rejects older sockets. Durable storage protocol 3,
schema 2, semantic/rules digests, profile format and isolated-card route hashing
remain unchanged. Literal independently serialized v3 room/journal/profile
fixtures cover this compatibility boundary in the eighth standalone
suite. Its remaining cases exercise malformed/spoofed samples, expiry,
reconnection, real socket backpressure and reliable reward traffic during pose
publication. All eight standalone Windows suites passed at `a05b21ae` in
[run 38973915072](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38973915072):
18.04 seconds total, including 5.21 seconds for presence. The late-join case and
literal v3 compatibility fixtures ran in that successful suite. This is
copied-value/socket/storage evidence, without a native model or game scene.

An off-by-default `remote-body` diagnostic now has source for one local-offset
body using owned archive/model/material/texture data and copied final native
matrices. It creates no second player actor. Submission occurs before the P1
draw lists and restores CPU graphics state. One 4 MiB model heap is retained;
archive decoding temporarily requests another 12 MiB from Zelda's small pool.
Actual allocation success, visual/material isolation and GPU lifetime remain
unverified. The prototype omits eye/brow multipasses, hands, equipment, shadows
and clothes variants, and has no interpolation yet.

The separate off-by-default `network-body` case now has an actual-room adapter.
It publishes copied final native poses through the authorized mounted room and
selects one authenticated same-area peer. Connection/quest generations, roster
epochs, sequence, age and scene changes control visibility and reset history.
It applies no local diagnostic offset or second world transform. A one-shot
Zelda-heap preflight guards the existing 4 MiB graph plus 12 MiB decode scratch.
The shared renderer initializer also checks actual Zelda free/largest sizes
before either diagnostic allocates, with a sticky refusal and unchanged parent
allocator flags. This admission check does not reserve against concurrent
allocations or qualify internal loader OOM paths. Seven graphs still require
a measured dedicated pool. Independent copied-value
policy tests are wired into both CI scopes. All five affected native translation
units passed no-output clang-cl checks, with 2,539 source/header pins and all
protected build metadata unchanged. Its two independent asset/policy tests and
all eight network suites passed at `1718517f` in
[run 38978719157](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38978719157).
The shared initializer's additional heap gate also passed a renderer-only
compiler check. The full Windows build at `a1e51826` passed in
[run 38979528898](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38979528898),
including the named game target and headless helpers. Its independent asset
and body-policy checks passed 390 and 41 checks respectively. Real networked
rendering and complete remote models remain pending.

Independent asset-bound tests passed in
[network run 38975421604](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38975421604)
alongside all eight network suites. They run without disc/SDK/renderer
dependencies. The first full Windows job failed on the new renderer's resource
header include. Scoped compiler checks then found an unavailable SDK accessor;
the renderer now uses the declared GD pointer and the correct resource include.
The corrected local-body diagnostic compiled and linked in the `494407af`
Windows run above. Native rendering still needs qualification. Explicit
optional retention of a separate owned-host helper
includes its source/run manifest and licenses, without changing the game
artifact or implying game qualification. Actual same-area two-game acceptance
and complete remote models remain required.

Source review subsequently found that the prototype's extra
`settingTevStruct` call could change shared palette/player-light state, a palette
table entry and gameplay RNG. Its stack lighting object could also leave a retained
material light pointer. The corrected source captures the receiver's original
current-draw lighting by value before local damage-fog effects, retains that
copy at a stable private address, and requires the same frame and scene/player
context before submission. A valid native lighting tag prevents the setter's
initialization fallback. This first slice uses receiver same-area lighting;
position-specific remote lighting remains future work. Console behavior is
unchanged and the hook returns immediately with diagnostics disabled. The
correction still needs a full build and actual native visual qualification;
the earlier `a1e51826` visual diagnostic was held after this finding.

## Power Bracelets source checkpoint

The native catalogue now includes item `0x28` as permanent MAX fact 24.
Collection ownership comes from the native collect bit. Remote application
uses `item_func_pwr_groove`, which sets that bit and selected bracelet equipment
without modifying local health, money, ammunition or X/Y/Z choices. Ownership
alone is insufficient for settlement: the incoming loop repairs a missing
selected-equipment effect at a safe boundary. Grab mode, carry actions, grab
animation and a retained grab-actor ID all defer that repair. Existing
recollection, temporary-inventory and event/menu guards still apply.

A prepared save also requires the bracelet effect to have settled in the same
quest and epoch before CARD I/O begins. The new fact changes the permanent
contract, so schema 3 and new semantic/rules digests intentionally reject old
profiles, room stores and client journals. Wire protocol 4 and storage framing
3 remain unchanged; previous files are preserved. The literal prior-schema
fixtures remain byte-identical and now exercise refusal without rewriting.

All eight standalone Windows suites passed at `2190269c` in
[run 38982333726](https://github.com/wowjinxy/bluewake-porpoise/actions/runs/38982333726)
in 18.05 seconds, along with both independent asset/body-policy tests. This
covers schema refusal, transport deduplication, store/journal replay and
copied settlement rules. A full game build is still pending.
Genuine authored collection, receipt/replay, lifting heavy objects, wrist
display and save/reload still require native qualification. Sword and shield
facts 22/23 remain reserved.

## Next implementation boundaries

These are source-backed design constraints for the remaining work. Additional
fact/operation formats require an intentional compatibility change before use.

- **Permanent gear:** sword and shield ranks need native collect bits plus
  selected equipment; bracelets have the unqualified implementation above.
  Defer held-sword and guarding changes until native models can refresh safely.
  Preserve recollection/minigame
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
