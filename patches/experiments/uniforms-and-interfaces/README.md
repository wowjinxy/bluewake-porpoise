# Smaller uniforms and specialized shader interfaces

Two opt-in renderer candidates are implemented here, against the current
selective vertex layout. They are frozen source experiments outside the active
runtime recipe. The installed game, player data and existing diagnostics are
preserved. Native state parity remains unresolved, so neither candidate is
promoted into the active build recipe.

The first candidate splits the complete 2,832-byte vertex constant value into
draw fields (656 bytes), matrix memory (1,536) and lights (640). Each bank stages
only a conservative leading range needed by the specialized shader and reuses unchanged
bytes within its current staging packet. Uber shaders retain complete banks.
Without Smooth Motion, canonical repeat tracking uses stable constant IDs and
does not copy or compare an extra complete snapshot. Interpolation retains
complete-state equality. Pixel constants and the HUD multiplier keep their
existing behavior.

The second candidate removes specialized vertex-to-fragment colors and UVs
that the emitted fragment program does not consume. All vertex calculations,
their order, local color/UV dependencies, physical inputs, decoder behavior,
uniform use and uber shaders remain complete. Zero-initialized locals preserve
forward emboss references. Output locations keep their previous numbers.
Direct and indirect texture consumers share the same fragment-demand analysis.

Only exact `DOL_GX_SPARSE_UNIFORMS=1` enables the uniform candidate. Only exact
`DOL_GX_SHADER_INTERFACES=1` enables the interface candidate. Both default off.
The uniform source uses pipeline-config version 16; applying the interface
patch produces version 17 with independent interface and uniform mode fields.
Full controls and sparse/pruned pipelines have distinct persisted identities.
These switches require the corresponding experimental executable; the current
installed executable does not contain this adaptation.

The user-playtested comparator is host `7c32e473...` from
`build/k7-adaptations-20261008/build1/work4/host/BlueWake.exe`, with translated
module `976184c6...`. The private builds retain that host's launcher/default
objects to isolate the renderer changes. Root main `55e04e1` separately added
the interactive selective default and safe-mode setting; those two unrelated
startup changes are excluded from this comparison. No PGO, module, mod selection,
or Tingle rescue wait-skip changes are part of these experiments.

The uniform host rebuilt 53 translation units and four archives from the exact
production closure. Its executable SHA-256 is
`101e2d2f96241abb42b8ddc6bdc67be52f32055900581e2cc290ee5df42f2eda`.
This build result is separate from native game and performance qualification.
The interface host rebuilt the twelve additional consumers of its changed
headers, retaining the verified uniform objects. Its executable SHA-256 is
`0f5efd84b3eed3c4d7fbf737ec879962d7c3d1e89c8ba55e7f952ff8c7cf4234`.

CPU and actual D3D12 fixture qualification passed. See
[CPU qualification](CPU_QUALIFICATION.md),
[GPU qualification](GPU_QUALIFICATION.md), and the final bounded
[evidence summary](evidence/qualification.json). The fixtures cover cache
growth/shrinkage, failed uploads, packet changes, interpolation identity,
both shader-bank modes, default-shader parity and exact synthetic color/depth
comparisons. Reduced fixture upload bytes do not establish a frame-rate gain.

The ordinary native title route completed 1,800 retraces with the same copied
CARD/settings/SRAM and retail inputs, physical EFB reads, no scripted buttons,
Smooth Motion off and no renderer rejection or failure. Seven comparisons have
exact P6 bytes at the single captured frame (retrace 1,100). Complete guest
checkpoint equality remains unresolved: the unchanged previous host repeated
only one of six checkpoints exactly; uniform off/on matched two, and interface
off/on matched one. This does not identify the cause or exclude differences
introduced by a candidate. Captured-frame equality does not certify the entire
intro or full gameplay.

The original common-cache runs retain their failed warmth receipts: cached
pipelines from both modes were reconstructed at startup. Subsequent private
profiles filter that immutable parent cache to a selected version and mode.
All five profiles have the same 241 normalized GX key/state payloads, identical
non-GX rows and byte-identical complete Dawn databases. Cache warmth and timing
are independent gates from the explicit state/pixel comparisons; none of the
instrumented correctness runs is used as performance evidence.

The derived-profile uniform off/on ABBA completed with zero terminal pipeline
creations, no positive pipeline intervals, no new semantic configurations and
no new Dawn keys in all four cases. Descriptive means were CPU **-3.80%**,
elapsed **-2.58%** and process cycles **-3.12%**. The exact workload gate failed:
three cases submitted/planned 8,431,724 draws with 3,024 noops; the last full
control submitted/planned 8,431,716 with 3,008 noops. All four terminal clock
summaries matched. The timing receipt remains failed and does not establish a speedup.
No net-versus-previous-host or interface timing groups were run after this
failure. The interface candidate therefore has no native performance result.

Final whole-input rehashes verified all 417 native ISO/DOL/REL files unchanged
and all 417 personal tester inputs equal to them. The installation, original
player files, K7 shader sources, foreign edits and prior diagnostics remain
unchanged. The personal tester was prepared with copied saves and separate
mode caches; it was not automatically launched. Separately, the dependency
lock now includes the already-active 0165 patch and correct manifest hash.
All 23 synthetic Windows packaging checks and active SDK verification passed.

The post-upload repair clears bank repeat evidence if a pixel upload fails
after the vertex banks were staged. The negative regression produces 5,664
wrong bytes when that invalidation is bypassed and zero with the repair.
Staging also refreshes packet identity after a capacity-driven segment split.
Earlier CPU runner, fixture-cap and GPU fixture-layout failures are preserved;
the qualification documents distinguish those from production issues.

`patches/01-sparse-uniforms.patch` applies to the exact active SDK tree
`52ade47eb3ed8b0b1d2e2b369a9bfa9c13c451f1`, rooted at RecompCore base
`e280c788dadabd18b085af0085f1558fc9ff5ecc`. It reconstructs Git tree
`9d08e22fc8499fd4a58cfe759397e320227059b7`.
`patches/02-specialized-interfaces.patch` then reconstructs
`72a939651ebddce5d332bfee7e1edfac6de61044`.
An isolated Git index verified both against all ten final source files, with
only CRLF-to-LF normalization for Git blobs; materialized source byte pins
remain exact in the binding receipts. The live SDK and active manifest are
unchanged by this handoff.

`sources/uniforms/` contains the ten uniform files. The four files under
`sources/interfaces/` replace their matching uniform files for the second
candidate. `bindings/` pins both source overlays and patches. The retained
design documents and `construction.json` describe earlier preparation stages;
final qualification and manifest hashes supersede pending or initial-copy
labels. Historical private runners need their pinned directory/toolchain
layout; they are provenance, not portable public build entry points.

Run `python -B -S tools/verify_handoff.py` for read-only allowlist, source and
patch integrity checks. `tools/refresh_manifest.py` refreshes that fixed
allowlist after an intentional evidence edit. Neither command compiles,
installs, launches a game or accesses a network.

Human donor credit and source notices are retained in [CREDITS.md](CREDITS.md).
No executable, translated game module, player data, retail Killer7 shader text,
DXBC/DXIL bytecode or pipeline cache is included in this source bundle.
