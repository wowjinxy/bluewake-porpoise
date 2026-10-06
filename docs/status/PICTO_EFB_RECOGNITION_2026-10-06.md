# Picto Box subject recognition: EFB color reads

Source review on 2026-10-06 identifies a missing core graphics path needed by ordinary Picto Box subject recognition. This is a source-derived finding, not a reproduced native photo failure. No game, renderer, device, or input run was performed for this review.

## Native behavior

The local `../tww` reference checkout describes the original recognition pass:

- `src/d/d_snap.cpp:1897–1907`: `dSnap_packet::draw` clears the alpha buffer, draws each candidate object with destination alpha `col * 4`, then calls `Judge`.
- `src/d/d_snap.cpp:1913–1933`: `ClearAlphaBuffer` uses destination alpha `0xFC`, which identifies background pixels.
- `src/d/d_snap.cpp:1943–1957`: `Judge` waits for `GXDrawDone`, selects `GX_READ_NONE` with `GXPokeAlphaRead`, reads each shutter-area pixel through `GXPeekARGB`, and uses the returned word's top six bits as the candidate index. Index 63 is background; other indices increment that object's captured-pixel count.
- `src/d/d_snap.cpp:1962–1971` and `:2146–2176`: captured pixels, projected area and the native selection rules determine the photo result.

The GZLE01 symbol reference places `Judge` at `0x800CE4A8`, `GXPokeAlphaRead` at `0x80322C98` and `GXPeekARGB` at `0x80322DC4` (`config/GZLE01/symbols.txt:3916`, `:13375`, `:13382`). The native alpha-read register implementation is in `src/dolphin/gx/GXMisc.c:113`; its semantics must be included in a color-peek implementation.

## Original host gap

`runtime/host/src/main.c:3678–3691` implements only the depth EFB read window through `aurora_peek_z`. An otherwise unhandled MMIO read returns zero at `main.c:3911–3912`. The Aurora SDK implementation in `ref/recompcore/GXRuntime/graphics/aurora/lib/dolphin/gx/GXCpu2Efb.cpp:9` likewise implements `GXPeekZ` only; no `GXPeekARGB` color/alpha path was found in that source tree.

Returning zero for the missing color read would cause the native `sp8 >> 26` expression to identify candidate zero for every sampled pixel. That consequence is an inference from the source, not an observed gameplay result.

The existing APIs cannot establish the required snapshot:

- `ref/recompcore/GXRuntime/graphics/aurora/include/aurora/gfx.h:37–46` describes framebuffer readback as the next submitted frame's present source. `lib/gfx/efb_readback.cpp:87` selects that present source.
- `include/aurora/gfx.h:49–55` explicitly describes depth peeks as asynchronous snapshots one or more frames old, suitable for visibility tests.

Subject recognition needs the candidate alpha pass that completed before the original `Judge` loop. A visual screenshot or stale snapshot does not prove that correspondence.

## Smallest implementation and verification slice

1. Implement a bounded color/alpha EFB snapshot corresponding to native draw completion, then expose the original color-peek reads and SDK `GXPeekARGB` semantics. Batch the snapshot once for a recognition pass rather than blocking on a GPU read for every pixel. Keep the native `Judge`, object selection and rewards unchanged.
2. Verify packed ARGB ordering, native alpha-read modes, EFB pixel format, coordinates and scaling. Preserve the existing depth-peek path. Audit draw/fence ordering and snapshot invalidation before choosing how to share renderer storage.
3. Extend the existing `gx_fifo_tests` target (`ref/recompcore/GXRuntime/graphics/aurora/tests/CMakeLists.txt:17`). Its `PeekZ` tests at `tests/gx_fifo_test.cpp:3055–3080` already exercise fallback, snapshot and bounds behavior. Add color packing/mode/bounds/reset checks and a shutter-area pattern containing `0xFC` background plus several `col * 4` alpha values, with exact candidate pixel counts. These authored tests do not qualify GPU timing or native photo taking.
4. Separately verify ordinary Picto Box recognition with the renderer, a populated native photo slot, a genuine CARD save and a fresh normal CARD reload. Native image capture uses `../tww/src/m_Do/m_Do_graphic.cpp:1519–1525`; photo serialization uses `../tww/src/m_Do/m_Do_MemCardRWmng.cpp:73–95`, and native photo loading appears at `:175`. Recognition, image capture and durable photo storage are distinct gates.

The main risks are same-draw synchronization, destination-alpha preservation, native EFB format conversion and the cost of repeated peeks. This initial review changed no renderer/runtime code.

## Implementation approach after renderer review

The current draw-done hook drains translated FIFO commands before reporting PE
completion; it does not itself establish a GPU fence. A lazy snapshot on the
first color peek can submit and read the current EFB once, without adding a
readback to every draw-done call or every pixel. Drain FIFO before taking the
recording mutex, require an open recording frame, and keep the snapshot in
renderer-owned storage. Invalidate it after new FIFO work, clears, pixel-format
changes, frame/device replacement and reset.

The existing mid-frame `read_efb_copy` and worker-ordered
`read_texture_rgba8` supply submission and bounded readback behavior. Capture
the current EFB render-pass source, including its MSAA resolve, rather than the
present source. Map logical EFB pixel centers with nearest sampling: the
generic scaled resolve uses linear filtering, which would blend the alpha
values that identify photographed subjects.

Destination-alpha drawing and alpha replacement already exist in GXCore.
The remaining bridge needs native ARGB packing, RGBA6 quantization and the PE
alpha-read register at `0xCC001008`, applying `READ_NONE`, `READ_FF` and
`READ_00` when returning each pixel. Local Dolphin sources provide the
coordinate and conversion reference; the existing depth-peek sampling code
provides a nearest-sampling donor. These are source-reviewed implementation
choices. The implementation and its present verification limits are described below.

## Implemented core bridge

The host now decodes the native color EFB window and preserves the complete PE
alpha-read register. Its callback reaches the Aurora renderer through
`dol_aurora_gx_peek_argb`. The original translated `GXPeekARGB`, `Judge`, subject
selection and rewards remain in use. This implements the translated game's MMIO
route; the SDK's separate direct `GXPeekARGB` entry is not added by this change.

The renderer flushes pending draw assembly, then submits and reads the current
EFB pass, including its resolved color target, before returning the first pixel.
It opens a continuation that preserves the EFB. Subsequent reads of the
unchanged FIFO/frame use that copied
snapshot, with alpha-read mode applied separately to each returned word. Failed
readbacks are cached for the unchanged pass, avoiding a GPU timeout for every
pixel. New FIFO input, frame replacement, state restoration and VI configuration
invalidate the snapshot. Coordinates use nearest pixel centers, and packing
covers RGBA6, RGB8 and RGB565 plus native alpha-read overrides.
The copy reserves aligned uniform storage and finish headroom, opening another
preserving segment when necessary. If a segment loses its recording frame, the
backend closes its flags so the existing recovery path can reopen it safely.

The optional `EFBPEEK` machine-state chunk stores the raw register without
changing the old PE chunk or runtime structures. A malformed chunk is rejected
before CPU or RAM restoration; older states with no chunk reset the register.
Ordinary memory-card saves retain their original format.

Optimized and AddressSanitizer host tests verify all 1,048,576 decoded coordinate
pairs, incorrect windows and widths, provider failures, bounds, and optional
state restoration. The actual extracted host save function also passes in both
builds, including exact new/legacy chunk bytes and preservation of the prior
state file after injected serialization failures. Renderer helper tests pass
3,697 checks in each build for packing, all subject IDs, coordinate policies,
quantization, cache invalidation, lost-frame flags and aligned copy headroom.
These tests do not execute a GPU.

The complete host main and the renderer's common, capture and backend source
files compile with the actual optimized production configuration. The ordinary
integrated Windows host also links successfully: four final source compiles,
two rebuilt runtime archives and the normal executable link pass, preserving
47 existing archive-member payloads and 166 unchanged linker inputs. The already
compiled host main is reused with its exact source/dependency pins. PE imports
and the application manifest match the preceding tester. This is a compile/link
result, with no GPU or game execution.

The exact patched dependency tree is verified against its pinned base, and the
active manifest and dependency lock both describe all ten patches. Initial
recipe failures and successful diagnostics are preserved under ignored build
folders. GPU ordering, ordinary native subject recognition, image capture and
populated-photo save/reload remain acceptance work. The existing controls tester
does not include this subsequent graphics change.

## Real GPU qualification

Patch 0162 adds an explicit, default-false noninteractive startup flag. The
manual `bluewake_efb_color_pixels_test` target uses this flag with its own cache
and user paths. It creates a hidden window, suppresses presentation, bypasses
the paused-window wait and skips audio, controllers and the SDL ImGui input
backend. Ordinary startup retains the default false value. All consumers of
the changed SDK configuration headers were refreshed for the fixture: 69 SDK
translation units and the authored test compiled, seven archives were rebuilt,
and the standalone executable linked with the production dynamic CRT.

On 2026-10-06, three separate hidden processes passed **403 checks each** on
the NVIDIA GeForce RTX 3070 through D3D12:

| Render scale | EFB target | Result |
| --- | --- | --- |
| 1 | 640 × 480 | 403 checks, zero failures |
| 1.5 | 960 × 720 | 403 checks, zero failures |
| 2 | 1280 × 960 | 403 checks, zero failures |

Authored FIFO commands reach the production gather pipe and host color-read
helper before any present. The checks cover exact RGB and subject alpha IDs,
the three alpha-read modes, snapshot reuse, invalidation after new FIFO input,
and preservation of previously drawn regions across captures. Alternating
single-logical-pixel stripes verify nearest sampling at the fractional scale.
Each process also checks its hidden, unfocused window, zero shown frames and
uninitialized SDL audio/gamepad/joystick subsystems before rendering and after
capture; shutdown retains zero shown frames and uninitialized device subsystems.

The first fixture lacked the identity TEV swap table. Packed-word diagnostics
confirmed red became white and green/blue became black while destination-alpha
IDs remained correct. Explicit BP F6/F7 identity selectors repaired that authored
setup; renderer code did not change for this correction. Both failed GPU runs,
their executable/source preimages, build recipe failures and verification
diagnostics are preserved in ignored build folders. The successful GPU result
is `build/core-efb-gpu-oracle-20261006-final/attempt1/result.json` (SHA256
`90b17e48b7f45325d8526e1de15e311d84cf74a5d851433431de01fce7836faa`).

The active manifest and dependency lock now agree on all eleven patches, and
their exact pinned tree verifies. A future ordinary host build must refresh
configuration-header consumers, including `main.c`; an old host config object
must not be mixed with the new SDK layout. The existing controls tester ZIP is
unchanged and does not contain this graphics work.

This qualifies authored current-EFB GPU reads in the tested RGBA6 setup.
Ordinary photographed-subject recognition, native image capture and populated
photo CARD save/reload remain open. Other GPU APIs, sample configurations and
actual device/readback failure recovery have not been qualified by these runs.

The verified basic Picto item award, manual save and fresh reload establish item and CARD/ledger persistence. They do not establish photographed-subject recognition or populated photo storage. The enhancement roadmap already keeps populated-photo gameplay verification open (`docs/ENHANCEMENT_ROADMAP.md:13`, `:82–83`). This core graphics task is independent of the disabled optional Tingle wait-shortening work.
