# Cached pipeline startup fence draft

This unapplied diagnostic patch adds an opt-in wait for cached pipeline workers
to finish their original notification, counter, prune and log epilogue before
guest startup. It addresses a source-proven readiness gap: an empty queue or
pending set can precede completion of that epilogue.

Set `AURORA_WAIT_CACHED_PIPELINES=1` to request the fence. It admits only the
native threaded D3D12 path, rejects an unclosed previous lifetime, and fails
closed on stop, generation change, cache failure, invalid pipeline, accounting
failure or a 120-second deadline. Default behavior keeps the existing startup
order. Worker callbacks and the existing counter/prune/log order are retained.

The successful summary reports loaded entries, completed worker epilogues,
creations, queued work and the current prune-pending flag. Successful preload
does not guarantee that a later shader state will hit the cache. The existing
late publication of the prune flag is a separate issue and is not repaired by
this draft; successful database persistence is unqualified.

The patch is relative to `ref/recompcore`. Its current-tree application check
passes with:

```powershell
git -C ref/recompcore apply --no-index --check ../../patches/recompcore/drafts/pipeline-preload-fence.patch
```

CPU tests compile the actual shared fence header. Optimized and ASan profiles
each pass 534 assertions, covering all 512 lifetime-admission combinations,
empty caches, completion before wait, the dequeued/map-removal gaps, spurious
wakes, multiple workers, closed loading, stop, cache/invalid-pipeline failure,
generation changes, accounting underflow and timeout. All four deliberate
mutations fail behaviorally: early finish, ignored in-flight work, ignored
loading closure and ignored generation changes. All 24 tool/test children exit
as expected and drain. The supported worker path finishes each tracked ticket
once; this is not a general duplicate-ticket detector.

Private evidence is retained under
`build/deep-debug-20261008/pipeline-preload1/`:

- `source2/source-receipt.json`: `6337ddf7f0765529c86b377e69edd18afdb9f8fd7a1f610f7b58935f950a81f9`
- `fixture-attempt5/result.json`: `a2bd6b475ebdeca312dd97fa2f8995ee4269938989ca8bfecc4b483c6e84ea51`

Earlier CRT/STL linker setup failures remain preserved separately. No test
annotation was disabled. Production compilation, game startup, steady-scene
performance and cache persistence remain unqualified. This infrastructure is
independent of gameplay milestones and tester builds.
