# Crowded title sequence: native CPU investigation

October 8, 2026. Investigation in progress; no new performance improvement is
claimed by this report.

The quantized paired-single playtest did not resolve the player's slowdown.
The fresh capture with observation tracing disabled still identifies observation
checks and translated game code as CPU optimization candidates. No repeatable
ordinary-play speedup is qualified yet.

**Benchmark environment correction (October 9):** actual command records show
that the main/worker sampling template and the chassis, observer-domain,
paired-single and per-VI timing families inherited
`BLUEWAKE_DIRECT_CALL_TRACE=1`, `BLUEWAKE_GAME_EVENTS_TRACE=1` and
`BLUEWAKE_AUTOSAVE_TRACE=1`. Earlier descriptions of those timings as free of
tracing were incorrect. Their results and failed gates are preserved as
**traced diagnostic comparisons**, and cannot establish ordinary gameplay
speed or that a guarded fast path was exercised. The two-file PGO training and
its first per-VI comparison also used this traced environment. A clean capture
and source-backed fast-path admission review are now complete.

Autosave tracing is enabled by variable presence in the frozen host: setting
its variable to `0` still enables it. A clean launcher must omit that key.
The visible paired-single playtest launcher strips inherited diagnostic
variables and does not set these trace keys. This benchmark error therefore
does not establish the cause of the player's observed slowdown.

### Clean capture and admission review, October 9

The original control host/module completed a fresh 1,800-VI route with direct-call
and game-event tracing set to `0` and the autosave trace key absent. The launcher
also removes inherited diagnostic variables. Its token-based filter preserves
`MAX_RETRACES`; a substring search for `TRACE` would incorrectly remove that
limit. All 24 runtime checks pass, including zero pipeline creations, complete
terminal GX/clock/dispatch records and unchanged inputs. This is a diagnostic
sampling pass, excluded from timing.

The owned 3 ms sampler collected 4,401 samples without errors over VI 771–1465:
3,337 (75.82%) in the translated module and 1,032 (23.45%) in the host. Independent
PDB attribution assigns 439 mutually exclusive innermost samples (9.97% of all
samples) to `host_chassis_requires_full` (163),
`bluewake_game_events_observes` (151) and `host_can_skip_observation` (125).
Including their inlined callees gives an outer union of 568 (12.91%); these
counts overlap and must not be added. The largest translated MAP owners remain
`func_803256E0` (143) and `func_802456E0` (130), the two selected PGO chunks.
Sample frequencies are not exclusive CPU time or a predicted gain, and the old
and new sampling windows differ.

Source review also corrects a possible overinterpretation of the benchmark
error: those three runtime trace flags do **not** globally disable the B/H
readiness predicates or H literal-domain registration. They do change event
observation and diagnostic work. The old shared query counters prove approvals
occurred, but cannot isolate H-specific admissions; the fresh clean capture has
no such query census. Loader registration alone proves neither hits nor savings.

The last visible paired-single playtest independently records the crowded intro
at roughly 39–43 VI/s with the game thread about 92–99% busy. That launcher was
already clean. This supports CPU pressure in the player's actual play mode;
neither hidden throughput nor these host measurements establish GPU execution
time or an individual function's exclusive cost.

## Current baseline and reproduction

The control uses host SHA-256
`7c32e473d137900b684e15afbf40f515e0e1c77a3fbd0fbb959e6cec3efc589a`
and module SHA-256
`976184c642ac87fdf026f181e90c1a1c3875bbbb833aad2d62174ca71dfc0916`.
Selective vertex inputs are on; sparse uniforms, shader interfaces, interpolation
and gather-pipe batching are off. The experimental paired-single module is
excluded from this baseline.

Each owned process boots the ordinary GPU backend into the no-input title
sequence, `sea_T`, room 44, and exits after 1,800 VI retraces. It uses private
copies of CARD/settings/SRAM and the same immutable warm cache. Physical input,
audio output, presentation and machine-state loading are disabled. No existing
game is interrupted. Native diagnostics preserve every attempted case and
assert normal exit, complete worker shutdown, zero rejected/failed submissions,
zero pipeline creation and unchanged source inputs.

Separate main-thread and GX-worker sampling passes cover approximately VI
750-1,450. The sampler operates only on handles belonging to the child created
for that pass. Every successful suspension has its own matching resume.
Sampling passes are excluded from performance comparisons.

## Fresh main-thread findings

There are 4,586 samples: 3,454 in the translated module, 1,097 in the host,
and 35 elsewhere. Sampling completed without errors or partial output.

A later loaded control (`literal-b1`) reproduces the crowded slowdown in
12 one-second performance windows ending at VI 866 through 1,365: 43.4–45.8 VI
retraces per second, roughly 21.7–22.9 game frames per second. The host reports
97–101% game-thread CPU time relative to elapsed wall time in those windows;
the slight overshoot reflects window/CPU accounting granularity. This supports
a game-thread CPU limit in this bounded route. It is not a GPU timing result or
a measurement of exclusive time in an individual function.

| Native function | Samples | Share of all main-thread samples |
| --- | ---: | ---: |
| `host_chassis_requires_full` | 267 | 5.82% |
| `host_can_skip_observation` | 177 | 3.86% |
| `bluewake_game_events_observes` | 143 | 3.12% |
| Those three functions together | 587 | 12.80% |

The current host already contains the previously qualified finite-observer and
pending-return filters. These are residual costs after those improvements.

The dispatch loop first proves an address misses the module's static watch
table, then checks interrupt quietness. The host callback subsequently repeats
the static-intercept lookup and interrupt tests. A proposed separate versioned
contract carries only those facts for that exact call. It must certify that the
module watch set contains every static host intercept, retain dynamic raw REL
aliases and all observer checks, and fall back to the complete predicate for
legacy modules or invalid contracts. Ordinary direct calls and native-family
queries retain their complete checks.

Exact instruction inspection of 35 hot translated chunks finds 441 samples at
CPU bookkeeping accesses: 169 cycle-observation suffix, 130 PC, 118 downcount,
19 deadline-budget and five cycle-budget accesses. This is 9.62% of all
main-thread samples, or 29.90% of the 1,475 sampled instructions in those chunks.
These accesses are not all removable. In particular, deadline and exception
boundaries remain necessary.

The existing `lean_memory.py` transform is off in the control module. It can
defer PC/suffix publication for ordinary RAM accesses in prepaid block copies,
while publishing exact metadata before slow accesses and flushing it before
observers and exits. Its applicability and emitted code require qualification;
source-site counts alone do not establish eliminated native instructions.

Guest-function attribution within these chunks is distributed. The largest
uniquely attributed function has 29 samples, less than 0.64% of all main-thread
samples. A rewrite of one frequently called graphics routine therefore cannot
be assumed to remove the overall bottleneck.

The uninstrumented control reports 217,672,609 direct-call observation queries
over this route, of which 216,610,516 allow continuation. This is a count of
queries, not their exclusive execution time. It reinforces the need to reduce
repeated checks while preserving the comparatively rare observation boundary.

## Isolated candidates and current results

The observation-facts contract has been implemented as an explicit
`BLUEWAKE_OBSERVATION_FACTS=1` experiment. It remains off by default. The private
host rebuild replaces only `main.c`; the module rebuild replaces three support
objects and retains all 813 translated game chunks. Production predicate and
dispatch-loop differential fixtures pass 24,284 checks per optimized,
AddressSanitizer, tracing and census configuration, including rejected
handshakes and removed-key negative tests. This is fixture evidence, not native
game qualification.

Ordinary native comparison does **not** pass complete-state equality: the first
control/candidate pair differs at two of six checkpoints despite identical
captured pixels. A repeat of the unchanged control differs at five checkpoints,
including CPU state at VI 900 and MEM1 at later checkpoints. Baseline variation
does not prove the candidate correct. A fresh 1,134-entry GPU-readback recording
now reproduces all six complete-state checkpoints and exact captured pixels
when replayed in the unchanged build. Replaying the same values in the candidate
also passes all six checkpoints and pixels. Actual physical GPU reads still
execute before diagnostic value substitution; five values differ in the control
replay and ten in the candidate replay. The strict index/VI/PC/address/kind/size
checks and full trace consumption pass. This establishes bounded correctness
with matched external inputs; it does not make the ordinary runs deterministic
or establish a performance gain. Every diagnostic timing remains excluded.

The source regression fixtures also pass with the new optional capability
absent: chassis precision (50,329 checks), real finite observers (13,320,192),
optimized health (10,484), and enhancement, dialogue, gather and song dispatch
fixtures, under optimization and AddressSanitizer. Narrow fixture repairs add
the default-null capability globals, include the real finite filter, and
canonicalize the health CMake root so the combined test project configures.

An exploratory four-run ABBA block also fails the exact terminal workload and
dispatch-count gate. Raw means are lower for the candidate, but neither those
means nor a favorable individual pair establish a speedup. Every case and the
failure are retained. An initial note inferred overlapping offline file I/O
from batch activity; subsequent log timestamps place that check after the
control game shut down and before the candidate started. Both the original note
and its correction are preserved. The failed qualification remains in force.

The existing lean-memory transform has compiled and linked independently for
the 35 profiled chunks, retaining 800 original module objects and the original
matrix archive. Generic and fixed-RAM real-resolver fixtures pass 96,000 cases
each, with a further 96,000 fixed-RAM AddressSanitizer cases and a detected
negative mutation. A narrow emitted RAM-fast region loses one suffix store and
two displayed instructions. Whole-function static store totals increase because
slow/refund paths are duplicated, so they cannot be used to claim a general
instruction reduction or speedup.

The 35-chunk candidate now passes all six complete-state checkpoints and exact
pixels with matched EFB inputs. Its ordinary native route also exits normally,
with clean renderer/pipeline and preserved-input checks, but has uncontrolled
MEM1 differences like the repeated controls. These results are retained.

A separate named controlled-input ABBA experiment removes checkpoint hashing,
frame capture, sampling and extra diagnostics. Both variants use the same
qualified diagnostic host, initial player files, warm caches, physical EFB reads
and 1,134-entry replay trace. All four runs match every terminal GX counter,
guest clock field and dispatch count, and consume the complete identical trace.
External system load remains uncontrolled.

| Controlled-input metric | Control mean | Lean 35-chunk mean | Change |
| --- | ---: | ---: | ---: |
| Process CPU seconds | 45.055 | 45.797 | +1.65% |
| Process wall seconds | 30.438 | 31.414 | +3.21% |
| Process cycles | 162.025 billion | 165.734 billion | +2.29% |

Both CPU/wall pairs regress: +1.14%/+2.88% and +2.16%/+3.53% respectively.
**Reject this candidate for performance; do not enable or expand it on the basis
of the reduced source bookkeeping.** This is a loaded fixed-input throughput
result, not ordinary displayed FPS or proof of a regression on every machine.
The original ordinary gates and every failed/negative run remain unchanged.

A separate register-frame readiness change proves the whole cached-MEM1 span
once when aliases and journals are absent and ownership is unchanged. All
remaining cases retain the per-word resolver. Generic and fixed-RAM optimized
and AddressSanitizer fixtures each pass 2,896,773 equivalent readiness queries;
mutations that remove the alias or fixed-owner guard are detected. This checks
predicate equivalence and nonmutation, not game behavior or performance.

An independent 30-chunk register-frame module retains 805 original objects and
the matrix archive. Its ordinary native intro exits cleanly and has identical
pixels, but differs at three of six complete-state checkpoints. All six states
and pixels match with the same recorded EFB inputs; the ordinary failure is
preserved. This establishes bounded diagnostic parity, not general gameplay
equivalence.

The separate controlled-input ABBA matches all terminal GX counters, guest
clock fields, dispatch counts and full replay consumption. Its results do not
establish a performance improvement:

| Register-frame metric | Control mean | Candidate mean | Change |
| --- | ---: | ---: | ---: |
| Process CPU seconds | 45.555 | 45.633 | +0.17% |
| Process wall seconds | 30.945 | 30.492 | -1.46% |
| Process cycles | 163.864 billion | 163.060 billion | -0.49% |

The first CPU/wall pair changes +1.00%/+0.10%; the second changes -0.65%/-3.01%.
External load remains uncontrolled. **Do not enable this candidate based on
these mixed results.** The source is retained behind
`BLUEWAKE_EXPERIMENTAL_GPR_FRAME_PREFLIGHT=1`; the default keeps the previous
per-word predicate. Public fixtures exercise both the default and opt-in path
with generic and fixed RAM owners.

A separate ordinary native alias-cost diagnostic reports 1,900 aliases and
`overlap_mem1=0` in both host and module at shutdown. This permits the C/D RAM
guards in the final title state and corrects a concern based on older host
comments. It does not count fast-path activations throughout the route. The
module records 122,554,366 resolver calls, of which 120,868,425 are immediately
pruned by bounds; 1,685,798 hit aliases. These instrumented counts identify a
further target for investigation, not resolver time or a prospective speedup.

Actual sampled FIFO helpers contain two out-of-line resolver calls, for the raw
FIFO address and its masked mirror. The resolver's 78 sampled instructions
include 50 prologue/epilogue sites. Existing maintained range bounds can reject
impossible full-span lookups inline, but raw and masked alias precedence, fixed
RAM ownership and the host graphics cache's independent one-byte probe must
remain. A separate private 36-object prototype moves that full-span bounds
rejection ahead of both resolver calls. It retains 799 original module objects
and the matrix archive; the installed host and module are unchanged.

Generic, fixed-RAM and fixed-RAM AddressSanitizer fixtures each compare 7,100
queries using exact CPU, RAM, alias, trace and pointer results. They also check
83 bounds-maintenance assertions. Wrong masked-address and stale-removal-bound
mutations fail the oracle. The ordinary native intro has identical pixels but
matches only one of six complete-state checkpoints. With recorded EFB inputs,
all six complete states and pixels match. Both results are retained.

The independent controlled-input ABBA has identical terminal GX counters,
guest clock fields, dispatch count and full replay consumption:

| Alias-envelope metric | Control mean | Candidate mean | Change |
| --- | ---: | ---: | ---: |
| Process CPU seconds | 44.695 | 44.391 | -0.68% |
| Process wall seconds | 30.202 | 29.969 | -0.77% |
| Process cycles | 159.944 billion | 159.598 billion | -0.22% |

The first CPU/wall pair regresses +0.52%/+0.83%; the second improves
-1.89%/-2.36%. This fails the predefined consistent-pair and one-percent mean
CPU/wall criteria. **No performance gain is accepted and the candidate remains
private and inactive.** The compiler jobs were held during measurement;
external background load remains uncontrolled. These measurements apply to the
fixed-input title experiment, not displayed FPS or general gameplay.

A separate direct-call census finds 16,958 literal readiness queries in 8,479
paired static-call blocks across the 35 original chunks. Actual watch-table
membership and code-domain checks can prove these miss at preparation time.
These are static counts, not runtime coverage. A distinct optional capability
has been qualified in a private 37-TU rebuild: 35 translated chunks, the
direct-call support TU and one new literal-certificate support TU. It retains
the other 797 original objects, two exact observation-facts support objects,
and the matrix archive. All 5,283 immutable literal keys miss the actual module
watch table; registration also verifies all 56 host static keys. Protected
equipment/health/native boundaries, unknown indirects and interpreter
continuations retain full checks. This does not broaden the original
chassis-only contract.

Production fixtures pass 24,284 existing contract checks plus 7,046 literal
checks each under optimization and AddressSanitizer; tracing and census builds
each pass 24,284 plus 7,044 checks. Removed-watch-key and broken-quiet mutants
fail. Actual M/MD closures pass for all 37 rebuilt TUs, and the module import
surface is unchanged with one new optional export.

Both native variants use the same new host and the same capability inputs;
the older observation-facts module lacks the literal export and takes the
original fallback. Ordinary runs have identical pixels but match only one of
six complete states. Matched GPU-readback inputs reproduce all six CPU, MEM1,
MEM2 and ordered-alias checkpoints and pixels. All 1,134 inputs are consumed
with exact context; six physical readbacks differ in the control and twelve in
the candidate before substitution. Ordinary differences remain preserved.

The subsequent independent ABBA matches all terminal GX counters, clock fields,
dispatch counts and replay inputs, but regresses in both pairs:

| Literal-call metric | Observation-facts control mean | Candidate mean | Change |
| --- | ---: | ---: | ---: |
| Process CPU seconds | 44.266 | 44.922 | +1.48% |
| Process wall seconds | 31.704 | 32.290 | +1.85% |
| Process cycles | 159.132 billion | 161.012 billion | +1.18% |

The CPU/wall pairs regress +1.97%/+2.21% and +0.99%/+1.48%. **Reject this
candidate as a speed improvement; keep it private and inactive.** Static
lookup elimination is insufficient evidence of a cheaper executed path. A
registration-lifetime and dynamic-coverage audit followed. This is a loaded
fixed-input title result with owned compiler jobs held, not general gameplay
or displayed-FPS evidence.

Native disassembly rules out an added nested readiness call as the regression's
cause: all 35 selected objects already outlined the old helper, and the new
helpers remain outlined. Inspected hot objects use tail jumps without new
spills. Four repeated guard reads follow from legitimate registration and
revocation ownership, but removing them needs a separate capability contract;
the existing synthetic global-tamper tests remain unchanged.

### Observer-domain specialization: first matched gain

A separate capability certifies the literal keys against the actual host's
event and finite-observer domains: 44 raw event addresses (43 canonical) and
48 unique finite-observer addresses. All 5,283 literal keys miss those domains.
The ten keys in the particle-feature interval retain its complete live
predicate. Global state, census, jumps, CPU/deadline/exception checks, quiet
state, overlap aliases and unknown-address fallbacks remain live. Descriptor
registration checks complete arrays, ABI, source identity and the existing
56 static keys before publishing the callback. Replacement revokes first.

The production observer fixtures pass 3,153,164 checks in each of four profiles
(optimized, AddressSanitizer, developer and edge census), including complete
CPU/counter/decision comparisons, all literal keys across 512 dynamic-state
masks and positive particle-feature witnesses. Descriptor-bypass and omitted
feature-predicate mutants fail. Three comment-only source-hash mutants prove
hash sensitivity only; they do not prove behavioral domain-change rejection.

The candidate changes one literal-state module object and the host's main TU;
the previously qualified literal helpers and translated chunks are unchanged.
Both variants use the same diagnostic host. All six complete guest states,
the full captured P6 image, and all 1,134 replay contexts match the earlier
observation-facts control.

Two separate timing comparisons now pass their fixed-workload
gates, with owned compiler jobs held and no capture, state hashing or sampler:

| Matched title comparison | Process CPU change | Process wall change | Process cycles change |
| --- | ---: | ---: | ---: |
| Original module to chassis-only facts | -1.39% | -2.37% | -1.91% |
| Chassis-only facts to total observer-domain candidate | -6.31% | -6.13% | -5.82% |

The first row's CPU pairs are -0.23% and -2.50%; its wall pairs are -2.09% and
-2.65%. The second row's CPU pairs are -6.15% and -6.46%; its wall pairs are
-6.25% and -6.01%. All terminal GX counters, clock fields and dispatch counts
match within each batch. These are separate common-host batches; do not add
their percentages or infer a direct original-to-total result.

The subsequent direct original-to-total comparison reproduces all six states
and the full image. The ordinary normal host also completes the title route
with clean runtime/renderer checks, zero watched pipeline completions and the
same captured image, without readback substitution. These correctness runs
are excluded from timing and do not establish whole-game behavior.

The reverse-order H-A-A-H timing replication preserves all terminal work but
**fails the paired-gain gate**. Its averages improve CPU -2.68%, wall -4.77%
and cycles -2.95%, while its CPU/wall pairs are -9.93%/-10.94% and
+4.83%/+1.66%. Candidate process CPU is 47.594 seconds in the first run and
53.594 in the second. External load remains uncontrolled; the source of that
variation is not identified. Preserve every sample. The earlier positive
observer result remains unreplicated, with no confirmed total speed gain.
**Keep the candidate private and inactive; do not repeat runs merely to obtain
a pass or promote the favorable average.**

The subsequent preregistered eight-run dispatch-thread experiment also
**fails the gain gate**. It used A-H-H-A then H-A-A-H, the fixed VI 750–1500
window and all five fixed 150-VI segments. The common timer host first matched
all six complete states and full P6 between A/H; its A checkpoints and image
also matched the earlier original host. All eight timing runs retained zero
pipeline creations, exact replay consumption, terminal GX/clock/dispatch and
identical segment work plus absolute start/end cursors.

Primary mean dispatch CPU is 15.902344 seconds for A and 15.941406 for H:
**+0.25% CPU, +0.24% wall and +0.16% thread cycles**. CPU pair changes are
+0.80%, -1.09%, +2.90%, -1.66%; corresponding wall changes are +0.90%, -1.17%,
+3.39%, -2.20%. Whole-process secondary changes are -0.20% CPU and +0.05%
wall. The raw calibration remains retained; unresolved CPU calibration steps
are not interpreted as zero timer cost, and no overhead was subtracted.
These traced results provide no repeatable benefit for H within that experiment,
including its predeclared crowded-scene window. Keep it inactive; no further block is
authorized to rescue this experiment.

### Floating-point helper pilot and pipeline accounting

Forced inlining of four unchanged floating-point helper bodies passes
3,546,112 complete-CPU fixture comparisons across optimized and sanitizer
profiles. The private 42-object module preserves the original import surface.
An ordinary native run passes its runtime gate but has the existing
uncontrolled state variation. Its matched-input run reproduces all six complete
states, pixels and terminal work counts in an offline comparison, but fails
the runtime gate with 34 pipeline completions. **No timing or promotion is
qualified for this candidate.**

The frozen host's counter includes queued cached-pipeline completions after the
first watched retrace. Both runs load the same 242 cached configurations, and
their final cache rows are byte-identical, supporting a startup-overlap
hypothesis. The exact 34 increments have not been identified. A bounded origin
and first-retrace counter diagnostic now records each completion's immutable
request origin and the exact counter snapshots used by the existing watcher.
The original failure and strict zero-pipeline gate remain unchanged.

Fresh original/42-object runs on the same diagnostic host both pass the strict
runtime gate, all six complete state checkpoints and exact captured pixels.
Each records 242 completions before the first watched retrace and zero during
the watched intervals: 218 cache-origin requests and 24 cache-derived uber
requests, with no runtime-origin requests. The bounded accounting closes with
no missing or duplicate records, overflow or pending requests. This supports the startup
overlap hypothesis for this build; it does **not** identify the historical 34
increments retroactively. Recording changes the diagnostic host and adds work,
so these runs are excluded from timing and do not qualify a speed gain.

A compiler-vectorization pilot enables SLP for one vector-heavy profiled chunk,
with every source byte and other compiler flag unchanged. Its emitted code
sections are byte-identical: 713,490 bytes and 129,977 disassembled instructions.
It provides no code-generation improvement for that chunk and is not scheduled
for a speed comparison. This result does not establish what other chunks would
do.

### Quantized paired-single controlled comparison

The isolated four-chunk/CPU quantized paired-single candidate is compared with
the original module on the same original physical-EFB diagnostic host. Both
consume all 1,134 recorded input contexts, pass the zero-pipeline runtime gate
and match all six complete state checkpoints and exact captured pixels.

A separate four-run A/B/B/A comparison passes the strict workload and paired
gain gates: process CPU -3.23%, elapsed time -2.99% and process cycles -3.15%.
The CPU pairs are -3.74%/-2.71%; elapsed-time pairs are -4.38%/-1.53%.
All terminal GX, clock and dispatch counts match exactly.

The single preregistered reversed B/A/A/B replication **fails** the paired-gain
gate while preserving exact workload counts. Its averages are CPU +0.02%,
elapsed time +0.34% and process cycles +0.01%. CPU pairs are -3.56%/+3.70%;
elapsed-time pairs are -4.15%/+5.02%. All four raw cases remain, with no discarded
or replacement samples. No further whole-process batch is planned to obtain a
pass. The initial gain is unreplicated; keep this candidate experimental and
do not promote it on these timings. Background load remains uncontrolled, and
no displayed-FPS, whole-game or ordinary native performance gain is qualified.
Earlier unmatched timing failures remain preserved.

### Game-module compiler profile

The authentic stable module build receipt records `profile: null`, O2 and no
module ThinLTO. The existing builder supports game PGO, and an app profile
exists, but neither establishes that this module was profiled. Checklist item
38 now distinguishes support from the selected build's actual configuration.

A private two-chunk pilot selects the two highest sampled translated owners
(150 and 133 samples). Source bytes, source paths, includes and every non-PGO
compiler flag remain fixed. Its pristine control rebuild matches both original
objects after normalizing only their COFF timestamps. Fresh instrumentation,
actual function/hash/counter coverage and subsequent emitted-code comparison
are required before any speed test. This is a narrow feasibility trial; no
profile-driven gain or broad module rebuild is qualified yet.

The pilot's fresh instrumented module has now completed the same six-state
and exact-P6 comparison against A, with all 1,134 replay entries consumed and
zero pipeline creations. One raw profile came from the owned game process;
the uninstrumented control emitted none. The merged profile matches all 161
instrumented function/hash records and 80,863 counter widths. The two selected
entry counts are 5,389,784 and 19,028,266. Fifty-nine known zero-count functions
remain in the profile. Profile-runtime, merge and inspection diagnostics were
checked even when their tools exited successfully. Training is excluded from
timing, and these counts do not establish a speedup.

The subsequent two-chunk profile-use build also passed its compile and link
gates: exact source/flag and dependency checks, 833 retained objects plus the
matrix archive, the original PE surface, and no profiling runtime. Emitted
code changed substantially. The first chunk's text grew from 709,841 to
1,006,344 bytes while static call sites fell from 9,136 to 8,368; the second
grew from 679,971 to 737,287 bytes with calls falling from 7,888 to 7,859.
These are static code-generation results. The final profile-use module then
matched all six complete states and exact P6 against A on the original
physical-EFB host, with complete replay consumption and zero pipeline
creations. Repeatable speed remains unqualified, and the pilot has not
expanded to more chunks.

A separate, preregistered timing experiment records cumulative wall time,
dispatch-thread CPU time and thread cycles at fixed VI boundaries. It uses one
common host, a preallocated buffer and shutdown-only output. Its primary window
is VI 750 through 1500, with five fixed 150-VI segments and eight counterbalanced
runs (A-H-H-A, then H-A-A-H). State, pixel and workload gates remain mandatory.
This will distinguish steady dispatch cost from startup and renderer-worker
process cost; it cannot remove scheduling noise or turn VI intervals into
displayed FPS. Its A/H result above failed the gain gate. A separate
profile-use comparison preregisters A-P-P-A then P-A-A-P with the same fixed
windows and gates; its observer capabilities are disabled in both arms.

That eight-run profile-use comparison is complete. All workload, state, pixel,
replay and zero-pipeline gates pass, but the paired gain gate fails. Primary
means change by -3.35% dispatch CPU, -3.71% wall and -3.52% thread cycles. CPU
pairs are -0.10%, -10.48%, +0.46%, -2.87%; wall pairs are -1.06%, -11.96%,
+1.43%, -2.84%. Preserve all runs rather than promoting the favorable average.
Both training and these timings used the three trace flags, so this does not
qualify ordinary-play PGO performance. All eight CPU calibration results remain
unresolved at timer resolution; a computed zero overhead is not evidence of
zero or immaterial cost, and no overhead is subtracted. The clean capture still
supports the same two owners for a fresh, separately preserved training profile.

## Fresh GX-worker findings

Of 4,147 worker samples, 1,568 (37.81%) land in
`ZwWaitForAlertByThreadId`. This confirms substantial waiting; it does not
measure exact idle time or establish the worker as the critical path.

Active samples concentrate in generic vertex decoding, derived graphics state,
texture/pixel preparation and submission. A submission-wrapper hotspot is at
stack restores immediately after a per-draw atomic diagnostic counter update.
Sampling skid prevents assigning all those samples to the atomic itself.
Counter ownership and lifecycle must be verified before removing or amortizing
that update. The earlier large per-packet snapshot clearing has already been
fixed; it is not a new candidate.

Normals, lighting and TEV evaluation already run in GPU shaders. The repeated
CPU dispatch and observation checks above cannot be fixed by changing those
shaders.

## Validation boundary and retained evidence

Instruction-pointer frequencies are neither call stacks nor exclusive CPU
time. Guest CFG ownership is incomplete at shared labels. Main and worker
profiles are separate passes; their percentages cannot be added. Offscreen
unpaced throughput is not displayed FPS. Background system load remains
uncontrolled, so comparisons must retain that limitation.

Correctness runs are separate from timing. They compare six complete logical
CPU/MEM1/MEM2/ordered REL-alias checkpoints and exact captured P6 pixels on the
same route. VI checkpoints do not drain GX; terminal shutdown does join the
worker before reporting totals. Any state, command-count or workload difference
must be preserved and investigated, including differences between controls.

Private diagnostic evidence remains under `build/deep-debug-20261008/`:

- `title-native1/{native-rip,offline-ranking1,result}.json`
- `title-worker1/{native-rip,offline-ranking1,result}.json`
- `translated1/{attribution1,contexts1,state-sites1}.json`
- `worker-analysis1/receipt.json` and `HARNESS_OPTIONS.md`
- `native/control-correctness1/` and `native/control-a1/`
- `native/{dispatch-complete-parity1,control-repeatability1,dispatch-abba1}.json`
- `native/dispatch-timing1-overlap-correction.json`
- `observation-facts1/fixture-attempt4/result.json`
- `observation-facts1/native-efb1/{control-input-repeatability1,dispatch-input-parity1}.json`
- `observation-facts1/{legacy1/attempt6,dispatch-compat1/attempt1}/result.json`
- `lean1/fixture-attempt5/result.json` and `fast-ram-codegen1.json`
- `lean1/module-attempt2/result.json`
- `lean1/native-efb1/lean-input-parity1.json`
- `native/lean-complete-parity1.json`
- `controlled-input1/{source-preparation,lean-controlled-abba1,batch1}.json`
- `gpr1/fixture-attempt1/result.json`
- `gpr1/module1/verification1/result.json`
- `gpr1/native-efb2/gpr-input-parity1.json`
- `native/gpr-complete-parity1.json`
- `gpr1/controlled-input2/{gpr-controlled-abba1,batch1}.json`
- `native/alias-cost1/`
- `alias-envelope1/{FEASIBILITY1.md,receipt2.json}`
- `alias-envelope1/fixture-attempt3/result.json`
- `alias-envelope1/module-attempt1/result.json`
- `alias-envelope1/native-efb1/alias-input-parity1.json`
- `native/alias-complete-parity1.json`
- `alias-envelope1/controlled-input1/alias-controlled-abba1.json`
- `literal-facts1/{DESIGN.md,site-census1.json}`
- `literal-facts1/fixture-attempt3/result.json`
- `literal-facts1/module-attempt4/result.json`
- `literal-facts1/native1/literal-ordinary-parity1.json`
- `literal-facts1/native-efb1/literal-input-parity1.json`
- `literal-facts1/controlled-input1/literal-controlled-abba1.json`
- `literal-facts1/diagnostic-design1/{native-path-review1,registration-lifetime-review2}.json`
- `chassis-controlled1/native-efb1/chassis-input-parity1.json`
- `chassis-controlled1/controlled-input1/chassis-controlled-abba1.json`
- `literal-domain1/fixture-attempt2/result.json`
- `literal-domain1/fixture-extra1/verification1.json`
- `literal-domain1/native-efb1/h-input-parity1.json`
- `literal-domain1/controlled-input1/h-controlled-abba1.json`
- `h-total-controlled1/native-efb1/total-input-parity1.json`
- `h-total-controlled1/normal1/h-total-normal1/result.json`
- `h-total-controlled1/controlled-input1/total-controlled-baab1.json`
- `owned-literal1/fixture-attempt1/result.json`
- `fp-inline1/semantics2/result.json`
- `fp-inline1/native-efb2/fp42-replay2/result.json`
- `fp-inline1/offline-diagnosis1/{result,counter-source-binding1}.json`
- `pipeline-origin1/{source2/source-receipt,fixture2/attempt2/result}.json`
- `pipeline-origin1/host1/attempt2/result.json`
- `pipeline-origin1/native2/{strict-pair1,offline-pair1}.json`
- `pipeline-origin1/native2/{origin-a1,origin-g1}/origin-accounting.json`
- `qps-controlled1/native-efb1/qps-input-parity1.json`
- `qps-controlled1/controlled-input1/qps-controlled-abba1.json`
- `qps-controlled1/reverse-baab1/preparation.json`
- `qps-controlled1/reverse-baab1/{batch-result,reverse-baab-analysis}.json`
- `pgo-hot2-1/recipe1/source-preparation.json`
- `pgo-hot2-1/recipe1/instrument/verification3/result.json`
- `pgo-hot2-1/training1/{training-batch1,training-parity1,profile-census1}.json`
- `pgo-hot2-1/recipe1/use/verification2/{result,codegen-comparison}.json`
- `per-vi-protocol1/PROTOCOL1.md`
- `per-vi-protocol1/source1/source-receipt.json`
- `per-vi-protocol1/fixture-attempt4/result.json`
- `per-vi-protocol1/host1/verification5/result.json`
- `per-vi-protocol1/native-preparation3.json`
- `per-vi-protocol1/{correctness-batch1,controlled-batch1}.json`
- `per-vi-protocol1/controlled-input1/per-vi-eight-run1.json`
- `pgo-use-native1/{correctness-batch1,native-efb1/pgo-use-input-parity1}.json`
- `pgo-use-per-vi1/{PROTOCOL1.md,preparation1.json}`
- `pgo-use-per-vi1/{controlled-batch1,independent-audit1}.json`
- `untraced-profile1/{preparation1,offline-ranking-summary1}.json`
- `untraced-profile1/title-untraced-native1/{result,native-rip,offline-ranking1}.json`
- `pipeline-origin1/{untraced-profile-source-review1,untraced-host-attribution1}.json`
- `pipeline-origin1/TRACE_FASTPATH_SOURCE_AUDIT.md`
- `slp1/attempt2/`

Translated game source, compiled game modules and private player data are not
part of this report or the Git change.
