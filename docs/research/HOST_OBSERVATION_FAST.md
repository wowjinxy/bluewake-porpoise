# Certified host observation shortcut

Direct translated calls ask the host whether an observation boundary is needed.
This query runs extremely often. The shortcut returns `true` only when the
complete original callback would also return `true`; every other case takes
that original callback. It changes neither guest instructions nor guest timing.

The fast predicate admits aligned, raw main-code addresses outside a static
union of all observer addresses and ranges. It still reads the current tracing,
interrupt, pending-return, overlap, and raw REL state. A bucket collision or
uncertain state selects the complete callback. The static union includes the
complete true-address domains of the six finite observer families, so a finite
Bloom-filter collision does not require running those observers.

`game_event_observation_view.h` exposes the actual event owner, subscription
mask, and pending-return buckets through one typed, process-lifetime object.
Main borrows its address before registering the callback. Attach, reset,
subscription, and cancellation update the existing fields in place. Shutdown
resets events and clears the borrowed pointer before freeing CPU storage. Only
the storage address is retained; observation answers and field values are read
fresh. This relies on the existing game-thread event API contract.

The CMake option `BLUEWAKE_ENABLE_MAINCODE_OBSERVATION_FAST` requests the
shortcut. A generated certificate enables it only for the admitted Windows
x86-64 Release profile and matching reviewed sources. Developer tracing, edge
census, native inventory collection, and native reward builds retain the full
callback. Builds without a certificate also retain that callback.

Both CMake entry points refresh the certificate before building the host. A
source/profile mismatch, missing Python, or checker failure writes certification
zero. The generated header is an explicit dependency of main's object; edits
after configuration therefore cannot leave an old enabled object in use.
Unchanged certificate contents preserve the header timestamp to avoid an
unnecessary rebuild. The checker normalizes CRLF to LF and compares every other
byte exactly.

The certificate is a reviewed contract for these sources, rather than a parser
that proves arbitrary future C predicates. Whole-file pins are intentionally
conservative: even an unrelated edit to a pinned file disables the shortcut.
For a source change, review all true-address domains and live guards, regenerate
the static table, update the certificate, and repeat implication, lifecycle,
state/pixel, and timing qualification. Updating hashes alone does not establish
that a changed observer is covered.

## Qualification on 2026-10-09

The actual public control and candidate were built from two fresh host objects
each, retaining the same other 168 link inputs and the same game module. Their
imports, resources, stack contract, and dependency closures passed review. The
candidate's fast callback is a leaf with no calls or stack use. The complete
predicate and chassis retain the control's native instructions; the fallback
only loses two argument moves that its internal call no longer needs.

The public O2 and ASan fixtures each passed 842,606 implication queries, 1,598
complete RAM comparisons, 206 grouped RAM checks, pointer lifecycle checks, and finite
bucket collisions. Default-off and inventory-collector fixtures took the full
fallback. The CMake wiring harness verified certificate enablement, unchanged
incremental builds, disabling and rebuilding after source drift, and fallback
when the option was disabled or Python or the checker was unavailable.

Two fresh controlled intro runs matched all six complete CPU/MEM1/MEM2/ordered
alias checkpoints and every byte of the final 1,228,335-byte P6 image. Both used
the same 1,134 recorded EFB inputs after retaining the physical GPU reads.

The subsequent fixed eight runs used A-G-G-A,G-A-A-G under existing machine
load. The primary window was retraces 750 through 1500. Its 547,802 blocks,
6,075,000,001 guest cycles, 99 EFB entries, and all five segment workloads and
cursors matched exactly. All four paired CPU and wall changes were negative;
the weakest pair improved 1.203% CPU and 1.070% wall time.

| Primary-window mean | Control | Candidate | Change |
| --- | ---: | ---: | ---: |
| Game-thread CPU seconds | 17.50390625 | 16.02343750 | -8.458% |
| Wall seconds | 18.112820325 | 16.238327650 | -10.349% |

These are controlled-input diagnostic measurements. They do not establish
ordinary native throughput, displayed FPS, GPU time, or whole-game behavior.
Per-VI timer hooks remain in both arms; CPU calibration resolution and full
hook overhead remain unresolved, and no overhead was subtracted. Individual VI
rows are correlated, not independent samples. An ordinary throughput comparison
and crowded gameplay qualification are still pending.

The raw ordinary control and candidate subsequently linked with the same 168
retained providers. The candidate consumed the enabled header generated by the
real CMake certificate target; no certificate answer was supplied by a compiler
flag. Removing the diagnostic hooks removed exactly four diagnostic imports,
with no additions. The private verifier accepted CMake's Windows CRLF output
after normalizing only CRLF to LF and retained the raw generated header.

The ordinary candidate then completed a visible 1,800-retrace intro with the
same C2 game module, copied player files, warmed cache, real clock, FIFO display,
audio, and SmoothMotion disabled. All 30 route and preservation checks passed;
there were no pipeline compilations or rejected/failed GX submissions. Native
stack sampling covered retraces 793 through 1509. This was a diagnostic run,
not a throughput comparison: the heavier intro still fell to 33.9 retraces
per second. Sampling overhead prevents treating that rate as a clean
speed measurement or as evidence that the remaining slowdown is fixed.

Raw evidence is preserved locally under
`build/deep-debug-20261008/host-maincode-negative1/`:

| Evidence | SHA-256 |
| --- | --- |
| Public fixture | `69e800754147de83637d298df074cb7c0044be7b6cd1fb6c1bff08d443f5d758` |
| CMake wiring harness | `ec1bea0b63f8dd6a1f039fa139731d5fd7f088816912ed879608958a5689fe1f` |
| Public two-host build | `52f6f90cdbfcdead52fb4adf79c13ece912a1ddc147a42f2149edc36da1884e3` |
| Six-state/pixel comparison | `68254f04ac3f7b9bb5c6c93d4f9ebad788d80ef85563f83ba683e8f7ef7e2afa` |
| Fixed eight-run comparison | `387c7d067751867e1af0a99f23e09f36509e7024c7d56eb9b4667cac90f32394` |
| Independent raw timing audit | `c0d478acca97b95d10b23629fb224efd83a14bea2ea3c334611ea1381da1897c` |
| Ordinary raw two-host build | `01dcf7b3185a5de134b47ef697e1cd8db3f31627b962d13ad6a7393ff1e0ed05` |

The ordinary visible diagnostic is preserved separately at
`build/deep-debug-20261008/ordinary-public-profile1/ordinary-public-candidate-intro1/`.
Its `result.json` SHA-256 is
`0c41d418065cd751e8a498fd4b2c1afc41b3b78209bae52b089e0f9e2de1e309`.
