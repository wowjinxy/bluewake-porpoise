# Contextual subtraction/multiply draft

Experimental handwritten helpers, qualified October 9, 2026. Both versions
passed native full-state fixtures. Version 1 also has an exact-flags caller
compile and a local timing test. These drafts are not wired into the normal
build, have not changed the installed game, and have no established game
performance gain.

The private candidate wraps six adjacent `fsubs -> fmuls` pairs in an existing
paid fast copy. Original precise labels, instruction statements, availability
checks, charges and fallback paths remain intact. Translated caller bodies are
not included here.

Both helpers reject unavailable FP or nonfinite first operands before modifying
state. A finite first stage retains the original rounding and publishes FPR/PS1
before reading aliased second operands in version 1. The success path defers the first
FPRF classification because the second instruction replaces it. Before an
exceptional second fallback, it classifies the saved rounded f32, preserving
DAZ-sensitive subnormal classification and canonical VE handling. Only when the
second multiplier is the first destination can its fresh widened f32 bypass the
generic force-25bit conversion. Both guest arithmetic operations and roundings
remain separate.

Version 2 resolves second-source aliases from the saved first value. It delays
the first FPR/PS1 writes when the successful multiply replaces the same
destination. A different first destination is still published, and every
canonical fallback receives the complete original first result and class.

Use requires the exact production gather-pipe, generated, inline-GPR and
inline-FP header order. No callback, observation, suffix refund, deadline test,
branch, precise label or other instruction may separate a pair. The finite
first stage relies on the existing cooperative CPU ownership contract; it does
not support asynchronous writers to CPU state.

Each version passed four profiles: generic/fixed CPU, each with optimized and
ASan builds. Each profile compared 62,183 complete 3,552-byte CPU states plus
host fenv, rounding and MXCSR, using the actual production header order and real
runtime providers. The corpus includes all 60 source register patterns,
exceptional operands, rounding/FTZ/DAZ, NI/VE, unavailable FP, precise deadline
exits, and 240 actual paid-path lazy-off/MSR-clear comparisons. RAM coverage was
807 individual 32 MiB queries and 61,376 queries in groups of at most 64. Four
deliberately broken variants were rejected. An initial alias-journal expectation
was corrected to the canonical address-minus-RAM-base contract; its failed run
is preserved.

Version 1's production caller compile preserves its 184-byte main frame and
undefined-symbol set. The natural helper remains outlined; a representative
finite path drops from 122 to 71 static instructions. Forced inlining still
outlines the final result writer and retains the intermediate writes; it is not
an established improvement over natural inlining.

Version 2 also passes the production-policy caller compile with the same frame,
imports and 49 actual dependencies. The representative finite path is 69
instructions, and its intermediate first FPR/PS1 stores are fallback-only.

The local timing test passed 384 state endpoints and 192 balanced batches
(192 million pairs). Local mean cycle and wall costs favored both variants,
with mixed individual comparisons. The standalone original subtraction helper
was compiler-specialized and failed exact production machine-code equality;
the multiply helper matched. These measurements cannot establish production
caller cost or game speed. A real module/game comparison remains required.

The fresh intro CPU-state capture gives these six sites insufficient observed
budget: three of 3,582 sustained-window samples publish a selected second PC,
including two named multiply-helper samples. All 60 static subtraction/multiply
pairs together match four samples. None match either fresh worst-30-VI window.
Assembly verifies that the original six sites publish the second PC before the
helper on the successful path. These counts are diagnostic associations rather
than an exact timing bound. The six-site module recipe is preserved unexecuted;
this performance branch is closed pending materially different evidence.

Private source receipt: `fp-context-assessment1/source2/source-preparation1.json`,
SHA256 `c20eb8ec4dc0cb29a31602b46a3f1fc70fea19ffe0742a87ca619f101602768a`.
Version 2 source receipt: `fp-context-assessment1/source4-delayed-publication/source-preparation1.json`,
SHA256 `7aac069efda004590c56151bc53eb3ae4959d6bf53c1805d5147b5848b584abb`.
Both helpers' exact bytes and native result hashes are pinned in
`source-manifest.json`.
