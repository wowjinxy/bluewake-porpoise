# Performance improvements to test

Updated **October 7, 2026, 1:31 a.m. CDT**. Implementation started from
`7bb4aec`; runtime remains `e280c788` with active patches **0152–0163**.
Checking a box means locally qualified for its stated benefit and recorded,
not merely downloaded. Gameplay FPS claims require separate timing evidence.
Donor gains are evidence to investigate, not promised gains on this machine.

October 9 benchmark correction: inspect the **actual constructed child
environment** before timing or profile training. The deep-debug families
accidentally enabled direct-call, game-event and autosave tracing; their retained
results are traced diagnostic evidence. For clean runs, direct/event trace values
must be `0`, and `BLUEWAKE_AUTOSAVE_TRACE` must be absent because its frozen
getter tests presence. Remove inherited trace/watch variables by exact tokens,
preserving `MAX_RETRACES` and intended performance logging. Runtime admission
needs source or hit evidence; do not infer it from a loader message or assume
these runtime trace flags disable every fast path. See
[the corrected investigation](DEEP_DEBUG_2026-10-08.md).

October 10: the wider private calling-convention candidate replaces twenty
translated chunks and the dispatcher while preserving the accepted C2 PSQ
ancestry. Actual selector tests, code/unwind checks and six complete intro
checkpoints/full pixels pass. The eight-run comparison establishes no gain:
paired CPU +0.88%, elapsed -0.007%, both 95% intervals spanning zero. It remains
inactive and closed to unchanged retries; no tester replacement follows. See
the October 10 section of [the investigation](DEEP_DEBUG_2026-10-08.md) for all
four pairs, tradeoffs, private receipt hashes and the preserved harness failure.

The next diagnostic uses separate instruction-cache, data-cache and branch
hardware counters. Their three memory profiles pass the installed WPR parser,
but the agent's Windows token lacks system-profiling privilege. The user's
administrator run recorded and stopped successfully, then failed before the
game launch because of a stale path validator. No new FPS gain is established.
Read-only query results, the unsupported
Windows 11 command and the corrected status-code assumption are preserved in
the hardware-counter section of [the investigation](DEEP_DEBUG_2026-10-08.md).
This diagnostic does not change the tester or gate unrelated port milestones.

The offline reader now builds, passes 28 pure checks and decodes both installed
sample and interval schemas against deliberate test bytes. The first actual
ETL export reached real PMC events but stopped at its 64 MiB output cap; its
partial files are preserved. The corrected capture launcher passes 59 pure
checks and independent source review. A separate reader with a 2 GiB output
cap builds and passes 34 pure checks, with its other parsing/time limits intact.
It exports all 393,155 events from the failed recording; missing CollectionStart
metadata still prevents interval qualification. A separate two-second no-game
File-mode probe is prepared, parser-validated and independently source-reviewed;
its administrator recording remains unrun. A complete game recording and metadata,
loss and ownership checks remain required before interpreting counters. Source
and exact build/audit receipts are preserved in the investigation.

## Implementation progress

All 38 numbered candidates remain in scope. Implement dependency groups in
isolated builds; preserve negative results and enable improvements only after
their applicable qualification. Tingle rescue wait-skip stays disabled.

The latest crowded-view investigation identifies game-thread CPU saturation
in the actual play-session log. An additional host-only pending-return filter
passed the existing event suite and 5,646,341 old-predicate comparisons each
under O3/ASan. Four serialized original/filter/filter/original Dragon Roost
runs reduced mean route time 10.89% and process CPU 9.20%; both pairs improved.
The separate feature-address experiment regressed and was restored. See
[busy-scene results](PERFORMANCE_BUSY_SCENES.md) for the exact settings,
receipts and limits. This does not establish full-speed displayed gameplay.

| Item | Current result | Remaining qualification |
| --- | --- | --- |
| 01 | Repaired late-array fallback passed 1,552 O3/ASan checks each. Repaired combined production passes the three-scale lighting/EFB GPU oracle. Fusion-only and all-with-immediates Outset captures match; combined Dragon Roost also matches, with rejected=0 and failed=0. | Keep disabled: serialized fusion-only ABBA/BAAB increased mean process CPU 17.68% and mean route time 2.50%; all four adjacent CPU comparisons regressed. Original failures remain; fewer submissions are not a speedup. |
| 02 | Opt-in 60-byte vertices passed 49,878 field/layout checks and 108 interpolation-capture checks in each of O3/ASan. Actual GPU checks at three scales and native Outset/Dragon Roost captures match the controls exactly. | More scene/interpolation and timing qualification. Broad interpolation ASan first-blend failure also occurs in canonical 11 + 01; cause unresolved. |
| 03 | Exact opt-in decoder passed 399,918 O3/ASan checks, full production and three-scale GPU checks. Decoder-only and combined native Outset/Dragon Roost captures match exactly. | Individual decode/route timing and more scenes. First combined package showed no repeatable throughput gain; default remains interpreted. |
| 04 | Completed-image upload-shadow candidate passed O3/ASan: 69,063 checks and 35,760 complete-buffer comparisons each. Serial identical frames reduced uploads by 99.17%, but continuous two-frame overlap saved no bytes because reservations declined. | Retain inactive. Find a useful real nonoverlap workload before paying for GPU/native integration. |
| 05 | V4 repairs the native-discovered TEV binding bug. O3/ASan each passed 16,225,362 checks; full production and three-scale actual lighting/EFB checks pass. Sparse-only and repaired combined native Outset/Dragon Roost captures match. | More interpolation and individual timing. First combined package showed no repeatable throughput gain; original negatives retained. |
| 06 | Repaired immediates passed 1,306,999 O3/ASan checks each, full production and three-scale actual GPU checks with active=1. Native Outset and Dragon Roost match, including repaired fusion; read-only caches confirm 181/218 active immediate GX configurations in the initial routes. | Interpolation, unsupported-device/reset and individual timing. Default and unsupported hardware retain dynamic uniform offsets. Hidden tests do not qualify presentation. |
| 07 | Enabled as active patch 0163. O3/ASan each passed 420,637 checks. Native Outset, Dragon Roost, Fortress sea and Hyrule captures match. Dragon Roost control/candidate/candidate/control repeated CI uploads 3,276 → 68 and total uploads 3,483 → 275 with exact images. | Combined renderer checks and serialized frame-time measurements. Forest Haven qualifies arrival-dialogue images only; no gameplay FPS or complete guest-RAM parity claim. |
| 08 | Bounded command reuse passed 11,239,116 O3/ASan checks each; authored allocations fell 16,000 to 8. Full production, three-scale GPU and native Outset/Dragon Roost combined captures match. | Real allocation counts and individual scene timing. First combined package showed no repeatable throughput gain; no total-heap/FPS claim. |
| 09 | Guarded opaque-copy path passed 4,345 O3/ASan checks each and full production compile/link. Actual hidden-surface shader/copy pixels match for full viewport, ImGui and 800x600 after ordinary overlay warmup. The corrected five-case fixture compiles and links with 93 output-path guard checks. | Full five-case qualification incomplete: corrected letterbox/recreation run and one fresh resumed attempt fail at D3D12 swap-chain creation (0x80070005). Preserve partial evidence and failures. Keep inactive; no display/timing claim or tester prerequisite. |
| 10–11 | Opt-in transforms passed 5,460,525 full CPU/budget/deadline comparisons each O3/ASan and 64 public parser checks. Normal builder now offers default-off dispatch-slots/return-ranges with preparation, cache/training and provenance; four integration tests pass. | Full module and matching-profile native A/B. Existing computed-goto entry remains. Actual irregular-map prefix comparisons fall 11 to 2 with slots or 3 combined; ranges alone and regular-stride maps do not improve. |
| 12 | `--f32-hw-widen` is available, off by default, with required gather/inline-FP checks and preparation/training/provenance identities. All 2³² float patterns passed in each of four rounding modes; special-value, FP-environment and ASan checks passed. Public-header machine code matches the exhaustive test. | Complete translated-module/native gameplay comparison and load-heavy timing before enabling by default. |
| 13 | Inactive known-single facts/helper passed the original 5,990,400 O3/ASan checks each. Six additional SUM/paired-RSQRTE programs expand the proof to 6,543,360 checks / 1,308,672 full-CPU cases per mode, including callbacks and interruption. No false producer fact found. | Complete opcode/alias corpus, module/gameplay and timing. Object grows; no benefit established. Original host-denormal negative and raw object-MD overwrite limitation remain recorded. |
| 14 | Narrow certified-helper lookup reuse passed 80,042 complete CPU/4 MiB RAM comparisons and 1,693 declines in each of O3/ASan. Each warmed translated-reference run passed 60,000 cases, including 38,747 accepted hits and 21,253 unchanged declines. | Build integration and native actor-heavy timing. The outer translated walk and per-call watch/observer predicates remain; current watched actor-ID policy still declines. |
| 15 | Three-entry environment-color replay passed 180,000 CPU/protected-RAM cases each O3/ASan. A separate repair bounds signed counters before replay; same 180,000 cases plus three finite controls and 24 unchanged extreme refusals pass. | Inactive. Module/admission, gameplay and timing; existing versioned native-entry gate required. Accepted-path host sticky/x87 equivalence is not claimed. |
| 16 | Two named J3D entries already exist. Separate inactive draft adds three missing leaves and passed 180,000 full CPU/protected-RAM comparisons each O3/ASan, plus midpoint, current-boundary, extreme-budget and unmasked-host controls. | Merge optional integration without duplicating existing entries, then module/admission, gameplay and benefit. Fresh versioned/direct-boundary checks remain. |
| 17 | Two inactive collision leaves passed 120,000 full CPU/protected-RAM comparisons each O3/ASan, exact positive controls and extreme-counter/unmasked-host refusals. | Optional integration, module/admission, gameplay and timing. Accepted-path host sticky/x87 equivalence is not claimed. |
| 18 | Inactive repaired 35-function GX draft passed 60,000 cases per function each O3/ASan: 2,100,000 cases and 1,317,695 exact acceptances per mode, at least 30,263 acceptances per function, zero mismatches. Fresh continuation verifies completed prefixes and finishes all pending ASan jobs; 80 cumulative roles. Patch/tests/memo exported. | Native module/admission, gameplay, GPU and benefit remain. October 9 refresh: the accepted C2 module still statically watches 80328F40, so the unchanged GX fog adapter declines there; the public host no longer permanently refuses that address. The GX provider/router is absent from C2, and integration also requires its qualified FIFO writer setup. Preserve original timeout, zero-admission, paused and fixture negatives; no accepted host sticky/x87 equivalence claimed. |
| 19 | Inactive finite-fmod candidate passed 24,000 cases each O3/ASan: 17,531 exact acceptances, 6,469 unchanged declines, subnormals, signed zero, four rounding modes and DAZ/FTZ. Final raw-stage suite passes seven tests and exact patch/tests/memo are exported. | Integration, native gameplay and benefit remain. Angle/RNG and other math leaves remain translated; no host-libm approximation. |
| 20 | Default-off native-bg-minmax is integrated with preparation, CMake certificate admission, cache/training and provenance. Original O3/ASan each passed 32,000 CPU/RAM cases; focused 20-22 signed-counter proof also passes. | MakeBlckBnd remains translated. Full module/gameplay and timing; original fallback retained. Default optimized machine code is unchanged. |
| 21 | Default-off native-quaternion is integrated. O3/ASan each passed 240,000 CPU/RAM cases, every signed angle on all axes and all 64 shifts. Fresh predicate and exact mutable table reads remain mandatory. | Full module/gameplay and timing. Default optimized machine code is unchanged; no host sine/cosine approximation. |
| 22 | Default-off native-game-atan is integrated using the exact guest table and existing host handshake. Live-source O3/ASan each passed 64,000 full-state cases and eight certification tests. Ten builder tests and twelve actual CMake configurations passed. | Module/gameplay and timing. Focused 20-22 proof passed 3,300 signed-counter combinations per mode; no guard repair needed. |
| 23 | Default-off native-plane wiring passes eleven builder tests/all sixteen selections and twenty-one CMake configurations. Exact C passed 40,000 full-state cases and 402,168 original-module boundary comparisons per mode. Real builder preparation exposed a later-stage main-fragment certificate; its digest is now corrected for original generated C. | October 9 refresh: the earlier permanent-host-F40 explanation is stale. Accepted C2 has no plane hook/define and retains its static watch barriers. Fresh preparation, compiled integration and complete current admission remain unqualified; keep disabled and preserve the initial preparation failure. Existing oracle proof does not qualify actual game admission; no watch bypass, speedup or tester prerequisite. |
| 24 | Inactive oscillator calc passed 36,000 cases each O3/ASan: 23,584 exact acceptances, 12,416 unchanged declines and 144,546 ordered real call/return boundary comparisons. Eight resumed raw-stage tests pass; patch/tests/memo exported. | Earlier save/restore-watch refusal evidence is preserved; the October 9 helper refresh does not requalify oscillator admission. Keep translated in the tester; integration, other envelope/mixer leaves and native audio/gameplay remain. Audio correctness and performance are separate. |
| 25-26 | CPU geometry-cache negative and required GPU ownership/write-footprint model recorded in PERFORMANCE_COMPATIBILITY.md. | Implement complete identity/lifetime mechanism before GPU retention or transform batching; sampled pointers/bytes are insufficient. |
| 27 | Native descriptor-setter donor targets a different frontend from retail FIFO consumption; Swiss maps already present. | Establish a costly corresponding call path before adaptation. |
| 28 / 30 | Narrow current-C projection passed 264,800 O3/ASan checks each; authored 16-ADDI block reduces CPU-r3 memory operands 19 to 2 while increasing object size. Offline DolIR effect model reviewed. | General callback/memory/FP/mod/observer closure, generation integration and timing. No general residency/scheduler claim. |
| 31-34 | Frozen backend/stereo/presentation/worker sources reviewed; compatibility findings and concrete requirements recorded. | Separate ABI migration, stereo scope, actual presentation acquire/submit tests, and corresponding completion/layout bottlenecks respectively. No mono/display gain established. |
| 29 | Callback-preserving dispatcher passed 32,000 full-state cases each O3/ASan, removing 29,536 runtime trampoline calls. Builder now offers default-off inline-cache-callbacks with actual runtime/gather contract hashes and preparation/cache/training/provenance identities. Fourteen preparer and four builder tests pass. | Full module/native qualification remains. Normal guarded-continuation preparation still refuses the unrecognized source template; the tester leaves this option off. FIFO drain, fresh callback and NULL exception behavior retained. |
| 35 | Full SHA-256 profile snapshots and compiler/tool readability implemented. Real Clang/Ninja rebuild/fallback checks pass. Latest shared builder regression passed 28 prepared-cache and 14 training tests; other optimizer changes remain unstaged. | Complete for build correctness; no gameplay FPS claim. |
| 37 | `--module-thinlto` is available, off by default. Seven integrated builder tests, fourteen training tests and real CMake option checks pass. Actual Clang/Ninja OFF/ON/repeat-ON/OFF builds retained nine exports and passed 400,000 cross-TU comparisons. | Full translated module, matching training/profile and native gameplay comparison. Mini DLL size is no game performance prediction. |

The first serialized 02/03/05/08 package comparison showed essentially unchanged
mean route time (75.586 to 75.582 seconds), with inconsistent adjacent pairs.
Complete-process mean CPU fell 1.34%, dominated by the final candidate run;
median CPU fell 0.57%. No default promotion follows. See
[the timing report](PERFORMANCE_NATIVE_TIMING.md) for settings, exclusions,
metrics and limits. The separate repaired-fusion comparison increased mean
route time from 74.899 to 76.769 seconds and process CPU from 110.676 to 130.238
seconds. All four adjacent CPU comparisons regressed. Fusion remains disabled.

Build-isolation diagnostic: an earlier root link recipe rebased wrapped output
flags but missed bare `-Xlinker /pdb:`. Two repaired links targeted the older
combined attempt4 debug-symbol path, overwriting that PDB. The original PDB
bytes are lost; the older executable, objects and link reproduction archive
survive. New repaired-fusion links guard bare and wrapped PDB/import/reproduction
paths and pin prior outputs. This qualifies earlier preservation statements;
original failure receipts remain intact.

Item 09 has a separate preservation exception: a diagnostic link overwrote its
older private `fixture-repro.tar` after a case-sensitive prefix comparison
missed a wrapped output flag. Historical tar bytes are lost; its old digest,
replacement digest and failure receipts remain. EXEs, PDBs, objects, SDK and
tester outputs were preserved. Subsequent links use the tested explicit output
guard. Details and partial GPU evidence are recorded in
`patches/recompcore/drafts/direct-presentation-copy-surface-tests.md`.

Items 36/38 remain profile-quality work: app counts must be retrained against
the selected stable runtime/compiler/source, and the existing game-PGO path
needs matching coverage, held-out routes and a no-tiered comparison. Build-cache
correctness in 35 does not complete either item.

Full mod-enabled source preparation exposed a newline-only item-29 contract
failure before any cache-site writes. The certifier now hashes exact known CPU
and gather text after CRLF-to-LF conversion; every other byte remains certified.
Fourteen preparer tests pass, including all four runtime/header newline pairs,
changed-C refusals and lone-CR refusals. The failed preparation and its log are
preserved under `build/performance-module-candidate-20261006-v4-mods/`.

The next full preparation reaches the source checks and refuses item 29's
unrecognized guarded continuation. At PC 80003388, the direct-call preparer
adds a next-PC/readiness guard before the original return; the cache-site
preparer only certifies an immediate return. It stops while collecting
proposals, before publishing any helper or chunk writes. That failure is
preserved under `build/performance-module-candidate-20261006-v5-mods/`.
The tester leaves item 29 off and retains the existing FIFO drain, mutable
callback and guarded continuation. Certifying this exact additional template
remains future work; standalone helper proof does not complete full preparation.

The full item-12 build exposed generated-load interposition in five native
arithmetic translation units. They now include a native-only FP wrapper that
temporarily hides and then restores the target-wide load option. The shared
`inline_fp.h`, gather header and translated include requirements are unchanged.
Five focused regression tests perform 23 actual Clang compilations; the five
native objects match the previous disabled implementation with the option off
and on, excluding only COFF timestamps. Fifteen existing source-certification
tests pass. This fixes build integration, not full-game qualification.

The isolated source snapshot also omitted the already committed Windows
`bw_getenv_cache.h`; its CMake path was correct. The failed full build reached
838 tasks under Ninja's keep-going mode but never linked: thirteen runtime
objects failed. The original log and completed objects are preserved under
`build/performance-module-candidate-20261006-v6-mods/`. A same-attempt continuation
must verify unchanged object dependencies, flags, sources and tools before
retaining the freshly compiled translated chunks and rebuilding the failed
runtime objects. No earlier build's objects or PGO profile may be substituted.

The same-attempt continuation is complete: all 823 successful objects, including
813 translated chunks, retained their verified bytes; thirteen runtime objects
were rebuilt and the module linked. The selected tester enables 10/11/12/20/21/22
and keeps 23/29 and Tingle rescue wait-skip off. It uses the standard O2
configuration with the three existing O1 fallbacks, no PGO and no ThinLTO.

The exact new host/module pair passed hidden copied-save Outset and Dragon Roost
runs through 3,300 retraces. Complete captured PPM bytes and loaded checkpoint
contexts match their controls. The already-running 16:9/BetterWW check also
passed. Additional automated runs were deferred when the user requested the
play-test handoff; this does not complete all 38 candidates or establish FPS.
The final 46-file tester passed hashes, ZIP CRC, DLL closure and the real upstream
content gate. Version: `20261007-performance-6275863`; ZIP SHA-256:
`e30382b919b9d0cae865f7d1700f56e6d8dd307292896135faf2963de5060c87`.
The package and actual gate receipt are under
`build/windows-tester-20261006-performance/`; gate result SHA-256:
`b1bdc9d16e9aa7eab7ce3e8d22ceecd81f6f98ba9abbb48c301c48443ed2541c`.

Item 35 private summary: `build/app-profile-cache-20261006/summary.json`,
SHA-256 `ade5655da120e5f502a9864bb917c89a658792757e17f5ea701625c0db7bfc9e`.
Real compiler receipt: `build/app-profile-clang-ninja-20261006-attempt2/result.json`,
SHA-256 `a187d274734cf3e4c8905e87c11592006e4e555fbb4e9369fe9fcdf55935a133`.
Initial missing-CRT and test-recorder failures are preserved beside the passing
results. Other optimizer changes in the shared builder/workflow are retained.

Item 37 is integrated as an explicit, default-off module option. Actual small
Clang/Ninja proof: `build/performance-item37-game-lto-v1/summary.json`, SHA-256
`346f415c235e236a0738d4e57ded658a9093c5125f1a8bb0b4441c5fb6d74637`.
Its public builder tests passed after deliberate hunk integration, preserving
other optimizer work. It retains precise FP flags, requires prepared C on
Windows Clang, limits linker jobs to one and fingerprints the option/helper
for preparation and training. Full-game ThinLTO remains unqualified.

Item 12 exhaustive summary: `build/f32-hardware-widen-20261006-attempt2/summary.json`,
SHA-256 `bcd2c95611eea6ab025a83fe57b7eed061cf58746ecef415c3adf099392c3739`.
The public-header integration rerun passed environment/pilot checks and exact
machine-code identity with those exhaustive objects:
`build/f32-build-integration-20261006/attempt1/result.json`, SHA-256
`c2cf172634417b5e21e5b60fb1a7da603f9c23e18c625fcedbaf0ab46125533a`.
Fourteen training/build-selection tests, real synthetic CMake configurations
and the preparation-cache lifecycle checks passed. No profile was retrained.

Item 07 native Outset receipt:
`build/performance-07-production-20261006/native-3300-attempt1/result.json`,
SHA-256 `cb5e0cb604657a9af9dcb5fc55cfa7c315baa9ac66de07c28b8116c5313c6364`.
The complete captured PPM is identical to the control; RGB SHA-256 is
`00e19929883fc4f2e07dc93305ea1c2bc8f3608bf5f99f31da82943653798353`.
This run occurred alongside CPU compilation and is not a timing experiment.

Item 07 Dragon Roost native control/candidate captures are also identical.
The route reduced CI uploads from **3,276 to 68** and total texture uploads
from **3,483 to 275**. Its paired receipts are under
`build/performance-07-routes-20261006/dragon-{control,texture2}-attempt1/`.
Captured PPM SHA-256: `12b8c56ef94b9039093417efb859a40436b7a665ab0eba49dea4359e752b4720`.
Forest Haven's arrival-dialogue captures match too, but both original runs
failed the expected ready-player check. Preserve those failures; they establish
cutscene image parity only, not a controllable gameplay or timing route.

Fortress sea and Hyrule Castle control/candidate runs also passed readiness and
exact captured-image comparison. The complete paired report is
`build/performance-07-routes-20261006/image-comparisons.json`, SHA-256
`06729419fbbed904cb0b502f575bf8c2996d5896e2c185c9883f28ea72359d37`.
Dragon Roost's reverse repeat is recorded in `dragon-reversed-comparison.json`
in the same directory, SHA-256
`f2e093ffd4db1741d9c897032d2b1ff0171f100df4c51c83405a343eb3e87933`.
Active SDK verification produced tree `94057b1820874643196ccd8c8660d6f4c2fdb18e`.
The checkout's CRLF representation was normalized only for comparison with the
tested LF source; the exact Git patch tree and manifest/lock agree. The initial
raw-digest assertion failure is preserved alongside the successful receipt.

Combined 01/02/07/08 compiled 82 SDK archive members, seven direct app consumers
and the authored GPU oracle. All producer libraries were refreshed along with
renderer consumers. Each of control, fusion, compact, command reuse and all
combined passed 403 actual GPU pixel/policy checks at scales 1, 1.5 and 2:
6,045 total. The native control's Outset capture matches the earlier baseline.
The native fusion failure is preserved in
`build/performance-combined-renderer-20261006/fusion-native-3300-attempt1/`.
Compact and command reuse separately passed the native Outset route with exact
control images. Compact also matched the Dragon Roost control image exactly.
The authored GPU fixture did not expose the real-game fusion array transition.
The added transaction-rollback regression passes 1,552 checks in each O3/ASan
run. Repaired native Outset passed with the same control image and shutdown
submitted=1,728,110, rejected=0, failed=0; 15,457,306 draws were fused.
Native receipt: `build/performance-fusion-array-fallback-production-20261006/fusion-native-3300-attempt1/result.json`,
SHA-256 `39e1e4bd1bdb1cbd81f00aba53f1dd4385f9f3726d525a4e08e918c359b37c00`.
This is correctness and submission-count evidence, not a serialized timing result.
Repaired sparse-uniform/decoder production receipt:
`build/performance-renderer-wave2-repair-20261006/attempt1/result.json`, SHA-256
`88d47b13de1197e01d944a0ded3d19785fb5f53809a2199f7cfa4df8629cd797`.
It rebuilt 52 changed translation units and reused 38 with verified dependency
identity, then linked the full host and authored GPU fixture.
Private combined compilation receipt:
`build/performance-combined-renderer-20261006/attempt4/result.json`, SHA-256
`ef555d651fdcca060b31423eec3710cf9dab70fe7ad05b141adc4d07239a89e2`.
Earlier output-path, missing Windows header and preparation failures are retained.

The native sparse-uniform v3 control failure is preserved under
`build/performance-renderer-wave2-repair-20261006/control-native-3300-attempt1/`.
A TEV early return bypassed full-binding finalization. The new authored
`tests/renderer_lighting_pixels_test.cpp` checks all eight light slots and four
attenuation modes, plus the earlier EFB pixels. It passes on the control at all
three scales and reproduces the candidate's unbound `vsl` error before repair.
Receipts remain under `build/renderer-lighting-oracle-20261006/`.

The game host's explicit `BLUEWAKE_RENDERER=aurora-noninteractive` route passed
unattended native GPU correctness tests. It requires explicit disposable
data/CARD/SRAM/state/cache paths and no live input/dialogs/settings file. Its
ordinary launch still selects the player window. Offscreen render tests cannot
establish displayed FPS or qualify display-presentation optimizations.

The rebuilt host refreshed all seven app configuration/header consumers against
the verified eleven-patch SDK archives. The launch-policy fixture passed 28
checks. Native startup and the full 3,300-retrace copied-CARD route both exited
zero and drained; startup/shutdown asserted one hidden, unfocused window, no
audio/controller subsystems and zero displayed frames. The longer run loaded the
native save, reached a ready Outset player and captured a visually checked
640×480 RGB frame. Source/card/settings/assets/runtime inputs stayed unchanged.
This qualifies the test route, not a performance gain or a visible launch.

Private native receipt: `build/noninteractive-host-20261006/native-3300-attempt1/result.json`,
SHA-256 `278ec6dbb2a77b73526684859362a80974ec55fc2045d1d8ee42fe9357143f65`.
RGB pixel SHA-256: `00e19929883fc4f2e07dc93305ea1c2bc8f3608bf5f99f31da82943653798353`.
The initial invalid test-clock run and compiler/recipe negatives are preserved.

This list preserves all performance leads from the
[fork audit](FORK_OPTIMIZATION_AUDIT_2026-10-06.md), including weaker ideas,
existing optimizations and unsuccessful experiments. Source links use frozen
commits. No optimization is enabled by creating this checklist.

| Test numbers | What they cover |
| --- | --- |
| 01–09 | Renderer changes: start here for gameplay FPS |
| 10–14 | Narrow CPU/compiler changes |
| 15–24 | Native replacements, tested one group at a time |
| 25–34 | Larger or less certain ideas; some require repairs first |
| 35–38 | Build correctness, cross-file optimization and profile quality |

Below the numbered cards are separate records for unsuccessful experiments,
already-present changes and other platforms. Use those to avoid duplicate work.

## Use the same test process for each item

1. Save the control and candidate source revisions, active patch recipe,
   compiler/profile hashes, settings and machine configuration. Build an
   isolated candidate; change one item or one required dependency at a time.
2. Prove correctness first: compare CPU/memory/device timing where relevant,
   then real-frame images, HUD, sound and game behavior. Exercise fallback and
   interruption paths. Run capture/verification separately from timing.
3. Use the same copied save, input route, resolution, texture pack, cache state
   and CPU affinity. Run control/candidate/candidate/control, then repeat in
   reversed order. Serialize measurements so another build/test cannot compete.
4. Start with Smooth Motion off; test 60/120 separately where relevant. Record
   game-update speed separately from displayed FPS, mean/p95/p99 frame time,
   process CPU, worker/GPU time and memory/upload volume. A displayed interpolation
   frame is not a new game update.
5. Accept only a repeatable benefit with no correctness failure or material
   regression in the other tested scenes. An isolated helper benchmark, smaller
   binary or reduced byte count alone does not establish a gameplay FPS gain.

Use Outset, Forest Haven, Dragon Roost, the Fortress sea and Hyrule Castle as
repeatable routes, plus the workload named on each card. Record exact save/input
identities; a location name alone does not reproduce a workload.

**Practical first pass:** 01, then 02 → 03, then 04. Also try 07 and the narrow
CPU changes 10–12. Test 05 before 06. This is priority order, not permission to
skip source dependencies. Elliott's full donor sequence is **01 → 02 → 07 →
05 → 03 → 08 → 04 → 06**, with profiler/test follow-ups between commits.

## Renderer: highest priority

- [ ] **01. [Combine tiny geometry draws earlier](https://github.com/elliotttate/RecompCore/commit/d687c6983a404156c540aca9f535c775217852e0).**
  Reduce repeated planning and motion matching before GPU batching.
  **Test:** compare fused/separate topology and images, then GX time in Forest
  Haven/Outset. Keep HUD/state boundaries and moving-mesh behavior correct.

- [ ] **02. [Use smaller vertices when possible](https://github.com/elliotttate/RecompCore/commit/6f52a68d14b4375afe55314ac0a66c490aa6f183).**
  Use 60 bytes instead of 132 for eligible geometry; retain the full fallback.
  **Test:** measure upload bytes and frame time; compare extra UVs, both colors,
  normal/binormal/tangent data, texture matrices, HUD and interpolation.

- [ ] **03. [Prepare vertex conversion operations once](https://github.com/elliotttate/RecompCore/commit/c680b4e3c84dfaef0081fb8d87c4e90e0159f4f7).**
  Avoid reinterpreting attribute formats for every vertex.
  **Test:** compare actual decoded output from an owned input snapshot for all
  supported formats, malformed inputs and fallbacks; then time sea/GX workloads.
  Depends on 02 or an explicit adaptation to the existing layout.

- [ ] **04. [Skip unchanged GPU vertex uploads](https://github.com/elliotttate/RecompCore/commit/39dcacb2e72546bb41b6eaf25b73aa0382742381).**
  Compare complete bytes with the shared GPU buffer's CPU shadow at the same offset.
  **Test:** record upload bytes/GPU time and image parity through moving objects,
  frame splits, queued frames and interpolation. Add cancellation/device-reset
  invalidation and test aligned neighbors/gap merging before timing.

- [ ] **05. [Upload only the constant blocks a draw needs](https://github.com/elliotttate/RecompCore/commit/160224821dbef6d57fd1df4536a92b050885be10).**
  Split draw data, matrices and lights instead of copying one large block.
  **Test:** compare every shader-read value and image; measure uniform bytes and
  binding/encoding cost, including lighting, HUD and interpolated frames.

- [ ] **06. [Change draw constants without rebinding everything](https://github.com/elliotttate/RecompCore/commit/400728a384a3f90456ff02940eb8237a49a005d7).**
  Use immediate offsets into storage buffers. Depends on 05 and adapter support.
  **Test:** measure encode/submit time; test unsupported-device fallback and
  backend reset. First repair the missing `hud_multiplier` shader rewrite,
  complete-field coverage, capability lifetime and shader-cache identity.

- [x] **07. [Remember two texture versions at the same address](https://github.com/elliotttate/RecompCore/commit/180f3896f8738cc2975c58486a66d694b3cbad61).**
  Stop alternating palettes from repeatedly evicting one another.
  Accepted for repeated native texture-upload reduction with exact captures;
  frame-time/FPS measurement remains separate and pending.
  **Test:** use Dragon Roost and a two/three-palette fixture; count texture decodes
  and uploads. Verify ordinary writes, aliases, palette changes and HD replacements.

- [ ] **08. [Reuse render-command allocations](https://github.com/elliotttate/RecompCore/commit/11c1369c56a04de086a053de491f365b7d0274a8).**
  Retain command-vector capacity between frames.
  **Test:** measure allocations/page faults and frame tails; stress queued frames,
  replay and scene switches to prove storage is recycled only after its last reader.

- [ ] **09. [Copy the finished image directly to compatible displays](https://github.com/jyapayne/Wiicompiled/commit/a2d0f9792c8190d91b02dbf89f1ff644bb9ca8d7).**
  Avoid an extra fullscreen shader pass when size, format and opaque composition match.
  **Test:** GPU presentation time and exact pixels at native/scaled resolutions,
  resizing and format/alpha changes. Preserve the shader fallback and overlays.

**Measurement helper:** Elliott added an optional
[GPU profiler](https://github.com/elliotttate/RecompCore/commit/48ad46f16e62f54302a760eced7ee3f4a9e9c406)
and a [larger zone budget](https://github.com/elliotttate/RecompCore/commit/126eee1ba46ca831c74a485a2e05fe2d6a3869b1).
Validate complete-frame coverage and profiler overhead. This helps explain
results; it is not itself an FPS improvement.

## CPU and individual native replacements

- [ ] **10. [Use denser CPU dispatch switches](https://github.com/AceSpectre/DolRecomp/commit/b6d5e7cb7c16103f4c3ef68b4d721f4a82977380).**
  Convert PC cases into instruction-slot cases while rejecting invalid addresses.
  **Test:** inspect optimized assembly first; compare every legal/gap/unaligned/
  alias entry and exact budget cut. Retrain changed profiles, then time gameplay.

- [ ] **11. [Reject out-of-range returns quickly](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/scripts/windows/return_ranges.py).**
  Avoid searching a return switch when the target belongs to another chunk.
  **Test:** all switch cases, gaps, outside targets and yields; require a precisely
  recognized original switch, equivalent state and less actual compiled work.

- [ ] **12. [Widen ordinary float loads with hardware](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/tests/f32_from_bits_test.c).**
  Use host conversion for normal values; retain exact special-value handling.
  **Test:** exhaustive float patterns, FP environment/state, signed zero,
  subnormals and NaNs; then measure load-heavy gameplay.

- [ ] **13. [Skip unnecessary floating-point rounding](https://github.com/AceSpectre/DolRecomp/commit/83398f1ccd93dc9f0e11ac626c0c11ae5b74ee5c).**
  Track values already representable as single precision.
  **Test:** exact FP bits/state across rounding modes, exceptional values,
  callbacks and FP faults; clear facts at every required boundary. Count removed
  operations and measure gameplay separately.

- [ ] **14. [Reuse actor-name lookup results](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/cmake/composite/native_search.c).**
  Batch consecutive actors that fail the same name search.
  **Test:** matches at every list position, no matches, short budgets, aliasing
  and watched helpers; preserve current observer/suffix checks. Time actor-heavy scenes.

**For 15–24:** certify the exact current prepared source first. Compare complete
CPU/RAM/FP state, cycle/deadline cuts, callbacks, aliases and rollback. Preserve
dynamic host observations. Keep existing native options identical in both builds,
vary one new group, and require nonzero hit counts. Additional native entries
remain opt-in locally. Set 7's cached quiet-host helper needs adaptation.

- [ ] **15. [Native environment colors — set 4](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/cmake/composite/native_kankyo.c).**
  **Test:** changing lighting/color blends and exact FP outputs; measure verified calls.
- [ ] **16. [Native character animation — set 4](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/cmake/composite/native_anim.c).**
  **Test:** keyframe endpoints, unusual scales and inverse transforms; movement/combat with many actors.
- [ ] **17. [Native collision arithmetic — set 4](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/cmake/composite/native_cc.c).**
  **Test:** collision boundaries, overlapping memory and cylinder centers; collision-heavy gameplay.
- [ ] **18. [Native graphics command writers — set 5](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/cmake/composite/native_gx.c).**
  **Test:** identical FIFO bytes/order/drains, memory writes and decline paths; then images and GX time.
- [ ] **19. [Native math-library routines — set 7](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/cmake/composite/native_libm.c).**
  **Test:** special FP values and exact random-state sequences; workloads with substantial calls.
- [ ] **20. [Native collision bounds — set 7](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/cmake/composite/native_bgblk.c).**
  **Test:** empty/degenerate bounds and identical collision outcomes; room/actor collision workloads.
- [ ] **21. [Native quaternion rotation — set 7](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/cmake/composite/native_rot.c).**
  **Test:** wrapping/extreme angles and exact transform outputs; animated scenes.
- [ ] **22. [Native integer angle calculations — set 7](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/cmake/composite/native_calc.c).**
  **Test:** all quadrants, zeros and boundary rounding; verify `cM_atan2s` call counts.
- [ ] **23. [Native geometry and wind calculations — set 7](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/cmake/composite/native_geom.c).**
  **Test:** degenerate planes/polar inputs, point winds and identical physics results.
- [ ] **24. [Native audio envelopes/channels — set 7](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/cmake/composite/native_jas.c).**
  **Test:** exact envelope progression, CPU/RAM and captured sound through music/effect transitions.

## Larger changes and less certain fork leads

- [ ] **25. [Retain complete meshes in GPU memory](https://github.com/zalo/aurora-arm/commit/07f3fbd575196886e14bb22a8107a0e065d4e846).**
  The persistent cache idea: avoid repeat conversion and uploads across frames/visits.
  **Before testing:** replace sampled list/pointer identity with complete change
  detection for actual list/array reads. **Test:** same-address rewrites, interior
  changes, room revisits, dynamic meshes, eviction, queued draws and device reset;
  record conversion/upload savings, memory and frame time. Distinct from 04.

- [ ] **26. [Batch different transforms using resident records](https://github.com/zalo/aurora-arm/commit/5c53a98b344f158b5eee0dc77eec2fa4389d72af).**
  Keep per-vertex transform records so more draws can share one submission.
  **Test:** depends on a safe 25; compare transforms, lighting, skinned meshes,
  HUD/interpolation and draw counts. Donor mobile results do not establish ours.

- [ ] **27. [Avoid repeated native GX descriptor setters](https://github.com/jyapayne/Wiicompiled/commit/b333669bdb687d7ec777c888ed0899ecb0072dd0).**
  Return early only when both SDK shadow and decoded state agree.
  **Test:** establish native SDK-path call counts first; recording/NRM/NBT aliases
  and CP writes must still work. Our retail path differs, and its maps already use
  Swiss hashing; replacing the map is not automatically a new benefit.

- [ ] **28. [Keep guest registers local inside selected functions](https://github.com/AceSpectre/DolRecomp/blob/83398f1ccd93dc9f0e11ac626c0c11ae5b74ee5c/src/backend/fn_emitter.c).**
  Extract selected translated guest functions and reduce CPU-state loads/stores
  and dispatch between calls.
  **Before testing:** adapt precise deadline charges/refunds, callback publication,
  native/mod/watch interception and aliases. **Test:** interrupted/resumed calls,
  helper-mutated registers and full state parity, then actor-heavy gameplay.

- [ ] **29. [Remove truly unnecessary cache-instruction callbacks](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/scripts/windows/cache_ops.py).**
  **Adaptation:** the explicit preparer retains the FIFO drain and current callback; only the runtime trampoline is removed. The donor PC-only rewrite stays declined.
  **Test:** pending FIFO bytes, each opcode, custom callbacks and exact PC/cycles;
  time only after the observable drain/observer behavior is preserved.

- [ ] **30. [Reduce state accesses in the offline intermediate representation](https://github.com/chrissotraidis/DolRecomp/blob/acb8e7b472f9a000ee98fbc52033f536c5e4bf4c/docs/STATE-ACCESS-PASS.md).**
  Analyze which CPU-state reads/writes can be reused.
  This is offline opt-in IR tooling, not an existing C-builder switch.
  **Test:** compare optimized assembly first: the compiler may already remove
  them. Require full FP/callback/resumption parity and rendered A/B afterward.

- [ ] **31. [Evaluate the newer LLVM/native execution architecture](https://github.com/ExpansionPak/DolRecomp/commit/c876e2b9e022e084aa9a690fe15e2ad79a0706cd).**
  A separate backend/ABI project, not a drop-in performance patch.
  **Test:** only after complete scheduler, memory, FP and hook compatibility;
  compare identical routes with our prepared C backend. Count compilation/cache
  savings separately from gameplay speed.

- [ ] **32. [Evaluate the native-port single-pass renderer](https://github.com/danieltobey/aurora/commit/dc9d1841fb2eb5370f43036bb5049a6570f0ce23).**
  **Test:** first map its assumptions onto our retail frontend; preserve GX
  copies/depth effects. Compare every pass/effect and GPU time. Do not delete a
  required pass merely because another native port can combine its passes.

- [ ] **33. [Start scene GPU work before waiting for a display image](https://github.com/shibbo/aurora/commit/6d88d468985bba3aeba3dce92acdcff87ad9d227).**
  Overlap scene rendering with swapchain acquisition using an extra submission.
  **Test:** frame tails at each present mode, hidden/resized windows and queue
  order; measure the extra encoder/submit cost as well as reduced waiting.

- [ ] **34. [Evaluate native Aurora completion/vertex-format changes](https://github.com/Frityet/aurora/commit/870f700098ae5dd381623a9bcff44205cb7ba003).**
  **Test:** identify a corresponding bottleneck in our worker/staging path first;
  verify FIFO layout, GPU completion and buffer lifetime, then rendered A/B.
  This fork uses a different frontend; no compatible gain is established.

## Build and profile inputs

- [x] **35. [Make changed app profiles rebuild the right objects](https://github.com/elliotttate/Wind-Waker-Recomp/commit/be97a2da80195352b7f4b12d13fae6a8810afb9a).**
  Put the profile hash into the compiler's input path.
  **Test:** unchanged input reuses objects; changed input rebuilds all affected
  objects; unreadable input falls back correctly. This is build correctness.

- [ ] **36. [Retrain the app profile for the selected source](https://github.com/elliotttate/Wind-Waker-Recomp/commit/944a1f3c2b130a8086295428a0847938a777a2d7).**
  Depends on 35 and compatible profile/toolchain fingerprints.
  **Test:** no profile versus existing profile versus locally retrained profile
  on identical routes. A donor binary profile belongs to its matching source;
  a newer date does not prove better performance here.

- [ ] **37. [Optimize across game-module files with ThinLTO](https://github.com/dougchansan/DolRecomp/commit/1ef0fcd60a491ba9b191dcaf94fd9419009c93c1).**
  The normal profiled app build already uses ThinLTO; the game-module compile
  path does not.
  **Preparation:** add `-flto=thin` to both C compilation and DLL linking in an
  isolated build. Use explicit `--module-thinlto` (default off). Preserve the prepared
  C backend, exact FP flags, CPU/RAM options and exported runtime hooks.
  **Test:** change only game-module LTO with matching source, profile and compiler.
  Record build time, peak memory, module size and rendered gameplay. Smaller
  modules alone do not establish a speed improvement.

- [ ] **38. [Train the game profile on more representative gameplay](https://github.com/dougchansan/DolRecomp/commit/0f069ef2edfd610f23b6ced16486d047966f15b2).**
  The builder supports game-module PGO, but the stable module used by the
  October 8 deep-debug comparisons records `profile: null`, O2 and no module
  ThinLTO in its authentic build receipt. Do not infer active game PGO from
  builder support or the separate app profile in 36. Qualify a fresh matching
  game profile before comparing coverage quality.
  **Test:** extend the training routes beyond the opening playbacks, then use
  existing retraining controls with matching source/compiler. Use `--no-tiered`
  in both builds to isolate profile quality, or record changed O1/O2 assignments.
  Compare profiles on held-out heavy scenes that were not used for training.
  Accept repeatable gains there without regressions elsewhere.

## Previously tested or explicitly shelved

Retest these only when the mechanism, compiler or targeted workload changes.
They are retained so we do not repeat an unsuccessful experiment accidentally.

| Experiment | Recorded outcome | What would justify returning |
| --- | --- | --- |
| [CPU geometry cache](GEOMETRY_CACHE.md) | Correct reuse, about 57% of vertices; no reliable rendered gain, median CPU about 3% higher. Inactive draft. | Lower validation cost, larger mesh admission or combination with GPU retention; measure the changed mechanism. |
| [Deferred PC stores](COMPILER_STATE_PERFORMANCE.md) | Fewer source stores/smaller DLL; no gain, mean CPU about 0.58% higher within run variation. Off by default. | Show reduced hot machine instructions on a new compiler/workload. |
| [Guarded indirect tails](COMPILER_STATE_PERFORMANCE.md) | Correctness passed; rendered results mixed, no reliable FPS improvement. Off by default. | A demonstrably hot dispatch target with different generated code. |
| Precise timing-helper argument rewrite | Prepared hot assembly identical; rejected. | First demonstrate a machine-code difference without changing exact yields. |
| Extra texture bind-group memo | Fixture parity passed; rendered comparisons did not establish a gain. Not promoted. | Frequent costly multi-texture binds in a matched scene. |
| Fine per-draw timing instrumentation | Timer overhead consumed about 79–80% of sampled cycles; substage rankings were distorted. | Lower-overhead, calibrated instrumentation; measure its own cost separately. |
| [Native particle/sea-wave replay, set 6](https://github.com/elliotttate/Wind-Waker-Recomp/blob/944a1f3c2b130a8086295428a0847938a777a2d7/cmake/composite/module_export.c#L293-L301) | Donor default off; no benefit in some areas, increased CPU at Forest Haven. | A narrower hot routine or reduced decline/rollback overhead. |
| [Two-stage GX worker](https://github.com/elliotttate/RecompCore/commit/ec5c1d3ee37acd1f6368cba520c479dbde56907f) | Donor shelved it after slower measurements; copied ring gave no gain. | A different workload/queue design, with synchronization costs counted. |
| [Switch threaded GX decoding](https://github.com/nx-mod/wiicompiled-nx/commit/70744cd165a62826615b4c38f307ef93c3a4d029) | Donor reports slower decoding due to frequent synchronization; disabled. | A design that actually removes the synchronization bottleneck. |
| [Straight prepaid-body copies](https://github.com/elliotttate/Wind-Waker-Recomp/commit/489f3029d3da772405a52bb84a460724f3f53653) | Removed upstream after PGO showed no gain and increased code size. | A new compiler/target and proof of changed hot code. |

## Already present: regression and on/off comparisons

These correspond to the 21 items in
[Elliott's older checklist](https://github.com/elliotttate/Wind-Waker-Recomp/blob/b39bd0dc9d18b801938a547969146185131eda19/docs/PERFORMANCE_OPTIMIZATIONS.md).
They are not missing new imports. Availability does not mean every option is on;
our desktop Smooth Motion default is Off. Use a supported switch or isolated
control build; never simulate disabling an optimization by changing game timing.

| Original item | Present improvement | Regression / measurement |
| --- | --- | --- |
| 1 | Apple PGO with outlining disabled | Inspect cold-chunk assembly and untrained actors. Windows x86 default outlining is already off; do not predict a gain from that flag alone. |
| 2 | Additional interpolated display frames | Compare real frames, motion and unchanged game-update rate. |
| 3 | Matching repeated scenery instances | Pan past grass/palms; inspect unmatched/newly visible objects. |
| 4 | Interpolation through rapid camera turns | Script turns; check cut detection and motion continuity. |
| 5 | Reuse repeated constants and blend used matrices | Test draw-heavy scenes; compare fields and matching cost. |
| 6 | Separate motion-matching helper | Check queue/cut order and game-thread waiting. |
| 7 | Three intermediate frames for 120 Hz | Measure cadence, clipping and staging capacity. |
| 8 | Regular presentation clock | Measure present intervals and scene changes. |
| 9 | Reuse GPU pass bindings/state | Compare every binding transition and encoding cost. |
| 10 | Late GPU draw batching | Compare topology/textures and actual submitted draw count. Distinct from new 01. |
| 11 | Mapped uploads for interpolation | Check staging lifetime and upload/allocation costs. |
| 12 | Reduce interpolation under sustained overload | Force overload; verify game speed and recovery hysteresis. Includes active patch 0154. |
| 13 | Avoid drawable waits while hidden | Hide/show, fullscreen and resize; measure hitch tails. |
| 14 | Less helper spinning / batched wakes | Check sleeping-worker progress and idle CPU. |
| 15 | Direct FIFO enqueue and reusable draw plans | Compare exact command order and allocation counts. |
| 16 | Graphics address cache / fewer actor-budget checks | Alias changes, worker reads and exact deadline cuts. |
| 17 | Inline mouse-camera fast check | Idle versus active mouse input and watched boundaries. |
| 18 | Faster scene transitions | Measure transition duration; verify scene, sound and save events. |
| 19 | Shorter knob-door transitions | Verify completion/cancel paths and correct destination. |
| 20 | Apply saved settings at startup | Restart; verify requested rendering/anisotropy settings. |
| 21 | Cheaper GX derived-state/texture/pipeline lookups | Mutate BP/CP/VAT/XF and texture state; compare cached versus recomputed results. |

Also retain checks for existing transform-snapshot reuse (0152), background
controller enumeration (0153), fixed CPU/RAM and inline register/memory helpers,
certified matrix/vector/skinning paths, libPorpoise's imported matrix slice,
DSP HLE, texture dirty validation, asynchronous/prewarmed pipelines and the
D3D12 ubershader fallback. Exercise hotplug, dirty aliases, shader misses and
sound where appropriate. The feature-dispatch interval guard is also present;
its earlier rendered measurements were inconclusive.

## Other platforms, other workloads and compatibility

Keep these recorded, but they are not unexplored Windows gameplay FPS fixes.

| Lead | Why separate / when to test |
| --- | --- |
| [Browser staging-range copies](https://github.com/Minithena/Wiicompiled/commit/e8b61f8e62fff4de974403f7f5f825cd75751d08) | Emscripten-specific full-range copying; native used-range uploads already exist. Revisit for a browser port. |
| [Apple checked-access PSQ repair](https://github.com/chrissotraidis/wiicompiled/commit/3a688ad39d64ff9f2ca091c02aa03f97b9ab5f2d) and [split fallback](https://github.com/patchzyy/wiicompiled/commit/9d182f831618235b0cf74ab891b8c3440b658e30) | Our type-0 PSQ already tries RAM fast paths. Revisit only if a forced slow call is observed. |
| [Android local symbol binding](https://github.com/KeithKirenai/wheeldroid/commit/168c087b266ee03058301931c3f7c7506257822e) | ELF PLT/GOT issue, not the Windows internal-call mechanism. |
| [Anonymous Mac guest-memory aliases](https://github.com/patchzyy/wiicompiled/commit/6fa24737d3362b6cba8162671745262751f457b8) | Avoids file-backed memory I/O on Mac; evaluate startup/memory behavior there. |
| [THP video decoding](https://github.com/nx-mod/wiicompiled-nx/commit/266950ac60a3ff2188681db91968affbb2f26335) | Video workload; test every decoded movie frame and decoder time separately. |
| Browser/mobile pipeline prewarming, ubershaders, memory resolvers and geometry repacking | Related facilities already exist here or address those platforms' drivers. Require a matching missing path before porting. |
| New libPorpoise simulator rendering | Imported matrix source is unchanged; its GX/EFB/CMPR/TEV work does not affect our matrix-only use. |
| [i386 calling convention / Xbox port](https://github.com/GTTeancum/DolRecompX/commit/0ba69ce113a45ac095841301992869edb8197d2b) | Different CPU/OS target; no Windows x64 gain implied. |
| Older PGO/fixed-state/compiler experiments | These concepts are already represented locally; compare only with a changed compiler/profile and inspect actual machine code. Game-module ThinLTO is a separate candidate in 37. |
| [GLES/Mali buffer growth](https://github.com/zalo/aurora-arm/commit/22f245a1067388e55a82e260e29fb8f56853dc7a), [follow-up](https://github.com/zalo/aurora-arm/commit/40071d0afb39a0a32284348925379ba292a86bad), and [GL shader compilation on the main thread](https://github.com/zalo/aurora-arm/commit/b46f42540181a48d3095b3105afd6164710b59d9) | Test on the matching driver/backend; no Windows D3D12 gain established. |
| [Vita GXM](https://github.com/robin994/aurora-vita/commit/ff5b2cf5ab6866fbf6e7708a0978157ddc5c0e7b) and [CDRAM shadows](https://github.com/robin994/aurora-vita/commit/321b77aff13f9ba8d977798c4adec77df4b35cad) | Vita-specific renderer/memory work; require device tests there. |
| [Switch threading](https://github.com/souldbminerr/aurora-switch/commit/fd44fb7ebe96cf44684116010e5e1fd9f6f9a327) and [Xbox UWP resolution/D3D12](https://github.com/kipters/aurora/commit/5e3bca38012f4c5c5fc82673b654c80fc252f44a) | Different platform integration; qualify on the matching hardware and workload. |
| Native 60/120 Hz game-logic hacks | Change the simulation; assess separately as gameplay modifications, not unchanged-behavior speedups. |
| [8 texture generators / 16 TEV stages](https://github.com/chrissotraidis/RecompCore/commit/2b64d3d2fe74ecdbf9c7455db282add7a4cd0109) | Missing dungeon-map correctness expansion, not a measured FPS gain. Coordinate shader/layout changes with 02/05/06 if adopted. |

## Record each result here

Copy this block for each numbered candidate. Keep large captures/logs and any
generated game data in ignored local build directories; record paths/hashes here.

```text
Test number / date:
Control source + active recipe:
Candidate source + exact source commit(s):
Compiler / game profile / app profile:
Machine / affinity / resolution / texture settings / Smooth Motion:
Save + input route hashes / cold or warm cache:
Correctness tests and real/interpolated image/audio results:
Control/candidate ordering and per-run measurements:
Game speed / presented FPS / mean, p95, p99 frame time:
Process CPU / worker time / GPU time / allocations / upload bytes / memory:
Candidate hit / decline / fallback counts:
Decision: accept / reject / inconclusive:
Reason and any prerequisite for retesting:
Private receipt paths:
```

## Crowded-scene follow-up - October 7, 2026, 02:16 CDT

The latest play log still shows a game-thread limit in crowded views. The
[finite gameplay observer filter](PERFORMANCE_BUSY_SCENES.md#second-pass-finite-gameplay-observer-filter-october-7-2026)
passed real-predicate O3/ASan checks and exact native Dragon capture/context
comparison. Four serialized control/candidate runs finished 9.96% sooner with
7.36% less process CPU; both pairs agreed. This is an additional host-only
improvement over d2285b8, not full-speed qualification of all busy areas.

Keep the partial native-RIP diagnostic, fixture failures and prior checklist
results. Selective host ThinLTO and gather batching remain inactive because
speed evidence is insufficient. Tingle rescue wait-skip remains disabled and
is not a prerequisite for this tester or unrelated gameplay milestones.


## Title-intro follow-up - October 7, 2026, 12:33 CDT

The title attract sequence still has a CPU-bound 20 FPS dip. The
[caller-context chassis guard](PERFORMANCE_BUSY_SCENES.md#title-intro-follow-up-october-7-2026)
admits the existing certified Maya helper without dropping GroundCross return
observation. Actual old/new predicates pass O3/ASan checks; separate title and
Dragon Roost captures match, and Dragon's GroundCross count is unchanged.
Four serialized title runs improve route time by 4.23% and process CPU by 4.41%.
This is a modest host-only improvement; full-speed title/gameplay is unresolved.
The current module is unchanged. Native GX writers remain a separate candidate
requiring fresh module admission, native/GPU checks and repeated timing.
Tingle rescue wait-skip remains disabled; preserve earlier results and failures.


## Native GX title result - October 7, 2026, 13:23 CDT

Candidate 18's private current-module experiment admits 29 GX writers. Actual
builds and the O3/ASan admission oracle pass; a separate native title frame
matches completely (requested VI 1100, completed VI 1103). Four serialized
1,800-VI runs show conflicting pairs: CPU -4.43% then +4.57%, wall time -3.68%
then +4.93%. Mean CPU is +0.065%, wall time +0.607%. Keep the candidate inactive.
Gather batching was enabled equally on both sides and is not independently
promoted. See the [complete result and limits](PERFORMANCE_BUSY_SCENES.md#native-gx-title-experiment---october-7-2026-1323-cdt).

Matching clocks, scene counts and FIFO call totals do not imply exact command or
guest-RAM equality: the final control has eight fewer draws, and interrupt
delivery hashes vary within controls. Preserve all receipts and failures in
`build/title-intro-20261007`; the decision is `gx-decision-v1.json`. Retest only
with a changed mechanism or concrete profile hypothesis. Normal-builder and
versioned-capability integration stay deferred. The installed gated tester
retains only the accepted animation guard change; the 20 FPS dip remains
unresolved, and Tingle rescue wait-skip remains disabled.


## October 7, 2026 evening: fresh CPU profiles and single-probe edge lookup

Performance remains the priority. Two fresh hidden, warmed, no-input title
diagnostics used the installed `f3198b53` host and `976184c6` module. Both
completed 1,800 VIs with zero renderer rejects/failures, no pipeline compilation,
no state load/save, copied CARD/private SRAM and preserved original inputs.
Sampling is diagnostic and excluded from throughput timing.

The game-thread profile contains 4,634 native RIP samples: 3,492 in the translated
module and 1,102 in the host. `host_chassis_requires_full`,
`host_can_skip_observation` and `bluewake_game_events_observes` account for
619 samples (13.36%) together; the inlined edge lookup itself accounts for
99 (2.14%). These are sample frequencies, not exact exclusive CPU time or
predicted gains. The second profile selected the busiest owned non-main thread
and confirmed its GX role from symbols. Its 4,378 samples include draw-plan
construction (506), core-plan observation (207), packet consumer (124), packet
emission (84), packet filling (72) and FIFO archiving (12). Another 1,711 hit
the installed Windows `NtWaitForAlertByThreadId+0x14` stub; no stack was collected
to identify the wait's caller or owner. Do not add percentages across threads
or treat waiting samples as CPU work.

A private candidate replaces only the fixed edge hash's probing lookup with
one verified load/compare: the same 56 keys, 256 slots, multiplier `0xE52766E2`,
shift 24, and the old zero-sentinel behavior. Generation refuses collisions
before rewriting the header. All dynamic flags, mirrored addresses, raw module
alias, interrupt handling and the accepted GroundCross/Maya continuation gate
remain unchanged. Only current `main.c` and `edge_intercepts.c` were rebuilt;
all other host link inputs, imports, resources and manifest match the installed
host. The module was unchanged. Candidate host SHA-256 is
`12eeb4823b72c9dc31cf9caf3d1b61cf13e2fde69ac7861b3f36660b600dcaa8`.

Fresh generator/collision-refusal, 63,005,941 exact lookup comparisons and
edge-switch/menu/mirror/raw-module tests pass. The historical lookup oracle also
passed the same comparison count under O3 and ASan. Fresh fixtures first hit
missing CRT-library and pthread dependencies; all failed attempts remain, and
the successful source-only fixture uses exact extracted CARD predicates.
Every child job is drained. Separate current/candidate title captures match
the complete P6 bytes at actual VI 1103 (requested 1100), SHA-256
`5c91962fa369b7f664fefc2f6a783ab601d7c5ba42b326130294f743bf531e96`.
This is one frame, not complete RAM/command-stream parity.

Four serialized, unsampled, uncaptured warm title measurements followed ABBA
order. Both builds retained identical settings, affinity, module and immutable
cache seed; all runs passed route checks with no owned compiler/other game
overlap. Complete-process measurements:

| Order | Build | Wall seconds | Process CPU seconds |
| --- | --- | ---: | ---: |
| A1 | Installed control | 38.828 | 55.796875 |
| B1 | Single-probe candidate | 38.125 | 56.640625 |
| B2 | Single-probe candidate | 37.187 | 54.875000 |
| A2 | Installed control | 37.781 | 55.984375 |

Mean wall time fell 1.693%, but mean CPU fell only 0.238% and process cycles
increased 0.017%. Paired CPU changes conflict: +1.512% and -1.982%. The policy
declared before measurement required both pairs to improve CPU/wall and both
means to improve at least 1%. Decision: **NO_CLEAR_ROUTE_GAIN; keep inactive**.
This does not establish better displayed FPS or resolve the 20 FPS intro dip.
The source-only draft is retained at
`patches/compiler/drafts/host-single-probe-edge-filter.patch`; it is not applied
to the production source or build recipe.

Local evidence is under `build/performance-focus-20261007/`: both native
profiles/rankings, `profile-summary-v1.json` (SHA-256
`8dbeed8f01d5bfa61159ddb9080906a85b87fc50309c848c5dba4fae80c67dd6`),
`edge-host1/work/result.json`, `edge-host1/tests4/result.json`,
`edge-capture-comparison-v2.json` (SHA-256
`5231d56e3cae2ad2dba3783e80f426a800d22effb65b012906b86725a14fbb82`)
and `edge-timing-summary-v2.json` (SHA-256
`7c88457f4a78c4eb3d56be6d53547b18a09e57fabc15dd1be5bcf8ef867acbe3`).

Next prioritize translated-code/dispatch and remaining dynamic observer cost,
using actual instruction profiles and CPU/RAM/budget/continuation contracts.
Worker draw-plan construction is a larger identified target than FIFO copying.
Stream-packet preparation may be reduced through an explicit sink capability
while preserving diagnostic sequences/counters/errors and full trace observers;
it is a narrower unimplemented experiment, not an established speedup. Avoid a
GD initializer or renderer-library rewrite without evidence of time spent there.
Installed binaries and the gated tester remain unchanged. Tingle rescue wait-skip
stays disabled, and every earlier negative experiment remains recorded.


## October 7, 2026 night: isolate GX statistics from the gather-pipe flag

The actual installed host's per-draw submitted/rejected atomics shared the
64-byte cache line at RVA `0x823AC0` with `g_display_copy_pending`. The game
thread reads that flag on normal gather-pipe writes; the GX worker increments
the submitted counter for each draw. The native worker profile has 184 samples
at the locked increment/next instruction. These samples identify a contention
candidate; they are not exclusive stall-time measurements.

Active runtime patch `0164-isolate-draw-statistics-from-display-copy-request.patch`
groups both counters in one `alignas(64)` object of exactly 64 bytes. Their
types, atomic operations, memory order, reset, increment and shutdown reads are
preserved. The actual candidate PDB places counters at RVA `0x823B00`, and the
flag at `0x823B40`, in distinct cache lines. Only the backend and graphics SDK
objects change; the other archive members, 64 direct host objects, imports and
embedded manifest are retained. The translated game module remains `976184c6`.
Candidate host SHA-256:
`79e595cb323481ac00e395c817acddaf13e9bdbcb026cd3299c5006a59e92a8d`.

Four serialized warm title runs used the same immutable cache seed, native
copied card, private SRAM, settings, module and affinity. No compiler or other
owned game overlapped. No capture, sampling, shader compilation, scripted input,
physical input, audio output or presentation occurred in these timing runs.

| Order | Build | Wall seconds | Process CPU seconds |
| --- | --- | ---: | ---: |
| A1 | Installed control | 38.188 | 55.734375 |
| B1 | Counter isolation | 36.063 | 52.796875 |
| B2 | Counter isolation | 39.719 | 54.921875 |
| A2 | Installed control | 43.078 | 57.312500 |

Both pairs improve: CPU -5.271%/-4.171%, wall -5.565%/-7.797%.
Mean CPU falls **4.713%**, wall time **6.748%**, process cycles **4.053%**.
This meets the predeclared timing policy of both pairs improving CPU/wall and
both means improving at least 1%. These are complete-process measurements on
one unpaced hidden intro route, not displayed FPS, latency percentiles, or
proof that the visible 20 FPS dip is resolved.

Independent correctness passes match complete intro P6 pixels at VI 1103 and
all six complete logical CPU/MEM1/MEM2/ordered REL alias hashes at VI
300/600/900/1200/1500/1800. Checkpoint timing is excluded. Both Dragon Roost
native cases pass all 22 save/inventory/scene/rendering checks; their complete
853x480 P6 frames and loaded checkpoint contexts also match. Original saves
and prior diagnostic inputs remain immutable.

Preserve the strict whole-GX-summary comparison failures: eight extra textured
draws and sixteen extra zero-quads occur in both control and candidate
observations. The first difference appears before capture or shutdown, and
its cause is unproven. Counters are not lost: submitted equals planned.
Direct-query totals also vary. Do not claim complete command equality or
guest-state equality between the six checkpoints. Additional complete state
and gameplay-image comparisons qualify this small storage-only change despite
the existing asynchronous-work variation; the strict failed analysis remains.

Source promotion: the normal builder verifies all 13 active runtime patches,
with candidate tree `c1f8d1e5f6c56826c90982eb0f827c19b0417fcd`. The three
materialized SDK files match the compiled candidate bytes exactly. Local
receipts under `build/performance-focus-20261007/`:

- `counters-observation-v1.json`, SHA-256
  `814509db1a0e46d7fb6dff001155e8dbce27b57de04c8dbe4944313ae3d01bee`.
- `counters-checkpoint-comparison1.json`, SHA-256
  `c598c2fb8c065a645fef476d9f2b384b478534889a914d9d36956267aa1a3cbe`.
- `counters-dragon-comparison-v1.json`, SHA-256
  `5cda98d4e61a979972864e309532718f534778186a8dd12080cda446a11f38ed`.
- `draw-counter-host1/promotion1/verification.json`, SHA-256
  `9c6d6df7125f91388d74c1edb3db4228aa43c4872b38559a46f4921ff03f0386`.

Tester packaging/install qualification follows separately. Tingle rescue
wait-skip remains disabled; all negative experiments and other optimizer work
are preserved.

### Counter-isolation tester and reversible installation

Source commit `3496f4ab5fc02f097ade8dd9d720cf4205b78286` is packaged in
`build/windows-tester-20261007-draw-counters/candidate/BlueWake-tester-20261007-draw-counters-3496f4a-windows-x64.zip`
(205,822,517 bytes; SHA-256
`930dee763930861cac60d8f2f6e1d2e6e958e3decdb0145632f9d104ccfb1266`).
The actual release-asset gate passes with its Windows translated-code exception;
no other finding is waived. Gate receipt SHA-256:
`cb0b37f898efa8995f599102152de33e497cee5131c7ca09f5e4fc54280bad24`.

The gated tester is installed at `E:/GPT5/WindWakerRecomp/BlueWake`. Exactly
`BlueWake.exe`, `BUILD.json`, `README.txt` and `SHA256SUMS` changed. All 42
other package files, including module `976184c6`, and all 448 player APPDATA
files and nine directories matched before and after. The old four files have
a verified backup and standalone guarded restore at
`build/performance-focus-20261007/counter-install1/attempt2/Restore-Previous-Tester.ps1`.
Installation receipt SHA-256:
`a85b5da5bbc45e21ca52e81ac1aca65a25857f553a65473847cb114e56dc9cdf`.
Isolated installer fixtures pass hash rejection before writes, rollback after
an injected replacement failure, successful replacement, exact restoration,
and nested save/settings preservation. The initial pre-write receipt-schema
failure is preserved separately.

No game, mouse/input control or public upload occurred during installation.
This tester contains the qualified counter storage change only. Experimental
dispatch and arithmetic inlining candidates remain private until qualified;
their results do not block this tester. The visible intro's reported 20 FPS
dip still requires gameplay confirmation.

### Private floating-point inlining experiment: not qualified

An O2 candidate forced eight existing scalar helpers inline in three selected
translated chunks, preserving helper bodies and floating-point flags. The
835-object closure retains 832 objects exactly. Candidate module SHA-256:
`d37de451726b0ca50c63367b2b2b2eb671488a30e1b2c216c067b107227e1816`.
It is inactive and absent from the tester. Its first checkpoint comparison
matches only VI300; CPU and MEM1 differ at VI600 and MEM1 differs thereafter.
Fresh control repeats match all six original checkpoints. Candidate repeats
also differ; no timing claim is made.

The unchanged arithmetic fixture passes 38 million full-state interpreter
comparisons plus 1,536 explicit division edges per build. Its control and
candidate machine code is identical apart from the COFF timestamp, so it does
not exercise the changed production outlining context or the store conversion.
This fixture cannot clear the production mismatch.

Saved states align at VI601/PC `0x80322BC8` with identical complete 3,400-byte
CPU PODs. MEM1 differs by 2,068 bytes, concentrated in lighting state and
display-list/heap copies. The same sun positions and distance values accompany
different visibility inputs, 0.5 versus approximately 0.61. Those inputs
reproduce the changed lighting ratios exactly. Source shows visibility depends
on queued depth peeks and asynchronously mapped renderer snapshots throttled
by a 30 Hz steady clock. This is a possible source of the divergence; it does
not establish an inlining miscompile, rounding error, or causal translated chunk.
The initiating cause remains unknown, and the candidate stays rejected for
promotion. Preserved conclusion:
`build/performance-focus-20261007/hot-inline1/state-diff1/negative-conclusion-v1.json`,
SHA-256 `410fd2f879265fa56c03d9c7e7a447346b5c10db4532f19cf4fe432ad3ff5bf8`.

### Private certified dispatch boundaries: correct in bounded checks, inconsistent timing

The V3 draft avoids repeating fixed-address observer checks after the module
proves a static watch-list miss. It preserves full V2 for ordinary direct/native
calls, and retains dynamic returns, reward checks, overlap/alias state, raw
module aliases, interrupts, diagnostics, CPU/budget/depth guards and gather
drains. Finite-filter hits and complete particle/wake ranges fall back to V2.
Registration validates the exact CPU ABI/size and ordered 1,085-key list;
mismatch or re-registration revokes V3. The normal scanner over the proposed
source overlay produces the exact original watch list. Future dynamic observer
or range changes still require contract review even if their keys are unchanged.

Private host `52478dbd` and module `0386a4b8` replace two host objects and three
module support objects, retaining all 832 other direct module objects. O3 and
ASan fixtures each pass 13,322,973 checks. The native candidate passes all six
complete guest checkpoints and matches complete intro P6 pixels. The old
submitted/rejected counters and display-copy flag remain in one 64-byte line,
at the same within-line offsets; V3 does not accidentally include counter
isolation. Existing GX-count variation is recorded, not treated as command
equality. No broader gameplay correctness or continuous-state claim is made.

Four warm, serialized, unpaced title runs have no capture, state hashing,
sampling, shader compilation, physical input, audio/presentation, or concurrent
owned compiler/game job:

| Order | Build | Wall seconds | Process CPU seconds |
| --- | --- | ---: | ---: |
| A1 | f319/976 control | 37.594 | 54.343750 |
| B1 | Certified V3 | 37.484 | 54.531250 |
| B2 | Certified V3 | 35.688 | 51.984375 |
| A2 | f319/976 control | 37.219 | 54.031250 |

Means improve CPU 1.716%, wall 2.193% and cycles 0.969%, but CPU pairs
disagree (+0.345%/-3.788%). This fails the predeclared requirement that both
pairs improve CPU and wall time, with mean improvements of at least 1% in both.
Decision: **NO_CLEAR_ROUTE_GAIN**, inactive. Do not repeat the unchanged
candidate or promote it based on the mean alone. The accepted counter tester
is independent of this result.

Preserved source draft and rebuild limitations:
`patches/compiler/drafts/host-certified-edges-v3.md` and adjacent `.patch`.
The normal builder may rebuild translated chunks; such a build and a combined
V3/counter build would need separate qualification. Neither was attempted.
Private receipts under `build/performance-focus-20261007/`:

- `certified-checkpoint-comparison1.json`, SHA-256
  `cbf276aed2c5cecda6b2b47d2e10bdb3242c8547fc51cbf5741eb2e78be30100`.
- `certified-capture-observation-v1.json`, SHA-256
  `5793e943beccf2f04de81e0942fc5cc8bb8e0268b9b3628b1cba12af0e2f14eb`.
- `certified-timing-observation-v1.json`, SHA-256
  `958344b73784b52f9d7dd30f7079203b9455600a35e81c7faee930879b91eee7`.

### Player-triggered slowdown capture (F7)

Press **F7 when a slowdown happens**, then keep playing for about five seconds.
A passive on-screen notice confirms the request and completion. The same action
is available under **F1 > Developer > Diagnostics > Mark slowdown (F7)**; it
closes the menu after accepting the mark. A second request during capture is
ignored. F6 save-state and F8 load-state remain unchanged. Existing keyboard
profiles using F7 migrate only those slots to Unbound; other mappings and
controller profiles are preserved.

The normal session log, opened from **F1 > Developer > Open the session logs**,
contains `[slowdown] begin/end` and chronological `[slowdown-sample]` records:
up to fifteen seconds of existing history and five seconds after the request.
Default log location is `%APPDATA%/BlueWake/logs/`; `BLUEWAKE_DATA_DIR` can
redirect it. Early exit, unavailable history or held-game gaps are explicitly
reported rather than filled with invented samples.

Samples reuse the existing approximately once-per-second worker/audio timing
snapshot. F7 does not enable per-GX-call profiling. The idle path adds bounded
history bookkeeping and uses the already-read retrace clock. The report separates
VI retraces (`vi_hz`), game submissions (`game_fps`), displayed frames
(`shown_fps`) and speed relative to 59.94 Hz (`speed_pct`). Interpolation mode,
frame counts, worst observed retrace wall gap, host/worker CPU, draw counts,
shader pipeline creation, audio queue/drop/throttle counts, and stage/room/player
position accompany those rates. Title/no-Link, held, fast-forward, no-present,
counter reset and intervals crossing the mark are flagged. Submit/drawable wait
is host-side timing, not a hardware GPU execution measurement; timing fields can
overlap. A marker identifies a useful interval, not an exclusive bottleneck.

Qualification on the accepted counter-isolation host, with the same translated
module `976184c6`: the focused fake-clock FPS fixture passed; optimized and ASan
runs of the actual offscreen settings UI, virtual controller bindings and controls
menu passed. These cover F7/button/status paths, busy requests, early exits,
no additional worker snapshot queries, and exact old-F7 profile migration.

Three serialized hidden native intro cases passed. The candidate matches all
six complete CPU/MEM1/MEM2/REL checkpoints and all 1,228,335 bytes of the
853x480 P6 capture. A real marker request at VI900 produced thirteen history
samples and six after samples, ending with `partial=0 reason=complete`. Every
marker line reached the actual private asynchronous session file unchanged.
All 449 original player files/nine directories and 46 installed files were
identical before and after. No physical input, visible game, public upload or
performance benchmark occurred. These checks do not replace the user's
visible play test or establish a new performance gain.

Local receipts under `build/slowdown-marker-20261007/`:

- `host1/verification2/result.json`, SHA-256
  `38025d36d0ac43d74447dc56c4c37ae33d30aeae9981a3bc23fd0b61b42275f9`.
- `fps-tests1/work/result.json`, SHA-256
  `757989212c03db5797f9692c17cb7f31932e6b715f698e3c15494da451687183`.
- `ui-tests1/result.json`, SHA-256
  `fb46b460fb73fc38036ece9464bdc4d7c4f0a95f07da2aa01bea44022d1743ec`.
- `checkpoint-comparison1.json`, SHA-256
  `440513087bb25fd64fdf1c22d6ba4c34f125912e3670b87e576a5a55e8ae1680`.
- `native-qualification1.json`, SHA-256
  `f93a59a17a0efcb82c927e43cb1038a5cf8a2803909d6788ffb8406ae1e14f79`.

Host candidate SHA-256:
`469f83a72d80a31b3235cbce5e6f3149922db03add022e6f09ce22cce1f43195`.
Only five host translation units were rebuilt. The accepted aligned counter
archive, translated module and all other link inputs are retained. The original
raw resource-section check failed because the larger host moved seven resource
data RVAs; a separate bounded resource-tree check proves all payloads and every
other resource-section byte exact. Private harness/parser negatives remain
preserved. Tingle rescue wait-skip remains disabled.

### October 8, 2026: first visible F7 slowdown capture

The player's title-intro marker completed at 00:02:39 CDT with fourteen history
samples and six after samples, `partial=0 reason=complete`. Smooth Motion was
off. The drop began before F7: VI743 and VI793 already reported 27.8 and
24.8 FPS. The six after buckets (VI835-1041) report **18.6-21.4 game/display
FPS**, averaging approximately **20.1 FPS / 67% game speed**.

| Recorded metric | Earlier title, VI388-687 | Six after-marker buckets |
| --- | ---: | ---: |
| Game/display FPS | 29.5 | 20.1 |
| Game-thread CPU, percent of one core | 96.6% | 95.2% |
| Game-thread CPU milliseconds/game present | 32.7 | 47.4 |
| GX-worker CPU, percent of one core | 17.3% | 62.8% |
| Reported recorded commands/game present, approximate | 1,121 | 4,180 |

There were no new shader pipelines, audio throttles or dropped-audio increments
in these buckets. GX drain waits total only 1.4-1.7 ms per after bucket;
present totals 2.9-4.0 ms, with submit/drawable wait nested inside. These are
host-side measurements, not hardware GPU execution times. Whole-session audio
starvation/stretch counts exist but cannot be attributed to particular buckets.
The data supports CPU-side game/graphics pressure, with more command work as the
intro becomes busier; it does not isolate one function or prove a GPU bottleneck.

`render_draws` comes from asynchronously published Aurora recorded-command stats,
includes multiple command types, and has separate merged-draw accounting. It is
neither a raw GX-call tally nor an exact current-frame GPU draw count. The first
after bucket crosses the marker; excluding it still yields approximately
20.1 FPS. Earlier/later intro phases are different workloads, so this comparison
is diagnostic correlation, not an A/B performance gain or regression result.

Next profiling target: game-thread and GX-worker work around VI800-1100,
including translated J3D/GX command production and planning before batching.
Particle, wake and cloth ranges are candidates for attribution, not established
causes. Preserve the negative broad-fusion/cache experiments; do not enable them
based on command counts alone.

The raw log remains private and unmodified. Snapshot and reproducible analysis:
`build/slowdown-capture-20261008/session-20261008-000217-43172-snapshot1.log`,
SHA-256 `074dca302d94a45b170e0dfeecade4a2b0a192f095526396eb9d1ff82d1725a1`;
`build/slowdown-capture-20261008/analysis1.json`, SHA-256
`0d1b37681724943f15e93dfb18a891ae98d72b012eb20439a7e6782f368b2071`.
This inspection launched no game, changed no installed binary/settings/save,
and used no desktop input. Tingle rescue wait-skip remains disabled.

### October 8, 2026: inactive GPU vertex-decoding experiment

The follow-up native sampling identified `JPADrawExecStripeCross::exec`
(`0x80263A68..0x802643B0`) as the most frequently observed named geometry
producer in this intro sample. Sampling is attribution evidence, not an exact
exclusive CPU cost or a predicted speedup. The inactive implementation and
results are preserved in
[`patches/experiments/gpu-raw-stripecross/README.md`](../../patches/experiments/gpu-raw-stripecross/README.md).

The GPU path decodes strictly eligible direct big-endian F32 XYZ/ST vertices
from raw FIFO bytes. A separate CPU shortcut retains the original particle FP
operations and guest live-outs, combining the final ten FIFO writes. Particle
simulation and geometry calculations still run on the CPU. Neither path is in
the active runtime recipe or installed tester.

The actual D3D12 pixel fixture passed 13 cases/26 frames without pixel or
validation differences; the planner fixture passed 814 cases. The CPU-tail
fixture passed 80,000 accepted cases and 54 unchanged declines. An actual cached
startup failure was reproduced and fixed in the private host by preventing raw
pipeline configurations from entering the decoded ubershader queue. Earlier
failures and their receipts remain preserved.

The fixed native candidate completed the 1,800-VI intro with the captured image
byte-identical to the installed control, but **normal-setting complete guest
state equality was not established**. Differences also occurred with both new
paths disabled, and one exact old-control repeat varied in MEM1. Disabling both
color and depth EFB peeks produced six complete matching CPU/MEM1/MEM2/REL
checkpoints and the matching full image. This controlled diagnostic suggests
readback-related variation; it does not identify every normal-setting byte
difference or qualify the normal settings.

At the maintainer's request, seven serialized hidden comparisons proceeded
under the current background load. One-second pre-case CPU samples ranged from
29.10% to 52.64%; these are not continuous load measurements. All seven cases
completed with clean renderer counters, actual intended path admissions and no
dynamic shader compilation. The primary ABBA observations averaged 47.016 s
process CPU/31.868 s wall for the installed control and 49.313 s/33.367 s for
both paths enabled. The pairs disagreed, and the strict workload-equality gate
failed because submitted-command/noop counts varied. These descriptive means
are **not a qualified performance regression or improvement**. Separate CPU-only
and GPU-only observations were single runs, so neither establishes a path's gain.

Decision: **experimental, inactive, no reliable speedup established**. Do not
promote this candidate or repeat unchanged timings as proof of improvement.
Normal-setting state differences require attribution before promotion; this
experimental blocker does not block unrelated core gameplay or tester work.
The installed tester, 450 original player files/nine directories, other agents'
three tracked changes, and the original slowdown capture/analysis were all
verified unchanged. Tingle rescue wait-skip remains disabled. No visible game,
physical input, installation, tester ZIP or public upload was performed.

Private receipts under `build/gpu-offload-20261008/native1/`:

- `loaded-cases1.json`: completed cases and preservation, SHA-256
  `fb2176d78942b44a50f0af18bb466309eb5422d10a66248a86055891d69d7eec`.
- `loaded-abba1-failure.json`: strict workload gate remains failed.
- `loaded-descriptive1.json`: descriptive loaded observations, SHA-256
  `cd234038113ac90d88d31b85cfb8da4d279262e8daeead556a9e3357495b9d2c`.
- `checkpoint-comparison1.json`: unresolved normal-setting state failure,
  SHA-256 `0ed5e0f7b9ca3a3c9c2333b1d3c57fa2914900d1ae38822d01e5270a57477772`.
- `checkpoint-control-repeat1.json`: retained control-repeat variation,
  SHA-256 `0487eaa30d1482a89d036f19df44fe9bb61118eb626453ed6b72adfd3f6a1b34`.
- `checkpoint-efb0-comparison1.json`: controlled peek-disabled state/image
  equality, SHA-256
  `a9fffc279d517baaf4855c308844ba8755ee234d142b9be0390237032f30466d`.
