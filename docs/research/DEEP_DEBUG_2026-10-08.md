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

The 4,852 RIP observations cover VI784 through VI1495 (18.323 seconds), not the
entire intro. Their individual timestamps and order were discarded. The 485
sparse stack snapshots retain relative and absolute performance-counter times,
but there is no paired wall/QPC anchor for exact VI alignment. Session log times
are asynchronous pipe receipt times. The lowest sustained logged interval is
VI939 through VI974 at 34.5 retraces per second; this is not displayed FPS.
Exact peak-window RIP attribution cannot be recovered from this capture.

A new root-owned focused capture now retains a raw QPC bracket for every RIP
and enables the already qualified host's per-retrace recorder. It uses the
same C2 module and tester settings/audio/FIFO/pacing, with copied writable data,
live controller/mouse input disabled and experimental capabilities off. Other
agents held source/tool work during capture. The process exits normally and
drains; all 35 route, clock, capture and preservation checks pass. It records
5,231 timestamped RIPs, 512 sparse immutable stack snapshots, 1,801 per-retrace
rows and 1,025 calibration readings. Sampler and host QPC frequencies both
equal 10 MHz; timer/capture errors and pipeline creation are zero. Raw capture,
player data and binaries remain private. Sampling perturbs execution; this is
not a performance comparison or a qualified speedup.

The exact new source is archived under
`docs/research/diagnostics/focused-intro-20261009/` with restore paths and hashes.
Offline naming must select the accepted new host/PDB explicitly: the copied
tester directory can contain an older PDB. Fixed and actual worst-window
selection follows the retained QPC records, rather than asynchronous log times.

Passive QPC alignment assigns 3,303 certain RIP observations to the fixed
sustained VI831..1351 interval, with no samples straddling its endpoints.
The owning thread consumes 11.546875 CPU seconds over 12.040652 elapsed seconds
(95.90% busy). The actual longest elapsed 30-VI window within VI750..1500 is
VI817..847: 0.750000 CPU seconds over 0.8032735 elapsed seconds (93.37% busy),
with 216 certain RIPs. Per-VI intervals include catch-up, waits and sampling
pauses and are not presented frames. Coarse CPU timer readings and unresolved
calibration steps remain retained; no overhead is subtracted and no individual
RIP receives exclusive CPU cost.

Offline naming is now complete for the selected windows. All 354 unique host
lookup requests resolve to both innermost and outermost functions using the
exact accepted executable/PDB; those requests are not sample counts. A passive
audit reproduces every selected-window raw RIP frequency. Counting each sample
once with the host innermost name and the module's qualified nearest MAP name
gives the following descriptive distribution:

| Named sample class | Sustained VI831..1351, 3,303 samples | Wall-worst VI817..847, 216 samples |
|---|---:|---:|
| Translated chunk/loop | 1,474 (44.63%) | 98 (45.37%) |
| Observer predicates/edge lookup | 468 (14.17%) | 31 (14.35%) |
| FP/PSQ/bit conversion | 292 (8.84%) | 20 (9.26%) |
| Call/memory readiness | 156 (4.72%) | 7 (3.24%) |
| Guest-call dispatch | 108 (3.27%) | 10 (4.63%) |
| Memory/alias helpers | 95 (2.88%) | 11 (5.09%) |
| Native replacement/math helpers | 216 (6.54%) | 11 (5.09%) |
| Other host/module/system | 494 (14.96%) | 28 (12.96%) |

No individual innermost named function exceeds seven of the worst window's
216 samples. This supports investigating common translation/helper work;
it does not identify a single dominant guest routine or establish removable
cost. Nearest MAP labels without an unwind span remain tentative for 188
sustained and nine worst-window samples. The old capture uses a different host
and has no per-RIP timestamps, so its similar broad distribution is not a speed
comparison. An initial audit omitted 191 old system-image samples; its retained
successor adds their exact module counts and passes total equality. This was an
audit-format repair, not a capture failure.

The naming and audit source is archived beside the capture source. Private
`named-window-summary1.json` has SHA256
`4430d55adc583c91d8d20dee7972d967d98d99e5db8d7dd74146bdefd9ee0d8d`;
`named-window-audit2.json` has SHA256
`fa9ad9a6a9df56c7089681cf78aea894b76a7c6b9dafd590f8669ec6ba15362f`.

A separate contextual arithmetic draft now combines adjacent finite
`fsubs -> fmuls` helper work in a private paid fast copy. The source census has
271 overlapping arithmetic adjacency edges across eight qualified chunks,
including 60 subtraction/multiply pairs; 25 of those use the first result as
the second multiplier. These are static opportunities, not execution counts or
a performance budget. Six private 0144 sites use the draft, preserving all
original precise labels and original fallback statements. The standalone
handwritten helper is preserved in
`patches/compiler/drafts/fp-context-pair/`; translated caller bodies remain
private. Both the original draft and a delayed-publication successor now pass
four native semantic profiles (generic/fixed CPU, optimized/ASan). Each profile
passes 62,183 complete CPU/fenv comparisons, including 240 actual paid lazy-off,
MSR-clear cases; 807 individual and 61,376 grouped RAM queries cover the real
32 MiB RAM. All four behavioral mutants are rejected. The successor resolves
aliased operands locally and delays first-result FPR/PS1 writes when the second
result replaces them, restoring the complete first result before fallback.

The original-flags version 1 caller compiles with the original 184-byte main
frame and undefined-symbol set. Its natural helper remains outlined; one
representative finite machine path falls from 122 to 71 static instructions.
Forced inlining retains intermediate writes and outlines the final writer.
A local 192-million-pair test passes 384 full-state endpoints and favors the
combined helpers in mean cycle/wall costs, with mixed individual comparisons.
Its original subtraction helper was compiler-specialized and fails exact
production-code equality, so these are **local-only measurements**, not a game
speedup. Version 2 also compiles with the original frame/imports and 49 actual
dependencies. Its representative path is 69 instructions; the two intermediate
FPR/PS1 stores are fallback-only.

A fresh root-owned intro capture now retains all 3,552 CPU-state bytes beside
each of 5,562 native RIP samples. All 36 capture checks pass. Raw-byte validation
independently rechecks the published PC and eight GQRs. The sustained window has
3,582 certain samples; 3,580 have GQR load/store type vectors
`[0,0,4,5,6,7,0,0]`. These vectors do not select an active GQR index. The published
PC can be stale, and this instrumented run is not a speed comparison.

Only three sustained-window samples publish one of the six candidate second
PCs: two named multiply-helper samples and one unresolved host sample. All 60
static subtraction/multiply pairs together match four samples, and none match
either fresh worst-30-VI window. Original machine code verifies that the six
sites publish their second PC before the multiply helper on the successful
path. This is insufficient observed budget for a six-site game experiment;
it is not a precise removable-time bound. The private module recipe is retained
unexecuted, and this performance branch is closed pending different evidence.
The helper machinery remains qualified and preserved. The installed build is
unchanged.

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
only; at that stage no emitted operation, fixture, linked module or game had
executed for this candidate. Runtime costs remain unqualified.
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
check was required before committing to an expensive game timing experiment.

The real-runtime differential fixture subsequently passes all four generic/
fixed optimized/ASan profiles, each with 3,155 complete CPU/32 MiB comparisons.
Each profile contains 2,478 whole-region cases, 474 actually reached direct
frontiers, 112 actual-access micro continuations, 70 read-only range rejects,
14 admission rejects and seven detected behavioral mutants. Required direct
and continued reach counts pass at all 47 frontiers on nine feasible baseline
paths; all 14 leaders have additional paid-state checks. This does not claim
that all 165 syntactic correlated states are feasible. Incoming sticky host
FP status, every float exponent boundary, signed zeros, NaNs/infinities, short
deadlines and callback/alias mutation are included. Callback case counts are
50 in generic profiles and 48 in fixed profiles.

All 100 owned rows exit normally and drain, with 44 fresh and 24 retained C
translation units and 174 discovered dependencies. ASan instruments the C
harness and providers; the emitted LLVM object remains uninstrumented.
The conservative binary64-subnormal compare fallback has source evidence but
cannot be reached through the entry-only region's LFS-dominated operands.
No integrated game, concurrency or performance claim follows from this fixture.

A later header-closure review found that the extracted oracle omitted the
production `inline_fp.h` override. The copied guest instruction bodies match,
but production scalar loads use hardware float widening while the fixture's
canonical fallback uses an integer conversion helper. The four-profile result
therefore qualifies this extracted canonical oracle, not the exact production
FP macro path. Direct production-cost equivalence is withdrawn. Future FP
fixtures must compare the preprocessed operation and complete header closure,
in addition to guest bodies. No candidate module was integrated or launched.

The subsequent local cost experiment closes this GroundCross46 version with a
negative result. It executes only the qualified production helper and its
canonical continuation, with no forced selector or mutant object in the timed
arm. All nine known-admitted paths pass full CPU/RAM/alias/fenv endpoints before
and after timing. The fixed eight balanced batches per path contain 72 million
measured calls and 18,432 warmups. All seven owned tool/native rows exit normally
and drain. Identical 3,552-byte CPU restoration remains inside both arms; no
overhead is subtracted. The standalone oracle frame is not the original giant
chunk frame, and the oracle header mismatch described above also applies.
This experiment does not measure exact production cost, game FPS or
route-weighted cost.

| Local path | Mean thread-cycle change | Mean elapsed change |
|---|---:|---:|
| 0 | +15.23% | +15.21% |
| 1 | -0.50% | -0.29% |
| 2 | +5.76% | +5.80% |
| 3 | +3.29% | +3.03% |
| 4 | +12.53% | +12.65% |
| 5 | +16.32% | +16.19% |
| 6 | +31.25% | +31.23% |
| 7 | +22.03% | +21.96% |
| 8 | +21.50% | +21.70% |

Eight of nine means regress; the small favorable path has mixed pair signs.
The coarse GetThreadTimes readings remain retained; thread-cycle and elapsed
results agree descriptively. There is no basis for an integrated module or game
timing attempt. This version is closed without a retry, altered batch or game
promotion. The correct emitted primitives and complete fixtures remain useful
research artifacts; they do not establish that a broader SSA rewrite will help.
Passive production code inspection also finds hardware scalar widening,
whereas the candidate uses integer conversion/classification, generic-target
instruction choices and a new 184-byte helper frame. These are cost mechanisms,
not measurements assigning cycles to individual instructions.

## Broad main-code observer shortcut, October 9

A new diagnostic checks a conservative negative observer predicate against the
actual existing callback, and always returns the existing callback's answer.
It admits only aligned original main-DOL text, excludes a generated union of
static hook addresses/ranges, retains the actual finite observer filter, and
checks current interrupt, pending-return, overlap and tracing state. Unknown
state falls back. Its ROM-free fixture passes 842,559 queries in optimized,
ASan, developer and census profiles, with full CPU/RAM and read-only checks;
four deliberate implication faults are detected. This is predicate evidence,
not permission to omit any side effects of the original callback.

The qualified diagnostic host completed the original visible C2 intro with all
34 route and preservation checks passing, no sampler or automated input, zero
pipeline creations and zero implication mismatches. Between VI 750 and 1500:

| Decision | Queries |
|---|---:|
| Proved eligible for the proposed shortcut | 100,685,229 |
| Finite observer bucket; retain complete callback | 20,438,657 |
| Outside admitted main-code domain | 1,654,128 |
| Static exclusion | 682,759 |
| Total | 123,460,773 |

The proposed shortcut applies to 81.55% of this window's queries. Across the
whole 1,800-VI run it applies to 175,373,317 of 217,671,251 queries. None falls
back for live policy, interrupt, pending return, overlap or raw REL state in
this particular route; these guards remain required for other gameplay.
Eligibility is not a speedup measurement. Diagnostic atomic counters and the
complete callback still execute. A counter-free candidate additionally needs
source proof of skipped side effects and borrowed-state ownership, followed
by controlled state/pixel and CPU/wall comparisons.

The subsequent source audit finds no required skipped cache initialization or
other write. Finite-family observers are already short-circuited on eligible
addresses. The current host owns CPU/event/alias mutations on its game thread;
DSP interrupt delivery is synchronous, and the query calls no dispatcher or
state-replacement service. This supports that existing ownership contract,
without claiming safety for concurrent external setters. The candidate adds
only the unchanged proof header and a `PROVED` return before the original
callback body. Its original fallback and tracing remain byte-identical.
The counter-free host builds successfully with one replaced main object and
169 unchanged physical link inputs. Its eligible native path calls only the
small event-storage getter; the original full predicate is reached on fallback.
The callback still saves four nonvolatile registers and allocates 40 stack
bytes, without RSP-relative spills. The retained full-predicate and chassis
callee bodies are unchanged.

Fresh controlled-EFB runs of both hosts then pass all six complete logical CPU,
MEM1, MEM2 and ordered-alias checkpoints and full 1,228,335-byte P6 equality.
Both consume all 1,134 recorded inputs while retaining the physical GPU reads.
The recorded physical-value differences (104 and 58) remain evidence of the
replay's diagnostic scope. Independent raw-log/pixel inspection confirms this
parity; it does not establish every-instruction or full-game equivalence.

The fixed eight-run `A-C-C-A,C-A-A-C` timing comparison completes all eight
routes without errors. Every primary/segment guest-work cursor, terminal GX,
clock and dispatch record matches, with no new pipeline creations. In VI
750--1500, mean main-thread CPU falls from 16.2852 to 15.5625 seconds (-4.44%),
wall from 16.3645 to 15.6862 seconds (-4.14%), and thread cycles by 4.09%.
Individual paired changes are:

| Pair | Main-thread CPU | Wall | Thread cycles |
|---|---:|---:|---:|
| 1 | -3.31% | -2.98% | -3.15% |
| 2 | -10.92% | -10.67% | -10.08% |
| 3 | -6.89% | -6.34% | -6.22% |
| 4 | +3.82% | +3.88% | +3.48% |

The predeclared all-four-pair CPU/wall consistency gate fails. Version 1 stays
inactive with all failures retained; there is no same-version rescue run,
ordinary-play speed claim or promotion. These results justify investigating a
materially leaner query, rather than claiming that the slowdown is fixed.
The timer's unresolved CPU calibration step remains recorded, and no overhead
is subtracted. The prospective changes are an outlined original fallback, a
leaf eligible path, and removal of the conservative finite filter only after
complete observer-domain proof and expanded qualification.

Exact private evidence is retained under
`build/deep-debug-20261008/host-maincode-negative1/`:
`fixture-attempt2/result.json` (`f3c0e65b...`),
`host-attempt1/result.json` (`26109ebd...`), and
`title-maincode-negative-c2-1/{result,reason-counts}.json`
(`47b6589b...` and `331512e2...`). Original player data and installed binaries
remain untouched. The counter-free build is `host-fast-attempt1/result.json`
(`5820b50b...`); fresh runtime evidence is under `runtime1/`, including
`native-efb1/maincode-host-per-vi-input-parity1.json` (`b5e5d2c4...`) and
`controlled-input1/maincode-host-per-vi-eight-run1.json` (`69df2b4f...`).

### Getter-free revision: controlled timing passes

Version 2 removes the conservative finite Bloom filter only after a constructive
source audit proves that all six observer families stay inside the static union:
46 raw DOL keys are excluded, and two private keys are outside the admitted DOL
domain. Dynamic configuration and return context select subsets of these keys;
they do not introduce arbitrary addresses. Interrupt, pending event return,
overlap, live tracing/policy and raw REL checks remain fresh and unchanged.

The event-storage pointer is borrowed once before callback registration from a
process-lifetime static object. Reset mutates that object in place. A null pointer
uses the original callback, and teardown clears it after final reset and before
CPU destruction. The original callback is outlined with `noinline`; no other
new compiler attributes or cached eligibility answers are introduced.

The actual four-profile fixture compares the complete candidate callback with
the original, including identical trace-counter deltas. Each profile passes
842,606 queries, 1,598 full 32-MiB RAM comparisons and 206 static-domain groups.
This retains the complete earlier corpus and adds seven pointer lifecycle cases
and 40 finite-filter collisions. All four deliberately broken consumers fail
the implication check. Optimized and ASan profiles admit 836,630 queries;
developer/census profiles conservatively use the fallback throughout.

The private one-object host build retains all 169 other physical link inputs.
Its actual fast wrapper has 491 instruction bytes plus five alignment bytes,
no nested calls, no saved registers, no stack allocation and no stack operands.
Unknown cases tail-jump to the outlined fallback. Original full-predicate and
chassis instruction bytes and named relocations remain unchanged. Whole main
text grows 480 bytes against the control and equals version 1's text size.

Fresh intro runs match all six complete CPU/MEM1/MEM2/ordered-alias checkpoint
rows and every byte of the 1,228,335-byte P6 image. The fixed eight-run timing
block then passes all route checks and the predeclared paired-gain gate:

| Pair | Main-thread CPU | Wall | Thread cycles |
|---|---:|---:|---:|
| 1 | -15.01% | -15.61% | -15.38% |
| 2 | -13.06% | -13.63% | -13.37% |
| 3 | -14.89% | -14.81% | -14.72% |
| 4 | -3.30% | -2.32% | -3.22% |

In the predeclared VI750--1500 window, mean main-thread CPU changes from
17.0703125 to 15.06640625 seconds (-11.74%), wall from 17.2411732 to
15.206241725 seconds (-11.80%), and thread cycles by -11.86%. Primary and all
five segment work/endpoints, terminal GX, guest clock and dispatch match; all
eight routes report zero terminal pipeline creations. No timing run captures
images, hashes guest state or samples PCs. All eight runs and calibration rows
are retained, with no overhead subtraction.
An independent audit reparses all 1,801 VI and 1,025 calibration rows in each
run and reproduces the four pairs, means, exact-work checks and passed gates.

This qualifies the named loaded, unpaced 1,134-EFB-replay experiment. It does
not establish ordinary displayed FPS, full-game speed or shipping promotion.
CPU calibration materiality remains unresolved. Public source landing requires
separate qualification of two host objects because the current public event
implementation lacks the private grouped storage/getter. No installed binaries
are changed by this experiment; version 1's failed block remains closed.

Private evidence in the same directory: `finite-true-domain-proof1.json`
(`d200bdfd...`), `fixture-fast-attempt2/result.json` (`4103728e...`),
`host-fast2-attempt1/result.json` (`d0db0fbf...`),
`predicate-cost-fast2/codegen1.json` (`568baf06...`),
`runtime2/native-efb1/maincode2-host-per-vi-input-parity1.json` (`b90068f2...`),
and `runtime2/controlled-input1/maincode2-host-per-vi-eight-run1.json`
(`2da614a3...`), with `runtime2/independent-timing-audit1.json`
(`626774c8...`).

## Ordinary intro with paired published PCs, October 9

The actual ordinary certified host completed another visible 1,800-retrace
intro with the same C2 module, copied player files, warmed cache, real clock,
audio, FIFO display and SmoothMotion off. All 32 route, sampler and preservation
checks passed; the owned game exited normally and was drained. Mouse and live
controller input were disabled. Pipeline compilations and GX rejections/failures
were zero. The slowdown remained: the log reported 30 below-target seconds out
of 34 watched seconds, with a lowest speed of 57%. Sampling perturbs this run,
so these figures do not qualify ordinary throughput or a new speedup.

The external sampler captured 4,455 native RIPs over the observed retrace
window 790-1487. Each accepted RIP has a four-byte read of the C2 CPU's published
PC during the same owned suspension. The PC offset is 0x280 in the qualified
CPU layout. The published PC can be deferred or left at a dispatch boundary;
it is an association with current state, not proof of the exact interrupted
PPC instruction. External log timestamps provide only coarse time brackets.
Two inherited runner prose labels incorrectly say there is no published-PC
read; the frozen layout and complete read series explicitly supersede them.

Of all samples, 3,490 interrupt the game module and 772 interrupt the host.
Restricting the joint counts to actual translated chunk/loop locations gives
J3D/J3DGD 423, collision 313, and GX 176. These are distributed routine
associations, not exclusive costs or removable time. The 281 all-location
save/restore associations include host and readiness glue; only 70 associate
those exact two routines with translated chunks/loops. Existing native matrix
replacements also account for some high-count matrix routine associations.

An independent calibration used the eight already byte-qualified CodeView
tables. Of 601 samples with both native-source and published-PC routine names,
584 agree at routine level (97.17%). Another 74 have shared/line-zero native
regions and one lacks a published range. Caller/callee transitions and shared
tails remain explicit limitations. Among all 676 calibrated samples, 201
interrupt instructions accessing cycle/PC metadata. No single arithmetic leaf
dominates, and this does not reopen the closed PC-defer, SSA or tail0145 trials.

The next bounded candidate removes the in-function copies inserted by
`fast_blocks.py` from exact C2 chunks 0145/0201, retaining their original slow
bodies, charges, refunds, entry labels and dispatch ABI. This differs from the
older whole-function precise twins and the closed musttail partition trial.
Source size falls about 39%; actual native size, correctness and performance
remain unproven at this checkpoint. No installed build changes follow yet.

Evidence is local under
`build/deep-debug-20261008/ordinary-public-guest-pc-profile1/ordinary-public-published-pc-intro1/`:

| Evidence | SHA-256 |
| --- | --- |
| Complete run result | `3ef6799f456d078fe05b265d755e3ec5394b382dd1ea40761d2ae716e0ea0f86` |
| Native RIP and published-PC capture | `2b1857c19325110d02432953af334d8e08b4e0d5cf648395d326e82f76b5fc2a` |
| Full association matrix | `511131bc867758f1b69f88877d259af1d21a9eb7ff1c33b16cb3c7c64204195a` |
| Independent CodeView calibration | `16afbc4859f227d6f513ef68b89af9c3ae1f8edb0616260d0356ebd31b4e17fb` |

### Removing in-function fast copies: correct, timing gate fails

The bounded two-chunk candidate was compiled, linked and run. It removes 922
in-function fast copies from chunk 0145 and 606 from chunk 0201, preserving the
original unified bodies and their instruction-level checks. Reapplying the
frozen historical transformation reproduces every executable source byte;
the marker comment has a different position. The original ordered compiler
flags, effective O2 policy, header bindings and 833 other module objects are
retained. Both changed objects compile successfully, and the actual linked
module passes input-order, provider, export, import, resource and preservation
checks.

Native text falls from 709,841 to 572,913 bytes in chunk 0145 and from 679,971
to 541,315 in chunk 0201: 275,584 bytes, or about 19.83%, combined. The main
frames fall from 152 to 104 bytes and 184 to 152 bytes respectively. Some small
loop frames grow, so this is not a uniform frame improvement. Static call-site
counts and smaller code do not establish lower execution cost.

Fresh correctness runs use the same qualified `2b9bc61e...` diagnostic host
with C2 `54119177...` versus the candidate `bd2e2351...`. Both pass all 32
runtime checks, exit normally and drain. All six complete CPU, MEM1, MEM2 and
ordered-alias checkpoints match, as does the complete 1,228,335-byte P6 image.
Both consume all 1,134 recorded EFB contexts while retaining physical reads;
their observed physical differences, 21 and 30, remain recorded. Terminal
pipeline creations are zero. An independent raw correctness audit passes.

The fixed eight-run order is A-G-G-A,G-A-A-G. Every game run passes, with
identical primary and five-segment work, absolute start/end cursors, GX totals,
terminal guest clock and dispatch. The primary 750-1500-retrace window executes
547,802 blocks, 6,075,000,001 guest cycles and 99 EFB entries in every run.

| Primary metric | Control mean | Candidate mean | Change |
| --- | ---: | ---: | ---: |
| Dispatch-thread CPU seconds | 17.80078125 | 18.3671875 | +3.182% |
| Elapsed seconds | 18.2779101 | 18.8790412 | +3.289% |

The four paired CPU changes are +2.249%, +0.606%, +11.603% and -1.701%; paired
elapsed changes are +2.187%, -2.235%, +15.662% and -2.219%. This fails the
preregistered consistent CPU/elapsed reduction gate. The candidate stays
inactive, and every raw run is retained. The mean regression under the
authorized background load is not a proof of its intrinsic cost on an idle
machine. Pre-case one-second load samples range from 25.67% to 41.83%; they do
not measure load throughout each run. Timer CPU-step precision remains
unresolved, and no estimated overhead is subtracted. These are controlled
diagnostic workload measurements, not ordinary visible FPS or a full-speed
gameplay result. The existing C2 module remains selected.

Local evidence under `build/deep-debug-20261008/`:

| Evidence | SHA-256 |
| --- | --- |
| `unpaired-hot-chunks1/attempt1/result.json` | `05c11c02989bd759076ae6af569440a9ec990621eff38f0bcce13f42ca15dcf1` |
| `unpaired-hot-chunks1/attempt1/codegen1.json` | `d8979b05919aae14bc2af81ce4850b53be144a7941197d3c497b6f9bcb8a5e03` |
| `unpaired-hot-chunks1/module1/attempt1/result.json` | `7b648e78bbf0378f6771754bda8776c5c563c9c340cc0065162d4ca3ecd04df2` |
| `unpaired-c2-runtime1/native-efb1/unpaired-c2-per-vi-input-parity1.json` | `a999a372771e1f21a25a37a2a7848f8fad83a152cfb538d0f339e6ad314cc9b2` |
| `unpaired-c2-runtime1/controlled-input1/unpaired-c2-per-vi-eight-run1.json` | `7caa69ee74cdbca923893f227f3380eef773f329105a7c7a6bde5d7e4dd1d55d` |
| `unpaired-c2-runtime1/independent-correctness-audit1.json` | `5a4b080175d63b0c5481451353158da8078d1cf67b1e2dbfd7d9a927c959d4dd` |
| `unpaired-c2-runtime1/independent-timing-audit1.json` | `a093cc8b03cd0882c0713c57d411b2d05f1548aedc4c5c1889130b5c4d06e7bd` |

## Ordinary public-host comparison, October 9

An unsampled visible A-C-C-A block compares the ordinary control host
`e5c93d9d` with public candidate `13c83a6a`, using the identical accepted C2
module. Both use copied player data, the same warmed cache, visible
2560x1440 output, audio, live RTC and wall pacing, with Smooth Motion off.
Sampling, checkpoints, EFB input replay, per-VI recording and experimental
observation capabilities remain off.

Two reporting mistakes were corrected before completing the block. The
producer's held-adjusted CPU busy value has no upper clamp, so a valid 101%
reading must not fail parsing. The ordinary host also unconditionally prints
one `observation-facts=off` status. The corrected check requires exactly that
line and rejects enabled, duplicate or additional capability registrations.
The first control's failed validation remains in `ordinary-public-visible4`;
it is not promoted or substituted into the subsequent four-run block.

All four subsequent games pass 32 individual checks, reach 1800 retraces,
exit normally, drain their owned jobs and create zero graphics pipelines.
The full comparison nevertheless **fails the exact terminal-work gate**:
live RTC timebase values differ, dispatch totals differ by two blocks, and
the final control has eight fewer in the submitted/planned GX counters and
sixteen fewer zero-quad noops. These differences remain visible; the gate
was not relaxed and no speedup is established by this block.

| Run | Rolling rate mean, endpoints 750-1500 | Whole-process elapsed seconds | Pre-run system CPU |
| --- | ---: | ---: | ---: |
| A1, control | 44.631 | 37.891 | 39.712% |
| C1, candidate | 49.294 | 35.922 | 54.785% |
| C2, candidate | 41.729 | 39.250 | 45.769% |
| A2, control | 38.768 | 41.610 | 38.654% |

Rates are raw rolling retrace diagnostics grouped by endpoint, not displayed
FPS or exact fixed-VI elapsed measurements. CPU values are one-second
pre-run snapshots, not continuous load controls. The two candidate passes
use identical binaries and show substantial variation. The maintainer
reported 23-24 FPS and then about 19 FPS while watching the sequence. A
process snapshot after the latter message identifies the active window as
the final control; it does not establish the exact frame/scene of either
reported reading. Full-speed ordinary intro or crowded gameplay is still
unverified. The earlier controlled eight-run result remains separate.

Local evidence under `build/deep-debug-20261008/`:

| Evidence | SHA-256 |
| --- | --- |
| `ordinary-public-visible4/batch-result1.json` | `007de714af661a371ba0cad1b5594f07241f00438fbad7cf52d8e2ec28caba88` |
| `ordinary-public-visible5/preparation1.json` | `671ccebaed7408d9d6b73dd9161cef60d3688ef20fbcaeff1aa66578724c84e3` |
| `ordinary-public-visible5/batch-result1.json` | `1c95db11201d6be1297d732073674170e447a2b7c2c3a76067f01df1438ebb98` |
| `ordinary-public-visible5/ordinary-public-visible-abba1.json` | `1189c6b42d64b51705a8b24491a967be7efc56a913605d1a9fcb6ff702ae6803` |
| `ordinary-public-visible5/local-raw-audit1.json` | `3621b14209b0cb12570c0c326e5d1e545c59d717f64023319e59f3cc8a4b96e2` |

### Private calling-convention feasibility

The pinned Windows Clang 19.1.5 accepts `preserve_none` and rejects both
directions of incompatible ordinary/private function-pointer assignment.
Actual COFF instructions and Windows unwind records confirm the private
convention. This is compiler evidence; no emitted probe object was executed.

A private prototype retains all 1528 original fast copies and changes only
the two selected main definitions, eight static-loop conventions, matching
private declarations, default-ABI wrappers and the selected original-function
dispatch calls. Three actual objects compile successfully with the original
ordered flags and exact substituted dependency closures of 48, 45 and 53.
No game module was linked or launched from these objects.

The machine code shows a significant boundary tradeoff. Main frames shrink
from 152/184 to 136/152 bytes, and the eight loop frames become 40 bytes.
Each default wrapper instead saves seven GPRs and XMM6-XMM15 in a 216-byte
frame. The shared `dolrecomp_call_original` frame grows from 56 to 248 bytes
and gains the same wide save set. The main functions still contain 9067 and
7673 static call sites respectively. Executable-section changes are only
-2336, -2848 and +240 bytes for the two chunks and module dispatcher.

This partial conversion moves preservation work into public boundaries and
the shared dispatcher. Static size/frame changes do not establish dynamic
cost or a speedup; the prototype remains inactive. Propagating a private
convention across internal calls would be a separate change, with public
callbacks, hooks and function-pointer types kept compatible.

| Local compiler evidence | SHA-256 |
| --- | --- |
| `internal-calling-convention1/attempt1/result.json` | `483bf7ef4dfe3b7fdb628de9388e2f76f7cf508d0f9cec8a327c987f7e97bd9c` |
| `internal-calling-convention1/candidate-compile-attempt1/result.json` | `e52b968c2e4abcbd23ffd1a3aa07a25c1cfb8447ca04fcf3f46a03121fd62971` |
| `internal-calling-convention1/candidate-compile-attempt1/codegen1.json` | `b37bc8fcf380def8651a2831ea5f9a7d52765bed7170e6f02bdf1d7de9f597f8` |

## Complete-function LLVM prototype, October 9

The ordinary 19 FPS feedback remains unresolved. Passive inspection of the
ordinary log also corrects a unit assumption: GX, present and end-frame values
are milliseconds accumulated per wall second, not milliseconds per frame.
The label `gpu` measures host `aurora_end_frame` elapsed time; it is not a
hardware GPU timestamp. Dip intervals show roughly 93-100% main-thread busy,
with GX and presentation accounting for only a few milliseconds per second.
This supports investigating main-thread work, without proving a single cause.
Audio has only eight of 4,286 source associations in the reviewed union;
no audio-quality or audio-offload change follows from this evidence.

A detached source overlay now adapts ExpansionPak/DolRecomp commit
`c876e2b9e022e084aa9a690fe15e2ad79a0706cd` to the original C2 CPU, scheduler,
memory callbacks and selected-call semantics. It uses the existing 3552-byte
CPU layout and ordinary Windows C ABI, with local register SSA across a
complete function CFG. It rejects incompatible runtime/native/state modes.
Exact original scheduling rows preserve paid/precise charges, deliberately
stale PC values, suffix writes and refund points. Callback reloads invalidate
entry-derived control facts. Canonical LFS widening uses integer operations
to retain signed zero, subnormals and signalling/payload NaN bits.

All 49 actual generator translation units compile. The actual complete
93-instruction GroundCrossGrpRp range emits all 93 resume entries, its
traversal loop, retained direct/indirect call continuations and original
central-return cases. Independent inspection confirms ten integer-only LFS
paths and no host float-widening instruction. The pure bit model covers
100,352 cases; this is not execution of the emitted object. Three real C2
adapter translation units also compile, including the complete private
original chunk with narrow scoped-return additions. Its original body and
the retained call bodies reconstruct byte-for-byte.

The first complete object lacked Windows unwind metadata and was rejected
for execution. Explicit asynchronous unwind emission repairs that omission:
the successor's `.pdata`/`.xdata` cover the exact private body and all 13
prologue operations. Its executable bytes and named relocations match the
prior object exactly. The default wrapper is a frameless tail jump. A prior
unused-constant compiler failure and a later runner bookkeeping failure are
retained with their actual logs and drained children.

This is still a compiler prototype. No new game module was linked, no emitted
game code ran and no performance comparison was made. The conservative object
has 26 memory-service frontiers with substantial guest-state publication and
reload traffic, making it unsuitable to assume a speed gain. A guarded fixed
MEM1 draft aims to avoid that traffic on ordinary RAM while retaining original
alias/journal/reservation/callback behavior. It is being reviewed separately.
Outer entry routing also requires an explicit return-ownership channel;
CPU.pc alone cannot distinguish a central return from a side exit. Original
scoped return gates must remain authoritative even across nested entries.

The function accounts for only 41 of 4,286 source associations in the current
ordinary profile. It tests the compiler architecture; optimizing it alone is
not a sufficient full-speed product fix. The generic compiler source is
preserved in `patches/compiler/drafts/canonical-c2-whole-function-llvm.patch`,
inactive in all game builds. Generated game code, scheduling rows and player
data remain local.

Local evidence under `build/deep-debug-20261008/`:

| Evidence | SHA-256 |
| --- | --- |
| `ordinary-feedback-assessment1/FINDINGS.md` | `c04810695d57f7f4150162f8362f252ed6e571c8f2d407752bd9e347b85dcd78` |
| `moderngekko-whole-function1/build-attempt4/output/groundcross93.obj` | `5b6c79ea4087bd32bcaccf2e9858095ed0e80aae14bc97a0c39dfa687f973427` |
| `moderngekko-whole-function1/build-attempt4/output/groundcross93.ll` | `7b22ac6e7aba3e250121bebc010b24a8fdc7c2ea811a23248e813b45c684cff2` |
| `moderngekko-whole-function1/build-attempt4/passive-codegen3.json` | `803ea7c1ad7501e23d366036454673e5a03302dedc59eafbb92fa30968460048` |
| `moderngekko-whole-function1/b-passive-audit2.json` | `a713273e19f03eaaddc0743564630fbef013843e55cc5d93f3af915c9c811062` |

### Guarded normal RAM and explicit return ownership

A separate opt-in successor now compiles all 49 translation units afresh and
emits the actual function with guarded fixed-MEM1 access and a paired central
return relay. The routed original C2 chunk and repaired canonical memory
service also compile. The fixed-storage module interface is ABI 5; the CPU
layout is ABI 6. These are distinct version numbers.

Independent actual-IR/COFF inspection admits all 93 resume entries and both
local backedges. Each of the 24 load and two store fast regions avoids full
guest-state publication/reload and runtime calls. Fresh raw-address/span and
alias guards remain; stores additionally require a null journal and preserve
matching reservation invalidation. Rejected accesses call the original
canonical service once. FPRF and callback-mutated state survive the joins.
The framed 57,536-byte private body has exact Win64 unwind coverage and restores
all nonvolatile saves; its ordinary wrapper remains a frameless tail jump.
The initial IR token count reported 4,559 PHIs; counting actual `phi` opcodes
corrects this to 4,401. There are 1,385 CPU loads and 3,904 CPU stores across all
paths. These counts include slow paths and do not establish dynamic savings.

Every actual guest `blr` that passes the original budget check now reports a
central transfer through a nested scoped reason channel. The unchanged original
C dispatcher then owns its range check, depth/SP return gate and continuation
switch. Other exits return from the chunk without PC-based inference or retry.
This deliberately adds an exit/reentry at guest returns; it is not a claim of
unchanged machine cost. Both new options default off.

Two passive parser failures were naming/opcode-spelling assumptions, preserved
before their narrow corrections. The earlier claim that two source snapshots
were substantively stale was also corrected: their normalized contents match;
only CRLF/LF differed. The actual integrated source retains asynchronous unwind
emission. An unused unsupported-width `abort` dependency was replaced by a
trap in a new service source, preserving the old source/build. Every admitted
width's body remains unchanged; the emitter rejects other widths.

The generic delta is preserved in
`patches/compiler/drafts/canonical-c2-fixed-mem1-and-return-relay.patch` and
private source commit `c989f634`. The private C2 module now links successfully
with one object replacement and three appends: 838 direct objects, 834 retained
original objects, 15 physical providers and 855 LINKREPRO members. Actual input
bytes and order match the frozen recipe. Original PE imports, exports, ordinals,
resources and ABI fields remain exact; the five original C2 CPU/REL/PSQ objects
and dispatch/export/mod selection remain retained. No game execution or
performance qualification is established by these compiler/link checks.

| Successor evidence under `moderngekko-whole-function1/` | SHA-256 |
| --- | --- |
| `build-attempt5/output/groundcross93.obj` | `53f32101b77787a8f8b63bd6b8776b2d72fa6ab96eb1a9d9c41730d3fafa76af` |
| `build-attempt5/output/groundcross93.ll` | `afa8357d82b5cd40aecfc1259f6b1dc3edb3c85ab1aa1e62902cbd3db8d8fa8c` |
| `build-attempt5/passive-fixed-memory1.json` | `0369ae1724f362c061c9f47392fd4b9f646d3630c958d84692413bf07a3589c4` |
| `canonical-fast-memory2/peer-shader-source-correction2.json` | `e3b3c7ddb3ebcc6582d70062eaca839541479b8b86af3fa39cec4d2e04bd012a` |
| `module-plan1/attempt1/result.json` | `28c9597fdc13d237ac04e6d6e480f1f12befa31cad1792f201621392fa351cc6` |
| `module-plan1/attempt1/output/gGZLE01_recomp.dll` | `3fa09e8306c5695c3d347a0531ff6f418961c9cb7d9c460794689e6d3355506f` |

### Actual emitted-code memory differential

The actual integrated backend now emits two synthetic memory consumers: the
canonical service path and the guarded fixed-MEM1 path. Both execute against
an oracle using the original C2 memory, gather and FP helpers, with eight
retained real C2 providers. All 382 cases and 764 comparisons pass. Each
comparison checks all 3,552 CPU bytes, all 32 MiB of MEM1, alias/MEM2/external
pointer storage, callback/journal/FIFO order and CPU snapshots, registry
mutation delta, host exceptions and MXCSR.

The corpus includes unaligned and boundary accesses, raw/masked alias priority,
write journals, matching and unrelated reservations, synchronous callbacks that
change live CPU controls, lazy-FPU exits, LFS bit patterns and short scheduler
budgets. Actual counters record 205 admitted fast accesses, 134 fallback
accesses, 148 direct reads, 57 direct writes and ten mutating callbacks. These
are route counts, not performance measurements. All eleven owned native rows
exit successfully and drain.

Before execution, independent inspection admits both actual COFF objects,
ordinary CPU-pointer ABI, all 30 emitted resume entries, complete unwind and
nonvolatile restoration, and exact imports. The standard `_fltused` data marker
is supplied by the actual linked CRT library. The earlier parser rejection is
retained. A pre-native entry-count error and a later mandatory-header gate
failure are also preserved; the corrected gate checks the actual backend
headers and an entire inverse to the production CPU header, while retaining
the production-header checks for the C consumers.

This is not whole-function gameplay or speed qualification. Only ten of the
30 emitted entry points are exercised. The synthetic ADDI prefix creates no
pending FPRF, and there is no second memory access following a callback that
changes aliases. Those paths retain source review only. Full intro state/pixel
comparison and proof that the game executes the new routine remain separate.

Local evidence under
`moderngekko-whole-function1/canonical-fast-memory2/differential1/`:

| Evidence | SHA-256 |
| --- | --- |
| `recipe3.json` | `578d4e98cc6713cf8a2aeb33c47c813fcedf043675ffa07a1062ed1ca16b24a4` |
| `attempt2/result.json` | `6467e86699a5767e4af20c4880dc89a011021659b48b4414ddee2cc8af8fd6c5` |
| `attempt2/passive-admission1.json` | `c6e3e8cb986f7d014d95abd4860f4ae46369fb2b9f22e57ee77247c4c2aa065d` |
| `peer-shader-header-repair3.json` | `6cfdb15094be341895699a7460df726d8d30fe1935254d0a3af997e83153d8f7` |
| `attempt2/independent-actual-audit1.json` | `9438e3aa834f83e4a4c8011fbd14698e65d84af8640f68f0583d38b6645bce4e` |

### Whole-function controlled intro correctness

The original C2 module `54119177...` and private LLVM candidate `3fa09e83...`
now complete fresh 1,800-retrace intro runs on the same qualified `2b9bc61e...`
diagnostic host. Both pass all 32 route, runtime and preservation checks, exit
normally and drain. All six complete logical CPU/MEM1/MEM2/ordered-alias
checkpoint hashes match, and the complete 1,228,335-byte P6 image is equal.
Both consume all 1,134 recorded EFB inputs while retaining physical reads;
their physical differences, 31 and 73, remain recorded. Terminal pipeline
creations are zero.

This qualifies state/pixel parity on that controlled route only. An independent
audit reparses the six raw checkpoint records, compares the complete P6 bytes,
and confirms all 1,801 per-VI work cursors, primary/five-segment work and terminal
GX/clock/dispatch records match. These are source-qualified complete logical
state hashes, not retained full RAM dumps.

A separate candidate run now uses the unchanged owned native sampler with the
same guest options, copied data, EFB inputs, checkpoints and capture. All 33
runtime checks pass, the game exits normally and drains, and comparison again
matches all six checkpoint hashes and the complete image. Of 4,283 native RIP
samples, 85 at 75 unique addresses lie inside the exact linked 57,536-byte LLVM
private body. The actual image, MAP, unwind range and mapping are pinned. This
proves execution of the new implementation, without proving every instruction,
resume entry or path. Sampling perturbs timing; its frequencies are not costs.
No fixed-order throughput comparison or ordinary visible FPS improvement has
been established for this candidate, and the playable build stays unchanged.

| Evidence under `moderngekko-c2-runtime1/` | SHA-256 |
| --- | --- |
| `correctness-batch1.json` | `870f49ad1869aabec0bc2ceb876e7f9738a9ded7966828cc396a3f5bb39b2834` |
| `native-efb1/moderngekko-c2-per-vi-input-parity1.json` | `077f016bfc8b84ccf55d12c5a4c659e8ef6b5d8a3263cbfd27795f4bb005f0d8` |
| `independent-correctness-raw-audit1.json` | `6b0a09b0836a1f9f65dca1287b2f910063f524fec68e9008a0e45ef52c04e070` |
| `native-efb1/moderngekko-c2-per-vi-g-coverage1/native-coverage.json` | `61f82c6926f599b2fd95f999d16d0b19b3d465f72bcfc67789e996b6529b5756` |
| `native-efb1/moderngekko-c2-per-vi-coverage-input-parity1.json` | `cdbb1fa9dee129dea89dae932fef033ed292855672727417392ba4367a034d91` |

### Initial-entry filtering: correct, controlled timing gate fails

The optional generic compiler patch
`patches/compiler/drafts/canonical-c2-entry-filter.patch` selects only the
normal F4 entry for the private 93-instruction function. All instruction
regions, scheduling rows and local continuations remain; other external
resumes and the five old central continuations retain original C2 execution.
Default compiler behavior still reports every canonical instruction entry.
The five-file patch has SHA-256
`e30baa9013838ab05cc272ed46d124bf52d8f1d6ffb271734fbffbd5a6d5008d`.

Actual fresh compilation passes all 49 generator translation units and the
paired routed-C translation unit. The emitted COFF is 32,709 bytes; passive
inspection binds its sole F4 external entry, 21 load/two store fast/service
pairs, imports and every native return to valid Win64 unwind data. Three
memory arms removed by optimization are proven unreachable from F4 in the
prior LLVM CFG; their original C2 tail remains available. The private native
body shrinks from 57,536 to 30,569 bytes, instructions 11,869 to 6,681, frame
328 to 264 bytes and optimized IR PHIs 4,401 to 1,130. None is a timing result.

The isolated module `5a4bdb02...` links with the original C2 dispatcher, CPU,
REL and mod-hook providers. Its exact PE import/export/resource surface
matches the accepted C2 module. Fresh correctness runs on the same `2b9bc61e...`
host pass all 32 checks and compare equal at all six complete logical
CPU/MEM1/MEM2/ordered-alias checkpoints and all 1,228,335 P6 bytes. An independent
raw audit also matches all 1,801 work cursors, primary/five-segment work and
terminal GX/clock/dispatch records. All 1,134 EFB contexts are replayed while
physical reads remain; physical differences of 96/9 are retained.

A separate candidate run passes all 33 checks, matches the six checkpoints
and P6 again, and records 49 hits at 48 native RIPs inside the exact new
30,569-byte private body. The unchanged owned sampler and pinned MAP/unwind
range establish actual execution. This does not establish every path,
instruction or resume, an access count, or a performance gain.

The subsequent eight-run comparison uses the frozen order A-G-G-A, G-A-A-G,
same host/data/cache/1,134-context replay and original 750-1,500 VI primary
window. Sampling, state hashing, capture and source/build jobs are stopped.
All eight native runs pass. Exact primary/five-segment work and start/end
cursors, terminal GX/clock/dispatch and zero terminal pipeline creations
match. The analysis fails only the preregistered CPU/wall gain gate:

| Pair | Dispatch CPU change | Wall change |
| --- | ---: | ---: |
| A1 to G1 | -3.277% | -4.603% |
| A2 to G2 | +1.303% | +1.873% |
| A3 to G3 | -1.969% | -2.414% |
| A4 to G4 | +17.774% | +18.637% |
| Four-run means | +3.320% | +3.253% |

Original/candidate mean dispatch CPU seconds are 18.121094/18.722656 and wall
seconds 18.376241/18.973945. Mean thread cycles rise 3.148%; secondary whole
process CPU/wall rise 3.985%/4.201%. No overhead is subtracted. The timer
calibration's CPU step remains unresolved, and the one-second pre-run system
busy samples range from 33.301% to 50.865%. They are not continuous background
load measurements. Mixed pairs under that load establish neither a repeatable
gain nor a causal regression. G4 is retained; no retry or dropped sample
rescues the failed gate. The candidate remains inactive, with no ordinary
visible FPS or whole-game qualification. It does not resolve the player's
23/24 versus 19 FPS observations.

An independent audit reparses every raw per-VI log and all 1,025 calibration
readings per run, recomputes the primary/five-segment intervals and four pairs,
and reproduces the strict gain failure. All 34 runtime checks per timing run,
normal owned exits, exact work and retained slow G4 are confirmed.

Two additional narrow findings remain separate from speed claims. Bootstrap
REL aliases (1,900) and all nonempty REL sections (2,316) are outside MEM1;
the 3,800 checkpoint spans repeat the same aliases in two metadata walks.
Those tables therefore do not by themselves disable the direct-RAM guard.
Actual arbitrary state-load aliases and other admission guards still matter.
An offline decode of the prior complete-entry run binds every sampled byte
to its linked body and finds five weighted samples in exclusive post-load
fast arms after three actual MEM1 loads. It sampled no load opcode itself;
this is evidence of those fast-arm executions, not a hit-rate or timing census.

| Private successor evidence under `build/deep-debug-20261008/` | SHA-256 |
| --- | --- |
| `moderngekko-whole-function1/build-attempt6/result.json` | `1727db2baa5d77c95726dd3f85facda0e59ffbacadf2510bf49ada8dd1e1eeb2` |
| `moderngekko-whole-function1/build-attempt6/passive-entry-filter1.json` | `07b939435ca22602710b4f1a1b9bcd7a396c45d7bb69869118c124f057da87ca` |
| `moderngekko-whole-function1/module-plan2/attempt1/result.json` | `d47d89854df02def1779ebd384e81b1c814b87ebb95b14cd8f909363fa37f8a7` |
| `moderngekko-entry-runtime1/correctness-batch1.json` | `63d538e708e39f86bda91efeb14036901c8f57f7e2501be912393002251d4443` |
| `moderngekko-entry-runtime1/independent-correctness-raw-audit1.json` | `0a50d7dd9b5777192eb31d3cdaebab5d0c8b0f46ced026119b0f1f15e760bdc3` |
| `moderngekko-entry-runtime1/native-efb1/moderngekko-entry-per-vi-coverage-input-parity1.json` | `33cad9ac91125886b6d497f3e919d889be4cdeac1f0ec03020683b4acfc71b03` |
| `moderngekko-entry-runtime1/controlled-batch1.json` | `c367a18b19473e782e741fc2333006f5aa153eab587ed74dabe046b2cb581902` |
| `moderngekko-entry-runtime1/controlled-input1/moderngekko-entry-per-vi-eight-run1.json` | `ad863b1894590e53e80ba8a0e40e45a52d40e6f88e78da0fcfbf2b1c1ce8ceba` |
| `moderngekko-entry-runtime1/independent-eight-raw-audit1.json` | `258c105622d9d9a4d237b5b8011aa95c445346815f73140d07b2a719dd792c81` |
| `moderngekko-whole-function1/alias-domain-proof1.json` | `631000fe3923ee43e761a114d87803626ce5227d0dad656d0ba50ffd5e08c770` |
| `moderngekko-whole-function1/coverage-native1/result1.json` | `ecb2227157130020b09cc143f75e6ef5682d3ecdd48a95a3900eee16a33a1e27` |

## Guarded display-list packet burst, October 9

A separate inactive experiment replaces only the unpaid 64-cycle TevKColor suffix at `0x802D82C8`. The original prologue, four color-byte loads, packing, overflow test and optional overflow callback run before the hook. Existing native-entry observer approval remains required. Declines retain the original precise/prepaid bodies; accepted execution rejoins the original central return dispatcher.

The helper constructs the ten-byte `61/BE32/61/BE32` packet while removing repeated cursor publication and per-byte memory predicates. Fresh fixed-MEM1/alias/journal/reservation/exception/budget/deadline guards and complete disjoint source/control/frame/output spans bound that reordering. Success charges exactly 64 guest cycles and reconstructs all modified GPR/LR/PC/suffix state; FP state and host fenv remain unchanged. This does not broaden alias or callback assumptions.

The actual original-C2 differential fixture passed 60,000 cases: 30,010 accepted full-state matches and 29,990 unchanged declines. It compared 3,552 CPU bytes with only the distinct RAM-owner pointer normalized, all 28,672 writable bytes per case, read-only protection elsewhere in MEM1 and initial/final complete 32 MiB image equality. Its eleven owned child rows exited zero and drained. Fixture success is separate from game evidence.

The private module (`e2cb244cc97c207dc099378726b9e5cef040406626fe967773214f4e5b4430f3`, 410,186,240 bytes) replaces two existing C2 objects and appends one provider, retaining 833 existing objects including all five C2 changes. Its actual source dependencies, 836 direct objects, 15 physical providers, 853 reproduction members and PE surface passed. The controlled title comparison then matched all six complete logical CPU/MEM1/MEM2/ordered-alias checkpoint hashes and the full captured P6. These logs retain complete logical hashes, not RAM dump blobs. Candidate admission was exercised: 367,052 accepted and 2,044 declined TevKColor calls.

The completed fixed-eight comparison **FAILS** the unchanged consistency requirement. In the primary VI 750-1500 window, mean main-thread CPU changes from 17.87890625 to 17.59375000 seconds (-1.594931%); elapsed mean changes from 18.070748800 to 17.857096575 seconds (-1.182310%); thread cycles change -1.685515%.

| Pair (control / candidate) | CPU change | Wall change | Thread-cycle change |
| --- | ---: | ---: | ---: |
| a1 / g1 | +11.881188% | +13.599754% | +11.404126% |
| a2 / g2 | -7.778738% | -7.753242% | -7.564392% |
| a3 / g3 | -2.510460% | -2.208340% | -2.503055% |
| a4 / g4 | -7.630162% | -8.023300% | -7.771786% |

Both means meet the one-percent reduction threshold; all-pairs CPU/wall improvement does not. Every native case completed and drained with all 34 runtime checks passing. The independent raw audit checked all 1,801 VI records and 1,025 calibration records per case, retaining route/input/zero-pipeline gates. Exact primary/five-segment work and endpoint cursors matched, as did complete terminal GX/clock/dispatch. Primary work remained 547,802 blocks, 6,075,000,001 guest cycles and 99 EFB entries. All eight raw cases and 1,801 per-VI records per case are retained.

The mixed pairs and current background load do not establish a reliable gain or a causal regression. CPU timer/calibration materiality remains unresolved; no overhead is subtracted. Per-VI rows are correlated, not independent trials. This is an unpaced EFB-replayed title experiment, not ordinary displayed-FPS or whole-game evidence. The experiment is closed/inactive with no retry, rescue, pair omission or promotion. The separate negative GroundCross LLVM experiment also remains inactive.

| Private evidence under `build/deep-debug-20261008/` | SHA-256 |
| --- | --- |
| `gd-packet-burst1/attempt1/result.json` | `1b87e65a3b8213cfdae5e0b0175506aad8776d23311a3d00d3b8c92feeef2c4f` |
| `gd-packet-burst1/module-plan1/attempt2/result.json` | `88867b8ae8cb52ecd77360a5c2465387dc93cb5c1b7c9d3b30e4f5b81acb98a4` |
| `gd-packet-burst1/module-plan1/link-attempt1/result.json` | `b1cef2d6f2758e2b5f23249a88b1f2abbe730b5d100c4560ce7e978b67d2209b` |
| `gd-packet-runtime1/correctness-batch1.json` | `05fd476b6813c3c6c9c311f9c97ea7c3961a05dfbd8105624fd4907589366e94` |
| `gd-packet-runtime1/native-efb1/gd-packet-per-vi-input-parity1.json` | `6e3befccbe46e991d9098483fbab51a94db065116dd40d4071b738cf8f177473` |
| `gd-packet-runtime1/controlled-input1/gd-packet-per-vi-eight-run1.json` | `61794021bfef10cbc96922bab27d8ae3ad9b9ba935d818c701c5da4ce1ecf187` |
| `gd-packet-runtime1/controlled-batch1.json` | `77b61d1b21ae0e27e00e5c82da306b583af204df9528490b9c73447cf5545bc4` |
| `gd-packet-runtime1/independent-correctness-audit1.json` | `54cbf54e2f01d159361e00e2608687fe6af1f4bc7e75a7ad33c74d56bc610fc1` |
| `gd-packet-runtime1/independent-timing-audit1.json` | `7a991939ba5e1bc6de9c529d2e1c113a8976a8690545cc22873a83f0b49e254c` |

The public archive preserves three inactive handwritten experiment/fixture files. Generated game source, module routing and player data remain private. The archive has no production build target or automatic installation.

### Further binary-matched source mapping, October 9

Two more debug-only object clones now match the actual C2 executable bytes,
named relocations, function offsets and complete Windows unwind data. Chunk
0187 uses the rebuilt paired-singles source and object; chunk 0186 uses its
unchanged retained object. Their original flags, absolute source paths and
48/46 dependency closures remain fixed. Four compiler jobs and two passive
CodeView readers completed and drained. No module was linked or game launched.

The new tables associate 114 native RIP samples from the existing ordinary
capture: 56 in the 0187 main procedure, 54 in the 0186 main procedure, and four
in two 0186 loops. Interrupted instructions have 41 direct PC/cycle metadata
operands, 21 direct guest-state operands, 35 arithmetic/move/control operations,
12 branches and five other-memory operations. No sampled instruction is CALL.
These are instruction-location counts, not removable CPU time.

The largest associated public routine is J3DTexMtx::calc with ten samples.
Native-source and published-PC routine regions agree for 102 samples, differ
for four, and remain unassigned for eight. Optimized/shared regions and stale
published PCs remain explicit limitations. This adds source evidence for
distributed translated J3D work; it neither identifies a dominant new leaf nor
reopens the failed PC-defer, SSA and tail trials on sample counts alone.

| Private evidence under `build/deep-debug-20261008/line-map-next2-2/` | SHA-256 |
| --- | --- |
| `preparation1.json` | `1372d553b97e5a5f3bae34df1b27a16c5f7444506706a70b0888da317fba93ce` |
| `attempt1/analysis-result1.json` | `b9253edc8fd4ec1e2e9a5b3001b695702d7a4a3584a6074ef14ebe71fea4b357` |
| `independent-audit1.json` | `e5484ff67d75f7c1794b1f081104741a166b7c5a28eaeaa8519c0b52c4ef501b` |
| `ordinary-attribution1/result1.json` | `87df44fdb17f3c1ad336dc50549fde9cc2f2af098da42a5a391bd2c47ee44b25` |

## Cold precise memory tails, October 9

The reported 19 FPS remains unresolved. A private integer-memory experiment
changes the control flow of existing prepaid copies: a failed original RAM guard
leaves the copy permanently for the original current-instruction precise part,
with `cycle_block_prepaid` still true. A completed access that encounters a
deadline refunds through the original next-instruction continuation. Neither
case replays a completed store or charges the block again.

This differs from the closed `lean_memory35` experiment: there are no observed
flags, duplicate slow services, or callback paths that rejoin the successful RAM
chain. The prototype recognizes only strict translated EA plus one integer
8/16/32-bit access, requires the accepted fixed 32 MiB MEM1 profile, retains the
original gather guards, and leaves unsupported forms unchanged. Precise bodies
gain only labels; complete source inverses were checked.

The first exact C2 chunk, 0145, has 824 transformed accesses across two functions.
Its original ordered compiler settings and actual 49-member M/MD closure passed.
The emitted representative success paths, with the deadline disabled and every
RAM guard passing, show a real structural difference:

- Block 851 falls from 76 to 63 native instructions, retains the incoming GPR3
  value across the first load, and reduces alias checks from three to one.
- Block 220 falls from 61 to 60 instructions on one outcome; the taken guest
  branch needs an additional layout jump and reaches parity. Its load-result
  chain was already retained in the original.
- Both still publish guest GPR results. Main and loop entry frames remain 152
  and 72 bytes. Whole-chunk text grows from 709,841 to 760,865 bytes (+7.19%),
  which remains a performance risk. Static call sites fall from 9,136 to 8,404;
  neither static count represents executed time.

Four real-memory differential profiles passed: pointer and fixed CPU, each with
fixed MEM1, at the accepted O2 policy and with AddressSanitizer. Each runs 100
synthetic shapes and 1,024 scenarios against original, prepaid and candidate
functions, totaling 1,228,800 function executions and 819,200 full comparisons.
They compare the complete 3,552-byte CPU, all writable MEM1 bytes, alias/MEM2
storage, ordered callback records, journal/reservation state and host fenv/MXCSR.
Both wrong-current-label and premature-prepaid-clear mutants are detected in
every profile. All 40 owned compiler/link/test rows drained; actual dependencies,
physical CRT/ASan providers and input preservation passed independent audit.

Two harness defects were found before native execution and preserved separately:
address/source-alias cases could legitimately write into protected test pages,
and alternate/null RAM owners initially retained an incorrect 32 MiB size. The
final corpus bounds the affected input payloads without changing instruction
bodies and gives those owners truthful sizes. These are harness corrections,
not evidence of candidate correctness before the final tests. FIFO addresses,
concurrency, extreme signed scheduler overflow and whole-game behavior are not
qualified by this corpus.

Across ten already source-qualified owners, 242 current profile samples associate
with recognized access regions, including 94 direct PC/suffix-store samples.
These are source associations, not exclusive removable cost or evidence that
their dynamic guards passed. Only chunk 0145 has been compiled. Its linked module
passed the actual 1,800-retrace intro against original C2 on the same host:
all six complete CPU/MEM1/MEM2/ordered-alias checkpoints, all 1,228,335 rendered
P6 bytes, all 1,801 workload cursors, GX/clock/dispatch counts and all 1,134
controlled EFB inputs match. Both native jobs drained with all 32 route gates
passing and zero terminal pipeline compilations. Independent raw audit confirms
these results. This qualifies the observed intro's semantics, not dynamic guard
activation, ordinary gameplay or speed. Matched timing remains pending;
production and the previous private tester are unchanged. There is no measured
speedup yet.

Private receipts under `build/deep-debug-20261008/`:

- `cold-twin-memory1/source-preparation1.json`: `a48eaaf49ebce1911cceb64624ac2c559323674e1f150f0433e3e8a035a003f9`
- `paid-twin-ram-compile1/attempt1/result.json`: `d39bd4d69f23fbc3b6dc12feb50ca8dd934edc905c86b863e1d66f88092f1f95`
- `paid-twin-ram-compile1/attempt1/codegen1.json`: `28918eff4294da17289eac0d7c1a0f4745ffa2690d305c55e6e732a9d339aeb6`
- `paid-twin-ram-compile1/attempt1/success-path-review1.json`: `9eada98fa91720295df3adfa42162d09382ddbf7d29faac1e42426b74f607e38`
- `cold-twin-memory1/fixture1/peer-e-source3.json`: `44fab79cc99a8642780f781b994a33e7f7d29da7a7a866caf06bee511c87934f`
- `cold-twin-memory1/fixture1/attempt1/result.json`: `1512932084da900318b0089a0785aed51bbc7d0660dc7ba4e30ae23a0dead313`
- `cold-twin-memory1/fixture1/actual-independent-audit1.json`: `7ec45855e755690f8256961c6055c12feea8ef64697ca68d4880699aec0cc341`
- `cold-twin-memory1/applicability1.json`: `ed0a7d96346b04d7a295e482ed5fb6dc7508e1ff9003c7f5011faadee23b667e`
- `paid-twin-ram-compile1/ten-chunk-scope1.json`: `8991116ae8983c89cbd9d296fd7b4b52868cf416ca26c1c24c324a8673561006`
- `paid-twin-ram-compile1/module1/attempt1/result.json`: `240718dde2914a11cd3d75e63009220876baad58ef61b3c213982c14d13a5cd7`
- `paid-twin-ram-runtime1/correctness-batch1.json`: `a197320d522111398681f149a62918a18f6983c3f620b6aa7babe9cf965e9a71`
- `paid-twin-ram-runtime1/native-efb1/paid-twin-ram-c2-per-vi-input-parity1.json`: `579434621f8fbe990d88283584dc686db597c1d8fa24b4fb4fd273299d304416`
- `paid-twin-ram-runtime1/independent-correctness-audit1.json`: `cb6a2449a77df3c92878e03b410050450ef80a1fbce0def187145a705deb8045`

### Ten-owner cold-tail expansion

After the one-owner intro pass, nine additional exact C2 owners were transformed;
0145's already qualified source and object were reused. The ten selected chunks
are 0041, 0100, 0144, 0145, 0181, 0182, 0186, 0187, 0200 and 0201, with 11,968
recognized integer accesses. All whole-source inverses and precise-body
label-only inverses passed. Chunk 0187 derives from the actual C2 PSQ-prepared
source; four other modified C2 objects remain byte-exact. This expansion does
not retain all five modified objects byte-exact, because 0187 is recompiled.

All 18 fresh M/compiler children and the two retained 0145 children passed and
drained, with original ordered flags and 477 total dependencies. Public function
sets remain unchanged and no undefined provider is added. Total selected text
grows from 7,114,409 to 7,511,561 bytes (+5.58%); this is a performance risk.
Additional compiler constants are admitted only by exact per-owner bytes,
size and flags in relocation-free, read-only nonexecuting COMDAT sections.

One source-only link preparation failed on forward-slash versus backslash
spelling of an existing 1,712-byte library pin. Its bytes and SHA-256 matched.
The partial response and failure were preserved; a new recipe normalizes path
identity while requiring unchanged bytes/hash. The actual ten-owner link then
passed: 835 direct objects, exactly ten original slots replaced, 825 retained,
15 libraries, 850 physical providers and 852 reproduction members. PE imports,
exports, resources, section permissions and ABI match C2. The private module is
410,583,040 bytes, SHA-256
`3d6f7585169663f3a8895838fa1942414b3a1b1ffeecca857551016875a2b4d0`.
The ten-owner controlled intro passes all six complete logical guest states,
the full P6 frame, all 1,801 work cursors and all 1,134 EFB inputs. Both owned
games pass all 32 route gates, exit normally and drain. Independent raw audit
confirms unchanged GX/clock/dispatch and zero terminal pipeline creation.

The subsequent fixed eight timing runs all pass their native/workload gates,
but the predeclared repeatable-gain rule fails. CPU means are 17.296875 versus
16.863281 seconds (-2.506775%); wall means are 17.499223825 versus 17.030676450
seconds (-2.677532%). Thread cycles average -2.629520%. All four pairs remain:

| Pair | CPU change | Wall change |
| --- | ---: | ---: |
| A1/G1 | +1.614350% | +1.013159% |
| A2/G2 | -0.638104% | -0.629281% |
| A3/G3 | -1.779026% | -1.759429% |
| A4/G4 | -8.972125% | -9.071315% |

Primary and all five segment workloads and endpoints match exactly, along with
terminal GX/clock/dispatch. The first pair regresses, so the experiment remains
inactive and this block is closed without extra rescue runs, a changed window
or a discarded pair. The descriptive mean does not qualify displayed FPS or
resolve the reported 19 FPS. Pre-run background-busy snapshots range from
23.56% to 65.53%; they do not establish a cause for any result. Timer calibration
remains unresolved, its retained component estimate is 0.0114441 seconds, and
no overhead is subtracted. The previous tester and production remain unchanged.

Private receipts under `build/deep-debug-20261008/`:

- `cold-twin-memory1/ten-source1/source-preparation1.json`: `28ea1d5e2f79f5af1fb0ab0ec266b2bd7334e39f989700f784bb8b6d24a1dc90`
- `paid-twin-ram-next10-compile1/attempt1/result.json`: `b364144aae526ceaa09577637f7bc7e27cbefbfc807ac264b95e2493d2f8c777`
- `paid-twin-ram-next10-compile1/independent-actual-audit1.json`: `e3659b22e4396183c9283ee9366129450bbb510f71375c8515f97c1b605fe23f`
- `paid-twin-ram-next10-module1/preparation-negative3.json`: `b33344973b00dba51a3c04d9ce3dfa5c4c6b288915ff94e5f1714534c98af6f3`
- `paid-twin-ram-next10-module1/recipe2/source-preparation1.json`: `f1b8bc3942708caa373a2297838308be6df0f401b6933fcb0320ec5ae2bb77ae`
- `paid-twin-ram-next10-module1/attempt1/result.json`: `b2a7015bbe2c54931d2c07e87931c4f5007a775a403777fc069f71e32099d5c9`
- `paid-twin-ram-next10-module1/independent-actual-link-audit1.json`: `27f9fa75c11491b7d4b4465d4a59b56cc8cc373b301a49d9b6cc0b89b7f1a61e`
- `paid-twin-ram-next10-runtime1/correctness-batch1.json`: `e10b672cb03998a883c043d94e3193dbc5abdc07ad9880f6aedbb570a42176e4`
- `paid-twin-ram-next10-runtime1/native-efb1/paid-twin-ram-next10-c2-per-vi-input-parity1.json`: `2c70e63b57d2ace253890d0c9c7a46d4f01fd2b76be2d21dc94f3e57c7a2e870`
- `paid-twin-ram-next10-runtime1/independent-correctness-audit1.json`: `f153f08e4452a835e6d993bc95c318ce4f534ec4d1d62424b885a90c93a4910c`
- `paid-twin-ram-next10-runtime1/controlled-batch1.json`: `ee460e391ea782e938c22ae1453c07bca65eec942addb567acce6b554b9af479`
- `paid-twin-ram-next10-runtime1/controlled-input1/paid-twin-ram-next10-c2-per-vi-eight-run1.json`: `671bd3cbe4965c0d15abc273e267d31716e00b365d32553e517f43aba2166657`
- `paid-twin-ram-next10-runtime1/independent-timing-audit1.json`: `0542dc050f064665023d344c3aae1654e6279fadc68229273ab11a2090f8e1e6`

## Follow-up feasibility checks, October 9

The current 4,455-sample capture was also inspected for a distinct optimization
after closing the ten-owner timing block. No further implementation is justified
by these source and machine-code checks alone.

Of the module's 3,490 samples, 2,194 belong to translated unwind ranges. The
remaining 1,296 split into 1,021 unwind-bounded owners and 275 tentative nearest
MAP labels. The largest confirmed individual helpers are chassis dispatch
(82), alias resolution (78), scalar multiply (70), original-call dispatch (65)
and GPR readiness (65). These are disjoint sampled RIP counts, not call counts
or exclusive time. Published guest PCs were not used to assign these costs.

Static dispatch descriptors would repeat the existing literal-domain,
compact-v3 and PC-to-function caches. Exception, budget, interrupt, pending
event, alias, lifecycle and hook selection remain live inputs. GPR readiness
additionally depends on current frame addresses and per-word memory ownership;
a target descriptor cannot replace that proof. Existing adjacent approval reuse
and whole-frame preflight results remain closed.

The cold memory tails already share the original precise code. The current
799 qualified native rows contain 62 direct guest-register-store samples,
59 in fast copies. Only 18 have a later write to the same register before the
conservative integer/memory boundary. That association is 0.404% of the whole
capture, before necessary exit publications. Existing codegen already retains
dependent values in native registers. There is no demonstrated material budget
for another local-register rewrite or evidence of a cache-stall gain from
moving the shared cold tails.

The ten original owners contain 3,284 scalar FP memory operations and 265
paired-single memory operations. Only 76 qualified current rows associate with
these instructions: 72 scalar and four paired-single. Nineteen are metadata
stores, including the 18 scalar rows already considered in the earlier closed
FP-only subset; none is an actual CALL instruction. The 55 unwind-bounded PSQ
helper samples include necessary memory and conversion work. This does not
establish a material callback-barrier cost for an FP-memory extension.

Finite scalar helpers do not save or change the host FP environment for each
operation. The core MXCSR updater is reached by FPSCR control changes; ordinary
exception/status updates modify guest bits. No environment-control instruction
appears in the 790 qualified rows checked for this question, and no named
setup-function sample appears in the helper census. These scoped absences do
not prove global absence, but offer no basis for an FP-environment cache.

Private source-only receipts under `build/deep-debug-20261008/`:

- `ordinary-public-guest-pc-profile1/ordinary-public-published-pc-intro1/nontranslated-native-census1.json`: `1089c302b4a6bda3e6b31e96629a98f97222b94c5da70a333e345305db07223c`
- `cold-twin-memory1/tail-local-feasibility1/result1.json`: `1f867d97b8e1f7b03ecfa30782a734d096eb8c65891befdbe80eacb2f292ed8d`
- `cold-twin-memory1/FP_MEMORY_COLDTAIL_FEASIBILITY1.json`: `c399ee3103790a313da2ba666ba115a98966e22e0d1bc31455338cf664a3d089`

No production code, tester, game data or existing experiment result changed
during these checks. No native job or timing run was required.

### Ryzen tuning pilot

A separate one-file compiler pilot added only `-mtune=znver2` to the exact
original C2 0145 source and command. It retained `-march=x86-64-v3`, the effective
O2 policy, FP contraction disabled and all original defines/includes. Both
owned M/compile children exited normally and drained; the actual M and MD
records match the same 48 original dependencies. An independent audit verifies
the ordered argument inverse, original inputs and public function/import sets.

Executable text grows 709,841 to 710,753 bytes (+0.128%). The main frame stays
152 bytes and the loop frame 72 bytes. Whole-object instruction sites fall
134,822 to 133,866 while static calls rise 9,136 to 9,138. These aggregate counts
include cold code and padding; they do not establish executed savings.

The inspected successful RAM path in block 851 remains 76 instructions and
435 bytes, with the same normalized instruction multiset and CPU/alias/MEM1
references. Block 220 falls from 61 to 60 instructions, 299 to 297 bytes,
because two integer CR-construction operations become one LEA. Its memory
references and emulation checks remain. This is a local instruction-selection
difference without a demonstrated material hot-path benefit. The pilot stops
before linking or game execution, as declared before compilation; production
and the tester retain their original tuning.

Private receipts under `build/deep-debug-20261008/zen2-tune1/`:

- `compile-preparation1.json`: `ccb5751c65b2ad27f94cfcba51fda580bc5c1a1d7400ade76d8f90ef1341fcee`
- `attempt1/result.json`: `97c1e19c490b019b0306ec6862adf9f5529b2e901ec512b44a853cb97d972ea6`
- `attempt1/independent-actual-audit1.json`: `6c5dbaf584a456ed08faadd499dcc4eea008046159784cbc33e53464cf9cd8ed`
- `attempt1/codegen1.json`: `eaa594cd878989f62b23186546fa67809af161dcaf53504ccc78bb611be1b7d0`
- `attempt1/path-comparison1.json`: `f48e1034b3c073eca3c2fff5aaa36e5929f05e8397632f2af0b161adada081a8`

The initial passive path parser's relocation-addend error is preserved. A
duplicate passive comparison writer was refused by exclusive output creation;
the successful comparison output remains unchanged. Neither event changed a
compiler input or required a native retry.

### Deferred result classification and native-helper admission refresh

A source-only proposal to defer the FPSCR result-class field across C calls
was closed without implementation. The current 4,455-sample capture identifies
16 confirmed classification instruction samples and five publication samples
(0.471% combined). Only one of those five is an actual FPRF store. This subset
is incomplete, not an upper bound: 61 nearest-symbol samples remain excluded,
and no dynamic overwritten-result frequency is known. Existing combined
FI/FR updates cannot be discarded with the class field.

A module-local pending descriptor would add raw-value/type stores and require
a new observer contract. Native skin can read the incoming field; callbacks,
checkpoints, autosave, relevant FPSCR operations and interrupted exits must
receive materialized state. The exact rounded f32 bits must be retained before
DAZ-sensitive widening. Rc/CR1 alone copies different FPSCR bits and is not a
class-field barrier. No material savings or inexpensive observer closure was
established; no candidate was built.

The same source review corrects an older blanket admission explanation.
Frozen public host `13c83a6a` no longer permanently refuses `80328F40`; its
remaining mentions are disabled runqueue tracing. Accepted module C2 still
lists F40 in its static watches, and the inactive GX adapter's
`gx_silent_probe` requires that static check to pass. Existing matrix/search
helper adapters instead use the full fresh host callback. This distinction
does not qualify a whole replacement or prove useful actual admissions.

The current mod table does not replace the helper owner or patch the inspected
F40/F84 helper bodies. A narrow future helper route would still need exact
provider, selected-original, call-site and return-target certification, the
full live host predicate, scheduling/memory order and callback continuity.
The LR-sensitive F84 refusal for `80246A04` must remain. No global module watch
was removed.

Current integration review matches all 49 GX raw fragments in six base/mod
forms (294 comparisons), and the plane draft's eight required fragments in
those forms (48 comparisons). C2 nevertheless lacks the GX provider/router
and its required FIFO writer setup. It also lacks the plane hook/define.
Neither candidate is ready to enable through a host flag. Their old oracle
proofs and the initial plane preparation failure remain preserved; there is
no new gameplay, GPU, throughput or displayed-FPS qualification.

Private source receipts under `build/deep-debug-20261008/`:

- `lazy-fprf-current1/result1.json`: `639d77aaae2b477aa5cc3f03a8de7a7fbd82284fd9ff545dc49b9b8c05c466b1`
- `lazy-fprf-feasibility1/OBSERVER_CLOSURE1.md`: `4ddaf402c4f31551fdcb65abde7a4dda3574e0c355767d4ee090d2cf6df258e9`
- `f40-admission-refresh1/result2.json`: `024a8dd6134fc21bbecf1052d39c978941fbe291e1ff9e38e13acb7ced9479e2`
- `inactive-native-current1/integration-audit1.json`: `3053cc9c74e63b4df8da53e76f4a1f365117f0f711f28b9e250b7cee385e022d`

### Current native ownership and remaining arithmetic candidates

A further offline merge binds the accepted C2 capture to exact original
objects and conservatively bounded native control flow. Of 943 examined
sample sequences, 708 have unique qualified CodeView source regions and 128
have unique native-CFG owners; 85 remain shared and 22 unknown. Another 1,251
of the 2,194 translated chunk/loop samples remain unmapped. Published CPU.pc
and nearest symbols do not resolve that missing ownership.

The largest uniquely named body is GroundCrossGrpRp with 29 samples (0.651%
of all 4,455), followed by J3D recursiveCalc with 20 (0.449%). The disjoint
J3D, J3DGD and GX name groups together account for 428 (9.607%); collision and
geometry account for 233 (5.230%). These whole-route frequencies establish
neither exclusive function time nor removable cost. No single large new leaf
replacement emerges from this mapped subset.

The recursive matrix traversal invokes six dynamic callbacks, including child
and sibling recursion. Its original source reloads child and sibling pointers
after the relevant callbacks; prewalking or caching the tree would change
that behavior. Its existing matrix copies already have native forms. The two
remaining line-geometry bodies have only 23 unique samples combined. No new
traversal or geometry replacement was built.

The current standalone scalar-FP associations contain 70 samples (1.571%),
including 21 at fmuls. Its emitted arithmetic already uses double multiply
followed by single rounding and widening. Converting both operands to single
before a hardware single multiply changes results: x=1+2^-25 and y=1+2^-24
give a different rounded product. Quantization, NI/NaN/status handling and
stfs bit conversion cannot be removed on this evidence. No scalar-FP candidate
was built or timed.

Private receipts under `build/deep-debug-20261008/`:

- `targeted-current-native-cfg1/result1.json`: `9bbceec40ea8121ea9b74e2c6919e5aa03f753cb8256d8d733e0c329916c8866`
- `current-primary-ranking1/result1.json`: `7060c1a67cc9f671060f9cc2757202aa670d4cf2bcc5c703afdb51540cfe6afe`
- `inactive-native-current1/TRAVERSAL_FEASIBILITY1.md`: `e3100dd843ea7338f1c70dbd5fcd24d4712e175c446e3b0d22ac9964747ed8b2`
- `scalar-fp-arithmetic-refresh1/result1.json`: `764e6f89fc42bd98efbf63c7230cf48525d3ff25835c4483e83677f42db87177`

### Read-only readiness compiler pilot: stopped before integration

A separate hypothesis tested whether a caller-visible read-only admission
query would let the compiler retain CPU values across the query. The complete
host callback cannot carry that attribute: some paths read atomic configuration
or diagnostics. Only the reviewed sufficient-negative main-code leaf has a
plain read-only source closure. The private compiler pilot used broad
`memory(read)`, never readnone, argmem-only or a no-alias promise. A false query
kept the existing return to the dispatcher; no full callback was hidden inside
the annotated function.

An authored fixture with separate opaque provider translation units showed
the intended compiler effect. Successful-query CPU reloads fell from four to
zero and three to zero in two cases. All four publication stores remained;
an intervening RAM store kept both queries, and an opaque observer still forced
three CPU reloads. Value-chain code nevertheless grew from 272 to 304 bytes.
Six serial compiler rows passed and drained. An earlier attempt compiled one
object but failed while recording its result; that partial attempt remains
preserved and is not counted as a successful run.

The real-code follow-up compiled exactly original C2 chunk 0145 with its
48-member dependency closure and original ordered O2/x86-64-v3/FP flags.
Only the copied readiness header and one experimental define differed.
Both dependency-query and compile rows passed. The external certified provider
was intentionally unresolved in this object-only experiment.

The real-code gate failed: all 24 other functions have identical raw machine
bytes and identical named relocation records. The main body still has 132,618
instructions, 9,067 calls and its original 152-byte frame. Only the readiness
helper shrank, from 144 to 80 bytes; no actual caller reload, publication or
spill reduction occurred. The preregistered protocol therefore stops before
provider integration, linking, game execution or timing. Authored compiler
behavior is not a game optimization or runtime-contract qualification.

Private receipts under `build/deep-debug-20261008/readonly-fast-query1/`:

- `PILOT_PROTOCOL1.md`: original stop criteria and scope.
- `actual-audit4.json`: `2b7a81682d8bf2235d98f618203ffb3b02b3c3494bd35a7080830b12f8cce747`
- `chunk-pilot1/attempt1/result.json`: `eda485e8ffbf16e03e6e3c805a5d6a9c3c470eef8f6e5560260215b02000f0dd`
- `chunk-pilot1/independent-compile-audit1.json`: `41e176df13dbdbb553af8628221350557f61e24ebb1284bca0556f8aaca26c9e`
- `chunk-pilot1/attempt1/codegen1.json`: `ad59887a300908ffc3600639af145a58e0199ea6a41c0dade98d1ba7e094328b`
- `chunk-pilot1/attempt1/function-scope1.json`: `995abb30f93f395b01123600fe2095f511b4ec3f96a1f8a7ef0e86638db04d57`

### Expanded current coverage and exact public-host attribution

A second bounded native-CFG pass examines eight previously unmapped original
C2 main owners. Their 276 samples split into 235 unique guest-function names,
33 shared and eight unknown. Exact current object bytes and code relocation
targets match the captured image; chunk 0160 uses its actual PSQ successor.
Both passive passes read 13,977,368 image bytes, below their 16 MiB bound.
Combined coverage is now 1,219 of 2,194 translated samples examined: 1,071
unique, 118 shared, 30 unknown and 975 unexamined. The largest newly named
routine, diffLight, has 18 samples. These are association counts, not exclusive
cost or a new optimization qualification.

The 73 previously unresolved mod-provider samples also now have unique actual
object providers. Duplicate MAP basenames had prevented the first match.
Exact external COFF definitions, retained link/source records and the compiled
mod-table function relocations resolve all 11 owners. This does not yet name
their interrupted guest instructions or establish the cost of a mod feature.

The captured public host has its own matching MAP and PDB. PE CodeView GUID/age,
MAP metadata, pdata starts and selected exact main-object payloads qualify
675 of its 772 samples; two remain shared and 95 unknown. Qualified body
counts include direct-call observation query 227, host main 92, chassis full
facts 49, GX write 41, complete observation predicate 38 and shadow FIFO 36.
No older host profile or nearest-only symbol supplies those counts.

Within the 227 direct-query samples, reviewed native CFG/operands partition
65 at the static page/mask lookup, 39 at live mode flags, 24 at interrupt/dirty
checks, 40 at event state, four at overlap, 22 at raw REL, 14 at raw-code ranges,
five at CPU/null/alignment and 14 at success. These identify instruction
locations, not rejection frequencies or removable time. In particular, the
65 lookup samples include one at the page load and none at the mask load;
64 interrupt address/bit-index preparation. Sampling does not assign load
latency to the following MOV instruction.

The old 81.55% shortcut acceptance was measured before removal of the finite
Bloom restriction. It cannot estimate the remaining callback workload of this
public host. No current query-address distribution or refusal-frequency
census has been inferred from the new capture.

Private evidence under `build/deep-debug-20261008/`:

- `targeted-current-native-cfg-next8-1/result2.json`: `8fcbd345a786d38166d8555583efcc896987a5ca4c28ea3c3955e35d49fac71f`
- `targeted-current-native-cfg-next8-1/summary1.json`: `5c70403db68c7af82f2c54707e3d4b29708ffef85c993e08ee42b89f9263498e`
- `current-primary-ranking1/unmapped-mod-providers1.json`: `03ad9fae0817e5da57d19593bb2a33e02799ec9786b756ce20eeafaee0ffaaae`
- `current-host-attribution1/result1.json`: `692fc73a5a045e7d816fac458a01ee406548bf3639b10b2074ca1ac98502c790`
- `current-host-attribution1/leaf-guard-partition1.json`: `d62f2bad49d46d6c6a90863104f5cc55136c5eb8352273a20ec11df9ffe7d611`
- `current-host-attribution1/static-lookup-detail1.json`: `1cf2cf84abcd37008ab53352344df5e53b65e65a746f9312a224d06f605606af`

### Immutable word-offset lookup: correct, timing experiment invalid

The current static directory selects one of 85 mask rows. A private variant
stores each row's word offset directly and indexes the same 1,360 mask words.
Independent source and actual-object checks compare all 844,800 aligned table
slots, including the 841,008 valid members and all 5,262 exclusions. Both use
12,530 bytes. The live query, membership bounds, mutable guards and fallback
are preserved; no answers or mutable state are cached.

The actual ordinary O3 compiler pilot uses the original main source and ordered
flags, a fresh product-CMake certificate and the exact 342-entry dependency
closure. Four serial owned children exit zero. The emitted lookup removes
SHL EAX,7: directory load now feeds one integer addition followed by the scaled
mask load, rather than shift plus addition. The static block shrinks from
63 to 59 bytes. Alignment absorbs those four bytes, so neither its 496-byte
function slot nor total text shrinks. All other 122 functions, their named
relocations, 98 unwind records and xdata remain identical. The selected query
has no calls, stack frame, spills or saved registers in either object.

This establishes a compiler effect, not speed or ordinary gameplay improvement.
The 65 sample locations do not forecast saved cycles. A separately recorded
linked protocol requires fresh six-state/full-P6 correctness and exactly eight
paired timing runs against the current public host, with the same C2 module.
It retains the one-percent CPU/wall mean and all-four-pair sign requirements.
The installed build and private tester remain unchanged.

The subsequent instrumented host links with exactly one replaced main object,
169 retained providers and the original 64 direct inputs. Its fresh product
certificate and actual 425-entry dependency closure pass independent checks.
The six complete CPU/MEM1/MEM2/ordered-alias checkpoints, all 1,228,335 P6 bytes,
all 1,801 work cursors and all 1,134 replayed EFB inputs match the control.
Both games exit zero with the complete route and input checks passing.

The fixed eight-run timing experiment stops at its sixth run, control A3.
All six games exit zero and drain, with matching work and EFB consumption,
but A3 reports 50 terminal pipeline creations against zero for the first five.
Its wrapper rejects the unchanged no-compilation requirement. The remaining
two runs and final analyser are not executed. Even the first two incomplete
pairs disagree in CPU/wall signs: -4.4510%/-5.1700%, then +3.2164%/+2.8289%.
These are invalid-experiment diagnostics, not speedup evidence. No run is
discounted, no replacement run is made, and the candidate remains inactive.

All six final pipeline databases are byte-identical to each other and contain
the same complete 242 logical rows as the seed. The seed differs physically
at six SQLite header bytes. The primary audit recorded this distinction;
the earlier statement here that finals equaled the seed byte-for-byte was
incorrect. Its recorded seed hash still matches the current seed.
Source inspection shows that cached background pipeline preparation increments
the same terminal counter, so startup completion crossing its first-retrace
baseline is possible. The retained logs do not establish the cause or timing
of these 50 creations. The original failure remains authoritative; this
possibility does not rescue the comparison or explain the ordinary 19 FPS.

Private receipts under `build/deep-debug-20261008/host-static-offset1/`:

- `peer-b-source1.json`: `13137549b98be166ad5192fe0729f59119e79507758275a48d3cb00c27589bdf`
- `compile2/attempt1/result.json`: `2a67b3090131ca5f2a3598c6afb0bebd33fac2b4270a2a09978fcf8976761e97`
- `compile2/independent-build-audit1.json`: `9e0399923264558b9a6c78191398f7bd8ff9f2530c579c63f6d04817499bf6a1`
- `compile2/actual-codegen1.json`: `17da205a6bde80ef3296616ff556b54c445d07b6547ce2a861a4dfe61f44579a`
- `compile2/peer-g-actual-summary1.json`: `23f6b16cfbd386771f61b513e340cb02eef72f45d942732835dd65d7851c707d`
- `linked1/attempt1/result.json`: `fd76d9152a11ac0d3a68e91b1b7fb7cb6267c5917d0df5d0845315faf7c47163`
- `linked1/independent-build-audit1.json`: `131701b6e6ef20ee71146dfdf7e5769abeba7557449ed8e01dbce2dbe81c3a8e`
- `runtime1/independent-correctness-audit1.json`: `ac9b686e2432d779acece57c53c6b13c7eb23be75174c63ec0ccb8bf8594da70`
- `runtime1/controlled-batch1.json`: `34f2615e7176928e27d1c0b55a5667667371eaa74a1c94423862842023e8b01d`
- `runtime1/independent-timing-audit1.json`: `b45101599f8fe88e7b697b1a6d862a46527b014e277227c7cccbb8c53485ceba`
- `runtime1/pipeline-failure-audit1.json`: `588bd1462243e61381654ea76148833443f624b549e22d6c8b3b475f5601280b`

### Further source checks, October 10

Three bounded follow-ups do not establish a substantial new candidate. The
ordinary CPU pointer parameter is unused beneath the fixed-global ctx macro,
and normal MEM1 already uses a separate declared global. Adding restrict to
that parameter supplies no missing separation; callback and alias paths still
retain their complete mutation contracts.

Current paired-arithmetic helpers have 47 pdata-bounded samples out of 4,455,
with one additional tentative location. This excludes 102 PSQ-related
locations and does not bound inlined arithmetic. Twelve sampled helper copies
use scalar lanes, but exact multiplier rounding, NI/fenv, classification and
fallback semantics remain necessary. No sizeable eligible SIMD target is
established. An initial source assessment incorrectly inferred disabled SLP
from the early flag spelling; the preserved successor binds the exact existing
frontend query, which enables both loop and SLP vectorization at effective O2.
The earlier 0181 flag-only SLP pilot already produced identical machine code.

Wider private calling-convention propagation remains distinct and unexecuted,
rather than a closed runtime failure. The two selected chunks contain nine
sampled PUSH/POP locations; the full 1,219-row qualified subset contains 28.
These are incomplete observed locations, not removable time or an upper bound.
No frequently traversed private call cluster is established, while the known
216-byte wrappers and 248-byte shared dispatcher remain costs. No implementation
or native pilot is selected from these source assessments.

Private evidence under `build/deep-debug-20261008/`:

- `cpu-ram-alias-feasibility1/assessment1.json`: `56498b74676c00f2ab751e058c6a6dcf2423945a77b97da220f6721c6a5b6f7c`
- `paired-single-simd-feasibility1/assessment2.json`: `637e73ee467a9d270e3e2d63c598d66ffbd69d77add080c49aa29ac9d08cf9bc`
- `paired-single-simd-feasibility1/slp-correction1.json`: `e565e8dff9a3d8d92a68c80e75b32f723061bfc54f05c54c3bdb5a39a9d7241c`
- `private-abi-propagation-feasibility1/assessment1.json`: `18b85a012d2eb282c4d306589898cb0b4b3035dd64b5edd10ffabd58b44cf922`

### Fresh donor review and additional current attribution, October 10

A bounded freshness check revisits six recorded repositories and 25 branches.
Five repositories retain their recorded heads. AceSpectre's DolRecomp
`fn-codegen` advances from `83398f1` to `483bb3f` with three October 9 commits.
The later October 8 inventory also records the old head; a preserved correction
replaces the initial freshness receipt's October 6-only baseline description.
This is a check of these recorded repositories, not a complete new fork census.

The locked-cache change requires a CPU/storage contract absent from current C2;
its donor Wii measurements are not local performance evidence. The new dense
PC-to-function lookup does not preserve current dynamic observers, native and
mod selection as a drop-in replacement. Neither is imported.

The [per-block base-proof change](https://github.com/AceSpectre/DolRecomp/commit/2b71262ef331ae10ff26c8139c95660d6c9b92f1)
offers a distinct source mechanism: reuse a proved address span for an unchanged
stack or global base. Its original RAM/MEM2 predicate and callback behavior
cannot be copied directly. A local subset needs the exact fixed-MEM1 ownership,
alias, journal and reservation guards, original paid precise continuation, and
unchanged PC/suffix/deadline/refund behavior. This is separate from the already
closed per-access cold-tail metadata deferral and save-GPR special handling.

Ten exact original C2 sources contain 610 conservatively repeated base epochs
covering 1,496 accesses. These source counts do not establish executed cost.
Only 31 current source-associated locations fall in repeated clusters, and
their branch/arithmetic instructions do not prove a removable guard budget.
A private compiler pilot therefore starts with two original chunks and a
stricter consecutive-access grammar. It covers 91 clusters and 224 accesses,
preserving every original metadata and refund line. Independent source review
checks both whole inverses, exact paid continuations and 819 modular boundary
cases. Native output, runtime correctness and speed require separate evidence.

The subsequent compiler-only pilot completes four fresh object builds and
their four dependency queries. Original ordered flags and actual M/MD closures
(48/49 and 47/48 entries) pass an independent audit. Fresh controls reproduce
every retained C2 function's raw code, named relocations and unwind metadata.
Only the two main translated functions change; the other 24/26 functions remain
exact. Their selected frames stay 152/200 bytes. Executable text nevertheless
grows by 4,512 bytes (0.635635%) and 11,104 bytes (1.556294%). The latter fails
the pilot's preregistered one-percent text criterion. That compiler pilot is
failed and is not relabeled as a pass. No module or game is run in that pilot.

This size criterion is a conservative selection rule, not a measured
performance regression. Actual successful-path work removal could motivate a
separately specified correctness and timing study of the same frozen variant;
it would not erase this failure, change flags/grammar, or establish ordinary
FPS from static instruction counts. The accepted tester remains unchanged.

An additional passive mapping pass examines 185 disjoint current samples in
eight retained original C2 owners: 146 unique, 36 shared and three unknown.
Exact code-to-COFF inversion and all 32,768 PC-table entries pass within a
13,220,328-byte PE-read bound. Cumulative translated coverage reaches
1,404/2,194 examined, with 1,217 unique associations (708 CodeView, 509 CFG),
154 shared, 33 unknown and 790 unexamined. The largest new guest association
has eleven samples. These are instruction locations, not exclusive routine
time, and establish neither a dominant new routine nor a speed improvement.

Private receipts under `build/deep-debug-20261008/`:

- `fork-freshness-20261010-1/result1.json`: `22521433d31f58433866a0f9d1be7be0a28bdad293638ee3c89cf7ecc354fbd1`
- `fork-freshness-20261010-1/adapted-base-proof-feasibility1.json`: `d78dcb346717775c78894426cd3d8d81ec54f8fd4288db87c27ad8b0c6cffcab`
- `base-span-applicability1/assessment2.json`: `83dd0b6339336a7287617f6dfdb8f1241425a51b8aa9275a6ea48c6392c92555`
- `base-span-ram1/source-preparation1.json`: `afa8c9668eba04429debf1efde5ea2bf649b499be64b67615da12947f190b72f`
- `base-span-ram1/compile1/attempt1/result.json`: `be06910e6d47e39d2ad3de9014e73f9a31a41e5220039345797054bc49a8b2c6`
- `base-span-ram1/compile1/independent-build-audit1.json`: `eae4cd3a872169c2e843c6606c9a326b1dd646e35632c83a1c964eb4de341a87`
- `base-span-ram1/compile1/attempt1/codegen-g1.json`: `436d6a3f404b0ee54cf9e66d6255dfc8621d2e32f0d59e8c0a1d9604c2d0e24f`
- `targeted-current-native-cfg-next8-2/result1.json`: `b8f4cbd0f6f033f8260ca7e27e0c0aa1409589eed9572c688278c5ed830e7a0c`
- `targeted-current-native-cfg-next8-2/summary1.json`: `cbbf079dad747339462c1b8b94cf3149489698dad94f6130119a6f46d1b455fc`

### Separate base-span correctness study, October 10

The original compiler text-size failure remains unchanged. A separately
specified study tests the same frozen transform because actual native paths
show reduced successful-path work: 75 to 47 instructions for a three-store
region and 163 to 60 for an eight-store region, with three/eight range checks
reduced to one. These counts assume admitted ordinary RAM and a zero deadline
budget. Their executed frequency and speed effect are unknown. All original
source PC/suffix/deadline/refund lines remain; the compiler also removes some
intermediate native metadata stores and combines two adjacent stores.

Independent source review found test-coverage gaps before execution, including
nonmonotonic access order and later callback-triggered refund seams. The
immutable source3 successor fills them without modifying the production
transform, helper or compiled candidate objects. It contains 204 shapes, of
which 108 transform and 96 explicitly decline. Reachable refunds after
callbacks one through six, no-refund controls seven/eight and a reservation
mutation before later stores are asserted from actual callback-time state.
Both wrong-paid-label and wrong-prepaid-state mutants must diverge.

Root then executes four configurations: pointer/fixed CPU state, each ordinary
O2 and AddressSanitizer, all with fixed 32 MiB MEM1 and the real 3552-byte CPU
and canonical gather/memory providers. All 40 owned children complete with
zero exits and normal drain. Each configuration passes 646,884 executions
across 204 shapes and 1,057 scenarios: **2,587,536 total executions**. Full CPU,
all 12 KiB writable MEM1, protected remaining MEM1, other memory owners,
callback traces and floating environment match without normalizing PC,
suffix, cycles or memory. No sanitizer diagnostic occurs. Actual M/MD closure,
physical link providers, ASan imports and preserved inputs pass an independent
post-run audit. This is synthetic differential evidence, not whole-game proof
or a performance measurement.

A later isolated pair links fresh control and candidate modules with exactly
the two qualified objects at slots 760/796. Each retains 833 other direct
objects, the five C2 replacements, 15 physical libraries and the original link
order. Both links complete and drain normally. Physical byte-ledger/provider,
actual M/MD, constants, PE export/import/resource/ABI and before/after input
records pass an independent actual audit. The fresh control equals accepted
C2 across the entire 410,185,216-byte DLL except three bytes in the COFF
timestamp. The candidate is 410,201,088 bytes. No runtime or speed claim follows
from linked-file size or these build checks.

A bounded join of the exact 91 clusters against the current ordinary native
capture finds only six uniquely source-associated samples across five native
RIPs and five clusters. Of 221 selected main-owner samples, 202 have unique
source-region associations; 196 of those lie outside the frozen cluster
tuples, with 19 shared/line-zero rows unresolved. The six include CPU result,
suffix and refund publication: they are not six proved removable range checks.
Neither selected native successful-path offset set has a captured sample.
This small source-associated footprint does not establish executed guard
frequency, a time bound, deadline distribution or an expected FPS gain.

Private receipts under `build/deep-debug-20261008/base-span-ram1/`:

- `fixture1/source3/source-preparation1.json`: `320d38adae4dbb7af23822c8eab57a8e1b5e7d1b8e45e080258b2736e6aa7641`
- `fixture1/peer-e-source3-review1.json`: `c2bcdeff88dfcd2022d598b05f7361e982d3d69aac78082b55f3fbd2dc6c5ee9`
- `fixture1/recipe2.json`: `6bff4e91f6c7bc7ce603f200ef04b8526b6f0269898fa4e9404ccd4a1cb9a355`
- `fixture1/attempt1/result.json`: `1be44a34f031ed30ca3fe3c979faf0c8b211c13a71a69645ec1b8c01473f8866`
- `fixture1/independent-actual-audit1.json`: `b106193c178388a078eb294550eb68cb4fc5cd804d5f4986f745b6a250f8bbd6`
- `module-pair1/attempt1/control/result.json`: `89c4fad04e23aa642a94d38b2a7071879d64dcd3548284d8326333bec9f2dc4e`
- `module-pair1/attempt1/candidate/result.json`: `58f8bbc1648dc7cea78ee8dfde43de51247ba7376a57a05cf1c095509dec8927`
- `module-pair1/control-versus-C2-binary1.json`: `4857bc8e641885fb2a3cbd698f8804d739fda7316c6c1a47bbdb17fd96ac82d8`
- `module-pair1/independent-actual-pair-audit1.json`: `0c4642cf1dbc4f94ba158dcf721a930b033f8d15ee9d1627315df441dbd2de1b`
- `materiality1/result1.json`: `30aeded6d62672c3127992ac3b83a8d3bccc53f8daea81a75bacfda0601c88be`

### Base-span runtime result and pipeline diagnosis, October 10

The fresh pair passes the complete intro correctness comparison. CPU, MEM1,
MEM2 and ordered aliases match at VI 300, 600, 900, 1200, 1500 and 1800.
All 1,228,335 P6 bytes, 1,801 work rows and 1,134 EFB input entries match;
clock, dispatch, GX, replay and terminal pipeline checks pass. Both owned
games exit zero and drain. A raw-output audit confirms these
results. This qualifies this recorded route, not every gameplay state.

The fixed eight-run timing batch stops at its third game, candidate C2.
The three native processes exit zero and drain, but terminal pipeline counts
are 0, 0 and 4. The third wrapper fails only its unchanged no-compilation
requirement. The remaining five games and final analyser are not run. No
replacement run is made. The sole completed pair has candidate CPU time
0.1759% higher and elapsed time 0.7328% higher. These partial diagnostic
numbers establish no qualified gain or ordinary displayed-FPS result.
The original compiler text-size failure remains; the candidate is closed
and inactive, with the installed build and tester unchanged.

All three final pipeline databases are physically identical to each other
and contain the seed's same complete 242 logical rows, configs and first-frame
values. They differ from the unchanged seed only at six SQLite header bytes.
Neither database equality nor the cached-pipeline completion log identifies
the origin or measured-window timing of the four reported creations.

Source inspection establishes a diagnostic limitation: restored and newly
requested pipelines increment the same completion counter, and a background
restore can be promoted into a foreground queue. The FPS watch begins counting
after its first baseline; existing VI records have no pipeline snapshots.
The retained outputs cannot retrospectively distinguish restoration from
new creation or locate the four completions within VI 750..1500. The failed
gate remains authoritative and does not explain the reported 19 FPS.

The next diagnostic will retain immutable pipeline origin and bounded QPC
request/start/completion events, joined offline to the existing VI timestamps.
Its source assessment proposes no startup barrier or change to earlier timing
criteria. A final asset check confirms all 417 inputs (including 415 RELs)
and routing unchanged. Tingle rescue wait-skip remains disabled.

Private receipts under `build/deep-debug-20261008/base-span-ram1/runtime1/`:

- `correctness-batch1.json`: `c1505a5d89d0e60f586078018bf16ea35024269dc878fda19946fda4b57ea036`
- `actual-correctness-audit1.json`: `f9e019aa2945728c16a907e4fec6925ef7b5e2eaacd94f00fe1dca6398e798d8`
- `controlled-batch1.json`: `86358b237c6159bd466546ccad27a188dcfa1fb8c1b0488baa8dd24329f9ab7a`
- `actual-timing-audit2.json`: `d7accdfe0c5e950bc6858cba6f0761a8f40c546016ec4b7f7d7a723564ea8a9a`
- `independent-incomplete-timing-audit1.json`: `035746bad3578c294b14933ebcc32f759381089fb6595ea57172cd7de8f1f790`
- `pipeline-failure-audit1.json`: `de5ee3ddc5cc3f664f65d20ff8982dcb5802987df98a62bee33f4786652e8a62`

The source feasibility receipt is
`build/deep-debug-20261008/pipeline-phase-feasibility1/assessment1.json`
(`ee1e13c0…`). The post-run asset receipt is
`build/k7-uniforms-20261008/native1/asset-inputs-after-base-span-20261010-1.json`
(`64e920fec03e3c0a489d06b6d3cc3069e234a1191cb0b741ed87196a4cdba736`).

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
- `visible-c2-profile1/peak-window-feasibility1/result1.json`
- `focused-peak-profile1/{preparation1,root-quiescence1}.json`
- `focused-peak-profile1/title-focused-c2-1/{result,native-rip,per-vi}.json`
- `focused-peak-profile1/analysis-source-preparation1.json`
- `focused-peak-profile1/title-focused-c2-1/{compact-window-summary1,event-vi-assignment1}.json`
- `focused-cpu-profile1/{preparation1,source-validation1}.json`
- `focused-cpu-profile1/title-focused-cpu-c2-1/{result,native-rip,per-vi,joint-window-summary1,target-joints1,helper-joints1}.json`
- `fp-context-assessment1/{fixture-recipe5,fixture-recipe6,independent-actual-audit5,independent-actual-audit6}.json`
- `fp-context-assessment1/attempt{5,6}/result.json`
- `fp-context-assessment1/caller-cost{1,2,3}/attempt1/{result,codegen1}.json`
- `fp-context-assessment1/local-cost1/attempt4/{result,analysis,codegen}.json`
- `fp-context-assessment1/{original-six-pc-publication1,six-site-performance-closure1}.json`
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
- `llvm-bounded-prefix1/fp-cfg-fixture1/fixture-source3/{fixture-preparation3,peer-source-review1}.json`
- `llvm-bounded-prefix1/fp-cfg-fixture1/{fixture-recipe1,peer-runner-source1,canonical-qualification1}.json`
- `llvm-bounded-prefix1/fp-cfg-fixture1/attempt1/result.json`
- `llvm-bounded-prefix1/fp-cfg-extension1/microbenchmark-source1/preparation1.json`
- `llvm-bounded-prefix1/fp-cfg-extension1/microbenchmark-recipe1.json`
- `llvm-bounded-prefix1/fp-cfg-extension1/microbenchmark-quiescence1.json`
- `llvm-bounded-prefix1/fp-cfg-extension1/microbenchmark-attempt1/{result,summary1}.json`
- `llvm-bounded-prefix1/fp-cfg-extension1/microbenchmark-attempt1/independent-audit1.json`
- `llvm-bounded-prefix1/fp-cfg-extension1/{extra-cost-mechanisms4,extra-cost-conclusion1}.json`
- `llvm-bounded-prefix1/fp-cfg-fixture1/independent-actual-audit2.json`
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
