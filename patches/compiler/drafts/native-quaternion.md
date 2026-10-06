# JMA quaternion replay — inactive item21 candidate

Elliott Tate app944a1f3c identifies JMAEulerToQuat80301150. The candidate adapts
the actual50-instruction translated function to the existing game-math v1 gate.
This incremental patch applies **after native-bounds-minmax.patch**; item20 remains
separately optional. No live composite/SDK/host/Builder source has been changed.
Opt in with both `--enable-quaternion` preparation and compiler definition
`BLUEWAKE_NATIVE_QUATERNION=1`. Omitting either keeps the original translation.
The unchanged existing v1 callback runs freshly on every call. Interior watched
instructions or changed/mod-variant body certificates prevent hook insertion;
no donor quiet-host cache is imported. New module code admission remains required.

The implementation loads the actual guest SDA shift and sine/cosine pointers,
reproduces signed16 half-angle rounding towardzero/u16 wrapping/shifts masked63,
all final GPRs/CA, scratch FPR halves,14 fmuls and4 sums with current accurate
inline_fp helpers, reservations,50 cycles and final suffix1. It does not compute
host trigonometry or assume canonical table contents. All six selected table
words are checked anew; output must be disjoint from them and SDA. Bounded finite
values/FP enabled/guest nearest, ordinary aligned RAM/no overlapping aliases or
journal, CPU/RAM physical disjointness and sufficient whole-function budgets are
required. Unsupported values/states decline before any mutation. No cached table
or host authority persists between calls.

Actual qualification:21 hidden direct roles passed. Each O3 and ASan fixture
ran240,000 cases, with every signed16 angle on each of3 axes (196,608 exhaustive
accepted cases), all64 variable shift counts, authored table contents, all4 host
round modes, random NI/FPSCR/register/reservation state, and complete CPU bytes
plus writable RAM compared to the original translated function. Exact accepted
and unchanged-decline counts are in summary.json and fixture stdout. Every host
FP exception flag and round mode matches the original; decline preserves them.
The24MiB RAM images are protected except authored SDA/output pages; tables and
all unrelated RAM stay read-only, with full images compared at completion.

Seven actual preparation tests cover changed fragment/mod variant, every watched
instruction and mirror, repeated hook preparation, optional off state, removed
certification, and option removal after failed preparation. CPU6 TUs, candidate
and fixture were freshly ASan instrumented. Personal original module1cc3062a and
system CRT remain uninstrumented. No routed composite/native gameplay, GPU/device
or performance claim is made. Fixture stderr is empty; existing CPU deprecated
atomic-init warnings are retained.

Preserved first attempt: the authored negative set output to the read-only sine
table but did not ensure the selected table word overlapped output, allowing a
legitimate candidate store to that protected page. Fresh attempt2 changes only
that negative to shift31/all offsets0, giving actual overlap and conservative
decline. The first access violation and all files remain intact. No production
candidate arithmetic changed in response. An inherited item20 result-label string
in the stopped attempt was also corrected in the fresh recipe, not its old result.

Portable recipe: apply both patches in an isolated checkout, configure
tests/native_quaternion with explicit BLUEWAKE_ROOT/personal ORIGINAL_MODULE,
matching Windows Clang19.44 SDK/CRT and optional BLUEWAKE_ORACLE_ASAN. The standalone
CMake recipe/exported-path source-test adaptations were source-reviewed, not run;
actual commands, MDs, inputs, runtimes and LLD tar live under the private actual2.
Generated game translations/personal modules are never exported. Default options
stay off; root owns integration and any native qualification.

The final export removes only self-attribution from source comments. Original
compiled proof sources and exports are preserved, with exact comment-only byte
inverses in the private item20 comment-only-delta.json. No arithmetic, guard or
fixture behavior changed, and no compiler or fixture rerun was required.

Builder integration addendum, October 6, 2026: the qualified replay and preparer
are now in the normal source behind the default-off `--native-quaternion` selection,
requiring `--native-game-math`. Preparation, CMake certificate admission,
cache/training identity and provenance use the same selection. It shares the
existing host `BLUEWAKE_NATIVE_GAME_MATH` handshake; no new per-leaf host
environment setting is introduced. Default optimized assembly is byte-identical
to the earlier source; COFF products differ only in their timestamp. Live-source
atan O3/ASan each pass 64,000 full-state cases and eight certification tests.
These source/build proofs do not establish a complete module or gameplay gain.
