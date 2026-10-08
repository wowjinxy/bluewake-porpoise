# CPU qualification

These are authored CPU/source-contract checks. They do not establish GPU
pixel parity, ordinary gameplay parity or a frame-time improvement.

The repaired uniform source passed fresh optimized and AddressSanitizer
builds, each with **16,242,119 checks and zero failures**. The fixture uses
the actual three-bank staging helper, both private-byte and staged-byte
comparison, growth/shrinkage, omitted fields, full control, packet changes,
overflow/retry and canonical interpolation identity across off/on transitions.
It compares current specialized and uber shader math with an independently
namespaced current-source control, and verifies declarations before text
normalization. Default generator calls equal explicit full-control calls.
Separate processes test the actual extracted runtime opt-in body with unset,
empty, `0`, `11` and `1`; only exact `1` enables sparse uniforms.

The authored 1000-draw workload copied 2,832,000 full-control bytes and
160,000 sparse bytes; aligned staging advanced 3,070,560 and 255,904 bytes.
This workload is an upload mechanism fixture, not a measured game workload.

The interface adaptation passed fresh optimized and AddressSanitizer builds,
each with **4,112 cases, 218,546 checks and zero failures**. Its 4096 admitted
key shapes use the current limits of eight TEV stages, five texgens and four
indirect stages. Sixteen explicit cases cover raster channels, disabled and
fallback direct/indirect coordinates, Color1 texgen dependencies, forward
emboss references and stable output locations. Both uniform modes compare
interface-off/default WGSL byte for byte with the independent exact uniform
source2 generator. Vertex calculations/order match after removing only added
zero-initialized locals and final exports; fragment math matches after
removing only input prelude declarations. These text checks do not execute WGSL.

The post-VS failure regression deliberately omits invalidation after B's
successful bank stage, retains A's canonical identity, then submits repeated A.
The negative exits 1 with **5664 wrong bytes** across both comparison modes.
Calling the actual repair helper exits 0 with **zero wrong bytes**. This
demonstrates that the regression detects the source1 cache hazard.

The earlier source1 CPU attempt is retained privately. Its authored key shape
incorrectly requested up to 16 TEV stages against the current eight-entry
array and exited with an access violation. The repaired fixture uses the
actual `kMaxTevStages`; production limits were not expanded. A prior runner
attempt also retains its redundant `-nostdinc++`/`-Werror` preprocessing failure.

All successful builds pin actual `-M`/`-MD` equality, source dependencies,
compiler/runtime files and linker reproduction members. Private exact receipts:

| Evidence | Private path relative to repository | SHA-256 |
| --- | --- | --- |
| Uniform O3/ASan | `build/k7-uniforms-20261008/source2/cpu-attempt1/result.json` | `a3667f050c05a612ed63a7723a9113fd535698ba9fa8a2d28913c4796f5f6603` |
| Interface O3/ASan | `build/k7-interfaces-20261008/cpu1/attempt1/result.json` | `00a503c54c20c8ed0183cb98140d5aa6562be360415227c6de1804713e30e257` |
| Post-VS negative/repair | `build/k7-uniforms-20261008/negative1/attempt1/result.json` | `6b7c119e63c7236e563a223fa5d5c1ecadb406709fa016377beb36eec57a56f0` |

The extra CPU fixture/control sources are copied byte for byte. Retained
runners preserve provenance and depend on the original private toolchain,
hidden-process helper and directory layout; they are not standalone public
build scripts. No binaries, outputs, linker reproductions, retail K7 code or
game data are included here. Human donor credit remains in `CREDITS.md`.
