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
prefix is not diagnostic free. The original failed attempt and its artifacts
remain unchanged.

The separately reviewed continuation completed all 15 remaining optimized
compiles and both complete host links. Together with the four borrowed settings
objects, the actual dependencies contain 3,636 mentions across 580 distinct
inputs. The OFF link contains 64 ordered objects and 170 closed inputs; ON
contains 73 ordered objects and 179 closed inputs. Both executables preserve
the original embedded manifest and add no imported DLL families or symbols.
Independent file-only checks cover the actual objects, dependency files,
ordered reproduction archives and executable metadata. All borrowed inputs
remain unchanged. These are build and file checks; neither executable was run.

The new core compile retains deprecated ATOMIC_VAR_INIT warnings, and both
links retain the warning that -gcodeview is unused during linking. Those
diagnostics remain alongside the original stopped camera attempt and settings
warning. The recipe is not diagnostic free.

The read-only provider regression now passes 436 authored checks in each
optimized and AddressSanitizer build. Its 17 failure cases cover revocation
and failures inside the current handler, including stops before subsequent
CPU reads, register reads, event queries and output writes. The fixture uses
the real recorder and local file operations with synthetic game inputs;
after the tested failure it protects an authored CPU page to detect any
continued access. It does not establish native game ownership or gameplay.

Both independent file-only reviews accept all 12 actual roles: eight compiles,
two links and two fixture processes. The dependency files contain 592 mentions
across 206 distinct inputs. Each link uses four ordered objects and matching
adjacent runtimes; every actual diagnostic stream is empty. The first reader's
mistyped hash argument and the second reader's overly small object-section
limit remain preserved. The corrected file-only readers inspect the original
artifacts; no target was rerun.

The reference, feature-OFF and feature-ON diagnostic hosts now compile and link.
Four new C compiles and three complete links passed; their actual dependencies
contain 799 mentions across 255 distinct inputs. Each link retains 72 accepted
objects, replaces main and adds the shared recorder, producing 74 ordered
objects and 180 closed inputs. Both independent file-only reviews verified
the reproduction archives, original generated and embedded manifests, and
actual executable metadata.

Each host adds only the recorder's _open import. The actual main object,
SDK import member, current API-set mapping and named UCRT export agree on its
provider. No DLL family or other imported name was added. All four compiler
diagnostic streams are empty; each link retains the same 101-byte -gcodeview
warning. Neither a host executable nor a game module was run. Adjacent runtime
deployment and native behavior still need qualification.

Both source reviews now accept the four isolated baseline/reference/OFF/ON
case plans. They retain the original 73-segment input route, normal CARD loading,
8500-retrace limit and separate save copies. Original periodic logical-state
hashes are sampled evidence; reference/OFF raw checkpoint equality and ON's
same-return timer checks remain separate requirements. The unchanged baseline
cannot emit the recorder's raw boundary files.

The first static dependency-reader run stopped on a Windows API-set contract
alias. Its exact full-name lookup missed the schema's hashed contract prefix
and importer-specific mapping. The failed output is preserved, and an independent
file audit confirms all 2307 recorded inputs remain unchanged. Microsoft's
[loader documentation](https://learn.microsoft.com/en-us/windows/win32/apiindex/api-set-loader-operation)
describes schema redirection; the numerical lookup correction is being checked
against the current table and [Wine's loader source](https://github.com/wine-mirror/wine/blob/master/dlls/ntdll/loader.c).
This investigation does not establish actual Windows loader behavior. No game
case has launched, and runtime staging remains pending.

The next two static reads exposed separate Windows dependency details. COMDLG32
delay-imports the explicit `WINSPOOL.DRV` basename; the reader's DLL-only suffix
check rejected it. Independent byte inspection confirms that descriptor and all
nine requested named/ordinal exports in the current provider. Microsoft's
[GetPrinter requirements](https://learn.microsoft.com/en-us/windows/win32/printdocs/getprinter)
also identify Winspool.drv. The corrected reader then stopped on a different
COMDLG32 delay import, `QueryWin32SubsystemHost`, whose API-set entry has an empty
default provider. The failed receipts remain unchanged. File-only preservation
checks confirm all 2329 and 2352 respective recorded inputs are unchanged.

The empty mapping is retained without inventing a provider. Microsoft's
[delay-load documentation](https://learn.microsoft.com/en-us/cpp/build/reference/linker-support-for-delay-loaded-dlls?view=msvc-170)
describes loading when an imported function is called; an import descriptor
alone establishes neither that call's reachability nor successful recovery.
Both source reviews accept a prospective distinction between required ordinary
imports and explicitly unresolved, proven-empty direct delay imports. The earlier
failures are not promoted to passing runs.

Both independent file-only reviews now confirm the current schema's complete
891 entries and 897 values, including every raw UTF-16 field and numerical
contract hash. All 2276 recorded reader inputs are preserved. The schema has
113 empty provider values and 42 non-DLL values: three Winspool mappings and
39 kernel-module mappings. Those raw values do not admit kernel providers or
prove that an imported function is called. The failing subsystem-query entry
has one all-zero 20-byte default value; no fallback provider is present.

The next file-only read implementing that distinction stopped on missing
`hvsifiletrust.dll`. Both preservation reviews verify all 2376 recorded inputs
remain exact, and the failure is retained. Independent raw-byte checks identify
direct delay descriptors in the current Shell32 and Windows.Storage providers,
requesting three and two named functions respectively. Neither matching edge uses
an API-set mapping or preceding forwarder. Their call reachability and recovery
remain unproved. Microsoft's
[optional delay API documentation](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi2/nf-libloaderapi2-queryoptionaldelayloadedapi)
describes checking availability before a delayed call; it does not establish
that these particular Windows providers perform that check.

A separately source-reviewed facts collector stopped at its declared limit of
256 physical providers while inventorying ordinary and conditional Windows
dependencies. Its first failure is preserved. It produced no complete graph or
required ordinary membership; this is a collection-limit failure, with no
observed game or Windows loading failure. No runtime file was copied and no game
case launched.

Both independent preservation reviews confirm all 2376 original inputs and
2303 current interpreter/runtime files remain exact. Earlier failed reads and
the collector's first stopped result are retained; neither review admits a
complete dependency graph.

The next source proposal will discover required ordinary imports and their
forwarders first, preserving raw delay-import tables as separate conditional
facts. Uninspected conditional targets remain unproved. A new prospective replay
prerequisite draft requires zero unresolved required ordinary edges and two
independent actual file reviews before a separate bounded-experiment decision.
It grants no automatic staging or native authorization. Earlier scopes and
failed reads remain unchanged; runtime staging and game replay are pending.

Both source reviewers also accept the repaired hidden replay runner. It checks
Windows reparse paths before bounded directory traversal and preserves terminal
failure reports when output inventory is invalid. The four original command
vectors are byte-identical. This is source qualification; runtime guard behavior,
Windows loading and native game outcomes still await actual runs.

Next are genuine baseline/reference/OFF/ON rescue runs
covering the six timer returns, original rewards,
cleanup, replay and save recovery. The initial provider covers headless sessions;
graphical-session ownership remains unqualified. The menu and ownership
candidates remain private, the public main is unchanged, and the existing tester
does not enable the skip.
