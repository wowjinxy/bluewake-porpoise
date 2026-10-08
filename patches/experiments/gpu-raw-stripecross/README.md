# Inactive GPU raw vertices and StripeCross experiment

This source experiment remains inactive. No reliable speedup was established,
and normal-setting guest-state differences remain unresolved. The active patch
list, installed tester and player data are unchanged. This folder contains no
binaries, disc data, saves or generated translated game code.

The GPU path decodes big-endian FIFO F32 XYZ/ST in the vertex shader, using the
existing vertex/storage buffer. The CPU StripeCross companion retains all 36
guest FP operations and replaces ten FIFO store paths with one exact 40-byte
emission or existing-buffer append. It skips no simulation, particle callback,
axis update or guest basis store. It does not move particle simulation/corner
math to the GPU.

The selected sources are renderer v3, `root-host5` and the pinned `module976` native
adapter/preparer, based on BlueWake `2c9dd7de8947b5fcda98f7086454f51905bb12d5`.
The independent opt-ins default off. Source patches are supplied for review and
future experiments; they are not registered in patches/recompcore/active.json,
and the normal composite builder does not enable this adapter.

Read [RESULTS.md](RESULTS.md) first: it retains the failed v2 cached restart,
the v3 fix, normal guest-state failures, diagnostic EFB-input equality and seven
descriptive measurements under substantial background load. That evidence does
not qualify activation, an installation or a performance tester release.

`sources/` contains the selected edited source files; `patches/` contains three
prospective root-relative patches. [INTEGRATION.md](INTEGRATION.md) documents
ABI, admission, fallback, full header-consumer rebuild and future builder work.
[VALIDATION.md](VALIDATION.md) contains fixture coverage and recorded command
recipes; [CACHE-RESTART.md](CACHE-RESTART.md) explains the startup bug.

The owner must supply exact pinned `module976` inputs for the private hook/oracle
preparer. Generated translation/oracle output is deliberately absent here.
`manifest.json` pins every included file. Read-only bundle verification:

```powershell
python tools/verify_handoff.py
```

Source-only verification does not build or run the game. Earlier drafts, failed
attempts and full diagnostic receipts remain retained in the ignored build
folder; their paths/hashes are recorded in evidence/native-and-loaded-results.json.
