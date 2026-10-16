# Local workspace routing

Recorded 2026-10-15 (America/Chicago). The primary implementation is the native PC source port.

| Role | Location |
| --- | --- |
| Active source, `main` | `C:\Users\wowjinxy\Projects\WindowWaker` |
| Normal launcher | `C:\Users\wowjinxy\Projects\WindowWaker\run.cmd` |
| Build executable | `C:\Users\wowjinxy\Projects\WindowWaker\build\native-win\switchwaker.exe` |
| Private handoff and diagnostics | `C:\Users\wowjinxy\Projects\WindowWaker\build\pc-bringup-20261015\CURRENT_STATE.json` |
| Preserved recompilation checkout | `E:\GPT5\WindWakerRecomp\bluewake-porpoise` |
| Earlier native checkpoint | `D:\Projects\WindowWaker` |
| Original personal disc | Existing ISO under `E:\GPT5\WindWakerRecomp\old` (read-only) |

Publish native changes from C: to `origin` (`wowjinxy/bluewake-porpoise`), branch `main`. Donors are `windowwaker` (`MaskeZenY/WindowWaker`) and `upstream` (`centollOS/SwitchWaker`). The old public main is preserved on `legacy/bluewake-recomp-20261015`; `pc-bringup` retains the pre-promotion native checkpoint. The promotion keeps both histories reachable without forcing the remote branch.

Leave the old E: checkout in place, including its dirty files, untracked experiments and diagnostic artifacts. Its local branch still describes the legacy project. Do not pull the native main into that directory or treat its build scripts as the active build. Do not rename or clean the nested repositories to make the parent folder look uniform.

Build on C: with an 8 GiB free-space reserve. D: had slow synchronous writes and reported a storage warning; E: had too little free space. Keep all earlier binaries, tester archives, saves, captures and inconclusive results. Read the current handoff timestamp before resuming experiments.

The E: parent routing files (`AGENTS.md`, `WINDOWWAKER_PC_LOCATION.txt`, `Play Wind Waker.cmd`) point to this checkout. Their contents are simple local pointers; the primary source and this routing record are committed here. No disc, save or game build is copied into the parent.

The sealed `build/WindowWaker-PC-tester-fd6fc89-20261015.zip` remains the qualified local tester. Subsequent source/default changes do not silently overwrite its manifest or executable. Current build identity and default checks belong in the Windows bring-up record. Public binary publication is a separate task and is not implied by source promotion.
