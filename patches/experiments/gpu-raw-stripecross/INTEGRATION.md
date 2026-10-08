> These sources remain inactive. Read [RESULTS.md](RESULTS.md) for the v3 restart pass, unresolved normal-state differences and unqualified loaded timings.

# Integration contract and prospective source promotion

## Selected files

The three patches target the BlueWake repository root. Patch 01 prefixes the
compact nested GXRuntime renderer v3 patch with `ref/recompcore/GXRuntime/`;
its original SHA256 is
`1af5313cf500b03ea381abdb91339324ace795db56f264e5c81f8e3176bc8f7d`.
Patch 02 is verbatim `root-host5` host glue, SHA256
`a43f113f10ee52f6ddff417b091fcad8c8841b1c47aa92d4d6775d421c5cd2a0`.
Version 5 adds the missing declaration of existing `g_host_plan_filter` used by
the policy; version 4 and failed host build `work3` remain retained separately.
Patch 03 adds only the two new native source files. It does not enable or wire
the adapter into a normal build. The copied complete sources and patches must
be reviewed together against the exact source pins before promotion; a parent
repository patch does not by itself commit a nested Git repository change.

Renderer edited files, relative to `ref/recompcore/GXRuntime/`:

- `graphics/gxcore/include/gxruntime/gxcore/gxcore.hpp`
- `graphics/gxcore/include/gxruntime/gxcore/shader.hpp`
- `graphics/gxcore/src/gxcore.cpp`
- `graphics/gxcore/src/gxcore_shader.cpp`
- `graphics/aurora/lib/gfx/gxcore_draw.hpp`
- `graphics/aurora/lib/gfx/gxcore_draw.cpp`
- graphics/aurora/lib/gfx/pipeline_cache.cpp (v3 startup raw-to-uber guard only)

`Root-host5` edited files:

- `ref/recompcore/GXRuntime/include/gxruntime/aurora_backend.h`
- `ref/recompcore/GXRuntime/backends/aurora/aurora_backend_private.h`
- `ref/recompcore/GXRuntime/backends/aurora/aurora_backend.cpp`
- `ref/recompcore/GXRuntime/backends/aurora/aurora_graphics.cpp`
- `runtime/host/src/hud_renderer.cpp`
- `runtime/host/src/main.c`

Native additions: cmake/composite/native_stripe.c and native_stripe.h.
The header has no additional CPUState fields. The module export is
`bluewake_composite_gpu_stripe_tails_v1(bool, ready, write_bytes, user)`;
ready has signature `bool(void*, const CPUState*, u32)`, and write_bytes is the
existing gather-byte writer type. No FIFO protocol or side queue is added.

## Renderer admission, lifetime and fallback

`build_draw_plan_into(..., allow_raw_pos_uv=false)` preserves decoded default
behavior. Raw plans require an explicitly installed sink policy and exactly
two direct attributes: F32 XYZ followed by F32 ST, stride 20, exact payload
length `vertex_count*20`, UV mask 1, no matrix-index bytes, normals/NBT/colors or
other attributes. F32 fraction fields remain legal because legacy F32 decoding
ignores them. Original topology, texture/TLUT state and uniforms remain built.

The raw plan holds a borrowed FIFO payload only until submission. Submission
copies its bytes to existing frame-owned vertex/storage staging. Shader binding
0 is the existing storage-capable vertex buffer; `instance_index` is the word
base and `vertex_index` selects a five-word vertex. The same generated transform,
lighting and TEV body follows the BE-F32 decode and white color/default inputs.
Indexed quad/strip topology remains unchanged. `PipelineConfig` version 14 and
`rawPosUv` distinguish raw pipelines; raw has no vertex-buffer layout. Raw fusion
uses contiguous 20-byte ranges and same pipeline; mixed decoded/raw plans cannot
fuse. Frame reset and FIFO save/load/drain still use existing ownership/order.

`materialize_raw_pos_uv` recreates legacy 34-float vertices, including exact
unused PN row bits and cached normal/NBT constants. Malformed descriptors reject
transactionally. Pipeline unready, interpolation, diagnostic plan texture and
forced ubershader mode 2 use decoded fallback. Raw counters describe submitted
plans/bytes and fallback causes, not a measured timing benefit.

Host raw opt-in requires exact `DOL_GXCORE_GPU_RAW_POS_UV=1`, interpolation off,
trace unarmed, no DOL_GXCORE_PLAN_TEX, and either no host plan filter or an explicit
geometry-independent filter promise. Re-registering any filter clears that
promise. The audited HUD filter changes constants/scissor/tint only and certifies
itself pre-init. Generic consumers remain conservative by default.

## Native interior boundary

First tail: entry `0x80263E7C`, resume `0x80263F3C`; previous PC
`0x80263E78`/suffix 53, final PC `0x80263F38`/suffix 5. Second: entry
`0x80264260`, resume `0x80264320`; previous PC `0x8026425C`/suffix 52,
final PC `0x8026431C`/suffix 4. Each tail is exactly 48
original instructions, ten stfs operations and 36 FP operations. Both normal and
prepaid copies are hooked, but only `cycle_block_prepaid` may enter. The existing
prepaid debit remains unchanged; neither charge nor refund is introduced.

Admission requires valid CPU/RAM, exact previous PC/suffix, FP enabled, no current
exception, no write journal, positive cycle budget, downcount<=0, and a positive
deadline budget (when active) at least the previous suffix. It requires original
FIFO base register `0xCC010000` (`stfs` displacement -32768 means `0xCC008000`), plain
stable aligned eight-byte MEM1 constants at r2-15972, no registered FIFO RAM alias,
the existing word writer and enabled byte callback. With batching, existing
length must be strictly below 216 so appending 40 cannot flush within the tail;
without batching, existing gather length must be 0. The host ready callback must
approve before any mutation. Rejection leaves CPU/RAM/gather unchanged apart
from private diagnostic decline counts.

The host callback preserves ordinary deadline, delivery, state trace, jump,
feature/game event, HUD/health, randomizer reward, quick item/dialogue/enhancement/
autosave and chassis/interrupt guards. Only the broad particle draw-tag range is
replaced with the exact audited no-op interior decision. Developer tracing and
edge census always decline. It skips no particle callback or earlier axis/basis
guest stores. All FP/PS1/FPSCR live-outs remain computed with existing helpers.

The host handshake needs exact `BLUEWAKE_GPU_STRIPE_TAILS=1`, Aurora enabled,
FIFO trace off, census off and existing byte writer available. The two opt-ins
are independent, allowing CPU-only/GPU-only/both controls in later measurement.

## Rebuild and future normal-builder work

Do not mix old DrawPlan/PipelineConfig consumers with new headers. Rebuild the
complete actual dependency union, including SDK gxcore, frontend/render sink,
Aurora common/pipeline_cache/gxcore_draw and host HUD plus
hud_customization_draw_plan consumers. Preserve unrelated archive members and
pin all actual MD dependencies and final link inputs. Exact private build records
are retained at these repository-relative paths:

- `build/gpu-offload-20261008/build1/work4/result.json`: original 18-TU host/SDK
  rebuild using the changed header-consumer union and selected host policy.
- `build/gpu-offload-20261008/build1/work6/result.json`: final one-TU v3
  pipeline-cache successor, preserving the other 17 objects and archive members;
  final host SHA256 is
  `508656271902f8d2dd8280e4d769452fd6c3fd96222322577151032f9f93c598`.
- `build/gpu-offload-20261008/particle-design1/module-attempt1/result.json`:
  exact `module976` closure with the new adapter and patched chunk objects;
  final module SHA256 is
  `7b10569a929e183ad31aa863b1f21edb689f0e5ee4da1a96016eb50ee3d551e3`.

These are private experimental build records, not normal-builder integration.
Native exercise and cached restart have passed their individual run checks;
ordinary guest-state equivalence and performance remain unqualified.

To qualify the CPU source, compile `native_stripe.c` with the same module CPU ABI,
native_inline_fp.h/gather_pipe headers and target flags as retained `module976`,
then substitute only its new object and the freshly patched `chunk0152` object in
the exact `module976` link closure. No PGO/LTO or generated-source regeneration is implied.
For prospective public integration, the normal composite preparation/build path
must add the two native files, compile the adapter and perform the exact bounded
hook insertions before chunks are compiled; wire this only after a separate
review. The helper below generates private outputs, never edits inputs:

```powershell
python tools/prepare_frozen_stripe.py --frozen-composite <module976-composite-src> --output <fresh-private-directory>
```

It pins `chunk0152` SHA256
`58b9345972121b76e8b82b7dcd1159fc7c40d493fa6991fce06c2d7325be9c61`
and `generated.h` SHA256
`ce68d056edc5868b9214ab8c228506510cc73c3895f8ac62a1ff563f4f70a815`.
It requires 48 instructions/ten stores in each literal tail, inserts only guards/
resume labels/include, and proves removing those additions restores the original
bytes. Expected overlay SHA256 is
`13bb3a5f144293146dedb164fae7f71daa1c02b0a2ce6b4cddf66be273204f11`.
A different game/source revision must decline and be audited afresh.

## Promotion remains blocked by normal-state and timing qualification

Before enabling or promoting, require matched decoded/raw native pixels,
identical complete CPU/MEM1/MEM2/REL checkpoints, parser rejection/order checks,
actual raw hits and fallback telemetry, staging/depth/adjacency/fusion behavior,
then quiet paired warm intro timings. If correctness or timing fails, retain this
source experiment inactive and preserve all negative receipts. Unset both opt-ins
for ordinary decoded/FIFO behavior. Any installation requires a separately
reviewed reversible replacement procedure.
