# Cache fallback dispatch, inactive item 29

Adapted from Elliott Tate's `scripts/windows/cache_ops.py` at app commit
`944a1f3c2b130a8086295428a0847938a777a2d7`. The donor replaces four cache
fallbacks with a PC increment. That replacement remains declined: our gather
wrapper drains pending FIFO bytes, and the current instruction callback can
observe or change CPU state, RAM, exceptions, metadata and future callbacks.

`cache-callback-dispatch.patch` adds an explicit `--enable` preparer and a
small inline dispatcher. It removes only the runtime trampoline for exact
`dcbst`, `dcbf`, `dcbi` and `icbi` emitted fallback/return sites. It drains the
FIFO, then reads and invokes the current callback. The NULL path calls the
actual runtime's `ppc_program_exception` with its exact existing illegal-
instruction arguments. A flush can replace the callback or queue more bytes;
there is no second drain. Other fallbacks and every surrounding instruction
watch, cycle charge, suffix, return and dispatch boundary remain byte-exact.
It does not substitute the interpreter's different cache-control API.

No builder option, SDK/host callback registration or generated game module
currently selects this patch. Omitted `--enable` performs no preparation.
The preparer requires exact runtime `cpu.c` and gather-header SHA-256 pins,
LF fallback-plus-return templates, and one generated header include; unknown
templates/helper macros decline before writes. It changes only four call
names and adds the helper include. Repeating preparation preserves bytes and
mtimes. It checks all chunks before publishing owned preparation outputs.

Before future integration, run this on a private prepared module copy with
gather-pipe preparation already present, before final prepared-source and
native certification hashes. Include the option, helper/preparer/runtime
identities and final chunk hashes in preparation, training and provenance
identities. Those builder hunks are deliberately absent from this export.
The exact source pins are intentionally restrictive; changed runtime/wrapper
contracts require renewed review. Never cache a quiet-host answer or use the
donor PC-only rewrite as a fallback.

The focused actual receipt is
`build/performance-item29-cache-callback-v1/actual2/result.json`, SHA-256
`15d2823e2ebe1e71871460b589a4e4ec287c6cb6eda593bed3298f34b9100f36`.
Twenty-one hidden CPU-only roles passed: eight preparation tests, then O3 and
ASan builds of the fixture, actual gather source and all six CPU sources.
Each fixture compared 32,000 complete CPU/RAM/FIFO and FP-environment results:
29,536 reference runtime trampoline entries versus zero candidate entries,
with every callback and delivered/pending byte preserved. This count measures
removed dispatch entries; it is not a timing or gameplay performance result.

Coverage includes all four operations, arbitrary register bits and both PR
states, custom/NULL/mutating callbacks, callback replacement during flush,
flush-generated pending bytes, PC overflow, watch/budget refusal, signaling/
quiet NaN register payloads, and all four host rounding modes with preexisting
FP exception flags. The observer/cache-control counters, PC/suffix/cycles/
deadlines, full CPU bytes and RAM remain equal. The runtime trampoline entry
counter is fixture instrumentation; the fallback implementation is genuine.
All six CPU TUs, gather source and fixture are ASan-instrumented. CRT/system
libraries remain prebuilt. No game, GPU, physical input or device was run.

A later additive source-macro refusal and public default-helper path update
passed nine preparation tests; the compiled helper/fixture stayed exact.
Exact earlier preparer/test bytes are retained as `actual2-*` files, with
their original receipt hashes verified. The first test attempt is preserved:
its authored Windows `write_text` generated CRLF and correctly hit the LF
guard. Only the fixture's byte writer changed before the fresh passing run.
Existing CPU deprecation warnings are retained; fixture/ASan stderr is empty.

`cache-callback-dispatch-tests.patch` exports the exact authored C fixture,
nine preparation tests and a standalone CMake recipe. After applying both
patches, run `python -B tests/test_windows_cache_callbacks.py`. A CPU-only
portable build uses `cmake -S tests/cache_callbacks -B <fresh-output>` then
`cmake --build <fresh-output>` and `ctest --test-dir <fresh-output>`; use
`-DBLUEWAKE_TEST_ASAN=ON` in another fresh output for ASan. Windows requires a
normal configured Clang/MSVC SDK and matching CRT/ASan DLL deployment. The
portable CMake recipe was source-reviewed, not separately run; the retained
actual direct-tool commands are in `actual2/commands.json`.

The reviewed helper/preparer and exported tests are now available in the public tree. All nine preparation tests passed after integration. The normal builder does not select this experiment; it remains explicit and no game module has been regenerated with it.
