# Busy-scene CPU work, October 7, 2026

The player's 01:01:56–01:04:09 CDT session identifies game-thread pressure:
92 of 121 watched seconds were below target, all classified `game-thread`.
The scene hint was `sea_T`, room 44, with lowest reported speed 48% and no
new pipelines. In 91 of those 92 samples the game thread was at least 90%
busy; graphics waits were small. Direct calls and the existing native
families were enabled. Lowering resolution would not address this measured
bottleneck. The original log remains private; SHA-256
`68930c3f82fc0dec74d8ce00d5a94d733a34966b2a686778ebfb44a35e54aba4`.

`bluewake_game_events_observes` previously scanned all 16 pending calls on
every ordinary optimized-call query. A conservative 256-bit return-address
filter now rejects impossible matches first. Successful new arms add a bit;
clearing every pending slot clears the filter. Individual cancellation may
leave stale bits, and collisions still run the original live-slot predicate.
No observation answer, guest state, dynamic callback or return is cached.

The existing event suite and a direct comparison with the previous predicate
passed under O3 and ASan. Each mode checked 5,646,341 cases, including arbitrary
32-bit returns, unaligned/mirrored addresses, shared and nested returns,
overflow, replay, collisions, saturation, cancellation, reset and callbacks
that change subscriptions. The fixture also verifies CPU/RAM preservation.
The initial fixture-only callback precondition failure remains preserved.

Only `game_events.c` was rebuilt into the ordinary host. Shipped main, controls,
renderer and translated module were retained. The new host is
`a629104974ce26cc74a01deca20f83f37f14dfafd8fa515ffc0b8bbc7abbbfd3`;
the module remains
`976184c642ac87fdf026f181e90c1a1c3875bbbb833aad2d62174ca71dfc0916`.
Actual dependency/link closure, imports, manifest, previous-output preservation
and 93 output-path checks passed.

Four serialized runs used the same copied native CARD, Dragon Roost route,
3,300 retraces, immutable warm-cache seed and affinity mask 65535 applied before
resume. They used hidden rendering, no physical input/audio/presentation,
interpolation, captures, machine-state load/save, sampling or shader compilation.
No owned compiler or second game overlapped measurement.

| Run order | Original host wall / CPU seconds | Filter host wall / CPU seconds |
| --- | --- | --- |
| Original, filter | 86.172 / 119.469 | 73.219 / 104.219 |
| Filter, original | 89.110 / 120.109 | 82.969 / 113.313 |
| Mean | 87.641 / 119.789 | 78.094 / 108.766 |

Mean complete-process route time fell **10.89%**, CPU time **9.20%**, and cycles
**9.22%**. Both adjacent CPU comparisons improved, by 12.76% and 5.66%.
There is meaningful run variation. This is two pairs on one offscreen route,
not displayed FPS, p95 latency, full-RAM parity or full-speed gameplay everywhere.

The original raw receipts and launcher are under
`build/crowded-scene-20261007/`. The timing summary SHA-256 is
`7b4ae29bfec28fe771fbfc7568077da73ee08652191fad6820765cda98c77c09`;
the host compile/link receipt is
`8b8a584427e561a9b64d5f1a00b67019534f8da5d16ed8f182d244ff966bfabf`;
the focused O3/ASan receipt is
`aa542684fd7024804d6f282e9394e10232e9f92c8fe62394b0d940d32534bc12`.

The exact new host/module also passed a separate 3,300-retrace native Dragon
Roost run, with all 22 integration checks passing. Complete captured P6 bytes
and independently parsed loaded checkpoint context match the previous host.
The comparison receipt is `build/crowded-scene-20261007/event-host-comparison-v1.json`,
SHA-256 `f9ecc399967e6b2beb245c73a74294896823c5926ff9047e74a57fe084df887a`.
This capture/save run is excluded from timing. It does not qualify physical
controllers, audible devices, displayed presentation or a complete playthrough.

A separate exact feature-address gate passed semantic checks but regressed
22 of 24 synthetic timing cases. Both public files were restored exactly;
the candidate, assembly, tests and failures remain private under
`build/core-feature-dispatch-20261007/`. Its decision receipt SHA-256 is
`867a473c5a67e969404e7f99510afe1fca9ea7c8e680ba8ccc12f2f39ad24a83`.
No promotion follows. Fusion, other unqualified experiments and Tingle rescue
wait-skip remain disabled; the earlier checklist evidence remains intact.
