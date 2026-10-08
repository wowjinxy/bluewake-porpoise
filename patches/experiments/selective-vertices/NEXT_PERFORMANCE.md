# Next performance work after selective-input v1

Relocation: original `compact1/overlay/sdk/` source paths map to this bundle's
`sources/ref/recompcore/`. References under `../native1/` are retained private
evidence paths; no captures or game outputs are bundled.

2026-10-08. Source receipt `996b05bee581cbbc9b8e031101b246433f1f89f00e121e5ef2a38202981038ed`.
V1 remains inactive and unqualified for promotion. These are unimplemented,
unmeasured follow-ups; the frozen v1 sources and failed attempts are preserved.

## What was measured

The native diagnostic census recorded 8,431,724 submitted plans and 37,714,670
vertices. Logical full-layout bytes were 4,978,336,440; selective bytes were
1,458,918,624: a **70.6947% reduction**, with zero filter/pipeline fallbacks in
that run. This measures decoder/submission storage volume, not hardware GPU
bandwidth, GPU execution time or displayed FPS. Source: `../native1/normal-selective/logs/0.log`,
lines 1682-1683. Endian conversion, indexed fetching, format decoding and draw
count still follow the original CPU path.

Ordinary native comparison failed complete state equality after the first of
six checkpoints, although its captured image matched. Controlled actual EFB
input replay matched all six states and the image; that diagnostic result does
not establish ordinary gameplay equivalence. Loaded installed-control versus
candidate ABBA timing failed strict workload equality and the timing threshold;
its descriptive CPU/wall changes do not qualify a speedup. Sources:
`../native1/normal-comparison1.json`, `efb-selective-comparison1.json` and
`abba-net1.json`. Keep EFB replay, checkpoints and selective statistics out of
timing runs. Installed host `469f83a7...`, candidate `7c32e473...`, shared module
`976184c6...` are distinct pinned artifacts.

## Prioritized source opportunities

All source locations below refer to the frozen SDK files under `overlay/sdk/`.

1. **Restore a constant full-layout decoder specialization.**
   `GXRuntime/graphics/gxcore/src/gxcore.cpp:1384-1397` performs the enable check,
   TLS recipe lookup and dynamic stride selection even when the mask is zero.
   Lines 1415-1420, 1499, 1508, 1525 and 1551-1554 add dynamic output offsets or
   attribute-presence tests in the vertex loop. A once-per-draw dispatch to a
   compile-time full-layout specialization can retain the preceding 33-float
   stride, constant offsets and defaults; nonzero masks keep the packed route.
   Do not assume candidate `full0` has the installed baseline's cost. Compiler
   hoisting may already remove some operations; no generated-code or timing
   improvement is established. Qualify both routes against the existing full
   canonical digest and malformed-input fixtures before timing.

2. **Reuse the successful early pipeline readiness decision.**
   `GXRuntime/graphics/aurora/lib/gfx/gxcore_draw.cpp:1482-1488` checks compact
   color/depth readiness; lines 1991-1992 check it again. A retained ready
   decision can avoid the second query, because cached readiness is monotonic.
   `pipeline_cache.cpp:1500-1504` takes a mutex whenever any pipeline is queued;
   a completely warm queue uses its cheaper fast path. Mode zero need not query
   readiness for ubershader fallback. Preserve canonical materialization before
   recording every cold/forced uber draw, and keep paired early-depth readiness.
   The avoided cost depends on cache/queue state and is currently unmeasured.

3. **Attach the selective recipe to the existing VCD/VAT walk cache.**
   `gxcore.cpp:776-791` already caches the consumed format; lines 1384-1397
   independently derive the mask and hash into a second 64-slot recipe cache.
   Cache mask, packed offsets and per-entry destinations with the first cache
   to reduce repeated format routing on small draws. This must be keyed only
   by immutable format information, not borrowed guest bytes or live uniforms.
   Retain all present normal/NBT fields and both physical texture-matrix words,
   including TEXMTXIDX5-7. Larger cache entries and collisions could offset a
   gain; the current stride histogram does not measure recipe cache misses.

Lower-priority options are exact validated size comparison instead of dynamic
modulo/division (`shader.hpp:649-653`), avoiding redundant white writes for
present packed colors (`gxcore.cpp:1418-1420`, fully overwritten at 1496-1501),
and one validated reconstruction recipe with reusable scratch capacity for
cold fallback (`shader.hpp:655-673`). The measured warm census had no fallbacks,
so reconstruction is not a demonstrated bottleneck there. Preserve all payload
and indexed bounds checks, integer bit copies, NBT cache updates and shader math.

Before promotion, resolve ordinary-state qualification and repeat matched,
stats-off installed-versus-candidate measurements with pinned warm caches,
module, route and controlled background load. A same-candidate full0/compact1
comparison is useful secondary mechanism evidence, not a substitute for that
net comparison. No further v1 source/build revisions are part of this handoff.
