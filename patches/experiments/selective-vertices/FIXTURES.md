# Rebuilding fixtures in an isolated source checkout

First verify all eleven originals and the SDK/root commits in the bindings, then
apply the inactive patches in a new checkout. Refresh every consumer of shader.hpp,
DrawPlan and DrawData; a partial object replacement is not a qualified host build.

Use the exact original commands in `fixtures/qualified-command-recipes.json` as
auditable recipes. They show real tested flags and input paths, not a relocated
installation. Replace paths deliberately with your own pinned source/toolchain
locations and record actual compiler -M/-MD closures and linker inputs again.
No compiler, system libraries, Dawn DLLs or game assets are supplied here.

- Decoder: compile `selective_decode_test.cpp` with render_sink.cpp, gxcore.cpp,
  gxcore_shader.cpp, gxcore_uber.cpp, texture_decode.cpp, texture_encode.cpp and
  guest_memory.c plus required GXRuntime support. Run a current full-reference
  build using COMPACT_TEST_REFERENCE, then candidate unset/0/11/empty/1 controls.
  Compare canonical semantic digests, not packed byte lengths.
- Interpolation: compile `selective_interp_test.cpp` with actual patched production
  frame_interp.cpp using the recorded production definitions/includes and needed
  support libraries. This is capture qualification, not a broad blend-suite pass.
- GPU: compile `compact_pixels_test.cpp` with unchanged gxcore_shader.cpp and the
  patched shader.hpp include first, link Dawn, then use compatible Dawn/dxcompiler/
  dxil runtimes. It creates offscreen D3D12 targets without a window or input. It
  requires dual-source blending and compares exact RGBA8 plus Depth32Float bytes.
  Both routes generate the same shader text. Its manually authored canonical
  vertices do not execute the production decoder or backend pipeline factory.
  Sparse texture slots share one authored image; early-depth multipass and Smooth
  Motion integration are outside this fixture.
- EFB helper: compile fixture.c beside efb_input_diagnostic.h as one C17 TU with
  the recorded Windows runtime recipe. The retained run_tests.py documents 25
  off/record/replay/refusal/malformed-input scenarios. Never use EFB replay for
  performance timings. Its source preparer performs a byte-exact inverse check.

Exact authored pixel/CPU preparers and predecessor fixtures are retained. Paths
inside those programs still refer to their original private build directories;
the pinned helper and metadata dependencies are references, not bundled payloads.
Run any rebuilt fixture from a fresh private output directory and preserve failed
attempts. This bundle contains no executable and no captured native input trace.
