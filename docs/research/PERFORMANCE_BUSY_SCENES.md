# Busy-scene CPU work, October 7, 2026

The player's 01:01:56–01:04:09 CDT session identifies game-thread pressure:
92 of 121 watched seconds were below target, all classified `game-thread`.
The scene hint was `sea_T`, room 44, with lowest reported speed 48% and no
new pipelines. In 91 of those 92 samples the game thread was at least 90%
busy; graphics waits were small. Direct calls and the existing native
families were enabled. Lowering resolution would not address this measured
bottleneck. The original log remains private; SHA-256
`68930c3f82fc0dec74d8ce00d5a94d733a34966b2a686778ebfb44a35e54aba4`.

`bluewake_game_events_observes` previously scanned all 16 pending calls on
every ordinary optimized-call query. A conservative 256-bit return-address
filter now rejects impossible matches first. Successful new arms add a bit;
clearing every pending slot clears the filter. Individual cancellation may
leave stale bits, and collisions still run the original live-slot predicate.
No observation answer, guest state, dynamic callback or return is cached.

The existing event suite and a direct comparison with the previous predicate
passed under O3 and ASan. Each mode checked 5,646,341 cases, including arbitrary
32-bit returns, unaligned/mirrored addresses, shared and nested returns,
overflow, replay, collisions, saturation, cancellation, reset and callbacks
that change subscriptions. The fixture also verifies CPU/RAM preservation.
The initial fixture-only callback precondition failure remains preserved.

Only `game_events.c` was rebuilt into the ordinary host. Shipped main, controls,
renderer and translated module were retained. The new host is
`a629104974ce26cc74a01deca20f83f37f14dfafd8fa515ffc0b8bbc7abbbfd3`;
the module remains
`976184c642ac87fdf026f181e90c1a1c3875bbbb833aad2d62174ca71dfc0916`.
Actual dependency/link closure, imports, manifest, previous-output preservation
and 93 output-path checks passed.

Four serialized runs used the same copied native CARD, Dragon Roost route,
3,300 retraces, immutable warm-cache seed and affinity mask 65535 applied before
resume. They used hidden rendering, no physical input/audio/presentation,
interpolation, captures, machine-state load/save, sampling or shader compilation.
No owned compiler or second game overlapped measurement.

| Run order | Original host wall / CPU seconds | Filter host wall / CPU seconds |
| --- | --- | --- |
| Original, filter | 86.172 / 119.469 | 73.219 / 104.219 |
| Filter, original | 89.110 / 120.109 | 82.969 / 113.313 |
| Mean | 87.641 / 119.789 | 78.094 / 108.766 |

Mean complete-process route time fell **10.89%**, CPU time **9.20%**, and cycles
**9.22%**. Both adjacent CPU comparisons improved, by 12.76% and 5.66%.
There is meaningful run variation. This is two pairs on one offscreen route,
not displayed FPS, p95 latency, full-RAM parity or full-speed gameplay everywhere.

The original raw receipts and launcher are under
`build/crowded-scene-20261007/`. The timing summary SHA-256 is
`7b4ae29bfec28fe771fbfc7568077da73ee08652191fad6820765cda98c77c09`;
the host compile/link receipt is
`8b8a584427e561a9b64d5f1a00b67019534f8da5d16ed8f182d244ff966bfabf`;
the focused O3/ASan receipt is
`aa542684fd7024804d6f282e9394e10232e9f92c8fe62394b0d940d32534bc12`.

The exact new host/module also passed a separate 3,300-retrace native Dragon
Roost run, with all 22 integration checks passing. Complete captured P6 bytes
and independently parsed loaded checkpoint context match the previous host.
The comparison receipt is `build/crowded-scene-20261007/event-host-comparison-v1.json`,
SHA-256 `f9ecc399967e6b2beb245c73a74294896823c5926ff9047e74a57fe084df887a`.
This capture/save run is excluded from timing. It does not qualify physical
controllers, audible devices, displayed presentation or a complete playthrough.

A separate exact feature-address gate passed semantic checks but regressed
22 of 24 synthetic timing cases. Both public files were restored exactly;
the candidate, assembly, tests and failures remain private under
`build/core-feature-dispatch-20261007/`. Its decision receipt SHA-256 is
`867a473c5a67e969404e7f99510afe1fca9ea7c8e680ba8ccc12f2f39ad24a83`.
No promotion follows. Fusion, other unqualified experiments and Tingle rescue
wait-skip remain disabled; the earlier checklist evidence remains intact.

## Second pass: finite gameplay observer filter (October 7, 2026)

The next actual play session still dipped near 14 game FPS in crowded views.
Its worst reported interval was 13.4 displayed FPS, main-thread utilization
98%, and only 1 ms per second waiting for the GPU. No shader compilation was
recorded. This supports continuing CPU work; it does not establish that one
particular guest routine is responsible.

A separate hidden main-thread native-RIP diagnostic collected 11,380 samples.
Translated-module addresses accounted for 65.5%, host addresses 26.8%, and
host observation gates 16.7%. These are sampled instruction frequencies, not
exact CPU-time attribution or call-stack totals. The sampler's terminal
SuspendThread call failed with error 5; the earlier samples and clean game
exit are preserved as a partial diagnostic. It is excluded from timing. The
matching host PDB and a byte-equivalent module linker map bound the names;
map generation changed only the DLL's COFF timestamp. The ranking receipt is
`build/cpu-hotspots-20261007/current-dragon-native/offline-ranking-v1.json`,
SHA-256 `a830e519f5503352f1846376a63afef92fbe50a41fd9d6e8109882bd13631242`.

The new 32-byte address filter rejects definite misses before six external
observer calls: HUD, health, Quick Items, dialogue, enhancements and autosave.
Their closure contains 48 canonical PCs, including the synthetic autosave
return and wind REL return outside the DOL. Clearing bit 30 only in the filter
conservatively admits mirrors; every hit still runs the existing exact
predicates. GameEvents' arbitrary pending returns, reward observation, feature
hooks, actor search and chassis checks stay outside this filter. The two gated
groups preserve reward's original position and the dynamic predicate order.
There is no cached dynamic answer or watched-address bypass. Actual generated
machine code confirms that misses skip all six calls.

The private oracle includes the real production predicates and dynamic states.
O3 and ASan each passed 13,320,192 comparisons, including all aligned DOL PCs,
mirrors, fixed-PC neighbors, full-width random addresses, pending returns,
configuration changes, call order and unchanged CPU/RAM. Its receipt is
`build/cpu-hotspots-20261007/observer-filter1/actual3/result.json`, SHA-256
`257d92c6544c83d732029f324b9238feb79b0cc7d12f3ac6ab05abd8cb1cef7e`.
Unrelated helper bindings abort if reached; this is a predicate oracle, not a
complete host link. Initial fixture MD-spelling and unused-symbol link failures
remain preserved. The repository regression adaptation uses production-symbol
inventories so changes to existing out-of-DOL constants are also exercised.

Only main.c was replaced in the ordinary O3 host. No game module, SDK library,
PGO profile or ThinLTO mode changed. Selected host SHA-256:
`4cf80f228323dfd377d4d9b6da172688bdbd3ff25465cbf55b944d90fa7a19d4`.
It retains module `976184c6...`; compile/link receipt SHA-256:
`9dc8820f14dfd5955bb0ee37c8a9ec34982411ccb8e5af76ab93f22609c52e74`.
A separate 3,300-retrace native Dragon Roost run passed all 22 checks. Complete
captured P6 bytes and parsed loaded checkpoint context matched the current
control host exactly. Comparison receipt SHA-256:
`25e1e5af4bc9d65505e3e4c2d49ac8d88ed8cd83764f4cd905b1ba0d572b30f2`.

Four serialized warmed Dragon Roost runs used the same copied native CARD,
scripted guest input, module, environment and affinity. They had no sampler,
state capture/save, pipeline compilation, physical input, audio output or
presentation. No owned compiler or other game overlapped a timing run.

| Run order | Host | Wall seconds | Process CPU seconds |
| --- | --- | --- | --- |
| 1 | Current control | 70.469 | 99.797 |
| 2 | Address filter | 63.469 | 92.484 |
| 3 | Address filter | 62.766 | 91.906 |
| 4 | Current control | 69.735 | 99.234 |
| Mean | Control / filter | 70.102 / 63.118 | 99.516 / 92.195 |

Route time fell **9.96%**, process CPU **7.36%**, and cycles **7.01%**.
Both pairs improved: CPU by 7.33% and 7.38%, route time by 9.93% and 9.99%.
This qualifies the filter for the tester. It remains two pairs on one offscreen
route, with no displayed-FPS, p95, full-RAM parity or full-speed whole-game
claim. The timing receipt is
`build/cpu-hotspots-20261007/filter-timing-summary-v1.json`, SHA-256
`82706f19bdd98fff66a11e67cca81396ef2e4956cd96de11c5563e1f876992d8`.

Two other candidates stay inactive. Selective ten-TU host ThinLTO passed the
native image/context comparison, but retained all hot external observer calls
and changed CPU by only -0.81% in one pair. Its CMake proposal and failed
preflight remain private. Gather batching passed its native image/context
comparison; its initial -2.62% CPU/-3.41% route difference is within prior
variation and lacks repeated qualification. Neither optional experiment delays
the filter tester. Their decisions and original evidence are preserved in
`build/cpu-hotspots-20261007/thinlto-decision-v1.json` and
`build/cpu-hotspots-20261007/batch-decision-v1.json`.
Tingle rescue wait-skip remains disabled.

The installed repository CMake/CTest regression also passed in fresh O3 and
ASan builds: 13,320,192 checks and 9,435,217 misses each, using all eight
production predicate sources. Receipt:
`build/cpu-hotspots-20261007/observer-filter1/cmake-retest/attempt3/result.json`,
SHA-256 `f04ea0e5368d25e7916f5190be553aeb6311db9bbf076618b2607b19e7a53064`.
The two earlier sanitizer CRT link failures remain preserved. Windows
execution is qualified; Apple/Linux platform branches have source review.


## Title-intro follow-up (October 7, 2026)

The 12:03 CDT play session still reports 19.4-22.3 displayed FPS in the title
attract sequence (`sea_T`, room 44), with 92-99% game-thread utilization.
GPU/present waits and pipeline compilation do not explain that interval.
A separate hidden native-RIP diagnostic collected 4,691 samples without errors:
73.5% were in the translated module and 25.8% in the host. These samples locate
work; they are not exact function CPU-time attribution. Preserve the diagnostic
and its qualified symbol map in `build/title-intro-20261007/title-current-native1`.

The host chassis guard previously treated every visit to shared restore PC
`0x80328F84` as requiring observation. GroundCross actually observes that return
only with LR `0x80246A04`. The guard now includes that caller context, retaining
the conservative mirror rule and every existing dynamic observation check.
This admits the existing certified Maya animation helper at its unrelated
restore return. It does not bypass watched edges or change native arithmetic.
The game module, SDK libraries, optimization flags and timing rules are unchanged.

The actual old/new predicate oracle passed 50,329 checks in each O3 and ASan
mode, including 7,199 permitted context refinements. It covers helper entries,
mirrors, module aliases, census/service/dirty flags, overlap identities and
interrupt combinations, and verifies unchanged CPU/full authored 32 MiB RAM.
Receipt: `build/cpu-hotspots-20261007/host-precision1/oracle-actual1/result.json`,
SHA-256 `0fc1c350473ad7a1d9e51b60d558a05aa28c12ca5c4d1e397c557ba4f72b4a15`.
This authored-state predicate check alone does not prove native arithmetic.

The selected ordinary O3 host is
`f3198b536a46a344e915ae648f2a903b3b834de26f3d70b83c9f74a07b9105c9`, with the
same `976184c6...` game module. Separate native title captures at VI 1100 match
complete P6 bytes exactly; candidate Maya accepts 163,587 calls versus zero
in the control. The 3,300-VI Dragon Roost run passes all 22 checks, with the
same complete capture and parsed checkpoint context as control. Both record
exactly 70,848 GroundCross returns with no deferred floating-point writes;
candidate Maya accepts 405,647 calls. Neither image/context comparison claims
complete guest RAM parity. Comparison receipts:
`build/title-intro-20261007/title-capture-comparison-v1.json` and
`build/title-intro-20261007/precision-host-comparison-v1.json`.

Four serialized warmed title runs used no guest input/warp, sampler, capture,
state save, pipeline compilation, physical input, audio output or presentation.
They used the same copied native CARD, module, environment and process affinity.
No owned compiler or other game overlapped these timing runs.

| Run order | Host | Wall seconds | Process CPU seconds |
| --- | --- | --- | --- |
| 1 | Control | 35.593 | 52.172 |
| 2 | Caller-context guard | 33.484 | 49.313 |
| 3 | Caller-context guard | 34.344 | 49.625 |
| 4 | Control | 35.234 | 51.328 |
| Mean | Control / candidate | 35.414 / 33.914 | 51.750 / 49.469 |

Route time fell **4.23%**, process CPU **4.41%**, and cycles **4.31%**.
Both pairs improved. This is a small accepted route gain, not a resolution of
the reported 20 FPS dip or proof of full-speed gameplay. Receipt:
`build/title-intro-20261007/title-timing-summary-v1.json`, SHA-256
`47bf868b6af65364437a4a4f0a2f4694cfa6ae0a75e8bb3ff100506c13f25a13`.

The separate perfect edge hash and native GX-writer work remain inactive until
their own current-module correctness and benefit checks pass. Existing negative
results and failed fixture attempts remain preserved. Tingle rescue wait-skip
stays disabled and is not a prerequisite for this tester or gameplay work.


The repository-relative chassis regression also passes actual standalone CMake
O3/ASan builds and CTest: 50,329 checks and 7,199 context refinements per mode.
All eight configure/build/CTest/dependency roles exited zero with preserved
inputs. Receipt: `build/cpu-hotspots-20261007/host-precision1/cmake-retest/attempt1/result.json`,
SHA-256 `159f72eacdcd11848747d85dc28cffe5c77ac90ab7ee4f089d4e8d62f5fb45d4`.
Windows execution is qualified; Apple/Linux branches have source review only.


The full animation oracle against the selected translated module also passed
180,000 cases in each O3 and ASan mode: Basic 37,464 identical / 22,536 unchanged
declines, Softimage 37,460 / 22,540, and Maya 33,533 / 26,467. Maya includes
9,285 compensated-scale acceptances. Every accepted reference follows exactly
one genuine watched-context outer-dispatch continuation; no watch is suppressed
and complete CPU/protected-RAM results match. All 22 tool roles exited zero,
with all 361 observed inputs preserved. Receipt:
`build/title-intro-20261007/native-mtxcalc-attempt4/work/result.json`, SHA-256
`cec816c62d322e2eb0a58affffa15a5d7f715fbb376a2c5230932633097f9844`.
The three earlier fixture/link failures remain preserved. Remaining observers
are authored quiet; this is not all dynamic observer configurations or host
x87/sticky floating-point equivalence. The production animation C is unchanged.
