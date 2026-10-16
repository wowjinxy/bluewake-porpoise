# Notes for agents

## Primary project and preserved history

This repository's `main` is the native Windows PC source port, adopted on 2026-10-15.
Publishing remote `origin` is `https://github.com/wowjinxy/bluewake-porpoise.git`.
`windowwaker` and `upstream` are donor remotes; do not publish to them.
The previous recompilation main is preserved on `legacy/bluewake-recomp-20261015`
and in the merge ancestry. Adapt legacy features through native functions; never
copy guest CPU/register/RAM adapters into this implementation.

On this machine the active checkout/build is `C:\Users\wowjinxy\Projects\WindowWaker`.
`E:\GPT5\WindWakerRecomp\bluewake-porpoise` is the preserved legacy checkout, including
uncommitted work; do not pull the native main into it, move it, reset it or clean it.
`D:\Projects\WindowWaker` is an earlier checkpoint, not the active build location.
Do not put new builds on E: or D:; preserve at least 8 GiB free on C: during builds.
See [Windows bring-up](docs/WINDOWS_PC_BRINGUP.md) for actual qualification and
[the migration plan](docs/NATIVE_ENHANCEMENT_MIGRATION.md) for accepted behavior.

Controls/menu are first. Preserve LB/Tab + D-pad shortcuts, ordinary D-pad/Down,
X/Y/Z and conducting directions when implementing them. Co-op means independent
exploration with shared permanent progress, using an owned TCP service inspired
by Anchor. It is not implemented or public Anchor-compatible. Real native
events, safe saves and room isolation precede shared rewards. Tingle rescue
wait-skip stays disabled; retain its old work/results and never make it a gate
for unrelated milestones or tester builds. HD migration follows a stable PC port.

Always read timestamps before responding or acting on old handoffs. Preserve
saves, private diagnostics, failed results and the playable baseline. Do not take
over the user's mouse. When using Ghidra MCP, never switch the current program;
target the intended program explicitly because other agents may share it.
Do not add AI authors/co-authors, generated-by attribution or session links to
commits/PRs. Preserve donor authorship/licenses and sign off new commits per
CONTRIBUTING.md. Keep disc data, builds, saves and private evidence out of Git.

For Windows changes build the named `switchwaker` target using the documented
clang-cl toolchain, plus relevant tests. Use isolated settings/card/run paths for
runtime checks. A successful build, frame cap, initial GCI structure check or
nonzero PCM does not prove gameplay, save/reload or audible quality. The Switch
and Mac instructions below apply only when working on those platforms.

## Reaching the Switch: the debug server

The console runs a TCP debug server (port 6543) when the options menu (Minus) has Depuración >
**Servidor de depuración (red)** set to *Activado*; it starts at the next start of the game. Full reference:
[docs/DEBUG_SERVER.md](docs/DEBUG_SERVER.md).

- The console's address: `build/switch_host.txt` (first line), else `export SWITCHWAKER_HOST=<ip>`, else
  `--host <ip>`. `build/` is not in git, so a fresh checkout needs the file written again.
- Check that it answers: `scripts/switch/switchwaker_debug.py ping` (or `info`: version, frame, stage and
  room, memory). If it does not answer, ask the user to start the game (and turn the server on if it is off);
  do not guess.

## Update the console

```sh
scripts/switch/build_native.sh && scripts/switch/switchwaker_debug.py deploy
```

`deploy` uploads `build/switch-native/switchwaker.nro` (the deko3d NRO, the default renderer) with the
`initial_pipeline_cache.db` and `initial_dksh_cache.bin` built next to it, checks each CRC32, restarts the game
and waits until it answers. The GL NRO (`build_native.sh --renderer gl`) lives in `sdmc:/switch/switchwaker_gl/`,
which the forwarder does not start. The restart (`reload`) needs the game started from the HOME-menu forwarder; from hbmenu,
`deploy --no-reload`, then ask the user to start it again. Building: [docs/SWITCH_BUILD.md](docs/SWITCH_BUILD.md).

## Debug

```sh
scripts/switch/switchwaker_debug.py log --save build/session.log &   # the log as it is written
scripts/switch/switchwaker_debug.py warps                            # the options menu's travel list
scripts/switch/switchwaker_debug.py warp 2 && sleep 10 && scripts/switch/switchwaker_debug.py shot build/shot.png
scripts/switch/switchwaker_debug.py press MINUS                      # buttons as if from the controller
scripts/switch/switchwaker_debug.py get native/user/settings.ini build/settings.ini   # relative to sdmc:/switch/switchwaker
```

After a crash the server is gone with the game: ask the user to start it again, then
`scripts/switch/switchwaker_debug.py lastlog` (the crashed session is then the second newest, see `logs`) and
`scripts/switch/switchwaker_debug.py crashes --fetch build/crashes`.

Developer variables without a menu row (`COS_TRACE`, `COS_BOOT_STAGE`, `MESA_*`, ...) go in the `[dev]`
section of `native/user/settings.ini` ([switch/native/settings-dev.example.ini](switch/native/settings-dev.example.ini)),
read at start: `get` it, edit, `put` it back, `reload`.

The debug server (`switch/native/source/debug_server.{h,cpp}`) and the client are shared with SwitchWakerHD:
keep the two copies the same.

## Defaults are for the player

A fresh install (no `settings.ini`) must start with player defaults: debug server off, frame-rate counter
and every diagnostic, perf or capture option off, stock clocks, mods off, no `[dev]` section. A new
diagnostic or test option ships off and goes in the options menu's Depuración section. Check the
fresh-install defaults again whenever the settings code changes; a player's saved `settings.ini` keeps what
they chose.

## Clocks

The CPU stays at its stock 1020 MHz ([docs/SWITCH_BUILD.md](docs/SWITCH_BUILD.md), "Run"); the GPU uses
Nintendo's official performance profiles. Do not propose an overclock as a fix: speed comes from the code.
The target is 720p (1280x720 internal) at 30 fps.

## The SD card without the debug server

- Best: hekate's USB mass storage (UMS). The SD mounts at `/Volumes/SWITCH SD` (FAT32): plain `cp`, then
  `sync`; the user ejects it before leaving UMS. The port's files are in `/Volumes/SWITCH SD/switch/switchwaker/`:
  logs in `native/logs/`, settings in `native/user/settings.ini`, caches in `native/user/cache/`.
- MTP is fragile: one operation at a time, `killall icdd` first (Image Capture grabs the device), never kill
  an MTP process mid-session, list one folder rather than the whole card. If a session fails twice, stop and
  ask the user to restart the MTP app on the console.

## Mac build

Build the target by name: `cmake --build build/native-mac --target switchwaker`, and check the date of
`build/native-mac/switchwaker` before trusting a run. The default target once left the binary a day old
without an error, and a whole debugging pass compared the old binary with itself.

## Checks

Per change, run the checks that change touches (its build, the affected stages, its own verification).
The full regression (`native/tools/regress.sh`) and the stage sweep (`native/tools/run.sh boot-sweep`) only
at checkpoints (every few integrated changes, or at the end of a phase); bisect if a checkpoint fails.
