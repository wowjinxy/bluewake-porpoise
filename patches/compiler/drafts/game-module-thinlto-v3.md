# Game-module ThinLTO â€” default-off item 37

Adapted from **Douglas Whittingham's** DolRecomp change
`1ef0fcd60a491ba9b191dcaf94fd9419009c93c1`. This draft adds a separate,
**default-off** Windows C game-module option. It changes neither app PGO/ThinLTO
nor the prepared C backend or ordinary optimization defaults.

`game-module-thinlto-v3.patch` preserves the reviewed integration changes, now merged into the public builder:

- Builder `--module-thinlto` / `--no-module-thinlto`, default false, forwarded
  as `BLUEWAKE_MODULE_THINLTO=ON/OFF` for both final and local-training modules.
- A small CMake helper: Windows Clang, existing shared target, no external
  native DOL objects; compile and link `-flto=thin`, link `-fuse-ld=lld` and
  `/OPT:LLDLTOJOBS=1`. Repeated inclusion/application is idempotent. OFF returns
  before platform/target checks and adds no flags.
- The option enters the generated-source input identity, preparation receipt
  and reuse selection, local-training fingerprint, and packaged provenance.
  Helper bytes enter preparation and training identity. Existing profile-use
  and instrumentation flags remain intact.

The patch is against the retained dirty builder preimage; it does not replace
other agents' builder work. The reviewed hunks were merged deliberately; unrelated builder work remains intact.
`game-module-thinlto-tests.patch` proposes seven normal Builder unit tests and
a standalone authored C DLL/executable project under `tests/module_thinlto`.
The public option is available and remains off by default; this preserved patch is not automatically applied.

## Actual results

Private evidence: `build/performance-item37-game-lto-v1/summary.json`.
Genuine Clang 19.1.5, CMake/Ninja and the qualified Windows dynamic-CRT toolchain
completed fourteen bounded roles in a single small authored project:

1. OFF: two actual COFF objects, DLL link, 100,000 cross-TU comparisons.
2. ON: both module objects rebuilt as actual LLVM bitcode, DLL link,
   100,000 identical comparisons and all nine explicit named exports retained.
3. Repeated ON: Ninja reported no work; both object hashes and timestamps
   stayed identical; another 100,000 comparisons passed.
4. OFF again: both objects rebuilt as COFF; another 100,000 comparisons passed.

The two public test symbols and seven synthetic host-resolved CPU/alias exports
were identical in all four PE export tables. Those functions are authored
stand-ins; the proof establishes linker export retention, not real guest alias
or host authority. DLL sizes were 9,728 / 9,216 / 9,216 / 9,728 bytes; these tiny
fixture sizes are not a game-module size or speed prediction. The normal
`-O2 -ffp-contract=off` flags remained unchanged. The helper was applied twice
and emitted each ThinLTO flag once.

Eight private Builder tests passed: seven exported semantic tests plus an
AST comparison proving all unrelated Builder methods, including app
configuration, unchanged. Four additional CMake NONE-project guard probes
correctly refused missing target, wrong target kind, external object and wrong
compiler. Default-OFF/unsupported and ON/unsupported guards also behaved as
expected. No full translated game module, local game training, gameplay,
GPU/device or performance test ran. This item has no ASan qualification claim.

Preserved negatives: `actual1` compiler-file Windows path escaping;
`actual2` an unintended Debug compiler probe requesting the absent debug CRT;
`actual3` a test-only CMake-P guard missing the CMP0077 minimum (the four mini
builds already passed); the first unit-test attempt omitted the real digest
helper from its authored source root; and an exact guard-message check
overlooked CMake line wrapping. Fresh recipe-only corrections passed; original
failures and source preimages remain in the private evidence directory.

## Portable test route after deliberate integration

```sh
python -m unittest discover -s tests -p test_windows_module_thinlto.py -v
cmake -S tests/module_thinlto -B build/module-thinlto-probe -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DBLUEWAKE_MODULE_THINLTO=ON
cmake --build build/module-thinlto-probe --verbose
```

Run `build/module-thinlto-probe/fixture.exe` with matching dynamic CRTs available.
Switch ON/OFF in the same small cache to inspect actual command-driven rebuilds.
This is Windows-specific and requires Clang/lld-link plus configured SDK/CRT
headers and libraries. The seven exported builder tests passed against the integrated public builder.
Fourteen training tests and six real float-option CMake configurations also passed.
The executed private mini-project proof is pinned below. No disc or game assets are needed for either fixture.

| Artifact | SHA-256 |
|---|---|
| Integration draft | `0fb5c152f4e47a6272d8f7cc975cffc7d1cd5d956d64bc98d49543c71bae02ee` |
| Test draft | `d62c998627b52b51ab9d42b338551eb939d30ce79394db1ab4da3a044839dfa7` |
| CMake helper | `40618fd42afa027c0be4eb55e4dec14307a9a450a05094c66f3f702feb88b883` |
| Actual mini result | `38fc32c604624497fb2dae9597fc6d71754f6085ed20c7c52027958f13f1c77a` |
| Private Builder test source | `856769c33cfba8fb2106a4acbaa5730cadab366eb9ec9ed2ba3e886db1432d08` |
| Four guard results | `7929a9cf811aa342d0f5f4e20200276af001c49b542b5d904fb27d1e2a465d86` |
| Summary | `346f415c235e236a0738d4e57ded658a9093c5125f1a8bb0b4441c5fb6d74637` |

Final V3 changes only the proposed provenance field indentation from the
executed private candidate. Its entire Builder AST is identical; the old
source/draft and one preparation-only substring-count assertion are retained.
