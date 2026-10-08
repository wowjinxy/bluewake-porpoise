# Quantized paired-single experiment — October 8, 2026

This experiment separates cheaper GQR scaling from integer PSQ inlining. The
generator option is off by default; the runtime patch is an inactive draft.
Scaling and the final isolation helper passed the bounded native state
comparison. The first inlining build failed one checkpoint despite matching
captured pixels, so it is not qualified. Loaded timing results are recorded below.

## Source and scope

Adapted from InfraredGod's GPL-3.0-or-later RecompCore commits
[`3d969ed1`](https://github.com/InfraredGodYT/RecompCore/commit/3d969ed1202cc60f950c0ffb878456649431a996)
and [`09ad2a16`](https://github.com/InfraredGodYT/RecompCore/commit/09ad2a1609028a3870790d540ba0e677a94d15c4).
Those commits establish the mechanism; their Sonic Heroes claims establish no
Wind Waker speedup.

- `patches/recompcore/drafts/quantized-psq-scaling.patch` replaces the two
  `ldexp`/`ldexpf` scale expressions with exact IEEE exponent-bit powers. GQR
  scales are signed six-bit values, so every power used here is normal and
  exactly representable. Conversion, rounding, NaN, clamp and truncation order
  remain the same. This avoids a table and leaves classification unchanged.
- `scripts/generate_composite.py --quantized-psq` additionally emits integer
  type 4–7 inline load/store helpers for GQR4–7. GQR0–3 retain the exact old
  type-0 helper bodies and their runtime fallback. Type 0 keeps its existing path; invalid
  formats and disabled nonindexed LSQE use the canonical fallback. The option
  changes only the two generated-header helper regions. Default output is
  byte-identical to the previous generator for the committed synthetic fixture.

Each operation snapshots its GQR once. Lane 0 is committed before lane 1's
load callback; a store reads lane 1 only after lane 0's write callback. This
adapts the donor's arithmetic while preserving our callback semantics. Real
memory helpers retain aliases, MMIO and observer effects. No guest ABI,
deadline, cycle, FPSCR, PC, mod or renderer policy is changed.

The active SDK is base `e280c788dadabd18b085af0085f1558fc9ff5ecc` plus
patches 0152–0165.
Canonical `GXRuntime/src/core/cpu.c` SHA256 is
`6f6f0251355bcf4e7f4beb45487e504fe6b4148e33719f928756f291068b106b`.
The inactive scaling patch SHA256 is
`8c9ce9fcf57ebce4bc00925c844ccba10de64e53a274aa09c38ad6faf61bfcfc`;
its resulting source is
`d9dd08fbb1aebb8fc312694a26fed3de01506ae16e89aa92f2d35b6f0b413875`.
Neither the dependency lock nor active patch manifest is altered.

## Correctness and generated code

Private arithmetic qualification compared the exact extracted old/new
production helper bodies: **42,343,424 comparisons in each O3 and ASan
profile**, zero mismatch. This comprises 8,421,376 exhaustive integer-load
cases and 33,922,048 boundary/adversarial/random store comparisons across all
scales and clamp domains.

The full runtime differential fixture links actual CPU, exception and
interpreter sources with actual emitted inline helpers and an independent
canonical PSQ oracle. For the final GQR-gated helper, reference O3, candidate
O3 and candidate ASan each pass **165,056 cases and 330,112 exact comparisons**,
architectural digest `b7bba9f071ef411b`. This includes 65,536 explicit cases
crossing all eight GQR indices with every type/scale/W/indexed/LSQE/load-store
combination and mutating MMIO. Coverage also includes GQR masking, all types/scales, W/indexed/LSQE forms,
real exception entry, complete CPU state, memory windows, aliases, MMIO and
callback traces, callback mutations of GQR/FPR/PS1, and all four rounding
modes crossed with FTZ/DAZ modes. Two deliberately incorrect lane-order
mutants are detected. These are synthetic helper results, not game proof.

The final isolation helper also passes candidate O3 and ASan profiles using
the actual historical module's `gather_pipe.h` macros: **five profiles total**,
with the same cases, comparisons and digest in each. That profile has batch
length zero and null writers; active batching and writer mutations are
unqualified. Integer 8/16-bit accesses bind to canonical `cpu.h` memory helpers
with scoped macro save/restore. Integer hardware-base operations delegate the
whole call through the existing gather wrapper after the unchanged type-0 path,
preserving its base-address drain. The earlier helper also passed the bounded
gather profile, so these results do not identify the cause of its native mismatch.

The committed ROM-free fixture is reproducible from a Windows developer shell:

```powershell
python -B -X utf8 tests/test_quantized_psq_runtime.py --compiler C:/path/to/clang.exe --output build/psq-runtime-test1
python -B -X utf8 tests/test_quantized_psq_generator.py
python -B -X utf8 tests/test_composite.py
```

Use a new output directory and an explicit Clang installation with ASan. The
optional private `--environment-json` supplies compiler variables without
emitting their values. `--skip-asan` reports limited O3-only qualification.
`--gather-dir C:/path/to/prepared/source/cmake/composite` adds the two bounded
gather profiles and pins both supplied headers. Use the exact build headers;
the current checkout's header differs from the historical prepared module.
The runner checks the exact canonical source, draft patch, oracle and resulting
candidate, then validates real `-M`/`-MD` dependency sets and preserved inputs.
The regenerated public fixture passed all five profiles and both negative
controls, with complete GQR coverage mask `0xFF`. Earlier broader-helper
99,520-case receipts remain historical evidence; the final 165,056-case receipt
qualifies the narrowed helper. All 21 generator/compatibility checks pass.

The production compiler flags are O3 followed by effective O2, x86-64-v3,
`-ffp-contract=off`, fixed CPU/global MEM1 and the existing forced-include
environment cache. Exact full `cpu.c` LLVM IR has eight `ldexp` calls before
and zero after the scaling change. Explicit classification builtins did not
improve PSQ branch count and were omitted.

The translated DLL embeds its own `cpu.c`; a host-only rebuild cannot deliver
the runtime change. The exact module closure contains 835 objects plus the
unchanged libPorpoise archive. Scaling rebuilds one CPU object and relinks.
An initial broader inline build covered all 531 PSQ-using translated chunks.
It was stopped after 32 partial objects and preserved with an explicit
`ABANDONED_BROADER_SCOPE` receipt, without a semantic failure. The final design
retains the old GQR0–3 path. Only four of 813 translated chunks contain GQR4–7
call sites (132 sites in 13.48 MB of prepared source): DOL0160, DOL0187,
DOL0188 and movie-player0000. The targeted native candidate rebuilds those
four plus CPU, retaining the other 830 objects and the archive by hash. This
is a targeted prepared-source candidate, not a claim of fresh whole-module
build byte identity. It does not reprepare unrelated game chunks, change
compiler optimization, or add PGO/ThinLTO.

An unlinked 191 KB GQR0-only sample was compiled with the first targeted
header, before canonical-memory isolation, as an extra check. Its `.text`
grew from 49,715 to 49,731 bytes, so fresh codegen
identity is **not** established. Exact old source bodies and runtime behavior
are established; the native candidate keeps the original linked objects for
these other paths. The sample object is excluded from the candidate link.

## Native comparison

Private cases use a fixed selective-vertex renderer host, copied player data,
physical EFB peeks, and the no-input title/sea_T room 44 route to 1,800
retraces. Interpolation, sparse uniform/interface experiments, audio output,
desktop input and presentation are disabled. Captures/checkpoints/stats are
separate from timing runs. Timing requires zero new pipelines, identical
immutable cache, settings, initial player files, affinity, terminal clock,
dispatch count, and all GX summary counters. Background CPU load is recorded;
loaded hidden throughput is not displayed FPS or whole-game performance.

Three original-module diagnostic runs produced identical complete captured
P6 pixels, six CPU/MEM1/MEM2/REL-alias checkpoints and GX counters. The second
run reported 144 new pipelines and failed the warm-cache gate; the first and
third created zero pipelines. All diagnostic timing is excluded, and the
failure is retained. All tests use new private directories.

The scaling-only module matches all six complete checkpoints and captured
pixels, with unchanged GX counters and guest clock. Its diagnostic terminal
dispatch is 1,325,895 versus the original's 1,325,893; this difference remains
unexplained and is not waived for the timing gate. The first targeted
inlining module matches pixels and five checkpoints, but CPU and MEM1 differ
at retrace 600. MEM2 and REL aliases match throughout; terminal dispatch differs
by two blocks. Comparing against the third original run reproduces this same
failure. It is preserved as a failed native candidate; matching the image or
synthetic tests does not establish correctness.

The final canonical-memory isolation build matches **all six complete guest
checkpoints and captured pixels** against the original module. GX counters
and terminal guest clock also match; no new pipelines were created. Terminal
dispatch is 1,325,895 blocks versus the original's 1,325,893, so exact dispatch
equivalence is not established even though the complete sampled state matches.
This establishes sampled state/pixel equality only on the bounded intro route.
It does not prove the earlier failure's cause, all gameplay paths, active
gather batching or writer callbacks.

A separate original-based counter module completed the same 1,800-retrace
route and printed all nine normal-exit census lines. It counted **1,485,168
admitted signed-16 loads through GQR5 and 93 signed-8 stores through GQR4**;
all other type4–7/GQR cells and denied calls were zero. Type-0 generated inline
operations bypass the counters, so this is not a total PSQ instruction count.
Scale values were not counted, so it does not establish how many operations
executed `ldexp` on the original path.
The diagnostic ran under compiler load, was explicitly ineligible for timing,
and changed neither the scaling candidate nor the installed game.

## Loaded timing result

The scaling-only A/B/B/A completed four uninstrumented runs with zero new
pipelines, identical GX summary counters and terminal guest clock. The strict
workload gate failed because one candidate run dispatched 1,325,896 blocks;
the other three dispatched 1,325,895. The original module's uninstrumented
count also differs from its diagnostic count, so a two-block difference is
not unique to the optimization. The gate remains failed and no sample is
discarded or rerun to select a favorable outcome.

For completeness, raw scaling means were 54.4297 versus 54.4688 CPU seconds
(+0.07%) and 36.8125 versus 37.5390 wall seconds (+1.97%). Those are descriptive
failed-gate results under varying background load, **not a measured speedup**.
Pre-case total CPU load ranged from 38.48% to 62.50%; it is a one-second
snapshot, not a continuous external-load measurement. The scaling draft stays
inactive.

The separate scaling-only / isolation / isolation / scaling-only comparison
also completed four zero-new-pipeline runs but failed the strict workload gate.
Guest clocks match, while dispatch counts and five GX counters differ. The
larger GX workload occurs once in each variant, so it is not unique to the
inline candidate. Submitted/planned draws are 8,431,716 versus 8,431,724;
no-ops and zero quads are 3,008 versus 3,024. These differences are retained,
not normalized away.

Raw incremental inlining means were 55.0859 versus 55.2188 CPU seconds
(+0.24%) and 38.0940 versus 38.6485 wall seconds (+1.46%). One pair improved
and the other regressed; pre-case CPU load ranged from 37.11% to 60.45%.
Neither comparison demonstrates a useful performance gain. The inline
generator option stays **off by default**, the scaling patch stays inactive,
and the player's installed build is unchanged. These loaded intro results
do not establish performance in other gameplay areas.

## Local evidence

The adjacent [source and numeric receipt](QUANTIZED_PSQ_2026-10-08.json)
pins final source/tests, five runtime profiles, both native comparisons,
retained failures, timing samples and preservation results. It contains no
game code, assets, player files or ambient environment.

`build/quantized-psq-20261008/` contains private source, arithmetic/codegen,
full runtime qualification, exact module recipes and native receipts. Failed
test-authoring, timestamp-identity and response-file validation attempts remain
preserved alongside the corrected attempts. Game code/modules, ROM inputs,
player files and ambient environment are not public artifacts.

The installed executable/module, player's card/SRAM/settings, active SDK,
dependency recipe and other agents' changes are independently hash-checked.
The disabled Tingle rescue wait-skip is unrelated to this experiment.
