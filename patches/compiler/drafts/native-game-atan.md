# Exact game table atan replay - inactive item22 candidate

Elliott Tate (elliotttate), app944a1f3c2b130a8086295428a0847938a777a2d7,
identifies cM_atan2s802460D0 and U_GetAtanTable8024609C. This incremental
patch applies after native-bounds-minmax.patch and native-quaternion.patch.
All three remain separately optional. Opt in with both --enable-game-atan
preparation and the compile definition BLUEWAKE_NATIVE_GAME_ATAN=1. No live SDK, host, Builder or
composite source has been changed by this export.

The actual current certifier requires both complete canonical translated
fragments, every base/mod variant, and absence of every main/helper/interior
watched instruction or mirror. The existing native_game_math_v1 callback is
fresh on each entry and runs before CPU dereference. No cached quiet-host
answer or new host permission is introduced. Both selected functions share
one translated chunk; the original call/internal return uses local labels.

The replay reads the actual mutable ushort lookup table and guest SDA
constants every call. It preserves signed-zero and all eight octants, nested
stack backchains, saved LR and every call-return LR, conversion stfd bytes,
scratch GPR/FPR halves, CROR, subfic carry, reservations, path-specific cycles
and final suffix2. It uses the current accurate FP helpers and actual fctiwz,
not a host atan/libm approximation. Whole96-cycle room is conservative.

Eligibility requires finite exact-f32 doubles or signed zero, bounded normal
magnitudes, FP enabled/guest nearest, ordinary aligned RAM without overlapping
aliases or journals, CPU storage physically disjoint from RAM, and stack
disjoint from SDA and the complete2052-byte table. The fresh scale must be
exact1024, zero exact+0, and epsilon a bounded positive normal f32. The octant
ratio is in[0,1], making every converted table index bounded0..1024. Integer
classification with an explicit opacity barrier avoids DAZ-sensitive folding.
Every unsupported case declines before FP or guest mutation; no speculative
rollback or persistent table result exists.

Actual qualification ran21 hidden direct roles, including8 preparation tests
and O3/ASan protected-RAM function oracles. Each mode ran64,000 cases with
8 octants x1025 ratio inputs x4 host round modes, random table/register/NI/
FPSCR/reservation state, signed zeros/epsilon neighbors, and guarded negative
budgets/deadlines/FP/exception/journal/alias/mirror/MMIO/overlap/constants and
NaN/Inf/subnormal/non-single inputs. Full CPU bytes, written RAM and host FP
flags/rounding match the original translated function; decline preserves them.
The24MiB images are read-only outside authored12KiB stack/SDA pages; the table
is read-only. Whole RAM images match at completion. Inaccessible CPU denied
entry and physical CPU/RAM overlap are explicit negatives.

The six actual CPU translation units, candidate and authored fixture were
freshly ASan instrumented. The personal original module1cc3062a and system CRT
remain uninstrumented. No native gameplay, GPU/device, performance or new
module-code-admission claim follows. All actual role logs, commands, MDs,
LLD reproduce inputs and source/runtime preservation pins remain private.

Portable test recipe: apply the inactive patches in an isolated checkout,
configure tests/native_game_atan with explicit BLUEWAKE_ROOT and personal
BLUEWAKE_ORIGINAL_MODULE, matching Windows Clang19.44 SDK/CRT, and optional
BLUEWAKE_ORACLE_ASAN. BLUEWAKE_GAME_ATAN_ORACLE_CHUNK can select the personal
unchanged source for the standalone preparation tests. These exported path
adaptations and CMake recipe are source-reviewed; actual proof uses the direct
commands in the private actual1. Personal generated code/modules are not
distributed. Root owns integration and any future native qualification.

Builder integration addendum, October 6, 2026: the qualified replay and preparer
are now in the normal source behind the default-off `--native-game-atan` selection,
requiring `--native-game-math`. Preparation, CMake certificate admission,
cache/training identity and provenance use the same selection. It shares the
existing host `BLUEWAKE_NATIVE_GAME_MATH` handshake; no new per-leaf host
environment setting is introduced. Default optimized assembly is byte-identical
to the earlier source; COFF products differ only in their timestamp. Live-source
atan O3/ASan each pass 64,000 full-state cases and eight certification tests.
These source/build proofs do not establish a complete module or gameplay gain.
