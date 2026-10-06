# Native background min/max — inactive item20 partial candidate

Elliott Tate's app donor `944a1f3c2b130a8086295428a0847938a777a2d7` identifies
GZLE01 `cBgW::MakeBlckMinMax` at `80247C4C`. The candidate adapts the exact
translated operation sequence into the existing BlueWake game-math gate.
Both patch files are inactive; no SDK, Builder, host or public composite source
has been changed or enabled. This is one of item20's two entrypoints.

Apply the implementation and test patches in an isolated checkout. Opt in only
by passing `--enable-bg-minmax` to native_game_math.py **and** compiling the
composite with `BLUEWAKE_NATIVE_BG_MINMAX=1`. Omitting either retains translation.
The current builder does not supply either option. The existing native game-math
v1 host contract and fresh entry query remain unchanged; a new module's admission
digest still requires normal qualification. There is no cached quiet-host answer.

The actual current certifier checks the exact translated fragment and every mod
variant, plus every internal watched instruction in canonical/mirror form.
Removing the option on a reused importer clears the optional entry even after a
failed prior preparation. All source preparation is transactional until every
required fragment has passed. The retained post-preparation chunk's other exact
native hooks are removed only in the authored source fixture to model its required
pre-optimization stage; that is not new production relaxation.

The candidate requires finite f32 values, ordinary aligned disjoint RAM, no
journal/overlapping aliases, a CPU object disjoint from RAM, FP enabled/guest
nearest, and enough turn/deadline budget for its maximum36 cycles. It uses the
actual integer widening, ordered comparison, single store and reservation helpers.
It preserves scratch registers, both FPR halves, FPSCR/CR, all stores, path cycles
and the final suffix1/2. NaNs/infinities, overlap, devices/mirrors, short budgets,
FP-unavailable and ineligible host state decline without CPU/RAM/FP changes.

Actual direct hidden Clang19.44 C11 qualification:21 roles passed, six real CPU
TUs + candidate + fixture compiled in both O3 and ASan. Each fixture ran32,000
cases:18,017 complete CPU/RAM translated matches,13,983 unchanged declines,
all64 independent store/no-store masks,776 short-budget cases and all4 host round
modes with preserved FP exception flags. Every fresh call queried readiness;
false readiness with an unreadable CPU pointer and disabled binding were checked.
Protected24MiB RAM has only the authored16KiB area writable; full outside RAM is
compared at completion. The personal original module `1cc3062a...` is invoked only
as a function oracle. It and system CRT are uninstrumented; six linked CPU TUs,
candidate and fixture are ASan instrumented. This is not routed-module/game/native
acceptance, nor a measured speedup. Existing CPU deprecation warnings are retained;
the fixture stderr is empty. Final optional-state cleanup adds no compiled C
changes and passed7 preparation tests separately.

Direct reproducibility: retained `build/performance-items19-24-audit-v1/run.py`
and actual1/commands.json record exact tools, SDK headers, objects, libraries,
MD targets, LLD reproductions, runtimes, environment and results. The exported
standalone CMake recipe is source-reviewed, not separately run. On Windows use
Clang with the corresponding MSVC/UCRT SDK, provision matching CRT/ASan DLLs
beside the authored fixture, then `ctest`. For source tests set
BLUEWAKE_GAME_MATH_ORACLE_CHUNK to the actual certified pre-optimization chunk.
Generated/private game translations and personal modules are not exported.

Preserved preparation negatives: missing sibling direct_calls import in the
private fixture, already-prepared unrelated hook placement at the wrong certifier
stage, PowerShell redirection treating successful unittest stderr as an error,
and a malformed one-line Python quoting attempt. These did not run or alter a
game and the final direct hidden qualification passed. No compiler/test failure
occurred in actual1.

The final export removes only self-attribution from comments. The original
compiled proof sources and old exports are preserved; exact byte inverses are
recorded in comment-only-delta.json. Arithmetic and guards are unchanged.

Remaining: MakeBlckBnd80247CD4 (loop bound, TransMinMax and PSVECAdd) remains
translated. Item21 now has a separate inactive quaternion candidate; items19,
22,23,24 remain under review. See native-leaf-19-24-audit.md.
