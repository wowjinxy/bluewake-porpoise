# Contextual subtraction/multiply draft

Experimental handwritten helper, preserved October 9, 2026. This draft is not
wired into the normal build. Source review passed; compilation, native semantic
fixtures, caller code generation and runtime measurements are still pending.
It has not changed the installed game and has no established performance gain.

The private candidate wraps six adjacent `fsubs -> fmuls` pairs in an existing
paid fast copy. Original precise labels, instruction statements, availability
checks, charges and fallback paths remain intact. Translated caller bodies are
not included here.

The helper rejects unavailable FP or nonfinite first operands before modifying
state. A finite first stage retains the original rounding and publishes FPR/PS1
before reading aliased second operands. The common success path defers the first
FPRF classification because the second instruction replaces it. Before an
exceptional second fallback, it classifies the saved rounded f32, preserving
DAZ-sensitive subnormal classification and canonical VE handling. Only when the
second multiplier is the first destination can its fresh widened f32 bypass the
generic force-25bit conversion. Both guest arithmetic operations and roundings
remain separate.

Use requires the exact production gather-pipe, generated, inline-GPR and
inline-FP header order. No callback, observation, suffix refund, deadline test,
branch, precise label or other instruction may separate a pair. The finite
first stage relies on the existing cooperative CPU ownership contract; it does
not support asynchronous writers to CPU state.

Pending qualification compares full CPU state, RAM, aliases, callbacks, host
fenv and MXCSR against the exact production header closure. The prepared corpus
covers actual register patterns, exceptional operands, rounding/FTZ/DAZ modes,
NI/VE, unavailable FP and precise deadline exits, with deliberately broken
mutants. The original-flags one-caller compile must also establish actual
inlining, branches, calls, frame cost and imports before any module experiment.
A local or game benchmark is a separate required step.

The prepared paid-path comparisons currently enable lazy FP. The lazy-off,
MSR-clear cases exercise unchanged precise code; a paid-path case for that
combination remains a source-review follow-up before qualification is complete.

Private source receipt: `fp-context-assessment1/source2/source-preparation1.json`,
SHA256 `c20eb8ec4dc0cb29a31602b46a3f1fc70fea19ffe0742a87ca619f101602768a`.
The helper's exact bytes are pinned in `source-manifest.json`.
