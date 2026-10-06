# Picto Box subject recognition: EFB color read gap

Source review on 2026-10-06 identifies a missing core graphics path needed by ordinary Picto Box subject recognition. This is a source-derived finding, not a reproduced native photo failure. No game, renderer, device, or input run was performed for this review.

## Native behavior

The local `../tww` reference checkout describes the original recognition pass:

- `src/d/d_snap.cpp:1897–1907`: `dSnap_packet::draw` clears the alpha buffer, draws each candidate object with destination alpha `col * 4`, then calls `Judge`.
- `src/d/d_snap.cpp:1913–1933`: `ClearAlphaBuffer` uses destination alpha `0xFC`, which identifies background pixels.
- `src/d/d_snap.cpp:1943–1957`: `Judge` waits for `GXDrawDone`, selects `GX_READ_NONE` with `GXPokeAlphaRead`, reads each shutter-area pixel through `GXPeekARGB`, and uses the returned word's top six bits as the candidate index. Index 63 is background; other indices increment that object's captured-pixel count.
- `src/d/d_snap.cpp:1962–1971` and `:2146–2176`: captured pixels, projected area and the native selection rules determine the photo result.

The GZLE01 symbol reference places `Judge` at `0x800CE4A8`, `GXPokeAlphaRead` at `0x80322C98` and `GXPeekARGB` at `0x80322DC4` (`config/GZLE01/symbols.txt:3916`, `:13375`, `:13382`). The native alpha-read register implementation is in `src/dolphin/gx/GXMisc.c:113`; its semantics must be included in a color-peek implementation.

## Current host gap

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

The main risks are same-draw synchronization, destination-alpha preservation, native EFB format conversion and the cost of repeated peeks. Renderer files currently have unrelated work in progress; this report changes no renderer/runtime code.

The verified basic Picto item award, manual save and fresh reload establish item and CARD/ledger persistence. They do not establish photographed-subject recognition or populated photo storage. The enhancement roadmap already keeps populated-photo gameplay verification open (`docs/ENHANCEMENT_ROADMAP.md:13`, `:82–83`). This core graphics task is independent of the disabled optional Tingle wait-shortening work.
