# Windows PC bring-up

Status recorded on 2026-10-15. The native GameCube Windows port boots to the title, and the build with three selected upstream fixes completes the real new-game flow through the prologue to controllable Outset. An Outset introduction window measured 29.97603 FPS; the repeated warm title window measured 29.99977 FPS. Full-game stability, broader performance, manual controller acceptance and audible quality remain open. HD features are deferred until the native PC port is stable.

## Checkouts and source pins

| Role | Location / revision |
| --- | --- |
| Active source and build | `C:\Users\wowjinxy\Projects\WindowWaker`, branch `main` |
| Qualified baseline | `875ea27a8a849f9541b24473e026d8ea0cd8e8c1` |
| Qualified post-import new-game build | `fd6fc89feb74729a415adc7487d8b503da9e5197` |
| Windows donor | [MaskeZenY/WindowWaker](https://github.com/MaskeZenY/WindowWaker), initial checkout `105e244` |
| Native upstream | [centollOS/SwitchWaker](https://github.com/centollOS/SwitchWaker); selected changes are reviewed individually |
| Retained initial Windows checkpoint | `D:\Projects\WindowWaker`; archive, not the active build location |
| Existing recompilation port | `E:\GPT5\WindWakerRecomp\bluewake-porpoise`; preserved separately |
| Aurora | `3227d76c60e1e782ca576610bce61c9e7744d8be`, plus this repository's `native/patches/aurora` |
| RecompCore DSP sources | `8ab24daee9c641634fda5cac30389ad4b2cfda5e`; the pinned checkout is read without modifying it |
| Decompilation configuration for asset headers | `snrubrm/tww` at `b09eebc39852e18397031d9a563a8eae124f21f1`, from `game/UPSTREAM` |
| Converged game-source reference | `zeldaret/tww` at `a1854d47ce5aa9d0f21c7fb5a71aaade63659693`, from `game/UPSTREAM` |

The active checkout moved from D: after small synchronous writes there stalled and the volume reported `Warning`. The archive and the previous E: project were retained. Build outputs remain on C:, with an 8 GiB free-space reserve used during this bring-up. No previous BlueWake build or tester package was replaced.

On 2026-10-15, the native checkout became the primary `main` of `wowjinxy/bluewake-porpoise`. The merge preserves the previous BlueWake main `d08c197a5e8d6e2e09b8de2c6e38044bb19b87e5` as an ancestor and on `legacy/bluewake-recomp-20261015`; its physical checkout and uncommitted work remain untouched. `pc-bringup` retains checkpoint `13d31f1`. Publishing remote `origin` now targets the user's repository; `windowwaker` and `upstream` remain donor remotes. See [workspace routing](WORKSPACE_LAYOUT.md) and [enhancement migration](NATIVE_ENHANCEMENT_MIGRATION.md).

## Reproduce the Windows build

Use this branch's source, a legally obtained plain `GZLE01` USA revision 0 ISO, Python 3.10 or newer with PyYAML, Git, Ninja and CMake 3.28 or newer. Visual Studio 2022 needs its C++ workload, Windows SDK and C++ Clang tools. Run from an x64 Developer PowerShell with `clang-cl.exe` and `lld-link.exe` on `PATH`.

The qualified toolchain was Clang/clang-cl 19.1.5 with lld-link, Visual Studio 2022's MSVC 14.44 headers/libraries, Windows SDK 10.0.26100.0 and CMake 4.0.0. Release game objects use `/O2 /Ob2 /DNDEBUG`. Stock `cl.exe` is not supported by the native game's compiler flags.

Run preparation once for a fresh asset directory:

```powershell
python -m pip install PyYAML
python native/tools/fetch_recompcore.py
python native/tools/gen_assets.py --stubs --out build/native-win/assets/GZLE01

$clang = (Get-Command clang-cl.exe).Source
$lld = (Get-Command lld-link.exe).Source
cmake -S native -B build/native-win -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_C_COMPILER=$clang" "-DCMAKE_CXX_COMPILER=$clang" `
  "-DCMAKE_LINKER=$lld" -DCOS_RUNTIME_ASSETS=ON
cmake --build build/native-win --target switchwaker --parallel 4
```

The executable is `build/native-win/switchwaker.exe`. The missing Python bootstrap commands are implemented without Bash. They retain mismatched existing dependency checkouts and reject nonempty asset outputs. Runtime asset stubs contain declarations and locations from the decompilation's configuration; the game loads their contents from the player's disc at startup. Generated data and fetched dependencies stay under ignored `build/` and `ref/` directories.

Aurora fetches its pinned dependencies, using Windows packages for Dawn, nod and SDL. The Windows post-build rule copies shared libraries and Dawn's dynamically loaded `dxcompiler.dll`/`dxil.dll` beside the executable. DSP compilation uses RecompCore's Windows `UNICODE`/`_UNICODE` contract and Aurora's `ZLIB::ZLIB` target. With `/MANIFEST:NO`, the lld-link executable rule retains response files directly instead of letting CMake's manifest wrapper expand the whole game's object list past Windows' command-line limit. Game objects remain linked directly in their explicit order.

For an ordinary local launch, set `COS_DISC` to the absolute ISO path and run `run.cmd` from that same shell. The default presentation rate is 30; higher-rate interpolation remains optional. Settings normally live in `build/native-win/user/settings.ini`.

## Primary-main defaults check

The source promotion also aligns the menu's built-in target rate with the existing 30 FPS pacing fallback. The Windows packager now emits comment-only settings instead of forcing 120 FPS, 4K and an FPS overlay. Saved player choices remain authoritative; no existing settings or tester archive was overwritten.

The affected `switchwaker` and `cos_pc_tests` targets rebuilt successfully in 5.453 seconds. The resulting executable is 13,498,368 bytes with SHA-256 `6ef7df5537f8afe1f18540482b79abfccd2d1ac63ebe415bb31c04dc09170903`. Evidence is under `build/pc-bringup-20261015/`:

| Check | Actual result | Private evidence |
| --- | --- | --- |
| Fresh settings without a target-FPS environment override | `COS_TARGET_FPS` absent from the launch environment; options smoke asserted effective setting 30 and overlay off; PASS at frame 360, exit 0 | `runtime-primary-fresh-default1/` |
| FPS menu change and persistence | Script opened the actual menu at frame 400, selected FPS, changed 30 to 60 then back to 30, and closed at frame 430; asserted saved `COS_TARGET_FPS=30`, scene `sea` room 44 and overlay off; PASS at frame 520 with 27 paused frames checked, exit 0 | `runtime-primary-fps-menu1/` |
| Native helper tests | `ok`, with unsupported POSIX fork/wait death checks explicitly skipped | `runtime-primary-pc-tests1/` |
| Windows package settings | Disposable fake-repository run verified both staged and zipped settings have no active overrides or `[dev]`, with the declared dummy files and launcher unchanged | `package-defaults-fixture/result.json` |

These isolated cases booted directly into Outset and used scripted input. They establish defaults, menu behavior and persistence, not a new whole-game performance or physical-controller result. The package fixture is not a new game package or release. The already qualified `fd6fc89` tester below remains unchanged.

## Baseline proof

These results belong to baseline `875ea27`, before the three selected upstream fixes below. The preserved baseline executable is 13,497,856 bytes with SHA-256:

```text
97cfa97dd56fef6e5e99058b7c6c19c102613181c7f5c6d48645aac3aa1bd91c
```

| Check | Observed result | Private evidence directory under `build/pc-bringup-20261015/` |
| --- | --- | --- |
| C/C++ scaffold, SDK headers and Windows file I/O | Build checks and the production-flags I/O runtime passed | `io-production1/`, `build-checks-20261015T223053Z/` |
| Native helper tests | `cos_pc_tests` returned `ok`; POSIX death checks explicitly reported `SKIP` | `runtime-pc-tests1/` |
| SDK smoke tests | Returned `ok`; unsupported POSIX process/death checks explicitly reported `SKIP`; AI tone DMA dump matched its input | `runtime-sdk-smoke1/` |
| Static initialization | 502 of 502 profile slots filled; zero errors; exit 0 | `runtime-static-init1/` |
| Disc traversal | 2,213 files, 177 directories, zero errors; exit 0 | `runtime-disc-ls1/` |
| Title | 1,500 frames, exit 0; captures at frames 600 and 1,400 inspected and verified as rendered title images | `runtime-title1/` |
| Direct debug Outset boot and scripted movement | `outset-control` reached at frame 4,100, internal elapsed 138.763 s; a 120-frame stick hold moved Link 358.2 units; exit 0 | `runtime-outset-control1/` |
| Outset visual check | Capture at frame 3,800 inspected: actual Link and Aryll rendered in Outset | `runtime-outset-control1/shot-003800.png` |

The renderer selected D3D12 on the NVIDIA GeForce RTX 3070. In this initial baseline case, Outset was entered directly with `COS_BOOT_STAGE=sea:44:206`; this case does not prove the real new-game flow. The later post-import case below covers that path. Controller input came from the virtual-pad script, not a manual controller session. The SDK tone and initialized DSP paths do not establish acceptable audible music or effects during play.

Each runtime evidence directory preserves its plan, executable identity, bounded process result, log and any captures. The original binary is retained privately as `build/pc-bringup-20261015/baseline-875ea27-switchwaker.exe`. These artifacts are ignored and are not distributed with source. See `CURRENT_STATE.json` in that directory for the running investigation's latest handoff; its timestamp matters.

## Post-import proof

The three selected fixes were rebuilt at `fd6fc89feb74729a415adc7487d8b503da9e5197`: build elapsed 315.344 seconds, exit 0. This executable is 13,498,368 bytes with SHA-256:

```text
c4367134a46edeebc2ecd9f5b061a0262215ba1570b5b88de1d34f09d459720b
```

| Check | Observed result | Private evidence under `build/pc-bringup-20261015/` |
| --- | --- | --- |
| Post-import static initialization | 502 of 502 profiles, zero errors; passed | `runtime-static-init2/` |
| Real new-game flow, M14 | Normal card creation, name entry, prologue and PLAY scene in `sea` room 44; one STB introduction demo completed; event ended and Link became free at frame 11,009 | `runtime-outset-real1/` |
| Real Outset control milestone | A 120-frame stick hold moved Link 361.2 units; `outset-real` reached at frame 12,420, internal elapsed 411.598 s; child elapsed 428.031 s, exit 0 | `runtime-outset-real1/result.json`, `logs/0.log` |
| Captures from the normal flow | Frames 1,500, 8,500 and 12,000 retained; frame 12,000 inspected and verified as actual Link and Aryll with the correct HUD after the introduction | `runtime-outset-real1/shot-008500.png`, `shot-012000.png` |
| Initial card file | Offline validation passed for the 98,368-byte GCI's structure, both save-block checksums, all three slot checksums and matching redundant blocks | `runtime-outset-real1/gci-validation.json` |
| Actual game audio output | Captured 409.97325 seconds of 16-bit stereo PCM at 32 kHz; sampled windows at 20, 60, 120, 220, 300 and 400 seconds contain nonzero samples | `runtime-outset-real1/audio-validation.json`, `audio.wav` |

The GCI check establishes an initial card write and valid inspected structures; no in-game reload was performed, and that initial write may precede committing the entered name. PCM output establishes actual game audio data, without establishing listening quality, synchronization or acceptable music/effects. Manual controller acceptance is also pending.

The first focused Jabun diagnostic (`runtime-jabun1/`) played 1,795 demo frames before reaching its 60-second diagnostic limit. Completion was not established. Its process returned exit 0, but that exit is not a passing Jabun behavior result; the incomplete evidence is preserved.

The second diagnostic (`runtime-jabun2/`) completed the `getperl_jab` event with `COS_UNCAPPED=1` and `COS_SYNC_PIPELINES=0`, then requested the stage change to `sea` (room 44 in the log). Its event artifact records `64 getperl_jab stage-change 2814 23.5 next=sea`; the owned child exited cleanly in 44.094 seconds. `Demo18` and its STB loaded, the cast had no missing entry, and the expected B35 guard dropped animation file `0x3b` because its kind is 4 rather than requested kind 5. The skipped-BRK fallback was not used. This qualifies that event path under the accelerated diagnostic; it is not a paced full-scene or audible-quality acceptance. The boomerang-door shadow case and in-process menu/2D-heap teardown case still require their specific runtime reproductions.

## Isolated runtime checks

Every automated game case uses a fresh directory with `COS_RUN_DIR`, `COS_SETTINGS` and `COS_CARD_DIR` pointing into that case. A nonempty explicit `COS_CARD_DIR` applies to ordinary launches as well as test milestones, without clearing an existing card; without that override, normal saves remain beside the executable. `COS_CACHE_DIR` can select a private cache; its parent must exist. A valid absolute `COS_DISC` prevents the Windows disc-selection dialog from opening during unattended checks.

`COS_SMOKE=static-init` runs before disc/GPU startup. `COS_SMOKE=disc-ls` traverses the disc before game startup. A named `COS_MILESTONE` exits successfully only when that milestone is reached. `COS_FRAMES` instead exits after that many outer game frames, so frame-cap completion alone does not prove that a requested gameplay state was reached. The Outset movement check uses `native/check/input/outset-control.txt`, requires a 120-frame stick hold moving more than 300 units and at least 3,600 frames after the start room is ready, and runs with a 220-second internal timeout plus a 240-second owned-process bound.

## Performance and remaining acceptance

The completed post-import normal-flow recording provides a qualified Outset introduction window, frames 9,000 through 10,800: 1,801 unique consecutive rows with strictly increasing timestamps, spanning 1,800 frame intervals over 60,047.978 ms. There are no screenshots inside this window; the case's captures occur outside it. This window precedes the introduction event ending at frame 11,009, so it is not a measurement of free exploration.

| Metric | Observed value |
| --- | --- |
| Endpoint frame rate | 29.97603 FPS |
| Median frame-start interval | 33.3325 ms |
| p95 / p99 interval | 34.24645 / 38.7493 ms |
| Maximum interval | 77.152 ms |
| Intervals greater than 33.34 ms / 50 ms | 889 / 5 of 1,800 |
| Mean measured busy time / explicit pacing wait | 3.70690 / 29.62809 ms per row |

The full offline report is `build/pc-bringup-20261015/runtime-outset-real1/perf-outset-9000-10800.json`. Frame rate uses the endpoint frame difference divided by timestamp difference; percentiles use linear interpolation. These values qualify this recorded window, not every area or uninterrupted 30 FPS delivery. Boot totals, screenshots and synthetic VI retrace counts are not frame-rate measurements. In 30 FPS mode the outer frame and simulation step are one-to-one, but original logo code requests 60 Hz before opening/gameplay requests 30 Hz. At higher presentation settings, interpolated render frames and simulation ticks differ.

Three matching title runs used no input, screenshots or tracing, with `COS_PERF=<case>/perf.csv`, `COS_PERF_EVERY=120`, `COS_HITCH_MS=34`, `COS_FRAMES=1800`, `COS_TARGET_FPS=30` and `COS_UNCAPPED=0`. All offline windows cover frames 900 through 1,800 with 901 unique consecutive rows and 900 intervals:

| Title case | `COS_SYNC_PIPELINES` | Endpoint FPS | p95 / p99 interval | Maximum interval | Intervals > 50 ms |
| --- | --- | --- | --- | --- | --- |
| `runtime-title-perf1` | `0` | 27.04659 | 63.27715 / 94.28531 ms | 255.273 ms | 117 / 900 |
| `runtime-title-perf2-sync` | `1` | 29.94189 | 34.46405 / 43.44005 ms | 94.660 ms | 7 / 900 |
| `runtime-title-perf3-async` | `0` | 29.99977 | 33.78110 / 34.35231 ms | 38.915 ms | 0 / 900 |

The reports are `perf-window-900-1800.json` in each case directory. The repeated asynchronous window spans 30,000.227 ms, with a 33.3325 ms median frame-start interval; its whole child run completed in 72.656 seconds with exit 0. The initially slower asynchronous result is preserved. The synchronous improvement did not repeat as a mode advantage: load and cache state were uncontrolled, a system CPU snapshot was approximately 63% with other applications and a linker active, and both modes share the same ready-pipeline lookup path. The ordinary pipeline default remains unchanged; these results do not establish a causal benefit from switching it.

Synchronous mode changes missing-pipeline handling: it waits for the requested pipeline instead of skipping the draw. Captures enable it by default for complete rendered images, so earlier captured new-game timings must not be substituted for the untouched title case. In particular, `new-game.txt` presses START at frames 600 and 720 to skip the title intro and enter the file scene. CSV `busy_ms` excludes explicit pacing waits but includes blocking; it is not exclusive CPU time. Windows `cpu_ms` is unavailable in these builds.

Remaining acceptance includes manual controller and menu use, audible music/effects, an actual in-game save/reload round trip, longer gameplay and further scene changes, plus the focused imported-fix reproductions above. The real new-game/prologue-to-Outset path is now covered by M14. These checks should preserve the existing build and private diagnostic results. They do not make optional HD features a prerequisite for native PC bring-up.

## Selected upstream fixes

Three reviewed SwitchWaker correctness fixes were imported after baseline qualification, preserving the original Pulpparty authorship. Normalized postimages matched the prepared patches. The rebuilt executable passed post-import static initialization and the full M14 new-game-to-Outset check. That general regression does not by itself reproduce each fix's original trigger; focused acceptance remains as stated above.

| Fix | SwitchWaker donor commit | Local commit |
| --- | --- | --- |
| Shadow correctness fix | `121cf2f21f4d1d8ad64ad7ad370b4c1087adcac5` | `99055d39c5f8fdd2992147d8e890af649d2a3c79` |
| Freed 2D heap correctness fix | `253de28ad11360b886c9c2c29361524453ba1cc9` | `453f0b9cb3589c0908d1b0cdd49085d7a9abfcac` |
| Jabun correctness fix | `eae8c4eb79be17c7b5f88f482c56366978c3dc5d` | `fd6fc89feb74729a415adc7487d8b503da9e5197` |

The first post-import build log is `build/pc-bringup-20261015/build-switchwaker-fixes-20261015T223721Z/output.log`. Broad upstream synchronization, Switch-only renderer changes and HD feature migration are deferred.

## Tester package status

The tester uses the qualified `fd6fc89` executable in the original 30 FPS mode, with the existing pipeline default. The ready local folder is `build/native-win/WindowWaker-PC-tester-fd6fc89-20261015/`; double-click `Play WindowWaker.cmd`. The sealed ZIP is `build/WindowWaker-PC-tester-fd6fc89-20261015.zip` (22,090,199 bytes; SHA-256 `9445a935055335d96134f03cbf48d080b9dcbbae9e84b223be4d02e594b22c8f`).

The staged executable was launched with its copied runtime DLLs and completed 650 frames with exit 0. It reached the title milestone at frame 561; the frame-600 capture was inspected and rendered correctly. Evidence is in `runtime-packaged-title1/`. The final archive contains 20 entries, and every declared file was checked against the manifest after compression. Only declared files are archived: no ISO, extracted assets, test cards, recorded audio or generated caches are included. Startup validation used the staged folder; archive entries were verified against that folder.

The launcher uses this PC's existing original ISO when present and otherwise leaves selection to the native file picker. Settings and saves remain in the extracted folder's `user` directory. The normal source-tree `run.cmd` is also configured locally for the existing ISO. `E:/GPT5/WindWakerRecomp/WINDOWWAKER_PC_LOCATION.txt` points to the active checkout; the legacy recompilation checkout remains separate.

## First native controls migration

Recorded on 2026-10-15 (America/Chicago). This slice adds a real Controls tab, moves the existing camera inversion rows there without changing their keys/defaults, and adds optional `COS_QUICK_ITEMS`, default off. Enable through F1 > Controls > Equipment shortcuts. Hold eligible controller LB or keyboard Tab and press Up to conduct, Left to deploy/fire the cannon while riding, or Right to deploy/lower the crane; release Right to retract and use native A to stow. Owned equipment and safe native procedures are required. Plain D-pad, Down, X/Y/Z assignments and LT/RT are retained. Conflicting custom profiles keep their native controls and decline the shortcut. Binding/profile editing, native event/save foundations and networking remain later milestones.

The named Windows actor libraries, `switchwaker`, `cos_pc_tests` and `cos_quick_items_tests` were built with the documented clang-cl toolchain. The final candidate was linked at 2026-10-16 01:06:12 UTC from this slice on top of source `53051479cae95c26b87ac4716e01f4d687beb1f6`. Its executable is 13,513,216 bytes, SHA-256:

```text
fe8e8d7be46653a3b05fdfed5951f7e4af888fa954496226a814f9fe2c3fe7ff
```

The final build log is `build/pc-bringup-20261015/build-migration-final-20261016T010602Z/output.log`. All game cases in the table use that same executable with isolated settings/card/run directories. Every listed child exited zero and its owned process job drained; explicit assertions below establish the requested result. Private case paths are relative to `build/pc-bringup-20261015/migration-controls-20261015/`.

| Check | Observed result | Private case |
| --- | --- | --- |
| Input lifecycle/profile policy | Pure tests passed: release gating, menu/focus/device/profile/scene changes, conflicting directions, consumed-direction suppression, physical and keyboard profile conflicts | `runtime-gate3/` |
| Live Controls menu | Three menu opens; both camera axes and equipment shortcuts changed and persisted; PASS at frame 660, 131 paused player frames checked, zero allocation failures; frame-616 Controls capture inspected | `runtime-menu-live2/` |
| Settings reload | Fresh process restored both camera axes and shortcuts enabled; PASS at frame 360; settings file hash unchanged | `runtime-menu-reload2/` |
| Native boat equipment | Already-owned unassigned cannon/hook used while riding; cannon deployed and two actual shots consumed bombs 30 to 29 to 28; cooldown repeat rejected; crane rope lowered from 20 to 60 and retracted to 20, then A stowed it; Down, X/Y/Z, ready index, ownership and actor identity preserved | `runtime-boat3/` |
| Missing boat tools | Actual riding context with bombs/hook absent; Left/Right ignored, ammo stayed zero, Down and assignments preserved | `runtime-boat-missing/` |
| Native conducting | Ordinary Windfall spawn `sea:11:9`; owned unassigned baton entered native TACT_WAIT; actual left/right conducting correct with camera inversion off and both axes on; native B cancellation returned to safe control; assignments and Down preserved | `runtime-foot2/` |
| Missing on-foot tools | Same Windfall context, all three shortcut tools absent; requests ignored, ammo/Down/assignments unchanged | `runtime-missing/` |
| Camera-only inversion | Existing camera case passed with opposite native yaw/pitch responses; conducting left remained direction 4 | `runtime-camera/` |
| Existing virtual PAD | 20 script lines, 81 frames checked, zero errors, including native edges/sticks and analog L/R | `runtime-pad/` |
| Diagnostic modifier transparency | MOD adds no GameCube bit; native analog L/R and Down preserved with shortcuts disabled; 9 script lines, 41 frames checked, zero errors | `runtime-pad-mod/` |
| Fresh defaults | With `COS_TARGET_FPS` absent, effective target remained 30, shortcuts and both camera inversions were zero, FPS overlay remained off; PASS at frame 360 | `runtime-defaults/` |
| Existing helpers | `cos_pc_tests` passed; unsupported POSIX-only death checks explicitly skipped | `runtime-helpers/` |

Earlier setup attempts remain preserved. `runtime-boat/` and `runtime-boat2/` exhausted their process bounds while the ordinary King conversation still needed native A confirmation. The successful retry sends normal A only in the live dialogue states; it does not write story flags or bypass event state. `runtime-foot/` waited in Outset's opening sequence with a neutral driver; the focused conducting retry uses the ordinary Windfall spawn above. These failed setup runs are not passing action checks. The first aggregate script also expected a nonexistent `pad-echo: PASS` string; `runtime-pad/` itself passed with its real zero-error count oracle, recorded separately in `pad-native-validation.json`. The original aggregate report is preserved.

The modifier is driven through the diagnostic input seam in these game cases. This qualifies the scripted native actions, not physical LB/Tab hardware use, full profile editing, full-game stability or an in-game save/reload transaction. The slice supplies no new performance benchmark or audible-quality acceptance. Earlier normal-new-game and performance evidence above retains its original executable identity. Tingle rescue wait-skip stays disabled and does not gate this slice or a local tester. The prior tester ZIP remains intact; a fresh controls tester is sealed separately only after the staged runtime reaches the title and its capture is inspected.
