# BlueWake native PC port

The primary `main` branch now uses the WindowWaker/SwitchWaker native C++ GameCube port. Game logic is compiled from the community decompilation and rendered through Aurora (WebGPU/Dawn); Dolphin's DSP HLE supplies audio. Windows is the platform qualified in this fork.

The previous BlueWake recompilation project is preserved on [`legacy/bluewake-recomp-20261015`](https://github.com/wowjinxy/bluewake-porpoise/tree/legacy/bluewake-recomp-20261015). Its history remains an ancestor of `main`. Enhancements and multiplayer will be adapted to native game functions and saves in small tested changes.

## Current status

The Windows build completes the real new-game flow through the prologue to controllable Outset. Measured windows reached 29.976 FPS during the Outset introduction and 30.000 FPS on a repeated warm title run. These are bounded results, not full-game performance or stability guarantees. Manual controller acceptance, audible quality, a genuine in-game save/reload round trip and broader gameplay remain open.

Fresh player settings target **30 FPS**, with the FPS overlay off. Higher presentation rates and visual options remain optional. See [Windows qualification and build evidence](docs/WINDOWS_PC_BRINGUP.md) for revisions, measured windows, preserved slower results and the local tester package.

## Enhancement migration

The [native enhancement roadmap](docs/NATIVE_ENHANCEMENT_MIGRATION.md) starts with controls, menu and equipment shortcuts, then native gameplay/save events and recoverable isolated saves. Shared-progress multiplayer follows those foundations: independent exploration, an owned TCP service, and permanent rewards applied safely on the game thread. Native multiplayer is not implemented yet.

The first native controls slice adds a **Controls** tab with camera inversion and optional equipment shortcuts. Enable **Equipment shortcuts** there: hold LB (keyboard Tab + arrow keys), then Up to conduct, Left to deploy the cannon at sea, or Right to deploy the crane at sea. Press Left again to fire the cannon; hold Right to lower the crane and release it to retract. The native A action stows boat equipment. Earned gear and a safe native action context are required. Plain D-pad, Down and X/Y/Z assignments retain their behavior.

Shortcuts start off. Conflicting custom controller/keyboard profiles keep their native controls and decline the shortcut; full rebinding/profile editing remains later work. Automated game tests use a diagnostic modifier, so physical-controller acceptance remains separate. HD additions follow a stable PC port; the optional Tingle rescue wait-skip stays disabled and does not block builds.

## Build and run on Windows

Use an uncompressed personal `GZLE01` USA revision 0 disc image. Disc images, extracted assets, saves and local builds are not included in this source repository.

Requirements: Visual Studio 2022 with Desktop development with C++, Windows SDK and C++ Clang tools; CMake 3.28 or newer; Ninja; Python 3.10 or newer with PyYAML; and Git. Use an x64 Developer PowerShell with `clang-cl` and `lld-link` on `PATH`. Stock MSVC `cl.exe` is not supported by the game's compiler flags.

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

$env:COS_DISC = "C:\Path\To\YourDisc.iso"
.\run.cmd
```

Use fresh build/asset directories when changing generators or compilers. Preparation fetches pinned sources under `ref/` and `build/`; runtime asset stubs contain declarations and locations, and asset contents are read from the disc at runtime. See the [full Windows guide](docs/WINDOWS_PC_BRINGUP.md) for toolchain details.

Press **F1** or **L + R + Z** to open the existing options menu. Normal settings and saves live under `build/native-win/user/`; an explicit `COS_CARD_DIR` selects a separate card directory. Keep original saves backed up before experiments.

## Credits and licenses

This fork builds on [MaskeZenY/WindowWaker](https://github.com/MaskeZenY/WindowWaker), [centollOS/SwitchWaker](https://github.com/centollOS/SwitchWaker), [zeldaret/tww](https://github.com/zeldaret/tww), [snrubrm/tww](https://github.com/snrubrm/tww), [Aurora](https://github.com/encounter/aurora), [Dusklight](https://github.com/TwilitRealm/dusklight), [Dolphin](https://dolphin-emu.org), and the DSP/recompilation work in [elliotttate/Wind-Waker-Recomp](https://github.com/elliotttate/Wind-Waker-Recomp). Selected donor commits retain their original authorship. Earlier BlueWake contributors remain in the preserved history.

Project code is [MIT](LICENSE), decompiled game source is [CC0-1.0](game/LICENSE), and the DSP HLE code is [GPL-2.0-or-later](native/dsp_hle/LICENSE). Keep individual third-party notices and licenses. This is an independent community project and is not affiliated with Nintendo.
