# Certified chassis boundaries, V3 (inactive)

`host-certified-edges-v3.patch` is an experimental source overlay, not an
active patch or part of the installed tester. Patch SHA-256:
`3fb7212400263253662cc4ee78f5633da9d15873674a9152562332e798b1e446`.
Git normalizes this patch to LF. The preserved private CRLF draft has SHA-256
`4a681f51a28ea3193c1fcb00a3e055a5e9d3549fc601689ef01fdac5607ac451`;
only line endings differ.

After `bw_edge_unwatched` proves a miss in the static watch list, a separately
versioned callback checks dynamic reasons to observe the boundary. Ordinary
direct calls and native replacements retain the full V2 predicate. Registration
requires the exact CPU ABI/size and ordered 1,085-key list; mismatches or
re-registration revoke V3. Dynamic returns, reward LR checks, overlap/alias
changes, raw module aliases, IRQs, diagnostics, budget/depth/exception guards,
and gather drains remain. Finite-filter hits and complete particle/wake ranges
fall back to full V2. New dynamic observer or range behavior requires review
even if its keys already occur in the static list.

The audited private host is `52478dbd` and module is `0386a4b8`. Exactly two
host objects and three module support objects change; all 832 translated/other
direct objects are retained. O3 and ASan fixtures each pass 13,322,973 checks.
Six complete intro guest checkpoints and the full P6 intro image match the
`f3198b53`/`976184c6` control. This is bounded correctness evidence, not a
full-game or continuous-state guarantee. The known asynchronous GX-count
variation remains recorded. PDB inspection confirms the old renderer counters
still share their flag's cache line, at the same within-line offsets; this
candidate does not contain the separately accepted counter isolation.

Private evidence is under `build/performance-focus-20261007/certified-edge1/`:
`final-qualification-v3.json`, `draft-promotion-v1/draft-manifest.json`,
`draft-promotion-v1/watch-generation-audit.json`, and
`draft-promotion-v1/patch-check-v1.json`. The normal scanner over the proposed
overlay produces the exact frozen list and identical `bw_edge_watch.inc`.
`git apply --check` passes against the audited working tree, including its
preexisting builder edits. The patch adds only the new header dependencies
to that builder file; it does not incorporate those unrelated edits.

Normal builder invalidation may regenerate and rebuild translated chunks.
That result needs separate qualification from the private three-object swap.
Any future promotion must also qualify a build containing both V3 and the
accepted counter-isolation patch. No such combined-build claim is made here.

Four serialized warm ABBA runs average CPU -1.716%, wall -2.193% and cycles
-0.969%. The CPU pairs disagree (+0.345% and -3.788%); wall pairs are -0.293%
and -4.113%. This fails the predeclared requirement that both pairs improve
CPU and wall time, with mean improvements of at least 1% in both. Decision:
**NO_CLEAR_ROUTE_GAIN**, inactive. No further unchanged-candidate run or broad
rebuild is required for this verdict. Timing receipt:
`build/performance-focus-20261007/certified-timing-observation-v1.json`, SHA-256
`958344b73784b52f9d7dd30f7079203b9455600a35e81c7faee930879b91eee7`.
