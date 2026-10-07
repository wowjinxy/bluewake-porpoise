# Known-single producer parity addendum

This inactive test-only patch follows `known-single-precision-tests.patch`. It preserves the original thirteen emitted programs and adds six SUM0, SUM1 and paired RSQRTE programs, including unknown high-precision carried lanes and destination/source aliases. Their outputs feed multiplier and FMA fact consumers.

Source review found no false fact in the questioned producers. Generated unary instructions narrow through 32-bit float bit conversion; both SUM paths narrow the carried C lane; paired RSQRTE narrows both estimates even after recording exception flags. Scalar FRSQRTE and paired RES remain unmarked. No candidate behavior or original qualification evidence changed.

Actual optimized and ASan fixtures each passed 6,543,360 checks across 1,308,672 full-CPU cases, including 143,360 callback cases and 615,936 interruptions. Both report digest `f38fc132e033221d`. The twenty hidden tool roles completed with preserved inputs. The result is `build/performance-13-fact-audit-20261006/actual-v1/result.json`, SHA-256 `ec518c89e7e94f9d09039a06a736dbef83044b615ea3e77b01c5c973db0ba300`. Existing qualified generator and CPU objects were reused only after their actual source/dependency identities matched.

The original proof's optimized assembly step overwrote its `reference.d` and `candidate.d` raw object dependency files. The earlier parsed object dependencies remain in that proof's `actual-v5/dependencies.json`; this addendum does not reconstruct the lost raw bytes. This addendum produces no assembly and retains fresh `reference.d`, `candidate.d` and `fixture.d` object dependencies in separate `o3` and `asan` directories. It does not use or claim separate `.obj.d` and `.s.d` names. Their exact identities are recorded in the accompanying JSON.

The exact generator opt-in remains `DOLRECOMP_KNOWN_SINGLE=1`, default off. These authored results establish the tested source contract, not complete game opcode coverage, native correctness or speed. Original negative attempts, including the subnormal host-flag issue, remain intact. A source-preflight command quoting failure was preserved before these tool roles.

The underlying donor is AceSpectre's `83398f1ccd93dc9f0e11ac626c0c11ae5b74ee5c`; this addendum supplies authored regression coverage.
