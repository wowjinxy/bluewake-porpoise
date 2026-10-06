# Historical RecompCore patches

The patches through 0151 are historical and are already in the pinned runtime.
`active.json` lists the additional patches applied to that exact base by both
builders. The verifier checks their SHA256 values and the complete resulting
tracked tree; local dependency edits and partial patch sets stop the build.
The dependency lock and build provenance record this recipe.

- 0152 adapts [Elliott Tate's transform snapshot reuse](https://github.com/elliotttate/RecompCore/commit/ef3e17f81f512a655ba40e64f1d4eda09ef50d54),
  retaining BlueWake's existing runtime fixes and optional full-copy verification.
- 0153 adapts [s-ilent's controller enumeration patch](https://github.com/s-ilent/Wind-Waker-Recomp/commit/b643dda2bb402e369f4a565073da52a586e33423).
  The Windows SDL hint is set before initialization at default priority, so an
  explicit override still wins. Pinned SDL already enables this thread by default.
- 0154 adapts [s-ilent's sustained rendering overload policy](https://github.com/s-ilent/Wind-Waker-Recomp/commit/f92ba1a43c5a58a08e40ce8a4129c08fe1ab2412)
  to the current seven-step interpolation implementation, keeping its slow-game
  median, hysteresis, settling and recovery behavior. It includes runtime tests.
- 0155 reconciles controller discovery after missed or duplicate hotplug events,
  preserves device mappings, and supports independent or cleared axis bindings.
  The PAD menu gate neutralizes every input field and waits for held keyboard,
  controller and virtual inputs to release. `tests/pad_backend_test.cpp` tests
  the production backend using SDL virtual devices without a GPU or game data.
- 0156 preserves UTF-8 card paths on Windows with local wide-path file, lock,
  replacement, removal and timestamp helpers. Active cards, validation,
  temporary files and rotated backups identify the same Unicode paths as the
  save manager. `tests/card_menu_test.cpp` uses actual serialization and OS
  locking under a Unicode directory, plus Unicode imports and exact backups.
- 0157 exposes decoder-only registration validation and the actual registry
  winner for managed texture packs. Startup removes a broken managed group so
  lower-priority packs and native textures remain available. Native sidecar
  mip counts and startup decoder budgets are bounded; inspection creates no GPU
  objects. The actual loader fixture runs without a renderer or desktop input.
- 0158 preserves valid samples in the final partial Zelda HLE sound block and
  fills only its remaining tail. The original code restarted filling at index
  zero and left the end unwritten. Actual donor tests reproduce that defect
  with a 64-sample prefix and 56-sample tail, then verify native completion,
  history, full blocks and looping in optimized, debug and sanitizer builds.
- 0159 calls the host once per Zelda HLE voice block after native filtering and
  before dry/reverb mixing. The callback changes scratch samples only; unity
  keeps native output and unknown ownership retains category volume. The host
  validates native music, sound-effect and stream ownership before applying a
  category gain. The target publishes its feature macro to keep header and
  implementation signatures consistent. Native loaded-game coverage remains
  a separate requirement from the donor mixer and ownership fixtures.
- 0160 transports ordered HUD pane descriptors through GXCore and installs a
  host filter before the renderer worker starts. Presentation flushes preserve
  descriptors until an explicit native return or reset. The filter changes
  projection, scissor, visibility and final fragment color; native geometry,
  textures and destination-alpha behavior remain intact. The shader constant
  layout changes with pipeline cache version 13. Customization is off by
  default; native hook and pixel qualification are tracked separately.
- 0161 supplies synchronous color EFB peeks for translated game MMIO. It drains
  FIFO input, flushes pending GXCore assembly, copies the current resolved pass
  and submits a preserving continuation before reading it. Unchanged peeks
  reuse one bounded snapshot; packing applies native pixel-format and PE alpha
  modes. Failed captures cannot retain stale pixels or claim a lost recording
  frame is open. Nearest sampling follows the existing depth-peek approach;
  channel-conversion semantics follow the local Dolphin `VideoCommon.h`
  reference. Host address/state and renderer helper fixtures are separate from
  GPU and native Picto recognition qualification; see
  [the implementation status](../../docs/status/PICTO_EFB_RECOGNITION_2026-10-06.md).

These patches record BlueWake's RecompCore changes as they were made. They are history, not a build
input: the series starts at 0008 (0001-0007 were never exported), so it does not apply to the
upstream base 5c3611e, and the local head it led to (3476998) was never published.

The build uses a fork instead. BlueWake's is https://github.com/chrissotraidis/RecompCore, branch
`bluewake`, commit 2d6063614a9bc899f6b4d11c7e7b3cd66e4d96f3: it contains the changes here through 0097
(some were revised by later ones), the files that were never committed on the development Mac, and the
DolRecomp submodule pointing at https://github.com/chrissotraidis/DolRecomp (5c91d6e). Wind Waker Recomp
builds from its own copy, https://github.com/elliotttate/RecompCore, branch `bluewake`, commit
8ab24da: that tree plus 0098 to 0112, with DolRecomp at https://github.com/elliotttate/DolRecomp
(b8b5345, 5c91d6e plus patches/dolrecomp/0019). Its `windows-release` branch adds 0113 (9618e9d,
the render worker paused while the swapchain changes). BlueWake now builds from
https://github.com/chrissotraidis/RecompCore, branch `bluewake-next`: 9618e9d plus 0114 (one DSP
interpreter table layout under the Microsoft ABI, so Exact/LLE audio no longer calls address 0
on Windows), with the same DolRecomp. The Builder fetches it at the commit pinned in
`scripts/builder/profiles/bluewake.sh`; see docs/status/DEVICE_BUILD.md.

Patches 0115-0125 add the ordered save, shutdown and audio fixes, opt-in display
timing, and a render-worker identity fix reproduced with ThreadSanitizer. The stability baseline
pin was `99e4748002d42c1a86fdcb33a47cd0e97292acff`. Tests and hardware limits are in
[the local stability ledger](../../docs/status/LOCAL_STABILITY_2026-10-01.md).

Patch 0126 preserves the published in-memory card contents after a directory-sync
error, while continuing to report the error. Evidence is in
[the overnight ledger](../../docs/status/OVERNIGHT_2026-10-02.md).

Patch 0127 resumes a paused, already buffered output before the overflow/drop
path can prevent recovery; its actual SDL dummy-device regression covers the full queue.

Patch 0128 keeps FIFO translation on the caller for the whole armed trace,
including frames before capture starts. Its synthetic regression compiles the
production start decision and preserves normal repeated worker starts/joins.

Patch 0129 makes the cross-thread frontend failure flag and submitted/rejected
draw counters relaxed atomics. ThreadSanitizer reproduces all three original
races with the actual linked globals; the same bounded probe passes afterward.

Patch 0130 rejects truncated declared vertex spans before attribute reads or
decoded-vertex allocation. ASan reproduces the original direct-float overread;
38 direct/u16-indexed one/two-vertex truncations and valid trailing bytes pass
in the existing runtime conformance test after the fix.

Patches 0131-0135 reconcile the optional global-memory and interpolation work,
including span and extended-alias safeguards. Patch 0136 directly imports
Elliott Tate's dual-texture post-transform correction. Patch 0137 preserves those
matrices in a versioned frontend save-state extension, accepts legacy states,
and tests direct/indexed FIFO capture and malformed-state rejection. The current
build pin is `18ba3b642588a33b9e8eac4aba7f713bb8d3d778`; the profile and dependency
lock are authoritative. New post-texture save states require this or a newer
runtime; regular memory-card saves are unchanged. BlueWake lava-scene acceptance
is still required; donor scene results are not transferred.

Patch 0140 changes session logging only: GX batches between 20 and 50 ms are summed
into one `[gx-slow-sum]` line every ten seconds, and batches of 50 ms or more keep
their own `[gx-slow]` line. A player's 104-minute Windows log had 33,712 of the old
lines. Evidence is in [the stability plan](../../docs/status/STABILITY_PLAN_2026-10-03.md).

Patch 0141 is Elliott Tate's slow-game detector for Smooth Motion (his RecompCore
`0bb1fef`, Wind-Waker-Recomp patch 0122), with his authorship: the median of the last
60 frame gaps, gaps of 150 ms or more left out, two slow medians in a row. A single hitch no
longer drops the in-between frames.

Patch 0142 counts the time the host holds the guest (a menu, the app in the background):
`DolAuroraFrameTiming.held_us` and `dol_aurora_held_us()`, so per-second diagnostics
leave it out. Logging only.
