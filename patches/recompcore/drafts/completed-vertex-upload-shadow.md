# Item 04: inactive vertex-upload candidate

Based on Elliott Tate's commit `39dcacb2e72546bb41b6eaf25b73aa0382742381`.
The patch targets canonical SDK tree `6297ec48f4da8bda4d25d767e49a1d10b079f1c5`
(0152–0162). No live SDK, active manifest, builder, or public source changed.

The completed GPU image determines which aligned vertex ranges need copying.
Original CPU staging appends and padding remain unchanged. The helper compares
only complete words within an immutable recorded prefix; it always copies a
partial final word and copies merged gaps from the original staging buffer.
It accepts arbitrary byte ranges, including current 132-byte and proposed
60-byte vertex strides.

Every recorded frame reserves the destination allocation. A frame recorded
behind any outstanding reservation uses full copies. Its immutable copy image
is captured after all appends and becomes known only after the original encoder
was submitted and successful queue completion covers the last interpolated
reader. Completion retains the actual buffer, applies images in submission
order, and rejects old allocation generations. Cancellation, failed completion,
unreported submission and resource mismatch invalidate knowledge. External
callbacks using the old uncorrelated submission hook decline the cache safely.

Actual O3 and AddressSanitizer runs each passed 69,063 checks and 35,760 complete
16-KiB buffer comparisons. Cases include partial alignment neighbors, 1024-byte
gap merging, different queued frame contents, out-of-order callbacks, cancellation
before/after submission, failed/partial work, staging reuse, allocation replacement,
concurrent completion, and a strong resource pin through the final reader.
The two real caller translation units (`common.cpp` and `aurora.cpp`) also compile
with the retained Clang 19.44-compatible SDK flags and exact Dawn callback API.
There was one existing unhandled `Rml` enum warning in `common.cpp`.

Measured authored upload counts for 120 identical frames:

| Sequence | Original bytes | Candidate bytes |
| --- | ---: | ---: |
| Serial, complete each frame | 1,440,000 | 12,000 |
| Continuous two-frame overlap | 1,440,000 | 1,440,000 |

The overlapping sequence declined 119 reservations and saved no uploads. Retain
this candidate inactive until real GPU measurement establishes useful opportunities.
It also adds a 24-MiB byte shadow, 6-MiB word-validity table, and temporary owned
copy images; CPU staging copies remain. No FPS, real GPU, visual/game parity,
real device-loss, or actual callback-scheduling result is claimed. Composition
with the compact-vertex patch remains unqualified.

`candidate-final.patch` was applied to fresh raw canonical source blobs and its
four resulting files hash-match the compiled candidate. `summary.json` records
the source, tool, actual dependency and result paths. The authored fixture is
`vertex_upload_shadow_test.cpp`, using the exact exported helper header.
`run.py` gives the six compile/link/test commands and hidden bounded launcher;
use a fresh output directory for another authorized run.

The original failed fixture and a caller recipe missing its donor INCLUDE
environment remain preserved. An initial patch-byte check also remains preserved:
it compared CRLF archive/worktree materialization against LF candidate blobs.
The final check uses actual raw canonical blobs with autocrlf disabled. None of
these failures involved a GPU or native game run.
