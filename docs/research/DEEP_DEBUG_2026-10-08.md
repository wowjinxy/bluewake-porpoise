# Crowded title sequence: native CPU investigation

October 8, 2026. Investigation in progress; no new performance improvement is
claimed by this report.

The quantized paired-single playtest did not resolve the player's slowdown.
Fresh native instruction-pointer sampling now identifies two broad sources of
game-thread overhead: repeated observation checks and publication of translated
CPU bookkeeping. This is a more useful starting point than optimizing the
most frequently called GX function without measuring its cost.

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
is being prepared so this proof can remove the repeated host lookup without
silently changing the chassis-only contract. Protected equipment/health/native
boundaries, unknown indirects and interpreter continuations retain full checks.

A compiler-vectorization pilot enables SLP for one vector-heavy profiled chunk,
with every source byte and other compiler flag unchanged. Its emitted code
sections are byte-identical: 713,490 bytes and 129,977 disassembled instructions.
It provides no code-generation improvement for that chunk and is not scheduled
for a speed comparison. This result does not establish what other chunks would
do.

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
- `slp1/attempt2/`

Translated game source, compiled game modules and private player data are not
part of this report or the Git change.
