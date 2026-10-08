# Selective decoded vertex inputs

This private experiment reduces decoded vertex stores and transport. It takes
the idea of selecting vertex inputs from the inspected port templates; it copies
no Killer7 shader text or binaries. Wind Waker's shader math, guest particle
math, GX state interpretation and generated WGSL remain unchanged.

`DOL_GXCORE_SELECTIVE_VERTICES=1` is the only enabling value. Missing, empty,
`0`, and `11` preserve the full decoded format. This switch is experimental;
neither patch is in the active recipe and no installed binary is replaced.

The established layout remains the canonical 132-byte representation.
`DrawPlan::vertex_layout_mask == 0` selects it. A nonzero mask identifies packed
locations in the canonical byte order, with `vertex_floats` carrying the stride.
Position and the integer position-matrix row always occupy the first 16 bytes.
Colors, UV0 through UV4 and normal/NBT follow only when physically present.
Both integer texture-matrix words are retained whenever any of the eight
physical TEXMTXIDX attributes occurs. The existing shader key supports five
texgens, so its narrower texture-matrix mask cannot decide which canonical
metadata may be discarded.

The decoder writes directly into the packed layout. A 64-slot thread-local
format recipe caches field offsets; neither the recipe nor its identity stores
guest geometry. Every original direct/indexed input is still consumed and
validated. Present normals, binormals and tangents remain decoded even when a
shader does not consume them, preserving the last-vertex cross-draw caches.
Float and integer matrix words are transported with exact bit-preserving copies.

Specialized shader bodies are unchanged. Their declared absent color/UV inputs
come from one canonical defaults element, stepped per instance: white colors,
zero UVs and other fields. The default buffer is retained per device and bound
once per render pass only when needed. Each draw still uses instance zero.
The selective pipeline includes its layout mask and uses cache version 15,
distinct from the earlier inactive raw-vertex version 14.

A cold or forced ubershader path reconstructs full geometry before recording
canonical color/depth/uber pipeline keys. A compact-ready path reuses the
pipeline references from its readiness check. Cached ubershader sibling states
clear the layout mask. There is no late switch interpreting packed bytes with
a canonical pipeline or the reverse.

Legacy plan filters receive reconstructed canonical vertices by default. A new
third `set_plan_filter` argument certifies selective awareness; the public
pre-init host certification is cleared whenever a new callback is registered.
Only the audited built-in HUD callback opts in: it changes constants, scissor
and tint, with no decoded-vertex access. Known trace, interpolation capture,
helper jobs, blended uploads and draw binding follow the plan/draw stride.
Fusion also requires identical masks and strides and retains ordinary topology,
constant, texture, index and interpolation compatibility checks.

`DOL_GXCORE_SELECTIVE_STATS=1` separately enables recording-thread counters,
reported after the FIFO worker stops. They record planned and submitted vertex
bytes, full equivalents, fallback reasons and stride distribution. They are
neither GPU hardware counts nor timings. Performance measurements leave this
diagnostic switch unset.

# Evidence and limits

- `decode-attempt3/result.json`: optimized current full-reference and selective
  candidate have the same canonical semantic digest `48bb8c3c89859c33` across
  597 diverse draws. Candidate controls unset/0/11/empty/1 each passed 455,292
  checks, including all 4,096 valid masks, every incomplete payload prefix,
  indexed bounds/unresolved arrays, full reconstruction, NBT cache updates,
  shader text, pipeline and uniform comparisons. The full reference passed
  114,643 applicable checks. Actual compiler `-M`/`-MD` sets and input hashes were
  pinned and compared; actual linker reproduction members were checked.
- `interp-attempt2/result.json`: actual production interpolation capture passed
  204,850 optimized checks across the canonical plus 4,096 packed layouts,
  including invalid counts/strides/truncation and copied-position lifetime.
- `decode-asan-attempt1/result.json`: the same 597-draw decoder fixture passed
  under AddressSanitizer in all five opt-in modes, each with 455,292 checks and
  canonical digest `48bb8c3c89859c33`, matching the qualified full O3 reference.
  `interp-asan-attempt1/result.json` passed 204,850 capture checks across 4,097
  layouts under AddressSanitizer. Both retained actual compiler dependency,
  linker reproduction, toolchain and runtime input pins; no game or GPU ran.
- `../pixels1/attempt2/result.json`: actual offscreen D3D12 selective/full
  comparisons passed 43 synthetic cases, 86 submitted frames and 1,413,797
  checks, with exact RGBA8 and D32 bytes and no uncaptured errors. All 50 WGSL
  source pairs were byte-identical. This covers declared/default inputs, NBT,
  UV0-4, texture matrix metadata and mixed selective/full/selective pipelines;
  it is synthetic renderer evidence, not native scene or performance proof.
- `../build1/work4/result.json`: the 18-TU private host/SDK build passed with
  preserved inputs. The complete build receipt remains owned by the build
  qualification work; this does not establish gameplay or timing.
- Failed decoder attempt 1 retains fixture API drift. Failed decoder attempt 2
  retains the real TEXMTXIDX5-7 metadata loss fixed before native integration.
  Its source snapshot is `decode-attempt2/overlay-failed`. Interpolation attempt
  1 retains an incorrectly ordered test include path; attempt 2 used the correct
  selective header and recorded its actual dependency closure.
- The prior fixed 60/132-byte draft remains untouched. Its documentation
  snapshot was incomplete: the research checklist also records actual GPU and
  Outset/Dragon Roost capture parity. Those results do not qualify this new
  selective implementation. Its unresolved broad interpolation ASan baseline
  failure also remains a separate limitation; focused capture is not a broad
  interpolation pass.
- Native v1 title-route experiments are complete and unqualified. Ordinary
  comparison retained `FAIL_PRESERVED_NATIVE_STATE_OR_PIXEL_DIFFERENCES`: only
  one of six checkpoints matched, although the captured P6 pixels were exact.
  Controlled replay of recorded actual EFB inputs matched all six checkpoints
  and pixels for full and selective layouts. Physical readbacks still executed;
  replay observed 12 full-layout and 72 selective physical differences across
  1,134 entries. This supports only controlled-input parity and does not explain
  away the unresolved ordinary state failure.
- Loaded ABBA retained `FAIL_ANALYSIS_GATE_PRESERVED`. Descriptive mean CPU time
  was -0.9876%, wall time -1.4699%, and process cycles +0.3056%. Candidate runs
  submitted/planned eight extra draws and sixteen extra no-ops; exact workload
  equality and the timing threshold failed. Strict warm-cache analysis passed
  with zero terminal pipeline creations and no new semantic configs in all four
  runs. These checks do not qualify performance or promotion.
- The native selective census stored/submitted 1,458,918,624 logical vertex
  bytes versus 4,978,336,440 full equivalents: 70.69465590397101% fewer bytes,
  with no filter or pipeline fallback in that diagnostic. Hardware GPU
  execution time/bandwidth and displayed FPS were not measured. Fewer logical
  bytes alone establish no speedup. Exact receipt hashes and bounded results
  are in `evidence/native-results.json`.

The final read-only draw-path audit found no defect for production-generated
masks. The instance defaults stream always uses instance zero, and pass state
resets at pass start and after Clear/GX/Rml draws. Arbitrary stride padding uses
modulo arithmetic, staging headroom covers its maximum padding, and fusion
requires identical masks/strides and contiguous vertex/index ranges. Helper
jobs copy the actual stride and vertex bytes before a reusable full fallback
plan can change. Both forced and cold ubershader paths materialize canonical
geometry before recording canonical pipeline keys; startup uber siblings also
clear the layout mask. The defaults binding gate relies on the producer policy
that all optional shader-declared attributes are retained; handcrafted masks
that violate that policy are not certified by this audit.

`source-freeze-v1` preserves the earlier coherent snapshot before the final
local pipeline-reference/default-binding cleanup. Current source pins and
inactive patch application checks are in `source-receipt.json` and
`baseline-bindings.json`. The completed native/timing gates leave this v1
experiment inactive and unqualified; source promotion has not occurred. The
translated game module and Tingle rescue wait-skip behavior are unchanged.

`NEXT_PERFORMANCE.md` records source-backed but unimplemented and unmeasured
opportunities. Original source paths under `compact1/overlay/sdk/` map to this
bundle's `sources/ref/recompcore/`; private `../native1/` evidence remains outside
the source payload. Normal state qualification remains unresolved.
