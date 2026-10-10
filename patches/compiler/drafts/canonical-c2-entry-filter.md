# Optional canonical initial-entry subset

Apply after `canonical-c2-fixed-mem1-and-return-relay.patch`. This generic
compiler delta is inactive in game builds and defaults to the existing entry
behavior. Its SHA-256 is
`e30baa9013838ab05cc272ed46d124bf52d8f1d6ffb271734fbffbd5a6d5008d`.

`canonical_c2_filter_initial_entries` limits the external entry switch to the
existing `entry_points` list. The shared collector/emitter admission requires
a nonempty, unique, aligned subset of actual non-fallback instruction blocks
in that function. Invalid subsets fail admission. Zero initialization retains
all canonical instruction entries. Internal instruction regions, scheduling
rows, local branches and call continuations retain their existing behavior.
Backend and driver must be rebuilt consistently after the options change.

A paired outer adapter must route only that subset to LLVM and keep every
other resume in the original C2 code. The private prototype selects only the
normal function entry; its five old central continuations use the unchanged
original dispatcher. An entry miss is not permission to skip guest work or
retry an instruction after partial execution.

Source inverses and the selection model pass. All 49 generator translation
units now compile, and actual COFF inspection admits the F4 entry, memory
guards, imports and Win64 unwind data. The private body falls from 57,536 to
30,569 bytes and optimized IR PHIs from 4,401 to 1,130. These are static counts.

The filtered successor has its own actual game qualification: six complete
logical guest-state checkpoint hashes and the full captured P6 frame match
the original C2 module on the controlled intro route. A separate sampled run
matches again and observes 49 native RIP hits at 48 addresses in the exact new
body. This proves execution, not complete path coverage or speed.

The fresh fixed-order eight-run controlled comparison fails its performance
gate. Mean dispatch CPU time is +3.320%, wall time +3.253%; paired signs are
mixed. All eight runs perform identical measured and terminal work. Background
load varies, so this is not proof of a causal regression; it establishes no
repeatable gain. Every run, including the slow final candidate, is retained.
The patch stays inactive and is not a fix for the ordinary 19 FPS observation.
Detailed results and private evidence hashes are in
`docs/research/DEEP_DEBUG_2026-10-08.md`.

Canonical generic cache fingerprints remain unqualified; the private driver
rejects both object caches and PGO controls.

Private source commit: `97102d7`. The patch contains only five generic compiler
files; game instructions, adapters and compiled modules remain local.
