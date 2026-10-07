# Native renderer timing comparisons

October 6, 2026. The first serialized native comparison does **not** establish
a repeatable route-throughput improvement. Keep the package optional.

The candidate combines compact vertices, prepared vertex decoding, sparse
uniforms and command-storage reuse (02/03/05/08). Both sides use the same
fixed-lighting executable and active texture patch 07; fusion and immediate
constants are disabled. This comparison cannot attribute a change to one item.

Eight accepted runs followed control/candidate/candidate/control, then the
reverse order. Each used the same native copied CARD, scripted title input,
3,300 retraces, affinity mask 65535 applied before resume, fresh copies of its
immutable warm-cache seed, no capture/state save and no interpolation. Every
run reached a ready Outset player, exited normally, drained and compiled zero
pipelines. Effective framebuffer was 640x480 at scale 2. Runs were hidden and
unfocused, without physical input, audio devices or presentation.

| Complete-process measurement | Control mean / median | Candidate mean / median | Mean / median change |
| --- | --- | --- | --- |
| Launch-to-exit wall seconds | 75.586 / 75.508 | 75.582 / 76.039 | -0.005% / +0.703% |
| CPU seconds, all process threads | 110.652 / 110.617 | 109.172 / 109.984 | -1.338% / -0.572% |
| Process cycles, billions | 397.386 / 397.007 | 391.941 / 393.757 | -1.370% / -0.819% |

Three adjacent candidate pairs took longer; the last was faster. Its larger
CPU reduction dominates the mean. Four repetitions per mode and uncontrolled
unrelated system load limit interpretation. Complete-process measurements
include startup, shutdown and worker CPU. These are unpaced offscreen route
measurements, not displayed FPS, game-update timing or p95/p99 frame times.
Terminal translated-block counts differ by at most one; no complete guest-RAM
parity claim is made.

The original reverse block is retained but excluded. An owned file-inventory
search overlapped run 7; it was stopped and a fresh complete reverse block was
collected. Accepted indices are 1, 2, 3, 4, 9, 10, 11 and 12. No owned compiler,
fixture or other GPU run competed with those accepted native measurements.

Private receipts:

- `build/performance-serialized-cpu-20261006/timing-summary-v1.json`, SHA-256
  `972b83918102ade4ae8c596ced97278a184f773cd2a9cfbd958e850d81e96dcc`.
- `build/performance-serialized-cpu-20261006/timing-analysis-v2.json`, SHA-256
  `38b7fa32e535ba145613c0edc6c56084671da69eae89c76eaee6c6da8ff0497a`.

The additive analysis corrects the first summary's framebuffer diagnostic:
that boolean searched for the wrong log spelling. Actual framebuffer lines
establish the identical scale above; the original receipt remains unchanged.

Correctness captures were collected separately. Further scene and
interpolation checks and individual timings remain required before promotion.

## Repaired fusion alone

The separate repaired-fusion comparison is a regression on the tested route.
Keep fusion disabled. All eight control/fusion/fusion/control then reversed
runs passed readiness, zero pipeline compiles, normal shutdown and zero rejected
or failed renderer submissions. Actual counters confirm fusion was active only
on the candidate, combining about 15.46 million draws per run.

Both modes used the same repaired combined executable, affinity 65535, copied
CARD/input, 3,300 retraces and mode-specific immutable warm-cache seeds. Compact
vertices, prepared decoding, sparse uniforms, command reuse, immediates and
presentation copy were off. Texture patch 07 remained active on both sides.
Every effective framebuffer was 640x480 at scale 2. Captures/state saves,
interpolation, physical input, audio devices and presentation were disabled.
No owned compiler, fixture or other native/GPU job competed with these runs.

| Complete-process measurement | Control mean / median | Fusion mean / median | Mean / median change |
| --- | --- | --- | --- |
| Launch-to-exit wall seconds | 74.899 / 75.265 | 76.769 / 77.148 | +2.498% / +2.502% |
| CPU seconds, all process threads | 110.676 / 111.234 | 130.238 / 130.969 | +17.676% / +17.741% |
| Process cycles, billions | 397.044 / 398.649 | 467.011 / 469.331 | +17.622% / +17.730% |

CPU and cycles increased in all four adjacent comparisons, by roughly 11-21%.
Three candidate runs took longer; one was faster. Reduced submissions do not
establish reduced CPU cost. These measurements do not identify which fusion
operation causes the regression. The same four-repetition, unrelated-load,
complete-process and offscreen limits described above apply. No displayed-FPS,
game-update or frame-time percentile claim follows.

Private receipt: `build/performance-fusion-serialized-cpu-20261006/timing-summary-v1.json`,
SHA-256 `61f6a732fcefa9adad4b18e45dad7df0a38dc65388feee2d462d35fb9c8fb99e`.
No runs were excluded from this separate experiment. Its original READY plan,
per-run logs and orchestration receipts remain intact.
