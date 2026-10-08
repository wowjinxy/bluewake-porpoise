# Selective vertex input source handoff

This completed v1 experiment remains inactive and unqualified for promotion.
Ordinary native state comparison failed; loaded ABBA timing failed workload
equality and the timing threshold. It contains source only and no tester build.

The ordinary title-intro route matched only **1 of 6 state checkpoints**, while
the captured frame was byte-exact. Controlled replay of 1,134 recorded actual
EFB inputs matched all six checkpoints and the frame for full and selective
layouts. Physical readbacks still ran and differed on 12 full-layout and 72
selective replay entries. This controlled diagnostic does not resolve the
ordinary state failure or establish gameplay equivalence.

Loaded ABBA descriptive means changed CPU time by **-0.9876%**, wall time by
**-1.4699%**, and process cycles by **+0.3056%**. Candidate runs executed eight
more submitted/planned draws and sixteen more no-ops. The strict warm-cache gate
passed, but the unequal workload and failed timing threshold prevent a speedup
claim. No hardware GPU execution timing or displayed FPS was measured.

Logical decoded/submitted bytes fell from 4,978,336,440 to 1,458,918,624,
**70.7%**. That diagnostic accounting is separate from hardware
bandwidth and elapsed performance. It does not establish a speedup.

The eleven files under `sources/` exactly match frozen source receipt
`996b05bee581cbbc9b8e031101b246433f1f89f00e121e5ef2a38202981038ed`.
SDK paths are rooted at `sources/ref/recompcore/`; the host change is rooted at
`sources/runtime/`. The two patches remain inactive; apply them only against the
exact originals recorded in `bindings/source-receipt.json` and verified SDK/root
HEADs in `bindings/baseline-bindings.json`. This bundle never modifies active.json.

`DOL_GXCORE_SELECTIVE_VERTICES=1` alone selects packed inputs; unset, empty, `0`
and `11` retain canonical 132-byte inputs. The decoder writes packed data directly
using cached recipes, preserves both texture-matrix words and NBT cache updates,
and reconstructs canonical vertices before a cold ubershader or legacy filter.
Generated shader bodies remain unchanged. The canonical default stream is bound
once per pass only when declared inputs are absent; that cleanup is already in
this frozen source. Pipeline cache version is 15.

`evidence/static-qualification.json` gives bounded CPU/GPU/host/helper results and
exact hashes of retained receipts. `evidence/native-results.json` records the
ordinary failure, controlled-input parity and failed timing gate verbatim with
pinned receipt references. `evidence/asan-results.json` records the completed
focused decode/capture passes. The earlier broad interpolation ASan baseline
failure remains unresolved; focused capture does not replace that broader suite.
The three principal fixtures are decoder, production interpolation capture and
offscreen D3D12 pixels. The EFB record/replay helper is correctness-only and must
remain excluded from timings. No input recordings or game output are bundled.

`NEXT_PERFORMANCE.md` proposes unimplemented, unmeasured follow-ups. Its original
source locations under `compact1/overlay/sdk/` map to `sources/ref/recompcore/`
here. References under `../native1/` identify retained private evidence, not
bundled game outputs. Resolve ordinary state correctness before promotion.

Run `python -B -S tools/verify_handoff.py` for read-only allowlist and hash checks.
After editing evidence, run `python -B -S tools/refresh_manifest.py` and verify
again. Neither tool compiles, installs, launches a game or accesses a network.

Original preparers/runners are included byte-for-byte for provenance. They use
private historical paths and are not portable entry points from this directory.
See `FIXTURES.md` and the pinned prerequisite references before rebuilding.
Killer7 shader binaries, cache data and retail shader source are absent. This
adapts the general idea of transporting only relevant inputs, not retail code.

The overlay generator `tools/compact1/prepare.py` was also checked read-only
against all eleven pinned originals and reproduced the frozen candidate bytes.
Its SHA-256 is `d83d6f5cd815c6528c04e7b75873e8daebf7d95c2fa1a46ac6a1f7fcbe9015db`.
The initial handoff staging script is historical construction provenance;
completed qualification evidence supersedes its initial placeholder values.
The retained compact-draft attribution snapshot also stays verbatim; its older
pending labels describe that preceding draft, not this completed v1 experiment.

The final independent preservation audit verified 46 installed files, 450 player
files, all 13 K7 files, three foreign changes, the existing capture/analysis and
active recipe, with unchanged source HEAD/status and 81 evidence files hashed.
Only a bounded summary and receipt hash are bundled; private inventories remain
outside this handoff. These preservation checks do not qualify the candidate.
