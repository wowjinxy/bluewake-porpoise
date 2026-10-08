# Qualification results: keep inactive

Evidence snapshot: 2026-10-08 06:29 UTC. All cases below use the hidden unpaced
title/`sea_T` room 44 route to 1,800 VI without physical input. Captured frames and
state-check cases are correctness diagnostics, not timing observations.

## Source and fixture evidence

- Literal `module976` CPU oracle: 80,000 accepted tails and 54 conservative declines;
  exact CPU/PS1/FPSCR, guest RAM, gather buffer, FIFO bytes and prepaid state.
- Actual decoder fixture: 814 cases, 330 admissions, 484 declines, 9768 checks;
  strict raw layout and exact legacy materialization/default cached constants.
- Real offscreen D3D12 shader fixture: 13 cases / 26 frames, 213411 checks,
  RGBA8 tolerance 0, zero mismatches/validation errors. These synthetic tests do
  not qualify all normal game renderer behavior.
- Private source/build closure checks passed. `Root-host5` corrected a missing
  declaration of existing `g_host_plan_filter`; renderer v3 fixed the cached
  raw-to-ubershader startup bug. No public source patch was applied here.

## Cached startup failure and correction

The initial v2 raw warmup completed 1,800 VI, but restarting from its v14 cache
crashed in D3D12 `CreateRenderPipeline` with `E_INVALIDARG` before guest execution.
Both merged legacy/candidate cache and unmerged candidate-only cache reproduced
it. Combining Dawn keys was therefore not required to trigger the failure.

Startup `queue_ubershader_state` cloned a raw configuration into `depthOnly=2` while
retaining `rawPosUv=1`, pairing decoded ubershader inputs with no vertex layout.
The `CHECK` was compiled out under `NDEBUG`. Version 3 adds only a nonzero `rawPosUv`
early-return guard to that queuing function. All six v2 renderer source files,
`PipelineConfig` version 14 ABI and runtime materialization fallback remain unchanged.

The v3 normal cached restart passed with host `50865627...` and module `7b10569a...`.
It exercised 39,926 raw plans, 582,328 vertices and 11,646,560 raw bytes, avoiding
76,867,296 decoded staging bytes. Pipeline/interpolation/diagnostic fallback
counts were all 0. Both native tails executed 114,206/114,194 times with 0 declines.
These counts prove the paths ran, not that they improved speed.

## Normal guest state remains unqualified

The normal candidate's full captured P6 image matched the installed control,
but complete checkpoint hashes did not: at 600/900 CPU and MEM1 differed, and
at 1200/1500/1800 MEM1 differed. Retrace 300, cycle counts, MEM2 and REL-alias
hashes matched. The same new host/module with both opt-ins 0 still differed at
600 CPU/MEM1 and later MEM1. Enabling raw decode/native tails is therefore not
necessary for the observed mismatch.

Repeating the exact installed host `469f83a7...`/module `976184c6...` also differed in
MEM1 at 600, with 5/6 complete checkpoints and the captured image matching. This
proves existing baseline nondeterminism, but does not account for every new
binary's differing byte. The normal-setting comparisons remain failed.

Existing depth peeks publish asynchronous GPU snapshots at a 30 Hz wall-clock
rate and return the latest completed sample to `GXPeekZ`. Guest sun visibility
code stores those values in MEM1. This is a concrete timing-dependent input
mechanism, not a byte-level identification of all observed deltas.

With only `BLUEWAKE_EFB_PEEK=0` added to both installed control and enabled
candidate, all six complete CPU/MEM1/MEM2/REL-alias checkpoint hashes and full
captured P6 matched. This switch suppresses both color and depth host MMIO
peeks. It isolates EFB input as a class; it neither proves depth alone caused
the mismatches nor qualifies ordinary EFB-peek gameplay. Receipt SHA256:
`a9fffc279d517baaf4855c308844ba8755ee234d142b9be0390237032f30466d`.

## Measurements under existing background load

A quiet preflight initially failed at 41.63% total CPU versus the 5% limit.
The user then requested a comparison under the current load while keeping the
prototype experimental. Seven cases completed individually and preservation
checks passed. These values are descriptive process CPU and wall seconds for
the entire unpaced intro; they are not measured gameplay FPS.

| Case | CPU seconds | Wall seconds | Pre-case total CPU busy |
| --- | ---: | ---: | ---: |
| Installed control A1 | 46.296875 | 31.282 | 30.87% |
| Both paths B1 | 52.281250 | 35.484 | 40.72% |
| Both paths B2 | 46.343750 | 31.250 | 32.31% |
| Installed control A2 | 47.734375 | 32.453 | 42.88% |
| Same new binaries, both off | 48.453125 | 33.032 | 30.86% |
| CPU companion only | 45.578125 | 31.547 | 29.10% |
| GPU raw decode only | 49.718750 | 33.922 | 52.64% |

The strict ABBA workload gate failed: A1/B1 submitted 8,431,716 plans with 3008
noops; B2/A2 submitted 8,431,724 with 3024 noops. The original gate was not relaxed.
Background CPU figures are one-second pre-case samples, not continuous measures
of external load. Normal guest-state differences also remain unresolved.

The CPU-only observation is promising but unreplicated. GPU-only had the
highest background load. No CPU-only, GPU-only or combined speedup is established,
and no default activation or tester installation follows from these values.
The combined-run means used 4.89% more process CPU and 4.71% more wall time
than the installed controls. The first pair was slower while the second was
faster, consistent with the failed workload/load controls. The single CPU-only
observation used 5.93% less CPU and 4.50% less wall time than the same new
binaries with both paths off; GPU-only used 2.61% more CPU and 2.69% more wall
time. These percentages remain descriptive, with no qualified causal gain or
regression established. Descriptive receipt SHA256:
`cd234038113ac90d88d31b85cfb8da4d279262e8daeead556a9e3357495b9d2c`.

Seven-case receipt SHA256:
`fb2176d78942b44a50f0af18bb466309eb5422d10a66248a86055891d69d7eec`.

## Requirements before promotion

Account for normal-setting state deltas at byte level and verify ordinary EFB
readback behavior; repeat matched full renderer/game correctness checks, then
quiet, workload-matched paired timings with separate CPU/GPU controls. Preserve
the failed comparisons and require evidence beyond an isolated faster run.
Until those checks pass, both paths and all three patches remain inactive.
