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

Fresh offline native-CFG attribution covers the clean capture's 35 hottest
translated chunks: 1,477 samples, of which 1,234 have unique guest-routine
ownership and 243 remain shared or unknown. Exact direct native memory operands
identify 457 samples on instructions accessing CPU bookkeeping: downcount 156,
cycle-observation suffix 154, PC 118, deadline budget 27 and cycle budget two.
That is 10.38% of all samples. These counts omit register-indirect accesses;
they do not establish that the operations can safely be removed. The largest
recovered guest owner has only 39 samples, supporting investigation of shared
translation overhead rather than a single dominant guest routine.

Source review also corrects a possible overinterpretation of the benchmark
error: those three runtime trace flags do **not** globally disable the B/H
readiness predicates or H literal-domain registration. They do change event
observation and diagnostic work. The old shared query counters prove approvals
occurred, but cannot isolate H-specific admissions; the fresh clean capture has
no such query census. Loader registration alone proves neither hits nor savings.

A later diagnostic uses a copy of the player's actual C2 tester, including its
settings, audio, FIFO presentation and 2560x1440 renderer at scale two. Physical
pad and mouse-camera input are disabled for the owned no-input intro run. It
reproduces roughly **17–20 FPS** in the crowded interval. The sampler records
4,852 RIP samples without errors: 3,555 (73.27%) in the exact C2 translated module
and 1,106 (22.79%) in the host. All 30 route/input checks pass; original tester
data and caches remain unchanged. These are diagnostic sample frequencies,
not exclusive CPU times or a speed comparison. The offline worker processes
all 485 immutable stack snapshots. C2 has no qualified module MAP/PDB, so its
frame labels retain raw RVAs; only the exact host PDB supplies function names.
No original-A module symbols or leaf rules are borrowed for C2.

A later passive relink qualifies a C2 MAP: all 835 object inputs and their order,
15 physical library providers and 852 reproduction members match the accepted
five-TU C2 build. The entire DLL matches C2 except four COFF timestamp bytes.
The first verifier rejected a warning already present in the original link;
that negative is retained, and verification of the same outputs accepts only
the exact original 129-byte warning. No game code from the relink is executed.
Exact-C2 RIP naming identifies `func_802456E0` (148),
`bw_direct_call_ready` (137), `func_803256E0` (113),
`func_802D56E0` (110) and `bw_fp_fmuls` (97) among the largest module owners.
These are sampled instruction frequencies, not call counts or exclusive times.

The last visible paired-single playtest independently records the crowded intro
at roughly 39–43 VI/s with the game thread about 92–99% busy. That launcher was
already clean. This supports CPU pressure in the player's actual play mode;
neither hidden throughput nor these host measurements establish GPU execution
time or an individual function's exclusive cost.

### Exact source-line attribution and local-state pilot, October 9

Two passive debug-line recompiles now map the existing visible-C2 capture to
the actual original `0145` and `0201` sources. Before using either mapping, the
verifier requires every executable-section byte and every named relocation to
match the exact object retained by C2. Both pass: 709,841 executable bytes and
50,147 relocations for `0145`; 679,971 bytes and 47,975 relocations for `0201`.
Only debug sections are added. These objects are not linked into a game or run.

| Qualified chunk | Sampled instruction addresses | Samples | Direct bookkeeping-operand samples |
| --- | ---: | ---: | ---: |
| `0145`, `func_802456E0` | 139 | 148 | 32 |
| `0201`, `func_803256E0` | 101 | 113 | 26 |

Every sampled address lands on an actual decoded instruction boundary. The
direct-operand counts include downcount, PC, observation suffix and deadline
fields; they omit accesses through registers. Optimized source-line intervals
assign 75 and 41 samples to cycle bookkeeping respectively, but those intervals
also contain inlined work. They must not be reported as direct metadata accesses,
exclusive CPU cost or removable time. Six `0145` samples have compiler line zero
and remain unattributed.

A private local-state prototype covers one 27-instruction, callback-free block
in `GXProject`. It keeps guest register values local, delays only the superseded
floating-point result classification and publishes committed state before every
slow exit. Optimized and AddressSanitizer fixtures each pass 1,672 differential
cases over complete CPU state, all 32 MiB of RAM, floating-point environment and
ordered callbacks. Forced exits, alias paths, exceptions, reservations, journals,
deadline edges and three deliberately broken variants are included.

The first production-policy compile exposes a drawback: it adds 9,712 executable
bytes while retaining the original fallback, and the new lane still calls 17
pure conversion or rounding helpers. A narrow private inline-helper successor
passes both fixture profiles and removes those calls, at the cost of 12,528
additional executable bytes including the preserved fallback and cold exits.
An offline concrete interpretation of the actual relocated COFF follows 645
native instructions instead of 1,421 for one ordinary-data case, with matching
complete CPU state and output bytes. Calls fall from 19 to zero; CPU FPR reads
fall from 36 to four, and PC writes from 29 to three. This is a code-path result,
not hardware execution or timing; the native differential fixtures remain the
semantic authority. This block has **zero samples in the visible capture**.
No route speedup, whole-module change or default enablement follows.

A separate passive census identifies 84 original fast-copy source intervals
containing 115 samples in those two chunks. Only four intervals contain any
floating-point result-writing opcode, with one sample each. The next local-state
pilot therefore targets the sampled integer/memory block `0201:164`, rather
than expanding the unobserved floating-point block. A text-only scan across the
35 historical hot chunks finds 7,298 possible integer prefixes; neither that
count nor samples in a parent interval certify any prefix's safety or runtime
admission.

The adjacent native-call prototype also passes its differential tests, but its
first fused helper retains a larger stack frame across the native body. A
tail-call successor passes all four profiles and releases its frame before
jumping to the body. The enumerated admitted path adds ten wrapper instructions
while removing one complete duplicated host predicate invocation. A source-only
342-site recipe is preserved; it has not been compiled or timed.

The borrowed live-observer view passes all four fixture configurations at one
literal target. Its frame-free successor removes calls and six conditional
branches on two enumerated successful paths, but adds 16 or 24 explicit reads
through borrowed field pointers. A separate source-only draft relocates the
actual private live objects into compact typed storage, preserving existing
writers and initializers. Its serialization aliases and restored values still
need compiled validation. Neither observer prototype has a qualified game
timing result.

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

### Observer-domain specialization: earlier traced comparisons

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

Two separate traced timing comparisons passed their original fixed-workload
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

Fresh clean training has now passed the independent six-state/full-P6 comparison
with all 1,134 replay entries and zero pipeline creations. The actual owned
process produced a new raw profile in a new directory; matching tools merged
and recognized every one of the 161 function hashes and 80,863 counter widths.
Its derived entry counts and zero set happen to match the traced predecessor.
Direct byte comparisons also prove that both the raw and merged profile are
identical to that predecessor. The existing profile-use binary can therefore be
reused with an explicit provenance bridge; no redundant rebuild is claimed or
required. This qualifies the training input, **not** the earlier traced timing
result or an ordinary-play speedup.

The separate clean eight-run comparison is also **unqualified**. Fresh A/P
correctness on the common timer host passes six complete states and full P6.
Seven timed runs pass every runtime gate; the final P4 fails only the strict
zero-pipeline gate with 13 terminal pipeline completions. Their request origins
are unknown. Earlier cached-startup diagnoses cannot identify these 13
retroactively. The original failed batch remains intact, with no replacement
run or relaxed gate.

Independent decoding still finds identical primary/five-segment work and both
endpoint cursors in all eight records. Descriptive means change by -3.19% CPU,
-3.29% wall and -3.08% cycles, but one pair regresses CPU +1.27% and wall +1.62%.
The other CPU pairs are -10.38%, -0.94%, -2.25%. Thus the paired gain rule fails
independently of the pipeline failure. These failed-gate means do not qualify a
speedup; unresolved CPU calibration remains unresolved, with no subtraction.

### Clean forced-inline comparison

The separately preregistered clean A/G42 comparison has also finished. Fresh
correctness on the same timer host passes six complete state checkpoints and
full P6 pixels. All eight timed runs pass the runtime gates, including zero
pipeline completions, exact 1,134-entry replay consumption, identical primary
and five-segment work, absolute endpoint cursors and terminal GX/clock/dispatch
totals. Both runtime trace flags are zero and the presence-based autosave trace
key is absent.

Independent decoding reproduces mean changes of -0.37% dispatch-thread CPU,
-0.63% wall and -0.54% cycles. Three pairs regress CPU (+1.79%, +1.88%, +3.60%)
and wall (+1.51%, +1.33%, +3.18%); the fourth improves CPU -7.55% and wall
-7.37%. The paired-sign rule and the one-percent mean rule both fail. This
candidate remains inactive, with no additional block or changed window to
rescue it. Zero CPU calibration spans remain unresolved. Removing static
helper calls did not produce a repeatable gain in this clean workload.

### Register-coalescing build-cost tradeoff

A new single-chunk pilot changes only the Clang register-coalescing cap from
10 to its version-19.1.5 default of 256. The selected 0145 source, effective O2,
other flags and all 48 actual dependencies remain fixed. The pristine control
already matches the original object except its COFF timestamp, and the new
bounded compile passes with preserved inputs. The cap controls compile-time
work rather than promising better code. [LLVM 19.1.5 source](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.5/llvm/lib/CodeGen/RegisterCoalescer.cpp#L103-L108).

Actual whole-object code inspection finds 976 fewer code bytes, 194 fewer
instructions and 18 fewer call sites, but **no change** in register-copy or
stack-memory operand counts. This does not support the proposed copy/spill
reduction for that chunk. Static stack references are not automatically spills,
and no module link, runtime comparison or speed qualification follows.

Independent COFF review locates all changes in `func_802456E0`, with helper
definitions unchanged. Nine direct-call readiness and nine GPR readiness static
sites disappear, along with nine PC store sites; downcount and suffix store
counts remain fixed. Possible native block sharing does not prove fewer dynamic
readiness calls. The exact 48 dependency inputs and drained compile job pass
review, so this bounded experiment is closed without a larger build.

### Effective O3 one-chunk pilot

The exact original A0145 chunk was also compiled with only its final effective
`-O2` changed to `-O3`. The earlier CMake `-O3`, cap-10 coalescer, disabled SLP,
strict contraction policy, fixed CPU/MEM1, source, include/define order and all
48 dependencies remain unchanged. Its retained pristine control is byte exact
to original COFF except the timestamp. Actual compiler M/MD match and all
inputs remain preserved; there was one owned compile and no link/game run.

Actual text grows 10,688 bytes (1.51%) and 2,649 instructions. Static calls
increase 98, register-copy sites increase one, and stack operand sites remain
103. PC-write sites increase 34, downcount-write sites decrease one, and suffix
writes are unchanged. Defined/undefined function surfaces remain fixed. These
are complete-chunk static counts, including cold paths, rather than dynamic
cost or proven spills. The pilot does not show the intended reduction in CPU
state traffic; preserve it without expanding to a broad O3 build or claiming
a semantic/runtime/performance result.

## Event-filter and register-save experiments

The event observer now has a private conservative negative-filter candidate.
It combines immutable static-entry hash buckets with the existing live pending
return buckets; a hit still executes the original exact predicates. It caches
no observation verdict. Full original/candidate translation-unit fixtures pass
37,758 comparisons in each optimized and sanitizer profile, with missing-entry
and missing-armed-return mutants detected. The actual host link replaces only
that observer object and retains the common timer main and other 169 physical
link inputs. Emitted code changes only the observer and grows 32 padded bytes.

Fresh A/event correctness passes six complete states and full P6. All eight
clean timed runs pass zero-pipeline, replay, route and terminal gates. Raw-log
independent decoding matches all primary/five-segment work and both endpoint
cursors. Descriptive mean changes are -7.59% CPU, -8.25% wall and -7.67% cycles.
CPU pairs are -2.89%, -17.65%, +0.44%, -9.26%; wall pairs are all negative.
The preregistered all-pairs CPU rule fails, so this remains an inconclusive
experiment, without rescue runs or promotion. Background load is uncontrolled;
all zero CPU calibration spans remain unresolved.

A separate register-save candidate splits a fresh sufficient RAM-span proof
from the verbatim old per-word predicate. It additionally checks the adopted
fixed RAM owner and compile-time size so accepted accesses retain the original
resolver/census effects. Four actual-resolver profiles (generic/fixed, optimized
and sanitizer) each pass 84,342 queries, with whole CPU/registry/census equality
and three meaningful alias/owner/journal mutants detected.

The one-chunk original A0145/cap-10 compile passes exact 48-dependency M/MD and
unchanged-source/flags checks. Its actual common helper is 176 bytes and 40
instructions, with no saved-register or stack frame, and tail-jumps to the
retained 464-byte cold predicate on uncertain cases. The caller is unchanged;
whole-object call/stack/metadata sites remain fixed because the cold path is
retained. The subsequent independent module replaces exactly 30 header
consumers from the clean profile: 29 fresh actual M/MD compiles and one
identical-input pilot reuse, with 805 original direct objects and the original
15 library inputs retained. Sources, effective O2/cap-10/strict-FP flags,
physical link inputs and the PE surface pass their checks. Fresh same-host
title replay comparisons match all six complete states and full P6 pixels,
consume all 1,134 EFB contexts, and finish with zero pipeline creations.
The complete fixed-order eight-run comparison now fails both speed gates.
Descriptive means increase CPU 0.4918%, wall 0.5473% and thread cycles 0.4837%.
The first three pairs reduce CPU/wall by 1.21/1.66%, 0.17/0.37% and
1.93/1.45%; the fourth increases them by 5.15/5.53%. Every runtime gate passes,
including zero terminal pipeline creations, unchanged terminal GX/clock/dispatch,
and exact primary/five-segment work and endpoint cursors. Independent raw-log
reparsing confirms the retained reports. All three agents acknowledged a drained
hold before timing and resumed after the batch terminated. Background load and
CPU timer resolution remain limitations; no overhead is subtracted.
The failed block is retained without extra runs or a changed window. This
candidate stays inactive, separate from the earlier traced selection and every
other candidate change.

## Clean paired-single comparison and startup readiness

The existing standalone C2 paired-single module passed a fresh same-host clean
six-checkpoint/full-P6 comparison. Its exact PE admission permits only the
source-proven removal of the ldexp import; all other import order/hints,
exports, ABI and resource payloads match the original. The complete 835-object
symbol census finds no remaining ldexp reference, and the two runtime scaling
expressions and five-profile fixtures remain bound to the original build.

The clean fixed-order timing attempt stopped after A1, C1 and C2. The first two
runs pass; the third exits normally with all route/state/input-preservation
gates passing except seven terminal pipeline creations. No eight-run result,
means or accepted gain is available, and the block is not retried. Independent
raw timer decoding confirms equal primary/five-segment work and endpoint
cursors across the three completed runs. Four brief source/receipt reads also
occurred after a timing-hold acknowledgement; the last two absolute times and
their overlap with measurement are unknown. Their durations and incorrect
inferred message timestamp are preserved separately.

Source review found a concrete startup-readiness gap: dequeuing is earlier than
pipeline creation, and pending-set removal is earlier than the original
notification/counter/prune/log epilogue. Either empty-container test alone can
report completion too soon. A private opt-in completion-fence draft tracks
workers through that epilogue and waits for closed loading, empty queues and
pending set, and no workers in flight. The second source draft also rejects a
previous unclosed nonthreaded/WebGPU lifetime through a shared nine-condition
admission helper. Threaded positive, lifecycle/error, all 512 admission
combinations and four behavioral negative fixtures use the actual production
helper. Optimized and ASan profiles each pass 534 assertions, and all four
mutants fail behaviorally. All 24 children drain and inputs remain unchanged.
Earlier CRT/STL link setup failures remain preserved; the final fixture pins
the matching annotation support library and retains sanitizer annotations.
The generic implementation is saved as the unapplied
`patches/recompcore/drafts/pipeline-preload-fence.patch`; its current-tree
application check passes. No production host build or game run is qualified yet.
The failed C2 log reports
242 cached completions after early VI
activity, but does not identify the seven creations' origins. This draft does
not yet explain or fix the steady crowded-scene CPU slowdown, guarantee future
cache hits, or certify successful cache persistence.

## Adjacent native approval

A source census of the exact clean 35-chunk selection finds 342 matrix native
calls immediately inside a successful direct-readiness edge. Each uses the same
CPU and raw literal target, with no intervening statement or callback. The seven
matrix targets cover concat, copy, vector, identity, translation, vector-array
and scale. Another 75 game-math/entry/vector/J3D/skin hooks are excluded because
they do not have this adjacency proof.

The first direct predicate currently wraps the host predicate invoked again by
native math. Pointer equality cannot certify reuse: callbacks may mutate state,
count, or alternate answers. A separate default-off owner capability requires
revocation before every related setter mutation,
preserve standalone/general APIs and all native preflight/counters, and prove
the exact targets' transitive predicate writer/thread closure. Asynchronous
preference/input producers make that last proof necessary. Source adjacency
alone is insufficient, and the initial census establishes no speed gain.

The seven-target current-host writer closure is now source-qualified for its
legitimate game-thread APIs, excluding custom/raw/concurrent writers. A private
default-off implementation binds a compact generation/target token to the CPU,
published PC and owner thread, and revokes before every parent setter mutation.
The generation saturates instead of wrapping. All 342 sites publish the exact
target PC first; only one caller has been emitted for an initial codegen pilot.
Legacy/general entry points retain their original callback behavior. Four
generic/fixed-memory O2/ASan profiles pass 95 full CPU/RAM/native-counter
comparisons each; all three behavioral mutants are rejected. The first
fixture compile's Windows identifier collision and a mutant harness that
initially reused the unmutated consumer object are preserved separately.

The single-caller codegen pilot also passes exact 51-file M/MD admission and
owned compilation. Its by-value token remains in registers, but the outlined
readiness helper grows from 144 bytes/36 instructions to 288 bytes/67
instructions, adds three nonvolatile register saves and a shadow frame, and
accesses thread-local state. The complete selected TU adds 352 code bytes and
78 instructions. This establishes a concrete added cost, not a speed gain;
the prototype remains inactive without a 342-site expansion or module build.

## Clean observer-domain comparison

The existing total H module was compared against original A on the common
per-VI host with tracing disabled. Fresh six-checkpoint CPU/MEM1/MEM2/ordered
alias hashes and full P6 pixels match, with the expected off/on capability
registrations and 1,134 controlled EFB inputs. A source-only comparator error
that initially demanded both arms use the off route was caught before launch;
the frozen successor tests the actual off/on routes and retains that failure.

The eight fixed-order runs have descriptive mean changes of -2.60% CPU,
-2.92% wall and -2.80% cycles. The third pair increases CPU 1.92% and wall
1.13%, failing the all-pairs rule. Independent raw-log decoding confirms all
eight runtime/cache gates, zero terminal pipeline creations, and exact primary
and five-segment work, endpoint cursors and terminal GX/clock/dispatch.
A source search also overlapped the first control case wrapper: available
clock readings bound the search to approximately
05:28:45-05:29:01 UTC, rather than establishing exact child start/end times.
Its overlap with individual measured VI windows cannot be reconstructed.
That self-generated activity is an independent measurement limitation.
The block is retained unchanged, with no extra runs or changed window and
no accepted controlled-gain claim even if its numeric rule had passed.

The separately preregistered composition of the event-filter host and total H
module also passed fresh six-checkpoint/full-P6 correctness. Its fixed eight
runs pass all runtime checks, exact primary/five-segment work and endpoint
cursors, terminal GX/clock/dispatch equality and zero terminal pipeline
creation. Independent raw-log recomputation gives descriptive mean changes of
**+1.44% CPU, +1.38% wall and +1.51% thread cycles**. CPU/wall pair changes are
+3.53%/+3.86%, +1.86%/+1.72%, +5.69%/+6.20% and -5.03%/-5.88%.
Both gain rules fail. This composition remains inactive and closed without
rescue runs; the individual candidates' earlier failures remain unchanged.
Neither component has an accepted speedup from this experiment.

## Narrow arithmetic and backend feasibility

The quaternion source experiment keeps its existing bounded finite-input,
alias, journal and whole-cycle guards, all rounding boundaries, registers,
stores and guest PCs. It proposes removing intermediate unobservable FPRF
classification and committing the final classification once. Existing A/G42
caller objects show that forced inlining retained finite/NaN/NI branches and
per-operation classification, so this is a separate mechanism. The original
multiply helper already combines FPSCR changes into one store; a duplicate
store is not the opportunity. Quaternion's dynamic share is unknown.

Peer source review caught an edge before any fixture compile: classifying a
rounded single after rereading its widened FPR can change subnormal behavior
under host DAZ. The failed draft is retained; its successor must retain and
classify the final rounded single directly. Corrected whole-TU fixtures now
pass 10,267 whole-CPU/64 KiB comparisons in each generic O2/ASan profile and
2,587 whole-CPU/32 MiB comparisons in each fixed O2/ASan profile. All four
deliberate arithmetic/status mutations fail with CPU-byte mismatches. The
reread-FPR mutant reproduces the DAZ/subnormal classification failure. Module,
gameplay and performance qualification remain separate and unfinished. Its
independent original-A module now builds with exactly one native-TU replacement,
834 retained objects, 42 actual M/MD dependencies, the original 15 physical
library inputs and generated manifest, and exact original PE imports, exports,
ABI and resource payloads. Static text increases 752 bytes and 138 instructions;
this is not evidence of a speedup. Fresh same-host clean title replay now
matches all six complete states and full P6 pixels, consumes all 1,134 EFB
inputs, and finishes with zero additional pipelines. The subsequent fixed eight
timing runs also pass every runtime gate, with exact primary and five-segment
work and endpoint cursors, terminal GX/clock/dispatch totals, and zero terminal
pipeline creation. Independent raw-log recomputation gives descriptive mean
changes of **+1.25% CPU, +0.67% wall and +1.13% thread cycles**. Three of four
pairs regress; CPU pair changes are +0.28%, +3.55%, -3.26% and +4.50%.
Both predeclared gain rules fail. This candidate remains inactive; the block is
closed without extra rescue runs or a changed measurement window. Background
load and unresolved timer-calibration limitations remain in the evidence.
A separate native_math include experiment targets its existing ps_sum0 calls.
Its complete original/candidate translation units passed 5,776 whole-CPU/64 KiB
comparisons in each generic O2 and ASan profile, and 2,192 whole-CPU/32 MiB
comparisons in each fixed O2 and ASan profile. These cover host rounding,
FTZ/DAZ, NI, arbitrary carried-lane NaN/Inf values and rejection paths. Three
deliberately incorrect carried-lane implementations fail with CPU-byte
mismatches. Actual dependency closures and frozen inputs remain unchanged.
The independent one-TU module also builds, retaining 834 objects and the original
physical link/PE surface. Its actual dependency closure increases from 42 to 44
only through the two unchanged inline headers. Static text increases 816 bytes
and 176 instructions. Its fresh same-host six-checkpoint/full-P6 comparison
also passes with all 1,134 inputs and zero additional pipelines. Controlled
correctness does not establish ordinary gameplay or a speed gain.

The fixed fixtures now explicitly own and compare the canonical runtime's
complete 32 MiB global MEM1. The earlier quaternion-specific explanation for
that requirement was incorrect: its native word/load/store helpers access
cpu->ram directly, and its native wrapper disables the generated hardware-load
helper. Separate buffers therefore do supply those native quaternion inputs;
there is no established wrong-input or zero-input failure. The stronger fixed
fixture coverage and input assertions remain useful. The source correction
is retained separately from the frozen drafts.

The first quaternion O2 fixture and separate ASan diagnostic crashed in the
fixture's input assertion. Existing machine bytes prove that its enum/pointer
expression emitted offset 0xffffffff00002004 instead of 0x2004. A narrowly
audited successor computes guest offsets in integer space before pointer
addition in both fixtures. Production sources, inputs, case counts, modes and
mutants are unchanged. Both corrected candidates passed all four profiles and
their seven total deliberate-failure tests; earlier crashes and the first
native-math pass remain preserved. No game or speed claim follows.

A bounded audit of the local LLVM backend found real SSA register promotion,
but also compatibility gaps: ordered external aliases, the host's native-region
query, the C backend's observation suffix/deadline contract, fixed RAM layout,
journaling and complete state publication are not yet qualified together.
Historical patch text does not establish that those changes exist in the
current backend. Standard local toolchain paths did not contain a compatible
LLVM C++ development SDK. No SDK provisioning, backend activation or game
compile was performed. Any future standalone leaf pilot must establish those
contracts before replacing original game code; it is not a prerequisite for
the current port work or a predicted large gain.

A separate eight-TU mixed ThinLTO pilot uses the installed Clang 19.1.5 without
an LLVM development SDK. It retains 827 original COFF objects and selects only
one measured chunk plus shared CPU/direct/native helpers. Driver queries reveal
that the final original frontend already enables both loop and SLP
vectorization despite its earlier `-fno-slp-vectorize` spelling. This pilot
preserves that effective command. Earlier references to a no-SLP flag describe
the command spelling, not proof of the final frontend policy.

The historical link-driver `-mllvm` register-allocation option is unused.
New LTO code generation therefore receives the threshold explicitly through
LLD's `/mllvm:` route, with IR/codegen O2 and a single backend worker. These
are verified against the actual commands. The first actual-M pass stopped
before compilation when the previously missing export-TU closure exposed nine
headers outside the frozen bank. A successor admits their exact source-qualified
identities. All eight dependency closures and compilations pass; the final link
retains the exact 835-object order, 827 original COFF objects, 15 physical library
providers, manifest and original PE imports/exports/ABI/resources. A postlink
Python namespace error is preserved; verification succeeds without rebuilding.

Fresh game correctness matches all six complete CPU/MEM1/MEM2/ordered-alias
states and full P6 pixels, consumes all 1,134 EFB inputs and ends with zero new
pipelines. All eight fixed timing runs pass the runtime checks and retain exact
primary/five-segment work, endpoint cursors and terminal GX/clock/dispatch.
Independent raw-log recomputation gives mean changes of **-2.50% CPU, -2.44%
wall and -2.28% thread cycles**. CPU/wall pair changes are -4.94%/-4.79%,
+1.79%/+2.14%, -5.33%/-5.62% and -1.25%/-1.24%. The all-pairs gain rule fails.
The candidate remains inactive; no rescue runs or ordinary-play speed claim
follow. Background load and unresolved calibration limits remain recorded.

Static code inspection finds 389 fewer call instructions in the selected hot
owner, but this needs a scope correction: its 1,398 removed FP-availability
helper calls were already guarded by an inline MSR[FP] success test. They are
on the unavailable-FP path, not evidence of 1,398 hot calls removed. Indirect
chunk-table calls and outlined direct-call readiness remain. Broader LTO needs
a separate hot-path hypothesis rather than extrapolating those cold calls.

## Inactive native-entry reuse

A source census of the older item 17 and 18 drafts matches every selected
original function/callee to A's pristine translated inputs. All 255 relevant
comparisons across the five current mod variants match too. This establishes
source compatibility, without claiming current runtime admission or speed.

The 35 GX entries and their callees cover 131 uniquely attributed clean samples
out of 4,401 (2.98%). That union includes 31 samples in shared save/restore code
whose callers are unknown. Keeping every current watch leaves 29 eligible
entries and 80 samples (1.82%); ambiguous overlaps are excluded. Six entries
remain blocked, including display-list/Begin/texture-load boundaries and the
three fog/TevOrder routines with genuine save/restore observations. The hotter
TevColorS10/KColor routines are outside the previously qualified set.

All 35 unchanged GX drafts would decline on the current route: their entry
guard requires both gather writers, while the user's build and clean controls
use direct word writes with byte batching disabled. Reuse therefore needs an
explicit coupled experiment comparing A/direct, A/batch and eligible GX/batch.
Batch-only speedup is not a prerequisite for testing its dependent native path.
No watch, transaction threshold, deadline or callback boundary may be removed.

The two independent item 17 entries, CalcDivideInfoOverArea and cM3dGCyl::SetC,
are currently unwatched and cover 32 clean samples together (0.73%). Neither is
already active. Historical fixtures remain useful prior evidence; rebasing
their additive hooks and qualifying the actual current module remain undone.
None of these sample frequencies is exclusive CPU time or a gain forecast.

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

## Local integer state and compact observer qualification, October 9

Two independent candidates over the existing C2 paired-single module completed
fresh correctness checks and their fixed eight-run comparisons on October 9.
Both passed the matched-input title correctness checks and failed the required
performance gate. Neither is promoted or selected for an ordinary tester build.
The ordinary crowded-intro slowdown remains unresolved.

The integer candidate admits 384 first prefixes in the two sampled chunks,
covering 3,106 guest instructions and 1,372 memory operations at widths 8, 16
and 32. It keeps admitted scalar state in native locals, publishes completed
dirty fields at an exit, and resumes the original current-instruction body
with its original charge/refund boundary. Opaque calls, floating-point work,
control transfers, uncertain labels and unsupported state remain excluded.
Actual 64-bit guest memory operations are absent from this admitted corpus.

The optimized fixture passed 18,568 cases and 18,571 comparisons of all 3,552
CPU bytes and all 32 MiB of MEM1, plus callbacks, alias/other-memory state and
FP environment. These include all 3,106 direct continuation seams and 6,212
actual-body RAM/MMIO microcases. Three semantic mutants were rejected. The
whole-corpus ASan child reached its frozen 300-second timeout without reporting
a semantic or sanitizer diagnostic, and drained. That timeout is retained.
A reviewed ASan-only successor reused ten qualified objects, replaced only
the range-selection main, and passed the identical corpus in four disjoint
bounded runs. Their complete reached/completion masks equal the optimized
run. Normal inputs complete 276 prefixes; the remaining 108 completion gaps
are explicit. Direct-seam tests and microcases do not prove normal activation
or admission frequency. Rounding and FTZ/DAZ combinations are cycled, rather
than a full cross-product for every prefix.

The compact observer candidate rewrites 16,939 certified literal query sites
in 35 chunks, covering 5,273 distinct keys. A compile-time token set admits
only those keys; it performs no key-table scan at each query. Existing main
and event globals move into compact typed storage while their writers,
readers, address-taking and host state table retain their original behavior.
The helper reads current owner, deadline, event, alias and lifecycle state;
it does not cache a prior skip answer. Registration validates the complete
certificate and storage identities. Missing/revoked registration and other
calls retain the original fallback. The option defaults off.

Its production-consumer fixture passed optimized, ASan, developer and census
profiles: 154 complete CPU/MEM1 cases, 12 lifecycle cases, 17 admission cases,
32 storage mutations, and 16,684 additional query comparisons per profile.
The broad query corpus includes every certified key in three modes, pending
bucket collisions/noncollisions and the excluded union. CPU/counter checks
occur per query; full-RAM checks are grouped. Three hot-consumer mutants,
three corrupt-certificate cases and three invalid-token compile cases were
rejected. This is behavior qualification, not an observed game speedup.

The compact common host retains the existing per-VI timer and physical EFB
read diagnostic, replacing only main and game-events objects: 170 physical
link inputs, 168 retained, 64 direct objects and two substitutions. Its actual
compiler closures, imports, normalized resources, stack, manifest and MAP
passed. Report serialization and receipt-reference errors were preserved and
repaired by verification-only successors; successful compiler/linker outputs
were reused without rebuilding. The game module builds remain separate:
the observer variant preserves C2 ancestry with 36 existing substitutions
and one new support object; the integer variant substitutes only two objects
and retains the other 833, including all five C2 replacements.

A separate read-only baseline diagnostic ran the unchanged b21e/C2 game for
1,800 VI using copied player data, the original 1,134-entry EFB replay and no
input. All six complete logical state checkpoints and the full captured P6
matched the earlier C2 correctness run. Its 128 asynchronous guard samples
contained 123 samples after initialization. All 123 had the expected fixed
32 MiB RAM owner, no MEM1 alias overlap, no write journal and no reservation;
the double-read global values were stable in every sample. Deadline budgets
were at least 1,000 in 116 samples, 100–999 in one, and 1–99 in six. These
necessary guard values rule out an always-on sampled RAM guard obstruction;
they are not atomic snapshots, fast-path hit counts or admission frequencies.
Concurrent build/fixture work was allowed for this correctness-only run.
Its durations and reported frame rates are excluded from performance claims.

Each candidate was compared independently against C2 on the same qualified
compact host, preserving all five paired-single changes. Fresh correctness
runs matched all six complete logical CPU/MEM1/MEM2/ordered-alias checkpoints
and the full captured P6. Both consumed the exact 1,134-entry EFB replay while
retaining the physical read before replacing its value. Physical values still
differed from the recording: 78/62 entries for the integer control/candidate
and 67/78 for the compact control/candidate. This establishes parity for that
controlled-input title route, not ordinary determinism or every-instruction
equivalence. A separate host-transposition comparison also matched all six
states and the full P6 for unchanged C2 on the earlier b21e host and the new
2b9b host. It is a correctness check, not a speed comparison.

The preregistered eight-run order was A-C-C-A, C-A-A-C, with the primary window
at 750–1,500 VI and five 150-VI segments. All sixteen runtime cases passed,
drained normally and reported zero terminal pipeline creation. Primary and
segment work and their absolute endpoint cursors matched exactly, as did
terminal GX counts, guest clock and dispatch. Direct-call and game-event
tracing were disabled, the autosave trace key was absent, and checkpoints,
captures and the read-only guard sampler were absent from timing runs.

Percent changes below compare candidate with its paired control; positive
values mean more elapsed time or thread cycles. CPU is the measured main
thread CPU time, not total machine utilization.

| Independent candidate | Mean CPU | Mean wall | Mean thread cycles | Paired CPU changes | Paired wall changes | Strict result |
| --- | ---: | ---: | ---: | --- | --- | --- |
| Two-chunk integer locals | +4.325% | +3.045% | +4.077% | +0.864%, +5.297%, +2.194%, +9.021% | -3.318%, +4.868%, +1.574%, +9.323% | Fail; all four CPU pairs regress |
| Compact observer storage/query | -2.054% | -1.513% | -1.721% | +5.044%, -4.707%, -0.096%, -8.240% | +4.697%, -4.220%, +0.537%, -6.866% | Fail; inconsistent paired gains |

The compact candidate's lower mean does not satisfy the unchanged requirement
for all four CPU and wall pairs to improve and both means to improve by at
least one percent. The integer candidate regresses despite its local codegen
and fixture results. These are completed negative experiments, with no rescue
runs, lane expansion, promotion, ordinary candidate selection or installation
change. Current background load and unresolved timer-calibration materiality
remain recorded; no overhead was subtracted. Earlier failed attempts, the
ASan timeout, 108 normal-completion gaps and prior uncontrolled-state failures
remain preserved. The generic ordinary-visible comparison adapter is source
only; its existing strict eight-run PASS prerequisite admits neither candidate.

The retained integer eight-run report is identified by SHA-256
`6a395ca48a5faa4a7d4786b89745be5bd45a1cf6ba5ee3acbe792575cf3cb629`;
the compact report by
`4e512ed20463724870faf22633239f010c7d88ee6bd2a6c5cbd116b6a0f09722`.
Their enclosing batch receipts retain `FAIL_PRESERVED`. Correctness and
host-transposition receipts remain separate from those performance failures.

## Observer entry census and expanded attribution, October 9

An accounting pass assigns every one of the existing visible capture's 4,852
native RIP samples once by image and outer physical owner. Generated chunk and
loop bodies contain 2,236 samples (46.08%), module support 1,319 (27.18%), host
runtime 1,106 (22.79%), and OS/CRT 191 (3.94%). All generated-body addresses
match exact MAP/unwind spans; names for 302 support samples remain tentative
nearest-MAP associations. Inline host frames and stack ancestors are not added
to this partition. Three host predicates plus the module readiness helper
account for 769 disjoint RIP samples, but this is not a removable-time ceiling.
The remaining generated work is spread over many owners, with no demonstrated
dominant scheduler/polling loop. Terminal idle PC and guest clock totals do not
measure idle cost. The accounting receipt is SHA-256
`11e026d8f51e1ed9e457d0939a0d799bd3a84e744c53104e74f1cb6caa7ee858`.

A separate diagnostic host adds two relaxed atomic entry counters to the
existing full-facts and can-skip-facts predicates. It changes one main object
on the qualified compact host and preserves the timer, EFB replay, event
object, original predicate decisions, imports and resources. These counters
make every run ineligible for timing; they are not enabled in the player
installation. The actual diagnostic host is SHA-256
`ac71a2a4309ac4492b8c792281fbfb9a76e75955e01c800efcc14c2f8ff2ccb1`.

Root ran the unchanged C2 and compact-v3 modules on that common host. First,
both completed the 1,800-VI replay with all six complete logical guest-state
checkpoints and exact captured P6 pixels equal. Then two separate quiet replays
disabled checkpointing and capture. Both quiet runs completed normally,
consumed all 1,134 EFB contexts, created zero terminal pipelines, and matched
the complete terminal GX, clock and dispatcher summaries. Their primary
VI 750-1,500 window has the same 547,802 blocks, 6,075,000,001 guest cycles and
99 EFB contexts, including identical absolute start/end cursors and all five
segment cursors.

An independent raw-log audit also matches the block, cycle and EFB cursor
triples at every one of the 1,801 recorded VI positions. The two predicates
are overlapping query stages and their counts must not be summed.

| Predicate entries | C2 control | Compact v3 | Difference |
|---|---:|---:|---:|
| Full facts, VI 750-1,500 | 148,210,839 | 119,071,888 | -29,138,951 |
| Can skip facts, VI 750-1,500 | 140,885,512 | 111,746,561 | -29,138,951 |
| Full facts, whole intro | 260,932,963 | 210,922,121 | -50,010,842 |
| Can skip facts, whole intro | 246,963,298 | 196,952,456 | -50,010,842 |

This establishes fewer entries into both host predicates during the same
quiet guest workload. The shortcut is not merely registered without affecting
these calls. The counters do not count unique queries or individual helper
hits, establish CPU cost, or qualify a performance gain. The earlier compact
eight-run timing failure remains unchanged; no candidate is promoted or
expanded on the strength of this census. The independent counts report is
SHA-256 `3527488d154f582b4c616c6bd409bd925657e4d18559ecdf696f4b326f36118f`.

Six additional passive CodeView objects now match the retained C2 executable
bytes, named relocations and symbol offsets exactly. Together with the two
earlier objects they cover 706 of the existing visible profile's 4,852 native
RIP samples. Of these, 639 have unique optimized-source/public-range
associations; 52 shared entry/dispatch/epilogue and 15 line-zero samples remain
unassigned. The audit verifies 86 direct-call frame occurrences at 54 distinct
sites against actual decoded E8 instructions and relocations. This adds
attribution to the existing capture without a new profiled game or replacing
any production object.

The mapped collision traversal family has 89 associated samples. All mapped
J3DGD routines total 100, including 35 for the two TEV color packet writers.
Those small leaf-shaped writers preserve deliberate repeated command words
and overflow callbacks; their counts do not support a large speedup claim.
GXProject has zero associated samples in the fully mapped owner. Chunk symbols
remain containers for many guest routines and must not be named as single
hot guest functions. These associations are not exclusive instruction costs.

A separate actual-COFF audit finds identical entry prologue bytes, saved
registers, stack allocations and unwind records in all 35 compact-v3 changed
chunks. The integer candidate does have larger common entry frames in the
two inspected chunks: four extra saved registers each, and additional stack
traffic in one. One pure four-operation path grows from 42 to 46 native
instructions solely through those entry pushes, whereas the long integer
path still shrinks from 181 to 109. This explains a concrete source of added
overhead, not the fraction of its measured regression caused by that overhead.
The integer rollout remains closed.

## Runtime locality and native function layout, October 9

A fresh binary-identical C2 experiment restricted only the owned primary
thread to logical CPUs 0-7, the first actual 16 MiB L3-sharing domain on this
Ryzen 7 3700X. The control allowed CPUs 0-15. Both processes retained the full
0xFFFF mask; the helper set and read the primary mask before resuming it.
Read-only correctness snapshots at 5 and 15 seconds confirmed both intended
primary masks and all 26 observed live workers at 0xFFFF in both arms. Timed
runs did not enumerate or query workers. This tests the selected affinity
policy, not observed migrations or cache misses.

Both correctness runs exited normally with zero pipeline compilations, the
complete 1,134-entry EFB trace, six identical complete guest checkpoints and
identical full P6 pixels. The fresh fixed eight-run order was
`A-C-C-A,C-A-A-C`. All eight runs completed with identical primary and segment
work, absolute block/cycle/EFB cursors, and terminal GX/clock/dispatch results.
The named primary window remained VI 750-1500 under current background load.

| Primary timing | Control mean | Restricted mean | Change |
| --- | ---: | ---: | ---: |
| Main-thread CPU | 15.21484375 s | 15.55078125 s | +2.207959% |
| Elapsed time | 15.2535059 s | 15.5901755 s | +2.207162% |
| Thread cycles | 54,560,456,388.5 | 55,738,350,432 | +2.158879% |

The four paired CPU changes were +4.504505%, +3.719008%, +2.812500% and
-2.272727%; elapsed changes were +4.703002%, +3.544303%, +2.472589% and
-1.978054%. The predefined gain gate failed. No additional domain selection,
retry, production affinity change or ordinary FPS claim follows. All eight
raw runs remain retained. The original batch's self-pin admission checked the
input list instead of the generated-output list and stopped before launching
any game; the minimal separately versioned repair and failed source remain.

A separate one-TU layout pilot divided the original 4,096-entry guest chunk
into four private native functions. Candidate-derived inversion reconstructed
every original guest instruction body and retained all 921 precise/fast pairs,
entry guards, true returns and central dispatch cases. The actual Clang 19.1.5
compile preserved the original optimization, ABI, FP and RAM flags, with an
exact 48-input M/MD dependency closure, normal exit and drained owned children.

The resulting machine layout was unattractive: executable text grew from
709,841 to 874,513 bytes (+23.20%), static instructions from 134,822 to 173,934,
and static calls from 9,136 to 9,772. Although the private function frames were
smaller, the active wrapper plus private frame and added return address needed
168-248 bytes versus the original 152-byte frame. These are actual COFF/unwind
properties, not measured game costs. This pilot was not linked or run in the
game. A separate guaranteed-tail-transfer variant has now passed its actual
one-TU compile and code-layout checks. It uses four private functions and a
shared dispatcher with the same two-argument signature. The export wrapper
and dispatcher have no frame; all 297 resolved transfers restore their own
frame before jumping, with no additional private return slot. Private frames
are 136/56/88/72 bytes versus the original 152. Whole-TU text is 728,481 bytes
(+2.626%) and static CALL sites are 9,224 (+88). These are structural results,
not runtime equivalence or performance results. At that compiler checkpoint
the variant had not yet been linked or run.

The source inverse reconstructs every byte of the original retained chunk.
Incoming prepaid state is not always false, but all 296 distinct internal
destinations overwrite it before reading it; the shared dispatcher never
reads it. This permits a fresh local flag at each tail entry without adding
CPU-state writes. Independent source review checked that distinction. The
earlier packed-return pilot and its negative layout results remain intact.
Some frozen pilot notes call the original flags "noSLP." That describes an
early argv spelling, not the effective frontend: later optimization flags
enable SLP. Both pilots retain the original compiler argv; there is no
effective SLP-disable claim.

The tail variant was subsequently linked with exactly one changed C2 object,
834 retained direct objects and the same 15 physical providers. Actual
LINKREPRO contents and order, exports/ordinals/imports, resources and ABI
fields matched the original. The post-link checker initially failed because
the pinned PE parser calls the reserved DWORD `Reserved1`, rather than
`Win32VersionValue`. A passive successor checked its raw bytes at optional
header offset 52 and repeated the complete original verification suffix.
The original failure is preserved; the linked output was not rebuilt.

Two fresh root-owned correctness games passed all six complete guest
CPU/MEM1/MEM2/ordered-alias checkpoints and full P6 byte equality. Both
completed the same 1,134-entry EFB trace with zero pipeline compilations.
An independent raw-log/state/pixel audit confirmed the result. The same
2b9 host was used in both arms, with all four observation options explicitly
zero and their registrations off. Other agents stopped local work before
the following fixed `A-C-C-A,C-A-A-C` timing block and remained stopped until
all eight games and analysis finished.

| Tail variant primary timing, VI 750-1500 | C2 mean | Candidate mean | Change |
| --- | ---: | ---: | ---: |
| Main-thread CPU | 17.67578125 s | 17.421875 s | -1.436464% |
| Elapsed time | 17.7227221 s | 17.502599175 s | -1.242038% |
| Thread cycles | 63,226,307,295.25 | 62,450,137,485.25 | -1.227606% |

The four paired CPU changes were -2.049530%, -6.324786%, +5.030891% and
-2.283539%; elapsed changes were -1.230958%, -6.612641%, +5.160928% and
-2.151124%. All eight runtime checks, primary/five-segment work, absolute
cursors and terminal GX/clock/dispatch matched. The mean gain condition
passed, but the predefined requirement for a CPU and elapsed reduction in
every pair failed. This is a mixed result, not a reliable improvement for
promotion. No rescue block, partition expansion or installed update follows.
All calibration CPU steps remained unresolved; no overhead was subtracted.
An independent audit reparsed all 1,801 VI rows and 1,025 calibration rows in
each case and exactly recomputed the means and four pairs. It confirmed that
the sole final failure is the paired gain condition. These current-load
timings are specific to this block and do not establish ordinary displayed
FPS or a solution to the crowded intro slowdown.

The earlier bounded SDK search missed the existing development kit at
`C:/devkitpro/msys2/ucrt64`: LLVMConfig identifies version 19.1.7, and the C++
headers, shared LLVM library, GNU import library and matching GNU compiler are
present. No SDK installation was required. A ROM-free emitter source capsule
can now test the existing SSA mechanism with an explicit MSVC object target.
SDK availability does not resolve the previously listed current-runtime
alias, deadline, observation, journaling or state-publication gaps. At this
checkpoint all 26 source files have passed actual dependency discovery,
compilation and exact M/MD closure checks. The first header-search attempt
failed before compiling because an early explicit system include directory
prevented C++ `include_next` from finding `stdlib.h`; a separately retained
query proved the corrected search order. The corrected full attempt reached
linking and stopped at the declared but undefined diagnostic function
`dolllvm_codegen_fingerprint`. That failed attempt remains preserved. A
minimal successor removed only the optional diagnostic call from the test
driver, retained the 25 qualified backend/IR/decoder objects, and checked the
complete 540-file prior dependency union before and after. Four fresh owned
steps compiled the driver, linked the generator and emitted Windows COFF for
all five synthetic cases. The actual IR contains 83 PHI instructions and no
state allocas; the scalar loop's running path contains arithmetic and branches
without state loads, stores or calls. This demonstrates the available SSA
mechanism, not current-runtime compatibility or a measured game improvement.
The 4,884-byte emitted object has 3,604 text bytes and was neither linked nor
executed. Its non-leaf functions lack Windows unwind sections; a future
production adapter must also establish its own ABI/frame/unwind behavior.

A separate constant-only two-frontend test compared the canonical production
C header against the standalone generator's C++ header. All 114 shared layout
constants and four union-alias constants matched, covering every one of the
53 shared members, CPU size/alignment and primitive widths. CPUState remains
3,552 bytes with alignment 8. Both objects contain no executable code. This
checks compiled layout only; callback conventions and the current runtime's
memory, observer, cycle and publication semantics remain separate obligations.
No backend or installed game binary has changed.

A separate SSA adapter now covers the ten-instruction integer
prefix of `GXCallDisplayList` at 0x80326B80-0x80326BA4, before the original
branch at 0x80326BA8. This is a semantic seed with only five associated sampled
instances, not a large measured hotspot or the earlier GXProject FP pilot.
Its complete original-C inverse and independent source review preserve the
leader's original cycle charge, paid first-instruction seam and irregular
PC/suffix publication at each continuation. Admission rejects unsupported
RAM ownership/size, alias overlap, reservations, journals, short positive
deadlines and CPU/MEM1 storage overlap before writing state. Each memory
access uses an unsigned strict MEM1 range check and explicit big-endian bytes.

The new adapter uses SSA promotion separately from the backend's incompatible
cycle and memory-exit protocol, and explicitly requests Windows unwind tables.
Five bounded generators have now emitted actual Windows MSVC COFF: the normal
region with eleven construction-time forced exits, and four deliberately
incorrect publication/order variants. The first emission attempt stopped at
an unused-constant warning in the deliberately broken suffix variant; its
failure remains preserved. A minimal successor marked only that mutant's
constant unused, retained the two successful emissions and completed the
remaining three. The emitted objects subsequently executed in the differential
fixture described below; no host or game module has linked them.

Independent passive inspection qualified all sixteen emitted functions: ten
have complete matching unwind records and six are frameless leaves. Every
return restores the saved nonvolatile registers. There are no native calls
or runtime imports beyond the three intended memory/alias/journal globals.
The normal object is 6,179 bytes; its main region saves seven host registers
(56 bytes). This setup cost is material for a ten-instruction seed. No speed
claim follows from scalar promotion or a smaller count of state accesses.

A source inventory found guard writers at startup or synchronous dispatch
boundaries in the examined common-host/C2 routes, with no off-dispatch writer
identified. Exported alias/journal setters do not enforce thread ownership;
arbitrary concurrent hosts and asynchronous PC-observer semantics remain
outside this proof. The ASan fixture profiles instrument the C harness and
canonical providers; the emitted adapter itself is uninstrumented.
No backend or installed game binary has changed.

The first generic-O2 fixture configuration has compiled all eight canonical
C translation units and completed linking. Preserved setup failures before
execution cover the reader's incorrect file-bound check for
uninitialized COFF sections, a missing shared-memory extern declaration,
and two unsuitable Windows linker-selection options. The fixes retain the
successful objects and the original test bodies; no comparison has been
removed or relaxed. These are harness/tooling failures, not semantic test
results.

The final four-profile fixture passes and all owned children are drained.
Generic and fixed-MEM1 O2, and their two C-provider ASan configurations, each
pass the same 1,396 original cases and 1,503 complete 3,552-byte CPU/32 MiB
memory comparisons. Each profile retains 508 callback cases, four rounding
modes and four DAZ/FTZ states. All eleven construction-time frontiers pass
88 direct comparisons; four reachable range-failure frontiers plus success
pass ten comparisons. Nine read-only admission rejections, six new semantic
negative variants and three retained legacy controls also pass. Actual
physical link-provider checks, M/MD closure and the final 166-file discovered
dependency union are preserved. This qualifies the bounded pilot's tested
behavior, not full-module integration, gameplay or performance.
An independent audit reproduced the four log summaries and checked all sixty
fresh owned rows, thirty-two C-TU M/MD multiplicities, actual argument vectors,
dependency-union records and physical-link-provider records. It did not
rehash the large C objects, runtime bank or link-reproduction archive bodies;
the original runner's before/after physical-input checks remain that authority.

A separate census examined 1,752 textual integer prefixes in the eight mapped
hot owners. Of these, 1,455 have the standard entry/prepaid/precise-exit seam.
Only the earlier 384 have the previous AST/fixture/ABI scope; they belong to
the closed, slower integer-locals candidate, not this new SSA adapter.
The compatible source intervals associate 193 of 4,852 innermost RIP samples;
excluding three retained final refund checks leaves 190 (3.916%). These are
source associations, not admitted execution or removable time. Helper-owned
RIP costs are separate. Another 117 samples occur later in parent copies
blocked first by FP/paired-single operations, mainly scalar loads/stores;
56 later integer-run associations lack a proved mid-region entry contract.
The census does not justify expanding thousands of tiny private helpers.

The next integer scaling case is a 113-operation display-list command block
with 83 ordered memory accesses and 19 associated samples. Its original
115-cycle charge, all 114 publication frontiers and 84 post-operation deadline
targets remain explicit. It can test a reusable emitter, but cannot alone
solve the crowded intro. A separate source inventory found local FP-state
modeling feasible for four finite/non-NaN scalar paths; exact FPR/PS1/FPSCR/CR,
NaN fallback and fenv/MXCSR behavior still require differential qualification.
Scalar memory lowering also needs its own exact model: translated `lfs` writes
both FPR and PS1, and translated `stfs` uses integer extraction/shift rather
than IEEE truncation or `force_single`. Interpreter scalar load/store entry
points add alignment exceptions that the translated bodies do not. A future
failure exit must therefore resume the exact original translated instruction;
substituting the interpreter entry would change behavior on unaligned access.

A reusable eleven-op integer emitter now consumes two original-derived plans:
the ten-op seed and the 113-op display-list block. Both plans reconstruct
their original C slices exactly and carry 125 independently checked publication
frontiers. Twenty-two parser/source checks and six mutated-manifest rejections
pass. The generator emits production and forced-exit test modules separately.
The first compile stopped on a C++ type-name collision with LLVM; a minimal
successor qualified two type references and preserved that failure. Eight
actual owned steps then compiled, linked and emitted both plans, with a final
494-file dependency union. The generated objects subsequently executed in the
two differential stages below.

Passive admission checks all four objects and 127 functions: two production
functions and 125 forced-exit test functions. All 116 framed functions have
matching complete unwind records; eleven true leaves need no frame record.
No native calls or imports beyond the three admitted globals remain. The seed
production text is 550 bytes with a 56-byte frame; the larger production text
is 4,306 bytes with an 80-byte frame (64 saved-register bytes plus 16 locals).
The separate test texts are 3,702 and 231,458 bytes. Stack-allocation parser
failures and the reused exact passive-tool logs remain preserved. These are
code-generation facts, not speed or semantic qualification of the successor.
Its conservative seed admission now requires the original eleven-cycle leader
bound rather than the earlier ten-op cutoff. The successor seed regression
passes in all four original configurations: 1,396 original cases, 1,507 complete
CPU/32 MiB comparisons and eight added deadline-boundary checks per profile.
The original frontier, range, rejection and semantic-negative checks remain.
Twenty-four qualified provider objects were reused exactly; eight changed C
translation units were compiled across the four configurations. All twenty-eight
fresh owned steps completed and drained. The generated code remains outside
ASan's instrumentation; this is differential correctness evidence.

The 113-op block also passes all four configurations, with 486 complete CPU/
32 MiB comparisons per profile. These include 228 direct forced-frontier
comparisons, 219 complete continuation/scenario comparisons, 27 reachable range
exits and nine read-only admission rejections. All 114 forced frontiers reached
their required direct and continuation counts; the 27 real range cases cover
23 distinct frontiers, not all 83 memory operations independently. Generic
profiles include 26 callback cases and fixed-memory profiles include 22; each
rejects three deliberately incorrect consumers.
Thirty-six changed C translation units and twenty-four retained providers cover
the four profiles; all eighty-four fresh owned steps completed and drained.
Active gather output, arbitrary concurrent hosts and full game-dispatch behavior
remain outside these fixtures. Neither stage is a performance measurement.

A private production overlay now changes only the corresponding prefix in
original C2 chunk0181, retaining its full-file inverse, original cycle charge,
all precise labels and remaining game code. The original-flags production
compile and module link now pass: 47 actual M/MD dependencies, one original
chunk replacement, 834 retained C2 objects and one additional production SSA
object. All five previous C2 objects remain, with 836 direct objects, fifteen
providers and 853 link-reproduction members. The original exports, imports,
resources and PE ABI match. Chunk text grows from 713,490 to 719,554 bytes,
plus 4,306 bytes for the SSA object; those counts are not a speed result.

Fresh root-owned control/candidate intro replays pass all six complete guest
CPU, MEM1, MEM2 and ordered-alias checkpoints and exact full P6 equality on the
same common host. Both actual games exit normally and drain. Installed
executable/module hashes were reverified unchanged. An independent build audit
reconciles the
actual M/MD, argument vectors and physical-provider records but does not
repeat the original runner's large module/archive body hashes.

The independent fixed eight-run SSA113 timing experiment is now closed with no
qualified gain. All eight games exit normally, drain and pass exact primary/
segment work, terminal GX, clock and dispatch checks, with zero terminal
pipeline creations. In the preregistered VI750-1500 window, dispatch CPU mean
changes from 15.1015625 to 15.10546875 seconds (+0.0258665%); elapsed mean
changes from 15.137861775 to 15.1614383 seconds (+0.1557454%). Thread cycles
increase 0.164136%. The four CPU pair changes are +1.240951%, -0.940439%,
+3.128259% and -3.255341%; elapsed changes are +1.584856%, -0.596827%,
+2.922792% and -3.221109%. Both the consistent-pair and one-percent mean-gain
requirements fail. This is not a reliable speedup, an ordinary FPS result or
evidence that the larger FP design will help. The result, all calibration rows
and unresolved CPU timer-step materiality remain; no overhead is subtracted.
The candidate is not installed or promoted, and there is no retry/rescue block.
The reusable compiler and correctness fixtures remain available for distinct
larger experiments.

An independent audit recomputed all eight raw 1,801-row VI streams and 1,025-row
calibration streams, primary/five-segment metrics, absolute work cursors and
all four pair changes. It confirms the failed gain result and all eight normal
drains, 272 runtime checks and zero terminal pipelines. It performs no new
game execution or large binary/archive rehash.

The next source-only scope is the 46-operation callback-free initial phase of
GroundCrossGrpRp, stopping before its original guest call. It has 27 source
associations of 4,852 (0.556%), ten scalar loads, six compares and fourteen
original leaders; this is not the full routine's sampled cost. The plan uses
integer FP bit models and retains path-specific leader charges and publication.
Review caught a concrete entry-guard counterexample: positive deadline1 with
downcount100 can satisfy a total-charge proof while the original suffix6
access still refunds. A conservative successor also rejects positive deadlines
below the maximum existing suffix8. The original comparison is suffix greater
than deadline, so deadline8 is sufficient. A pure integer boundary model checks
19,542 states and all cumulative totals0-47 for 4,779 admitted states. This is
static model evidence, not emitted-code, full-CFG, game or performance proof.

The GroundCross46 source is now frozen and independently reviewed against the
original bodies and complete source inverse. It covers fourteen leaders and
forty-seven current-instruction frontiers. The implementation uses path-local
dirty state and exact reached charges; it preserves the paid E8 continuation
and unpaid external exits. Its mandatory positive-deadline minimum is eight.
The source review also checks signed-zero equality, reversed negative ordering,
infinities, NaN fallback and the original FPSCR OR behavior.

The first emission attempt passes with four normal, drained tool jobs and 493
discovered dependencies. It emits one production function, sixty-one separate
test functions (forty-seven frontiers plus fourteen access microfunctions),
and four separate one-function semantic mutants. This is compilation evidence
only: no emitted game operation, fixture, linked module or game has executed
for this candidate. Complete-state parity and runtime costs remain unqualified.
Before interpreting later timing, a separate diagnostic
must establish successful runtime admissions on the matched intro route; the
timed object must remain free of its counters and exports. No speedup is claimed.

Passive inspection now admits all six objects and sixty-six functions with
complete Windows unwind and stack/CFG checks. All are framed; the production
helper saves eight registers and has 120 local stack bytes (184 bytes including
saves). It contains no native CALL and imports only the three admitted globals.
The initial verifier failures are preserved: valid executable cold tails after
the final RET and a legitimate stack-local instruction required narrow parser
successors. All twelve original tool logs were reused; no emitted operation
executed during these repairs.

The exact full0145 caller probe also compiles and drains with equal actual
48-file M/MD sets. Its sole new undefined reference is the SSA helper. The
original and candidate main functions retain the same three saved registers
and 128 local stack bytes. Caller text grows 2,128 bytes and the separate helper
adds 5,874 bytes; these static totals include cold alternatives and establish
neither an executed-path reduction nor a speedup. A local eligible-path cost
check is planned before committing to an expensive game timing experiment.

## Validation boundary and retained evidence

A separate snapshot-only native call-stack diagnostic now passes owned
synthetic qualification. It resumes the captured thread before any DbgHelp
work, then unwinds immutable context/stack bytes against pinned local image
metadata in an owned helper with a five-second deadline. All 39 snapshots
recover the actual optimized leaf, four recursive frames, dynamic-frame
middle, outer and entry chain; system frames outside the admitted image stop
explicitly. Truncated stacks remain partial, invalid image/unwind identities
are rejected, and callback/frame/read caps and owned timeout/drain tests pass.
The original RIP sampler remains unchanged. Synthetic capture latency does not
establish game profiling overhead or a performance improvement.

A separate original-A hidden title run now passes all 24 route/input checks,
normal owned exit/drain, 1,800 retraces and zero terminal pipeline compilations.
It collects 3,688 native RIP samples and 368 sparse immutable stack snapshots
between actual retraces 798 and 1,451. The largest observed suspend interval is
0.3276 ms, with no sampler error or pause-limit stop. The source-qualified
static lifetime admission accepts only the exact host/module images and their
registered unwind ranges; it rejects alternative inventory/reward module
leases and all general leaf inference. Original saves and inputs are retained.

The snapshot-only helper completes all 368 rows in about one second after the
game drains. The naming pass finds 268 rows with at least two physical frames,
96 with one and four with none. Stops remain explicit: 81 unregistered leaf
gaps, 282 reads outside admitted images/captured bytes, four unknown frames
and one StackWalk failure. Common dispatch frames are inclusive ancestry,
not evidence that their full descendant cost is removable. Raw return-PC
boundary attribution remains tentative. The original naming pass's duplicate
dictionary-key error is preserved; its narrow successor changes serialization
only. No instruction/stack sample is a throughput or displayed-FPS result.

A separately qualified original-A leaf extension uses 26 exact linked byte
ranges and decoded instruction boundaries. All other gaps and unavailable
frames retain their original stops. Pure verification admits those ranges and
rejects 34 semantic/range counterexamples. It caught an in-range JMP into a
displacement byte; the narrow correction rejects it without changing any
admitted bytes. The offline worker completes in 1.453 seconds with all 368
original snapshots unchanged. Exactly 49 rows gain 243 physical frames, and
all previously recovered frames remain identical prefixes. Rows with at least
two frames rise from 268 to 317; 32 unregistered leaf gaps and 331 uncaptured
or system-boundary read stops remain. Added ancestry confirms repeated host
approval paths but does not measure their exclusive cost.

The actual hidden renderer log reports framebuffer 640x480 at scale 2. During
the crowded sampled interval, existing perf rows report about 42–45 VI/s and
97–101 percent dispatch busy. The 58–67 VI/s rows belong before or after that
interval, not evidence that the crowded slowdown disappeared. The earlier
visible C2 run reports roughly 39–43 VI/s and about 20 shown FPS, with a
2560x1440 framebuffer at scale 2. Its ordinary audio/presentation, real clock,
saved options, card and module differ from this unpaced original-A diagnostic.
This preserves a qualitative reproduction while limiting direct comparison;
neither framebuffer differences nor current background load establish the
cause of the remaining drop.

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
- `untraced-profile1/{guest-attribution1,guest-contexts1,guest-summary1}.json`
- `pgo-clean-training2/{source-validation1,profile-equivalence1}.json`
- `pgo-clean-training2/training3/{training-batch1,clean-training-parity1,profile-census1}.json`
- `pgo-clean-per-vi2/{preparation1,correctness-batch1,controlled-batch1,independent-audit2}.json`
- `fp-inline-clean-per-vi1/{preparation1,source-validation1,peer-source-review1}.json`
- `fp-inline-clean-per-vi1/{correctness-batch1,controlled-batch1,independent-audit1}.json`
- `fp-inline-clean-per-vi1/controlled-input1/fp-inline-clean-per-vi-eight-run1.json`
- `coalescer-cap1/{source-preparation1,attempt1/result,attempt1/codegen1}.json`
- `o3-pilot1/{source-preparation1,attempt1/result,attempt1/codegen1,attempt1/symbol-census1}.json`
- `inactive-native-applicability1/{source-census1,feasibility-receipt1}.json`
- `event-negative1/{native-preparation1,source-validation1,peer-source-review1}.json`
- `event-negative1/{correctness-batch1,controlled-batch1,actual-independent-audit1}.json`
- `event-negative1/host1/attempt3/{result,codegen1/result}.json`
- `gpr-split1/{source1/source-receipt,fixture-attempt1/result}.json`
- `gpr-split1/pilot-attempt1/{result,codegen1}.json`
- `gpr-split1/clean-module5/attempt1/{result,actual-objects1,actual-link-inputs1,PE-surface1}.json`
- `gpr-split-clean-per-vi1/{preparation1,source-validation1,correctness-batch1}.json`
- `gpr-split-clean-per-vi1/{controlled-batch1,independent-attempt-audit1}.json`
- `gpr-split-clean-per-vi1/controlled-input1/gpr-split-clean-per-vi-eight-run1.json`
- `gpr-split1/hold-deviation2.json`
- `h-clean-per-vi1/{preparation2,source-validation2,correctness-batch1,controlled-batch1}.json`
- `h-clean-per-vi1/{independent-audit1,timing-incident1}.json`
- `h-clean-per-vi1/native-efb1/h-clean-per-vi-input-parity1.json`
- `event-h-combined-clean-per-vi1/{preparation3,source-validation4,correctness-batch1}.json`
- `event-h-combined-clean-per-vi1/{controlled-batch1,peer-controlled-audit1}.json`
- `event-h-combined-clean-per-vi1/controlled-input1/event-h-combined-clean-per-vi-eight-run1.json`
- `llvm-backend-feasibility1.md`
- `quaternion-local1/{DESIGN2.md,existing-caller1.json}`
- `quaternion-local1/{SOURCE_CORRECTION1.md,fixture-attempt1/result.json,diagnostic-asan1/result.json}`
- `quaternion-local1/{fixture-address-fault1,fixture-address-repair1,final-cpu-handoff1}.json`
- `quaternion-local1/fixture-attempt2/result.json`
- `quaternion-local1/module1/attempt1/{result,codegen1,actual-link-inputs1,PE-surface1}.json`
- `native-math-inline1/{fixture-preparation3.json,fixture-attempt1/result.json,cpu-qualification1.json}`
- `native-math-inline1/fixture-attempt2/result.json`
- `native-math-inline1/module1/attempt1/{result,codegen1,actual-link-inputs1,PE-surface1}.json`
- `quaternion-clean-per-vi1/{preparation1,source-validation1}.json`
- `native-math-clean-per-vi1/{preparation1,source-validation1}.json`
- `quaternion-clean-per-vi1/correctness-batch1.json`
- `quaternion-clean-per-vi1/{controlled-batch1,peer-controlled-audit1}.json`
- `quaternion-clean-per-vi1/controlled-input1/quaternion-clean-per-vi-eight-run1.json`
- `native-math-clean-per-vi1/correctness-batch1.json`
- `qps-clean-per-vi1/{preparation1.json,source-validation1.json,pe-surface3.json,ldexp-closure1.json}`
- `qps-clean-per-vi1/{correctness-batch1,controlled-batch1,independent-attempt-audit1}.json`
- `pipeline-preload1/source1/source-receipt.json`
- `pipeline-preload1/{source2/source-receipt,fixture-preparation2,fixture-preparation3}.json`
- `pipeline-preload1/{fixture-attempt2,fixture-attempt3}/result.json`
- `pipeline-preload1/{fixture-attempt4,fixture-attempt5}/result.json`
- `native-preapproval1/{DESIGN1.md,census2.json,source-audit1.json}`
- `native-preapproval1/writer-closure6/result.json`
- `native-preapproval1/source1/source-receipt.json`
- `native-preapproval1/qualification-negative3/result.json`
- `native-preapproval1/{pilot-source-preparation1,pilot-source-preparation2,pilot-header51-admission1}.json`
- `native-preapproval1/pilot-attempt2/{result,codegen1}.json`
- `native-stacks1/{test-preparation1,codegen1,offline-preparation1}.json`
- `native-stacks1/{source-tests1,compile1,abi1,synthetic1,offline-tests1}/result.json`
- `native-stacks1/game-admission1/{source-admission1,root-profile2-peer-review1,ranking-preparation2}.json`
- `root-game-profile1/{preparation1,preparation2}.json`
- `root-game-profile1/title-stack-native1/{result,native-rip,stack-ranking2}.json`
- `root-game-profile1/title-stack-native1/offline-request1/preparation.json`
- `root-game-profile1/title-stack-native1/offline-unwind1/{result,unwind}.json`
- `visible-c2-profile1/{preparation1,offline-source-preparation1,ranking-preparation1}.json`
- `visible-c2-profile1/title-visible-c2-native1/{result,native-rip,stack-ranking-visible1,rip-ranking-visible1}.json`
- `visible-c2-profile1/title-visible-c2-native1/offline-unwind1/{result,unwind}.json`
- `visible-c2-profile1/c2-map1/{source-preparation1,verification-source2,verification2/result}.json`
- `visible-c2-profile1/ranking-map-preparation2.json`
- `line-map1/attempt1/{native-identity1,sampled-lines2,sampled-instructions1}.json`
- `line-map2011/attempt2/{native-identity1,sampled-lines2,sampled-instructions1}.json`
- `cursor-fprf1/source3/source-receipt.json`
- `cursor-fprf1/fixture-attempt2/result.json`
- `cursor-fprf1/codegen-attempt2/{result,lane-cfg1}.json`
- `cursor-fprf1/codegen-attempt3/{result,concrete-path2,success-path-summary2}.json`
- `root-lane-coverage1/result2.json`
- `integer-lane-feasibility1/result1.json`
- `native-adjacent-stack1/{qualification-attempt1,negative-attempt1,codegen-attempt2}/result.json`
- `native-adjacent-stack1/codegen-attempt3/{result,admitted-paths2}.json`
- `native-adjacent-stack1/expanded2/handoff1.json`
- `observation-live-view1/source3/source-receipt.json`
- `observation-live-view1/source5/source-receipt.json`
- `cursor-integer-general1/{source1/source-receipt,source-validation1}.json`
- `cursor-integer-general1/fixture-attempt2/{optimized-result,result,row-47}.json`
- `cursor-integer-general1/{range-preparation1,fixture-ranges-attempt1/result}.json`
- `cursor-integer-general1/module1/{recipe3/source-preparation,attempt1/result}.json`
- `observation-live-view1/certified-v3-source1/source-preparation1.json`
- `observation-live-view1/v3-fixture1/{qualification2,attempt2/result}.json`
- `observation-live-view1/certified-v3-host1/verification6/result.json`
- `compact-c2-runtime1/{control-validation1,guard-c2-result1,plan1}.json`
- `compact-c2-runtime1/guard-efb1/{preparation1,launch1}.json`
- `compact-c2-runtime1/guard-efb1/c2-guard-baseline1/{result,guard-samples1}.json`
- `integer-c2-runtime1/{preparation1,correctness-batch1,controlled-batch1,secondary-host-transposition1}.json`
- `integer-c2-runtime1/native-efb1/integer-c2-per-vi-input-parity1.json`
- `integer-c2-runtime1/controlled-input1/integer-c2-per-vi-eight-run1.json`
- `compact-c2-runtime1/{preparation1,correctness-batch1,controlled-batch1}.json`
- `compact-c2-runtime1/native-efb1/compact-c2-per-vi-input-parity1.json`
- `compact-c2-runtime1/controlled-input1/compact-c2-per-vi-eight-run1.json`
- `observation-live-view1/module-v3-1/attempt3/result.json`
- `observer-census-host1/{source1/source-preparation1,attempt1/result}.json`
- `observer-census-runtime1/{preparation1,peer-source-review1,correctness-batch1,counts-batch1,observer-entry-counts1,independent-counts-audit1}.json`
- `observer-census-runtime1/native-efb1/observer-census-input-parity1.json`
- `line-map-next6-1/attempt1/{result,analysis-result1}.json`
- `guest-routine-attribution1/{result2,audit2}.json`
- `exclusive-native-budget1/result1.json`
- `affinity-c2-runtime1/{preparation1,preparation2,source-validation2,peer-source-review2}.json`
- `affinity-c2-runtime1/{correctness-batch2,controlled-batch2,independent-correctness-audit1}.json`
- `affinity-c2-runtime1/controlled-input1/affinity-c2-per-vi-eight-run1.json`
- `chunk-partition1/{source6/source-receipt1,compile-preparation2,peer-source-review1}.json`
- `chunk-partition1/compile-attempt1/{result,codegen1}.json`
- `chunk-partition1/{TERMINAL1,MUSTTAIL_DESIGN1}.md`
- `chunk-partition1/{prepaid-flow1,tailcall-terminal2}.json`
- `chunk-partition1/tailcall-source2/{source-receipt1,peer-source-review1}.json`
- `chunk-partition1/tailcall-abi-attempt1/{result,tail-codegen1}.json`
- `chunk-partition1/tailcall-compile1/{result,tail-codegen1,unwind-tail2}.json`
- `chunk-partition1/tailcall-module1/{recipe1/source-preparation1,passive-preparation2}.json`
- `chunk-partition1/tailcall-module1/attempt1/{negative,result,actual-link-inputs,PE-surface}.json`
- `chunk-partition1/tailcall-module1/qualification2/qualification2.json`
- `chunk-partition1/tailcall-module1/{peer-runtime-source1,independent-correctness-audit1}.json`
- `chunk-partition1/tailcall-module1/independent-timing-audit1.json`
- `tailcall-c2-runtime1/{preparation1,source-validation2,correctness-batch1,controlled-batch1}.json`
- `tailcall-c2-runtime1/native-efb1/tailcall-c2-per-vi-input-parity1.json`
- `tailcall-c2-runtime1/controlled-input1/tailcall-c2-per-vi-eight-run1.json`
- `llvm-sdk-ssa1/DESIGN1.md`
- `llvm-sdk-ssa1/{recipe4,peer-source-review4}.json`
- `llvm-sdk-ssa1/{SDKsearch-order1,attempt3,attempt4}/result.json`
- `llvm-sdk-ssa1/{recipe5,peer-source-review5,passive-inspection1}.json`
- `llvm-sdk-ssa1/attempt5/{result,codegen-inspection,actual-link-inputs}.json`
- `llvm-production-abi1/{recipe1.json,DESIGN_ERRATUM1.md}`
- `llvm-production-abi1/attempt1/{result,canonical-witness,standalone-witness}.json`
- `llvm-bounded-prefix1/{fixture-plan1,peer-source-review1}.json`
- `llvm-bounded-prefix1/source1/source-receipt1.json`
- `llvm-bounded-prefix1/{peer-fixture-source1,guard-writer-audit1}.json`
- `llvm-bounded-prefix1/emission-attempt1/result.json`
- `llvm-bounded-prefix1/emission-attempt2/result.json`
- `llvm-bounded-prefix1/passive-admission1/negative.json`
- `llvm-bounded-prefix1/passive-admission2/admission.json`
- `llvm-bounded-prefix1/{fixture-binding-failure5,recipe7}.json`
- `llvm-bounded-prefix1/fixture-attempt1/result.json`
- `llvm-bounded-prefix1/fixture-attempt{2,3,4,5}/result.json`
- `llvm-bounded-prefix1/fixture-attempt6/result.json`
- `llvm-bounded-prefix1/{canonical-qualification1,peer-fixture-actual-audit1}.json`
- `llvm-bounded-prefix1/next-scale-feasibility1/{result1,original-frontiers1}.json`
- `llvm-bounded-prefix1/FP-state-footprint1.json`
- `llvm-bounded-prefix1/LFS-STFS-footprint1.json`
- `llvm-bounded-prefix1/generic-integer-plan1/{preparation1,validation1}.json`
- `llvm-bounded-prefix1/generic-integer-emitter1/{source-preparation1,peer-source-review1,peer-manifest3}.json`
- `llvm-bounded-prefix1/generic-integer-emitter1/emission-attempt{1,2}/result.json`
- `llvm-bounded-prefix1/generic-integer-emitter1/passive-admission5/admission.json`
- `llvm-bounded-prefix1/generic-seed-fixture1/source-preparation1.json`
- `llvm-bounded-prefix1/generic-seed-fixture1/{recipe2,canonical-qualification1}.json`
- `llvm-bounded-prefix1/generic-seed-fixture1/attempt1/result.json`
- `llvm-bounded-prefix1/generic-seed-fixture1/peer-actual-audit1.json`
- `llvm-bounded-prefix1/generic-integer-fixture1/{fixture-plan1,source-validation1,peer-source-corpus1,recipe2,canonical-qualification1}.json`
- `llvm-bounded-prefix1/generic-integer-fixture1/attempt1/result.json`
- `llvm-bounded-prefix1/generic-integer-fixture1/peer-actual-audit1.json`
- `llvm-bounded-prefix1/generic-integer-module1/source1/source-receipt1.json`
- `llvm-bounded-prefix1/generic-integer-module1/{recipe1/source-preparation1,peer-source-review1,independent-build-audit2}.json`
- `llvm-bounded-prefix1/generic-integer-module1/{compile-attempt1,module-attempt1}/result.json`
- `ssa113-c2-runtime1/{plan2,source-copy1}.json`
- `ssa113-c2-runtime1/{binding1,preparation1,source-validation2,peer-generated-source1,correctness-batch1}.json`
- `ssa113-c2-runtime1/native-efb1/ssa113-c2-per-vi-input-parity1.json`
- `ssa113-c2-runtime1/{independent-correctness-audit1,root-quiescence1,controlled-batch1}.json`
- `ssa113-c2-runtime1/controlled-input1/ssa113-c2-per-vi-eight-run1.json`
- `ssa113-c2-runtime1/independent-timing-audit1.json`
- `llvm-bounded-prefix1/fp-cfg-extension1/{source-assessment1,entry-bounds-model1}.json`
- `llvm-bounded-prefix1/fp-cfg-extension1/DESIGN1.md`
- `llvm-bounded-prefix1/fp-cfg-extension1/{plan-preparation1,source-preparation1,emission-recipe1}.json`
- `llvm-bounded-prefix1/fp-cfg-extension1/emission-attempt1/result.json`
- `llvm-bounded-prefix1/fp-cfg-extension1/passive-attempt{1,2,3}/admission.json`
- `llvm-bounded-prefix1/fp-cfg-extension1/cost-attempt1/{result,codegen1}.json`
- `llvm-bounded-prefix1/fp-cfg-extension1/RUNTIME_ADMISSION_DESIGN1.md`
- `llvm-bounded-prefix1/groundcross-continuations1/{source-review1,emitter-source-review1}.json`
- `integer-lane-feasibility1/{eight-owner-handoff1,eight-owner-result1,eight-owner-frontiers1,next-region1}.json`
- `integer-lane-feasibility1/EIGHT_OWNER_REPORT1.md`
- `v3-entry-frames1/result1.json`
- `cursor-integer-general1/regression-audit1/{receipt1.json,FINDINGS1.md}`
- `ordinary-visible-c2-1/{PROTOCOL1.md,preparation1.json}`
- `native-stacks1/game-admission1/{leaf-source-preparation3,leaf-pure-tests3,leaf-coverage-audit1}.json`
- `native-stacks1/game-admission1/leaf-request1/{preparation,stack-ranking1}.json`
- `native-stacks1/game-admission1/leaf-request1/offline-unwind1/{result,unwind}.json`
- `mixed-thinlto1/{build-preparation2,hot-owner-codegen1}.json`
- `mixed-thinlto1/verification1/result.json`
- `mixed-thinlto-clean-per-vi2/{preparation1,correctness-batch1,controlled-batch1,peer-controlled-audit1}.json`
- `pipeline-origin1/{untraced-profile-source-review1,untraced-host-attribution1}.json`
- `pipeline-origin1/TRACE_FASTPATH_SOURCE_AUDIT.md`
- `slp1/attempt2/`

Translated game source, compiled game modules and private player data are not
part of this report or the Git change.
