Finite observer source regression.

Repository paths are the relative paths under this directory. The fragment is
included by runtime/host's BUILD_TESTING scope and Windows' explicit
BLUEWAKE_WINDOWS_REGRESSION_TESTS scope. It adds no shipping host sources.
The entrypoint is target/CTest bluewake_finite_observer_filter_test; enable
BLUEWAKE_FINITE_OBSERVER_ASAN in a separate build for the sanitizer mode.

Exactly nine C TUs are compiled: eight wrappers include the complete current
production C files, and oracle.c supplies the authored comparison. No gxruntime
library, main executable, guest assets, renderer, devices, input or game is linked
or started. Unrelated FPU/event/alias helpers abort if an observer reaches them.

Wrapper inventories reference actual production macros/static constants and the
actual health damage-return array. Changed existing non-DOL return constants are
therefore recompiled and tested, including BLUEWAKE_AUTOSAVE_RETURN and
BLUEWAKE_ENHANCEMENT_WIND_RETURN. All aligned DOL PCs and their mirror remain
exhausted; a newly added non-DOL predicate PC also requires updating its wrapper
inventory. No automatic proof for arbitrary new predicate constructs is claimed.
The actual main chain must exactly match the reviewed chain at configure time;
otherwise preparation fails instead of silently testing a historical main body.

Private predecessor actual3 passed 13,320,192 checks in each O3 and ASan mode.
That evidence does not qualify this adaptation. Build and run fresh O3 and ASan
instances through the fragment's CMake target. Both modes must complete the same
count and retain CPU/RAM and call-order assertions, including outside-DOL returns.
A minimal C-only project can include CTest and this repository fragment, avoiding
the SDK build. Windows uses /OPT:REF, Apple uses -dead_strip, and Linux uses
--gc-sections to discard unrelated unexercised production functions.

Windows Clang 19.1.5 qualification on 2026-10-07 exercised that minimal CMake
entrypoint, its nine-TU target, and CTest in fresh O3 and ASan directories. Both
passed 13,320,192 checks with 9,435,217 misses. The actual Ninja dependency records
include all eight production C sources. Two earlier ASan CRT link failures are
preserved; the fragment explicitly selects the dynamic CRT and DLL ASan runtime
for Windows Clang. Apple/Linux linker branches have source review only here.
The result is build/cpu-hotspots-20261007/observer-filter1/cmake-retest/attempt3/result.json.
