The C17 fixture compiles the actual production chassis predicate and edge table
against authored host globals and the real CPUState definition. Its reference
retains the prior coarse table-hit guard. The permitted difference is limited
to quiet canonical 0x80328F84 with LR other than 0x80246A04. The mirror with the
GroundCross LR remains conservative.

The preparation script compares the complete actual predicate with the reviewed
one, extracts the actual function for compilation, and rejects changes to the
full edge service, census wrapper, canonicalization, and GroundCross edge and
per-turn guards. A future production change needs a reviewed oracle update.

Checks cover every actual edge-table key and mirror, null CPU, helper contexts,
dynamic module aliases, census/service/dirty flags, overlap identity/pointers and
phase, and EE/decrementer/PI-mask combinations. The fixture checks CPU ownership
and compares the entire authored 32 MiB RAM image after all probes. It exercises
no game, device, renderer, physical input, or native module.

Include `cmake/host_chassis_precision_test.cmake` in the existing host
BUILD_TESTING and Windows regression scopes. The target is
`bluewake_host_chassis_precision_test`; set `BLUEWAKE_HOST_PRECISION_ASAN=ON`
for the ASan mode. A small C-only CMake project may set BUILD_TESTING and include
the same fragment without building the SDK or shipping host. GNU-style Clang or
GCC is required. Windows uses the dynamic CRT; Apple uses dead_strip and Linux
uses gc-sections.

The actual minimal CMake entrypoint passed O3 and ASan on Windows Clang 19.1.5:
50,329 checks and 7,199 permitted context refinements in each mode, with eight
hidden configure/build/CTest/dependency roles and preserved source inputs.
The receipt is build/cpu-hotspots-20261007/host-precision1/cmake-retest/attempt1/result.json
(SHA256 159f72eacdcd11848747d85dc28cffe5c77ac90ab7ee4f089d4e8d62f5fb45d4). Apple/Linux branches are source reviewed; they
were not executed here. This establishes predicate behavior, not native Maya
arithmetic, routed parity, or a speed gain.
