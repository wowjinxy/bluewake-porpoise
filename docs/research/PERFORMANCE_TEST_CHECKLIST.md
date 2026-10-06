# Performance improvements to test

Updated **October 6, 2026, 4:31 p.m. CDT**. Implementation started from
`7bb4aec`; runtime remains `e280c788` with active patches **0152–0162**.
Checking a box means locally qualified and recorded, not merely downloaded.
Donor gains are evidence to investigate, not promised gains on this machine.

## Implementation progress

All 38 numbered candidates remain in scope. Implement dependency groups in
isolated builds; preserve negative results and enable improvements only after
their applicable qualification. Tingle rescue wait-skip stays disabled.

| Item | Current result | Remaining qualification |
| --- | --- | --- |
| 01 | Isolated fusion candidate adds immutable array ownership, complete primitive groups and HUD/state boundaries. Actual O3 and ASan each passed 948 fusion checks; retained shader/plan tests also passed. | Combined renderer GPU/game images, array-copy cost and serialized timing. |
| 07 | Isolated two-identity texture cache: O3 and ASan each passed 420,637 checks. Authored two-palette alternation used 2 uploads instead of 1,000 with identical decoded pixels. | Full renderer integration, real GPU/game images and serialized timing. No gameplay FPS claim. |
| 10–11 | Opt-in dispatch/return transformations in implementation; existing computed-goto entry dispatch is preserved. | Exact state/budget tests and optimized assembly, then matching-profile gameplay A/B. |
| 35 | Implemented full SHA-256 profile snapshots with compiler/tool-specific readability. 13 cache tests and 12 training tests passed. Real Clang/Ninja rebuilt affected C/C++ objects for same-name/size/mtime changed profiles, reused unrelated objects, and handled invalid-profile fallback. | Complete for build correctness; no gameplay FPS claim. |

Item 35 private summary: `build/app-profile-cache-20261006/summary.json`,
SHA-256 `ade5655da120e5f502a9864bb917c89a658792757e17f5ea701625c0db7bfc9e`.
Real compiler receipt: `build/app-profile-clang-ninja-20261006-attempt2/result.json`,
SHA-256 `a187d274734cf3e4c8905e87c11592006e4e555fbb4e9369fe9fcdf55935a133`.
Initial missing-CRT and test-recorder failures are preserved beside the passing
results. Other optimizer changes in the shared builder/workflow are retained.

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

- [ ] **07. [Remember two texture versions at the same address](https://github.com/elliotttate/RecompCore/commit/180f3896f8738cc2975c58486a66d694b3cbad61).**
  Stop alternating palettes from repeatedly evicting one another.
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
  **Blocked until adapted:** the donor PC-store rewrite skips our FIFO drain.
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
  isolated build. There is no current module LTO switch. Preserve the prepared
  C backend, exact FP flags, CPU/RAM options and exported runtime hooks.
  **Test:** change only game-module LTO with matching source, profile and compiler.
  Record build time, peak memory, module size and rendered gameplay. Smaller
  modules alone do not establish a speed improvement.

- [ ] **38. [Train the game profile on more representative gameplay](https://github.com/dougchansan/DolRecomp/commit/0f069ef2edfd610f23b6ced16486d047966f15b2).**
  Game-module PGO is already enabled; test better coverage rather than importing
  it again. This is separate from the app profile in 36.
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
