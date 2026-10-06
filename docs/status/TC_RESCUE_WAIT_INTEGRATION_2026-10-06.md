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
The stopped verifier result is preserved; correcting that metadata check does
not establish a successful ownership fixture or full host build.

Next are the real-runtime ownership fixtures and complete host links, followed
by genuine OFF/ON rescue runs covering the six timer returns, original rewards,
cleanup, replay and save recovery. The initial provider covers headless sessions;
graphical-session ownership remains unqualified. The menu and ownership
candidates remain private, the public main is unchanged, and the existing tester
does not enable the skip.
