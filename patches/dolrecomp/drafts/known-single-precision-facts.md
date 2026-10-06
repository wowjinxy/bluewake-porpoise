# Inactive item 13: known single-precision facts

This draft adapts the idea in AceSpectre's `83398f1ccd93dc9f0e11ac626c0c11ae5b74ee5c` to the current C emitter and accurate composite FP helpers. Human attribution is retained. It does not import the donor's function scheduler or change the CPU ABI.

Only the exact generator environment value `DOLRECOMP_KNOWN_SINGLE=1` enables it. Empty, `0`, `11`, and `true` produce C and header bytes identical to the baseline. Every computed-goto entry starts with unknown facts. CFG joins, counted-loop boundaries, native hooks, option alternatives, callbacks, and unknown instructions invalidate facts. PS reciprocal estimates are deliberately excluded from guaranteed single-precision results.

The two source patches are separate: `known-single-precision-facts.patch` applies to the compiler; `known-single-precision-composite.patch` applies to BlueWake's composite helper. The authored fixture patch adds the actual emitter driver and complete CPU-state comparison under `tests/known_single/`. These remain inactive drafts; live compiler, builder, and manifests are unchanged.

Actual O3 and ASan runs each passed 5,990,400 checks over 1,198,080 cases, including 143,360 callback cases and 591,360 interrupted cases. They compare all 3,552 CPU bytes, RAM, callback/hook counts, guest FPSCR/exception state, host FP exception flags, and MXCSR sticky flags. Inputs include rounding modes, NI, FTZ/DAZ, special values, every interior entry, native option hooks, callback register mutation, and observation deadlines. Digest: `dcaafc8ed92d23e5`.

The initial optimized run found identical guest results but a changed host denormal flag. The single `fmaf` path now requires known normal-or-zero operands, nearest rounding, NI off, and host DAZ/FTZ off. The original failure remains in `build/performance-13-known-single-20261006/actual-v4/`. The successful combined receipt is `actual-v6/result.json`, SHA-256 `3ae0c1e7cc5f936f8bbc88e1d0a1c4551709f8a5c5d3580daaba00e51b09b656`. Earlier recipe and archive-audit failures also remain preserved; the successful ASan link was qualified and reused without relinking.

Optimized assembly contains a new scalar single-precision FMA path, so this changes machine code. The candidate object grows from 56,389 to 57,212 bytes and adds fact and FP-mode checks. No speed benefit is established. Exhaustive destination/source aliases, the complete opcode corpus, native rendering, gameplay, and timing remain unqualified. Keep the draft disabled pending those checks and workload measurements.
