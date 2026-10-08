> Fixture evidence below was captured before native integration. Current native and loaded-comparison results, including failures, are in [RESULTS.md](RESULTS.md).

# Completed fixture evidence and reproduction recipe

These are pre-integration source fixtures. They do not establish native game
correctness, visual equivalence of an entire intro, stable game FPS or speedup.
No game window, OS input or mouse control was used by these fixtures.

## Completed tests

- CPU literal `module976` oracle: 80,000 accepted tails plus 54 conservative
  negatives. Both tails, direct 40-byte emission and no-flush batch append were
  compared against the exact independently extracted prepaid bodies. Full CPU
  state (including PS1/FPSCR/downcount/suffix), 32 MiB guest RAM, complete gather
  buffer and FIFO stream were compared. Random FP inputs include signed zero,
  subnormal, Inf and NaN. This oracle is generated game source and is deliberately
  absent from the handoff; tools/prepare_frozen_stripe.py recreates it from owner
  supplied pinned inputs.
- Actual production decoder fixture: 814 cases, 330 raw admissions, 484 conservative
  declines, 9768 checks. Default API stays decoded; component/attribute enumeration,
  all topologies/counts/PN matrix rows, malformed payloads/projection/cull and
  unusual F32 bit patterns are covered. Materialization compares full byte-exact
  legacy vertices, topology, pipeline, constants and cached normal/NBT state.
- Actual D3D12 offscreen shader fixture: 13 cases, 26 frames, 213411 checks;
  full RGBA8 tolerance 0, zero mismatches, zero Dawn validation errors, each image
  nonclear. NVIDIA RTX 3070, driver 32.0.16.1088. Covers quads/strip, nonzero word
  base, BE XYZ/ST, texture UV, TEV register/texture color, blending/destination
  alpha, scissor, fractional/perspective transforms, geometry/default-normal
  texture-coordinate source and ordered overlap/state.

GPU fixture scope uses the generated normal/raw shaders and real D3D12, but
directly creates synthetic Dawn resources/pipelines. It does not exercise actual
Aurora frame staging, depth prepass or adjacency fusion, nor all cached-light/NBT
shader branches. Decoder fixtures do compare preserved NBT/lighting constants.
Native integration has now been exercised; its exact evidence and unresolved
normal-setting limitations are recorded in RESULTS.md.

The CPU oracle fixtures use inert FIFO writer collectors and do not exercise
every host MXCSR rounding or flush-to-zero mode. A real direct bulk write could
service pending presentation at a different byte boundary; this is a remaining
hypothesis, not a demonstrated defect. The normal-setting divergence with both
opt-ins disabled and the installed baseline's repeat divergence prevent
attributing the observed state failure to the enabled shortcut.

Retained result SHA256s:

- gpu-pixels: da0c7a26bb8ff47e815a44a82cf29f17e7fad8615dbc7cf0a91805a5108ad736
- raw-plan: 5f6759c3dabac56b31d18fe5ac934fe91a4268333a6720948c2fa9f24bf7e082
- cpu-stripe: see validation-summary.json (captured from the existing receipt).

## Reproduction from source

`fixtures/qualified-command-recipes.json` preserves every actual compiler/link/
run argv from those successful qualification receipts, including exact Clang,
resource paths and CRT libraries. They contain local absolute paths and must be
mapped to your private output/source locations rather than copied blindly. They
are retained command evidence, not a new portable build system. No binaries or
vendor SDK are included; supply the same approved Dawn/SDK/toolchain inputs.

1. Verify this handoff and source pins. Create fresh private output directories
   and retain every failed attempt. Do not write to the installed game or live
   source tree. Coordinate the compiler slot before running any commands.
2. For decoder tests, copy fixtures/raw_plan_test.cpp and raw_plan_helpers.cpp
   into a fresh work directory. Generate `raw_decoder.cpp` beside them:

   ```powershell
   python tools/extract_decoder.py --gxcore-source sources/ref/recompcore/GXRuntime/graphics/gxcore/src/gxcore.cpp --output <fresh-work>/raw_decoder.cpp
   ```

   Compile the two fixture TUs as C++20/O3 with MSVC compatibility 19.44, dynamic
   CRT, `_CRT_SECURE_NO_WARNINGS`, and the exact gxcore/frontend/runtime includes.
   The unused-variable suppression belongs only to the inherited fixture setup.
3. For pixels, compile fixtures/raw_pixels_test.cpp and the selected production
   gxcore_shader.cpp. Use C++20/O3, MSVC compatibility 19.44, dynamic CRT and
   `-fuse-ld=lld`; link the pinned webgpu_dawn import library and approved CRT/SDK
   libraries. Copy pinned Dawn/dxcompiler/dxil DLLs only to private test output.
   Run the console fixture without a window/surface. It selects actual D3D12.
4. For the CPU tail fixture, generate the exact oracle privately, then compile
   `native_stripe.c`, generated stripe_oracle.c, fixtures/native_stripe_test.c and
   existing `gather_pipe.c` using the successful receipt flags. Important flags:
   O2, `-ffp-contract=off`, `-fno-slp-vectorize`,
   `-DBW_F32_LOAD_HW_WIDEN=1`,
   `-DBW_GUEST_MEM1=bw_guest_mem1`, `-DBW_GUEST_MEM1_SIZE=0x02000000u`.
   Supply the same CPU/runtime ABI and retained support library closure. Run
   the fixture with argument 20000. Do not use a reimplemented arithmetic oracle.
5. For each TU, run a dependency-only `-M` pass first, pin the complete union,
   compile with `-MD`, require exact dependency-set equality and unchanged hashes.
   Preserve `/reproduce` link receipts and audit actual physical link inputs.
   Run only after successful source/closure checks; record hardware/results.

Historical failures remain in the original private folders: GPU attempt 1 lacked
MSVC compatibility 19.44; attempt 2 selected the old linker; successful attempt 3
reused exact pinned successful objects and switched to lld. Decoder attempts 1/2
were fixture-only warning configuration fixes; attempt 3 passed. A cp1252 source
receipt preparation failure is retained separately; final selected overlay was
restored from immutable UTF-8 frozen inputs. These failures did not edit live
sources or weaken production admission.
