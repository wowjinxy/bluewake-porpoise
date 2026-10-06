# Tingle rescue integration qualification

The timer core remains committed and independently qualified with 634 authored
checks in each optimized and AddressSanitizer build. Shipping host integration
and genuine rescue qualification are still pending.

The separate menu/settings candidate completed its 42 planned roles: 28 source
compiles, four links and ten fixture processes across optimized and sanitizer
builds. Two headless ImGui fixtures exercised search, preset preview/cancel,
section apply, persistence and reload. Eight separate startup processes exercised
setup, reload, successful Safe Mode backup/reset, and a blocked-backup failure.
The preference defaults to off. Controller profiles and unrelated settings were
preserved; failed backup left stored settings unchanged and disabled the option
for that session. These are real local file and authored UI checks, with no
native game or input backend.

The private ownership integration exposed a Windows portability defect in its
first enabled main compile: our compatibility layer implements joinable pthread
handles but provides neither pthread_self nor pthread_equal. The separate
repair uses GetCurrentThreadId on Windows and the normal pthread identity APIs
on POSIX. The fixture obtains copied thread-check results while its real worker
is alive, then consumes them after joining through release/acquire publication.
It preserves rejection before code queries or guest CPU reads, native thread
context and stack checks, and existing lifecycle/save guards. POSIX is source
reviewed here; it has not been compiled or run in this Windows cohort.

All four main compile cases now pass: optimized OFF/ON and sanitizer OFF/ON.
These objects were compiled with the real host definitions and CPU layout; they
were never linked or executed. The original missing-function compile failure
is preserved separately.

The subsequent optimized configure command returned success, but its verifier
stopped because it required compiler paths in CMakeCache.txt. This toolchain
sets ordinary compiler variables instead. The actual generated C/C++ compiler
records and FileAPI toolchains record the expected copied Clang 19.1.5 paths.
Independent file-only reviews now accept that completed configuration. The
replacement check requires generated compiler records and FileAPI to agree on
the pinned paths, compiler IDs, version, frontend, Windows x64 ABI and empty
implicit search inputs; a conflicting cache entry still fails. It also restores
the original frozen copied-source records and one already approved Windows shell
pin that the continuation had omitted. Its three changes have an exact inverse
to the stopped verifier, with no game source or qualification command change.

The actual configuration contains exactly 34 runtime sources and six fixture
sources, 192 pinned consumed CMake inputs, six owned generated inputs and 69
reviewed tool-path mentions. The four main compile cases close 800 dependency
mentions across 254 distinct inputs; the enabled objects use the Windows thread
provider and disabled objects reference no cutscene-wait API. All 58 existing
generated and log files remain unchanged. The original compiler and verifier
failures stay preserved. Those checks accepted only the completed compile and
configure prefix; the remaining five roles used a separately reviewed output
folder.

Both real-runtime ownership fixtures now pass: 2,396 authored checks and 282
dispatches each in optimized and AddressSanitizer builds. The fixtures exercise
the actual runtime and event dispatcher with synthetic owner, code and CARD
inputs, including a real worker thread; they do not establish native game
ownership. Independent file-only reviews verified all 80 compiled objects and
their dependencies, both 34-member runtime archives, the ordered link inputs,
embedded manifests and matching adjacent runtimes. The sanitizer fixture
reported no errors.

The optimized build returned success but emitted a retained 406-byte CMake
cache-location diagnostic: a lowercase drive spelling left the historical
creation-directory marker unchanged during the reviewed path copy. All actual
compile and link outputs used the new owned folder, no optimized reconfigure
occurred, and all 37 original, staged and installed cache files kept their
hashes and modification timestamps. This accepts the fixture evidence with
that recipe diagnostic; it does not qualify the copied recipe as diagnostic
free. No target was rerun or artifact corrected.

The complete host build accepted a separate source graph with 64 ordered objects
for OFF and 73 for ON. The new Settings layout is consumed by exactly three
existing objects; all three are rebuilt, along with the settings definitions.
Independent reviews checked their actual dependency files and x64 objects after
these first four compiles passed.

The next compile, mouse_camera.c, stopped at the copied Clang inttypes.h check
for an unsupported MSVC compatibility version. Its 1,038-byte diagnostic and
failed dependency file remain preserved; no camera object or later host link
was produced. A separate continuation restores only the camera's
-fms-compatibility-version=19.44 flag, matching the passing conducting fixture's
same source and compiler. It borrows the four accepted objects and requires
its own review and authorization before compiling the remaining sources.

The settings compile also retained a 975-byte nonfatal format-security warning
for a fixed availability-name string passed to ImGui::TextWrapped. The accepted
prefix is not diagnostic free. No failed compile was relabeled or rerun.

Next are complete host links and genuine OFF/ON rescue runs covering the six
timer returns, original rewards,
cleanup, replay and save recovery. The initial provider covers headless sessions;
graphical-session ownership remains unqualified. The menu and ownership
candidates remain private, the public main is unchanged, and the existing tester
does not enable the skip.
