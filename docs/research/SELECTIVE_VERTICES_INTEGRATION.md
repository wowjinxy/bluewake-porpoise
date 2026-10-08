# Selective GX vertex inputs in Windows builds

On October 8, 2026, the user played the experimental build and reported that it
was "a bit smoother actually," then requested that it be committed. This is a
subjective playtest result, without a measured FPS or frame-time claim.

Active runtime patch **0165** contains the exact ten SDK source changes tested
in the [frozen experiment](../../patches/experiments/selective-vertices/README.md).
The root HUD registration is integrated with the same selective-safe contract.
All eleven materialized source files match that experiment's candidate SHA-256
pins. The frozen bundle records its earlier inactive status; this document
records the subsequent integration after the user's playtest.

Normal interactive Windows Aurora launches now select the compact layout when
`DOL_GXCORE_SELECTIVE_VERTICES` is absent. An explicit `0` retains full vertices;
other explicit values keep the SDK's exact-value behavior. Safe mode forces `0`.
Headless and noninteractive launches receive no new default, so their explicit
comparison settings remain in control. Other platforms retain the SDK's opt-in
default. Pipeline cache version is 15; shader arithmetic remains unchanged.

To disable the selective path for one Windows launch in PowerShell:

```powershell
$env:DOL_GXCORE_SELECTIVE_VERTICES = '0'
.\BlueWake.exe
```

The active recipe verifies against RecompCore base
`e280c788dadabd18b085af0085f1558fc9ff5ecc`, producing tree
`52ade47eb3ed8b0b1d2e2b369a9bfa9c13c451f1`. Patch 0165 SHA-256 is
`5ff08cf1c80fde7e9b27dfb3162416c704662139fb9dd091e0ddefc7ccb5e6c6`.
The six runtime-patch regression tests and three runtime-training identity
tests pass. The existing decoder, focused interpolation and D3D12 fixture
results continue to apply to the identical SDK/HUD sources.

Both changed Windows startup/settings translation units also compile with the
retained O3/x86-64-v3 production commands. Actual `-M` and `-MD` input sets match,
and those inputs remain unchanged. This is compile-only validation of the new
default and safe-mode wiring, without a new host link or another gameplay claim.
Private receipt `build/selective-vertices-integration-20261008/compile1/result.json`
has SHA-256 `760f197e0f851d7583fb4e5ddd292dbd1f67a778b5f6f0811ee09b2df06294bd`.

The earlier ordinary intro-state comparison still failed: one of six complete
checkpoints matched, despite an exact captured image. Controlled EFB input
replay matched all six checkpoints and the image, but does not resolve that
ordinary-state failure. The loaded ABBA workload and timing gates also failed;
the 70.7% reduction in logical vertex bytes is not a proven speedup. All original
receipts and failed attempts are preserved, with no upgraded qualification
claim based on the user's subjective report.

No installed executable or player data is replaced by this source integration.
The separate playtest build and its copied saves remain available. The next
complete build must refresh every consumer of the changed layout headers; the
earlier eighteen-TU host build documents that scope. Tingle rescue wait-skip
remains disabled, and unrelated optimizer work is outside this commit.

The earlier compact-vertex contribution by elliotttate is credited in the
experiment's [credits](../../patches/experiments/selective-vertices/CREDITS.md).
