# Fork optimization refresh — October 8, 2026

Three genuinely new mechanisms deserve isolated testing: remove a redundant
direct-call observation lookup, inline quantized paired-single operations, and
skip PCM diagnostic scans when audio logging is off. None has a measured
speedup in our current Wind Waker build. This pass imports or enables no code.

Subsequent implementation and local qualification of the paired-single
candidate are tracked in the [PSQ experiment](QUANTIZED_PSQ_2026-10-08.md).
The census and source findings below remain the original audit snapshot.

Compared with the [October 6 audit](FORK_OPTIMIZATION_AUDIT_2026-10-06.md), using
actual published SHAs and source diffs rather than repository update dates.
Local baseline: `main` at `0dae32f847b510fc35f3d13ba44390f6d108ea51`;
RecompCore base `e280c788dadabd18b085af0085f1558fc9ff5ecc`, active patches
0152–0165. The uniform-bank/interface experiments in that commit remain inactive.

## Coverage

Fresh public GitHub queries began at 22:15 UTC on October 8. Fork and branch
pagination reached its terminal page; transient API failures and successful
retries remain in the local receipts.

| Network | Listed repositories | Branch refs | PR coverage |
| --- | ---: | ---: | --- |
| BlueWake / Wind-Waker-Recomp | 44 | 361 | 41 unmerged heads, fetched |
| RecompCore / Aurora | 142 | 1,272 | 393 all-state PR records |
| DolRecomp / ModernGekko / libPorpoise | 86 | 191 | 53 all-state PR records |
| WiiCompiled | 99 | 424 | 156 all-state PR records |
| Separate WWHD related-port review | 35 | 103 | Branch review; no separate PR census |

There are **371 distinct core-network repositories and 2,248 branch refs**;
including the related WWHD review gives 406 repositories and 2,351 refs.
`odcwjvpdmb/aurora` still returns 404, so its branches/PRs are unavailable.
WWHD's reported fork count differs from the public listing. These totals describe
visible published refs, not private/deleted forks or an atomic GitHub snapshot.

The app networks have 19 newly seen unique heads and 134 newly reachable commit
diffs. Runtime has 19 new/moved branch refs; newly enumerated old PR-only heads
are additional coverage, not automatically new optimizations. Unchanged SHAs
reuse the prior pinned audit; older-dated newly exposed work is classified below.

## Recommended experiments

### 1. Split dynamic observation checks from address interception

[jkoehler11/bluewake `a094ebff`](https://github.com/jkoehler11/bluewake/commit/a094ebff32092826979872dfa8c788435c3dff51),
October 7; merged upstream as `20fae5c4ba7d5196360e9a5df72d52a02f4906e1`.
When the translated module already proves a direct-call target is unwatched,
the host can retain dynamic checks while avoiding a second intercept-table lookup.

Our `runtime/host/src/main.c:2314` still routes the direct-call query through the
full `host_can_skip_observation` predicate. This is a relevant target: the local
[October 7 profile](PERFORMANCE_TEST_CHECKLIST.md#october-7-2026-evening-fresh-cpu-profiles-and-single-probe-edge-lookup)
sampled chassis/observation/game-event predicates 619 times out of 4,634 game-thread
samples. That is diagnostic sample frequency, not a predicted speedup.

The donor reports a Steam Deck sea/44 profile with host-edge share 10.2% → 7.5%
and chassis share 6.03% → 0.95%. These are the author's profiling shares, without
raw traces in this audit; they are not whole-frame timing or our FPS results.
Our prior perfect-hash experiment is a different change and remains inconclusive.

**Adaptation/test:** preserve game events, finite HUD/health, rewards, quick items,
dialogue, enhancement/autosave observers, GroundCross caller context, interrupts,
tracing, mirrors and raw module aliases. Prove the current module's unwatched
contract covers every skipped intercept and survives reset/module replacement.
Compare the actual old/new predicates and callback order before timing matched
title and crowded-scene runs.

### 2. Inline quantized paired-single loads/stores

[InfraredGodYT/RecompCore `09ad2a16`](https://github.com/InfraredGodYT/RecompCore/commit/09ad2a1609028a3870790d540ba0e677a94d15c4)
and [`3d969ed1`](https://github.com/InfraredGodYT/RecompCore/commit/3d969ed1202cc60f950c0ffb878456649431a996),
October 7: inline the integer quantization formats, use exact power-of-two scale
tables instead of `ldexp`/`ldexpf`, and inline NaN/infinity classification.

Our `scripts/generate_composite.py:519` only inlines unquantized type 0;
other formats still call the runtime. Active `GXRuntime/core/cpu.c:493` and
`:517` still use the scale helpers. Integrate through our generator/runtime
contract, avoiding duplicate helper definitions. Inlining requires rebuilding
the translated module; replacing the host alone cannot deliver that benefit.

**Adaptation/test:** compare exact FP bits, all quantized types/scales, signed
limits, clamp/NaN behavior, W/indexed forms, HID2/LSQE and exception fallbacks.
Preserve ordered memory callbacks: the donor loads both lanes before writing
the destination FPR, while our canonical helper writes lane 0 before reading
lane 1. RAM arithmetic checks do not establish equivalent MMIO callback behavior.
Keep cycle/deadline/suffix and observer boundaries exact. Count actual quantized
hits before a matched full-module A/B comparison.

The donor's equivalence and intro-speed claims concern Sonic Heroes. They provide
no Wind Waker or local-machine performance result.

### 3. Gate audio-only diagnostic scans

[CypherNoodle/Wind-Waker-Recomp `134039ac`](https://github.com/CypherNoodle/Wind-Waker-Recomp/commit/134039ac31655c709048e8ff93c21b49755f7be5),
October 7: only calculate PCM nonzero counts, peaks and FNV hashes when
`g_audio_queue_log` is enabled. Our active prepared SDK's
`GXRuntime/backends/aurora/aurora_audio.cpp:166–175` still walks every audio chunk
unconditionally; these fields feed diagnostic logs.

**Adaptation/test:** take only the diagnostic guard, preserving current capture,
recovery, stream lifetime, buffering, throttling and sample delivery. Compare
delivered PCM byte-for-byte with logging on/off and retain identical enabled-log
statistics. Measure scan count and guest-thread CPU separately from frame rate.
The donor has no measured gameplay gain. Switch ring-buffer/memcpy changes are
separate platform work. Expected scope is smaller than the first two candidates.

## Other findings and exclusions

| Source | Finding | Decision for this build |
| --- | --- | --- |
| [ModernGekko `944efaa9`](https://github.com/InfraredGodYT/ModernGekko/commit/944efaa94830d2f491d756bef055a9ad3a70ae61) | Installs mod host-call hooks only when loaded mods intercept guest code. | Supports auditing callback admission, but our enhancements need hooks even without external mods. Combined boot hashing/cache claims are not isolated FPS results. |
| [BlueWake `34aabeba`](https://github.com/chrissotraidis/bluewake/commit/34aabebad9f8a15b9e4557fd63fe626902f1db6d), [measurement `dfef1ea0`](https://github.com/chrissotraidis/bluewake/commit/dfef1ea095d9329f91ac9fea8c2a4d91117dcf7f) | New `--no-cold` additionally disables LLVM PGSO; it differs from our existing `--no-tiered`. Donor bird route: 26.27 → 26.86 s with equal blocks/checkpoints. | Negative for that Linux/Ryzen route; do not promote as a speedup. |
| [DolRecomp `7ed1f94d`](https://github.com/thebardockgames/DolRecomp/commit/7ed1f94d1686afd1128c5bf26af85321e4f5bfb5) | Newly exposed October 4 function-region/direct-call and MSR-check-hoisting implementation. | Architecture reference; its budget guards lack our deadline/precharge/suffix protocol. No measured gain; not a translator replacement. |
| [libPorpoise `90e1eb5e`](https://github.com/cybervisi0n/libPorpoise/commit/90e1eb5ea20e6886abca8458a8b9e127b66b9a00) | OpenGL konst-uniform location fix. | Outside our imported matrix slice; all six imported matrix/header/license blobs still match our pin. No new matrix optimization. |
| [Aurora `f569c318`](https://github.com/999sian/aurora/commit/f569c318b71ed9b9a2d6d498eec3a844347b7d0d) | Public enqueue wrapper around the existing render-worker queue. | The underlying queue already exists here; no new scheduling engine or demonstrated gain. Android fragment-density work is hardware-specific. |
| [Aurora Vita `6acc6a88`](https://github.com/robin994/aurora-vita/commit/6acc6a889d0bec1fb206b1c73cb693a434c132df) | Gated backwards TEV color/alpha liveness analysis and texture-sample sharing, October 8. | Lower-priority shader experiment. Our WGSL driver may already remove dead arithmetic; any sample-sharing proof must include indirect coordinates, bias, wrap, swaps and derivatives. No Windows gain demonstrated. |
| [WiiCompiled `897899a5`](https://github.com/mitch030504/Wiicompiled_VR_Frame/commit/897899a5f94ffa61eab6c28bb2377e9578b8dd66), [`d577d7e3`](https://github.com/patchzyy/Wiicompiled/commit/d577d7e352d03c1ec592b2bb80064cc9105f7576) | Older PR-only sleeping staging waits and shared return dispatch. | Equivalent sleep-based waiting and shared return-switch structure already exist locally. |
| [WiiCompiled `c305fba7`](https://github.com/Minithena/Wiicompiled/commit/c305fba7c596ecff6cbfd8c3b98a46bd24d7f684) | Reverts browser pacing/prefetch work. | Exclude; current canonical tip is version-only. |
| [WWHD `134617ee`](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp/commit/134617ee4a17ba115cd2630eeda93d18c73e8f58) | Continues resolved draw state until a relevant register-state group changes. | Useful validation pattern; first identify residual repeated work in our existing DrawPlan/state caches. Different GX2/Latte runtime. |
| [WWHD `8d06651d`](https://github.com/depende3000/SwitchWakerHDRecomp/commit/8d06651d2edd9685b8394c7380e569954fda895f) | Interprocedural single-precision facts remove redundant rounding. | A new cross-function implementation of checklist item 13, requiring REL/callback/yield/FP-state analysis; no local gain. |
| [WWHD `95a3e7b8`](https://github.com/rhemfur/ZeldaWWHDRecomp/commit/95a3e7b8ceda6730aad194bcc68c500479977e8c) | Identical generated shader text plus binding metadata aliases shader/pipeline objects. | Profile redundant WGSL identities first; donor reports a memory benefit on Galaxy S25, not our frame rate. |

Switch affinity, SD logging/cache pragmas, Dolphin dual-core settings, LLVM-only
ADDO lowering and VR-only scissor fixes do not establish a compatible Windows
speedup. WWHD's BC compute decoder is a fallback for GPUs missing native BC;
its documentation establishes no repeatable throughput gain. Its surface interval
index was explicitly skipped after profiling, and its interpolation matrix change
fixes a race with unchanged CPU time.

Existing primitive-fusion, geometry-cache, two-stage-GX and particle/sea replay
negative results remain intact. Previously reviewed dispatch slots, return ranges,
normal-f32 widening, native sets and PGO work are not relabeled new discoveries.
Tingle rescue wait-skip remains disabled and is not a prerequisite for any item.

## Receipts and validation

Detailed source diffs, current-code comparisons, all ref inventories, API failures
and retries are preserved under `build/fork-optimization-audit-20261008/`:
`apps/findings.{md,json}`, `runtime/findings.{md,json}`,
`compiler/{FINDINGS.md,findings.json}`, `wiicompiled/{FINDINGS.md,findings.json}`,
and `root/{related-census1.json,related-reviews.json,related-findings.md}`.
The compact adjacent JSON receipt pins the inventories and selected source SHAs.

This was source research only. No game/build/benchmark was started or stopped,
no installed binary or SDK was replaced, and concurrent source changes were
preserved. No runtime performance result is claimed for this pass.
