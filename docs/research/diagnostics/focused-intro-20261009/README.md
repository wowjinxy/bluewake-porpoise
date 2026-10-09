# Focused intro diagnostic source checkpoint

These templates preserve the exact Python source used for the private
2026-10-09 focused intro capture. They contain no translated game code, game
assets, player data, captured process memory or executable binaries.

The `.py.in` files are archival source, not a standalone profiling installation.
Their original location is
`build/deep-debug-20261008/focused-peak-profile1/`; the filenames and byte hashes
are recorded in `source-manifest.json`. Paths are relative to that location.
They depend on the retained private diagnostic baseline, qualified host and
module, and player-owned inputs named by the source preparation. Restore only
to an isolated copy of that private workspace. The runner rejects changed
inputs and existing case directories. It does not operate an existing game.

The sampler adds a raw QPC bracket for every captured instruction pointer,
outside the existing suspend/resume scope. The already qualified host records
per-retrace QPC, thread CPU time and thread cycles. Both clocks' frequencies
must match. The runner owns its new process in a kill-on-close Job before
resuming it, disables live controller/mouse input, copies writable player/cache
files and stops after 1,800 retraces. Sampling is diagnostic and perturbs timing.

Root-owned case `title-focused-c2-1` exited normally and drained. All 35 route,
clock, capture and preservation checks passed. It emitted 1,801 per-retrace rows
with no timer errors and created no pipelines. Raw memory captures, player data
and binaries remain private and ignored. No speedup is established.

The copied tester directory can contain its older PDB. Offline host naming must
explicitly select the qualified `2b9bc61e...` executable and its adjacent
`a8ef9413...` PDB from the source preparation, rather than that copied PDB.

The follow-up archive also preserves `prepare_names1.py`, `name_windows1.py`
and both passive naming audits. All 354 unique host lookup requests resolved
in both naming views. The successful audit reconciles selected-window raw
frequencies and counts each sample once using the host innermost function.
The first audit's old-report total assertion failed because the old function
table omitted 191 system-image samples; the successor restores only their
exact module counts. Both sources are retained. These scripts require the
private accepted inputs and do not contain those inputs.
