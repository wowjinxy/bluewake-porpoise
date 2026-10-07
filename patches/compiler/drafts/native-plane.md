# Exact plane calculation composition — inactive item23

Elliott Tate (elliotttate), app944a1f3c2b130a8086295428a0847938a777a2d7,
identifies cM3d_CalcPla8024A6F0 and its SDK vector dependencies. This patch
applies after the inactive MinMax, quaternion and game-atan patches. Select
both --enable-plane preparation and compile definition BLUEWAKE_NATIVE_PLANE=1.
The existing BLUEWAKE_NATIVE_GAME_MATH host request/versioned handshake remains
required. No live host, SDK, Builder or composite has been changed here.

The composition uses unchanged accurate native_vec.c implementations of
PSVECSubtract, PSVECCrossProduct, PSVECMag, PSVECScale and PSVECDotProduct.
Every actual save/restore/vector entry and return retains a fresh read-only
native_game_math_v1 query. The provider sees the real CPU, current PC/LR,
downcount/suffix and the guest RAM already written at that original boundary.
No quiet answer is cached. Alias generation is checked after each query.
Sixteen queries remain on a normalized plane; twelve on a degenerate plane.
The host can refuse any one and retain the original translated path.

The shared certifier requires complete main, vector and inline save/restore
source fragments in every base/mod variant. All skipped interior instructions
and mirrors must remain unwatched. Only the two exact inline save/restore entry
PCs use dynamic rather than static permission; their original entry/return
queries are preserved. Preparation is default-off and removes stale optional
hooks when disabled or certification fails.

Preflight requires bounded finite inputs, FP enabled/guest-nearest, paired
single configuration, positive whole160-cycle room, ordinary aligned RAM,
no journal/overlapping aliases, CPU physically outside RAM, and disjoint
input/output/frame/constants. Guest SDA constants are read fresh and checked.
The optional path requires masked host SSE exceptions before speculation.
CPU, frame56 bytes, normal12 bytes, distance4 bytes and fenv/MXCSR are saved.
A later leaf/provider refusal restores them before the untouched translation
runs. Exact original operation/store order, every scratch GPR/FPR half, CR,
FPSCR, reservations, branch cycles and final suffix2 are retained. Nested raw
vector diagnostic counters count attempted leaves, including rolled-back work.

Actual optimized and ASan qualification passed eight preparation tests and
40,000 authored protected-RAM cases per mode:26,582 complete translated
matches,13,418 unchanged declines,20,796 normalized/5,786 degenerate results.
Each mode also compared complete CPU plus authored RAM at402,168 original
entry/return callbacks through the personal module's genuine direct_calls_v2
mechanism. Four host rounding modes, NI/FPSCR/reservations, invalid memory,
overlaps, journal/aliases, budgets, FP and late refusals are covered.
A focused six-role continuation reused exact qualified code objects and
asserted all16 boundaries×3 refusal/journal/alias-generation rollbacks,
physical CPU/RAM overlap and unmasked-host declines, plus five original
controls/76 boundary comparisons. Host FP flags and complete MXCSR match;
the protected24MiB images match at completion. No independent x87-register
stack/status equivalence claim is made beyond the used fenv API checks.

Preserved failures are preparation evidence only: V1 used a post-native SDK
chunk with pre-native certificates; V2 source preparation initially counted
nine entry hooks but omitted three copied fast-path hook blocks; V3 supplied
a mirror-only watch set rather than watched_addresses()'s paired forms.
V4 uses the exact known twelve-block fixture normalization and actual watch
set contract. Production C stayed unchanged throughout those corrections.
Unknown/modified hooks or source bodies are not normalized by production.

Six actual CPU translation units, both native sources and authored fixtures
were ASan-instrumented. The personal original module and system CRT remain
uninstrumented. No gameplay/GPU/device/timing/speedup/default-enable claim
follows. CPU deprecation diagnostics are retained; fixture/ASan logs are clean.

The portable CMake recipe requires an explicit isolated checkout, matching
Windows Clang19.44 SDK/CRT and a personal unchanged module. The optional source
test can use BLUEWAKE_PLANE_ORACLE_CHUNKS to select the retained personal
prepared chunks. Portable path adaptations are source-reviewed; actual proof
uses private recorded direct commands. No personal generated code/module is
distributed. Root owns integration and any future native qualification.
