# Actor-name lookup reuse — inactive item 14

Adapted from **Elliott Tate's** actor-name search at app commit
`944a1f3c2b130a8086295428a0847938a777a2d7`. The implementation baseline is the
current individually certified search/judge; the donor's unversioned batched
host walk is omitted.

`actor-name-search-reuse.patch` adds a bounded thread-local SearchName memo,
enabled only by `BLUEWAKE_SEARCH_NAME_REUSE=1`. Undefined/zero keeps the current
path. Every hit calls the actual versioned host-observation predicate anew and
checks CPU/RAM identity, alias generation, the preserved register/suffix key,
available cycles, stack disjointness, and every previously read table/name
byte. Sixteen hits expire an entry. List/node/actor/filter/parameter reads,
observed helper boundaries, guest cycles and transactional declines stay live.
The roughly 11 KiB memo contains copied data, not borrowed guest pointers.

No SDK, builder, module or host opt-in is activated. The current ordinary
host actor-ID optimization owns the watched JudgeFilter boundary; its dynamic
predicate normally declines this reuse. Any later integration must explicitly
review that ownership. This candidate does not batch the outer actor list,
prove real-scene reuse, or establish a gameplay speedup. Full read-set byte
revalidation adds host work.

## Actual qualification

Private results are retained under `build/performance-item14-actor-search-v1`.
The exact candidate and uncached reference plus six real CPU TUs passed O3 and
AddressSanitizer runs. Each mode reported:

- 80,042 whole-CPU/complete-4-MiB-RAM comparisons; 1,693 unchanged declines;
  all 825 name-table positions plus absent, and ten actor-list positions.
- Steady sequence: 69,384 avoided host lookup simulations and 4,956 actual
  simulations; 72,370 total hits and 358,050,220 revalidated bytes overall.
- Original translated-call oracle: 60,000 authored protected-RAM cases,
  38,747 identical accepted calls, 21,253 unchanged declines, zero mismatches;
  38,747 warm hits, 41,715 fresh lookups and 196,565,068 checked bytes.
- Actual preparation certifier: all six original search entries accepted;
  altered judge source and an interior watch rejected.

The translated oracle executes only the selected function on authored RAM;
it never starts a game. The original translated DLL is **uninstrumented**.
The ASan candidate, uncached helper and six CPU TUs are instrumented, with
non-authored translated-oracle RAM protected. The portable recipe below covers
the self-contained CPU/RAM fixture, not that separately retained oracle or
game-source certificate inputs.

Attempts 1–4 remain preserved. Two stopped on include/source-layout recipe
errors. The next two exposed a fixture physical alias pointing into the
candidate's RAM while the reference used a different RAM image. The final
fixture remaps that one physical alias per execution; ordinary aliases retain
steady generations. Candidate production logic did not change for these fixes.

## Reproduce the authored fixture

`actor_name_search_reuse_test.c` is byte-identical to the qualified fixture.
The file-only recipe verifies the patch/preimage/fixture and exact current
versioned configure/ready excerpt, then prepares a fresh source tree. It does
not modify the checkout, invoke a compiler or load a game module.

```sh
python patches/compiler/drafts/actor_name_search_reuse_recipe.py \
  --repo . --runtime ref/recompcore --out build/actor-reuse-sources
cmake -S build/actor-reuse-sources -B build/actor-reuse-o3 -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release
cmake --build build/actor-reuse-o3
ctest --test-dir build/actor-reuse-o3 --output-on-failure
cmake -S build/actor-reuse-sources -B build/actor-reuse-asan -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DREUSE_ASAN=ON
cmake --build build/actor-reuse-asan
ctest --test-dir build/actor-reuse-asan --output-on-failure
```

Use a configured Clang toolchain with SDK/CRT headers and libraries. Windows
uses the dynamic CRT and shared ASan runtime; ensure their matching DLLs are
available to the fixture. POSIX links libm. The CMake recipe itself has not
been rerun as a separate qualification; the actual results above use the
retained direct-tool recipe. No game/assets/original translated binary are
needed for this portable fixture.

## Pins

| Artifact | SHA-256 |
|---|---|
| Candidate patch | `ee403bcd3e2458bae1419fec4896e3443df501e76f9916db76bfe333a0a84a79` |
| Patched search | `f19a5e24a6e15ea52a2301bf135d229436a63a64b0da083eeb297a45fc8cfa4c` |
| Authored fixture | `c70c62144d9b064b6ad4d4b2bb2cba5e85df78656cbd3c2659da73a7ee3a610c` |
| Actual O3/ASan result | `33570dfdc5059ebc722c25b0085f081205cf52e816abbeb39981291ae1113e67` |
| Preparation certificate | `c72c61bcb0b07a7f9499507f4a48bbead01d0da842c75e25c2372ff6a4673b6b` |
| Private summary | `e0facb38ef2fb1d04cfa8775127cdb1b67e2ba60470658ed24ba89efb60c35c0` |
