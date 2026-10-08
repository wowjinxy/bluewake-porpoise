# Conservative specialized shader interface pass

This is an implementation design, not implemented or measured work. It follows the smaller-uniform experiment; it must use a new overlay and must not change the frozen uniform experiment's sources or receipts.

## First implementation boundary

Keep all vertex inputs, physical decoding, selective vertex layout, cached normal/NBT state, matrix-index words, vertex calculations, calculation order, uniform declarations, upload-use decisions, and uber shaders unchanged. Only remove specialized vertex-to-fragment outputs that the generated fragment shader never reads. A graphics driver might already eliminate these; no performance benefit is established.

The safe transformation replaces internal `o.color0`, `o.color1`, and `o.uvN` storage with local variables initialized to the same zero values as the current `var o: VertexOut`. Compute every color and texgen in the original order. Assign only fragment-consumed fields to `o` before returning. Keep position as the existing output and preserve existing output location numbers for each key, including any holes and the existing UV base. This retains emboss forward references and local dependencies without attempting vertex dead-code elimination.

Do not prune declarations alone. The current TEV prelude reads both raster colors and quantizes every UV before examining the stages; those synthetic reads must be made conditional on the same consumption descriptor.

## One shared fragment consumption descriptor

Proposed plain descriptor:

```cpp
struct FragmentConsumption {
  uint8_t colors; // bit0=color0, bit1=color1
  uint8_t uv;     // generated texcoord outputs 0..kMaxTexGens-1
};
```

Compute it once from the same normalized `ShaderKey` and the exact conditions already used by fragment generation. Use it to emit `VertexOut`, final local-to-output assignments, and the TEV prelude. Do not derive a second heuristic from raw vertex attribute presence or from texture resource bindings.

* Non-TEV: keep color0 conservatively, including the textured passthrough path. Add UV0 whenever `textured != 0`, exactly matching the existing sample condition. `num_tex_gens > 0` only gates the STQ projection check; it does not gate that sample. A textured key with zero generated coordinates already references undeclared UV0 and must be reported as a pre-existing invalid-key boundary rather than silently repaired by this pass.
* TEV raster: scan every emitted stage, without constant folding or dead-stage elimination. A stage consumes raster data when any color argument is 10/11 or any alpha argument is 5, using the existing tests. For ordinary raster channels 0/1, add that color channel. Indirect bump channels 5/6 do not consume an interpolated raster color. Mirror any existing fallback/default branch exactly rather than inventing a new out-of-range policy.
* TEV direct sample: add the stage texcoord only when the generator's `direct_sample` condition is true. Apply its exact invalid-coordinate fallback to UV0.
* TEV indirect sample: independently add the indirect stage texcoord when `ind_stage < num_ind_stages` and the current `sample_indirect` condition holds (`ind_matrix_index != 0 || ind_bump_alpha != 0`). This read is not conditional on direct texturing being enabled. Apply the current invalid-coordinate fallback to UV0.
* Preserve existing fragment-side coordinate calculation, wrapping, accumulated indirect offsets, STQ projection, alpha/fog/HUD/destination-alpha behavior, and late depth output.

Source anchors in the active shader before this pass: `gxcore_shader.cpp:292-313` unconditional raster/UV prelude; `318-320` raster demand; `333-337` direct sample; `351-360` indirect read/fallback; `1028-1133` ordered texgen calculations; `1148-1157` passthrough.

## Later dependency pruning, separate experiment

Only after interface-only parity is qualified, a larger `ShaderConsumption` descriptor could include fragment masks, local UV masks, local color masks, and required normal/NBT/light/matrix banks. Begin with fragment masks and compute a backward fixed-point closure through every consumed emboss texgen's source texgen. A Color0/Color1 texgen requires that locally calculated color even when the color is not a fragment varying. Geometry/normal texgens, indexed texture matrices, lighting channel fallbacks, cached NBT, and all matrix rows retain their existing dependencies. Preserve ordered initialization semantics, particularly a forward emboss source.

If this later pass changes light/matrix needs, the same descriptor must feed both WGSL uniform declarations/use and `vertex_uniform_use`. Until then, that helper must stay deliberately conservative. Pruning local work and changing upload decisions together would make a parity failure harder to locate.

Raw input pruning is outside this proposal. Current specialized shaders always declare color0/1 and UV0..3 inputs, and the backend default-stream logic depends on that contract. Selective layout also retains unused physical TEXMTXIDX5..7 and cached NBT data for canonical reconstruction; shader output demand is not permission to discard them.

## Required qualification

Run the actual specialized WGSL through D3D12 and compare exact RGBA8 and Depth32Float outputs against the immediately preceding uniform candidate, independently for complete and sparse bindings and full/selective vertex streams. Keep uber unchanged and byte-verify it. Require actual shader module and pipeline creation with zero validation errors, not only normalized-text checks.

Add diagnostic cases for both raster channels across several TEV stages; Color1 texgen with no fragment raster1; unused intermediate UV needed by emboss; forward emboss source initialized to zero; indirect sampling with direct sampling disabled; invalid direct/indirect texcoord fallback; absent color fallback; zero/one color-channel gating; cached and vertex NBT; projective UVs; alpha discard and reverse-Z depth; HUD and destination alpha. Retain any failed attempt before repair. Actual backend/native tests and matched performance comparisons remain separate gates; this fixture does not certify production pipeline factory transitions or speed.
