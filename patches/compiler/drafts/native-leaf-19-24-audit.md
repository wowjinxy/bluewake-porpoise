# Items19–24 current-entry audit

Scope: current `cmake/composite/native_game_math.c:5148`/native_entries.c and
their actual preparation certifiers, against Elliott Tate app donor944a1f3c.
No matching donor entrypoint was already implemented in the current composite.
Similar names in native_game_math/native_vec/native_mtx/native_bg are different
functions and must not be counted as these imports. Existing game-math oracle
and source-guard tests are reusable infrastructure, not evidence for missing entries.

| Item | Exact donor entries | Disposition and useful next work |
|---|---|---|
|19 libm/RNG|8032EC1C __ieee754_fmod;80330E34 fmod;80246044 cM_rad2s;802462C8 cM_rnd;802463B0 cM_rndF;802463E8 cM_rndFX;80330C84 sin;8033071C cos;80330D5C tan|All9 missing. Retain exact translation. Importing host libm/approximate trig is not an equivalent replacement. A future exact replay must include RNG state bytes, overflow/NaN/signed-zero/rounding, callees and budgets.|
|20 background bounds|80247C4C MakeBlckMinMax;80247CD4 MakeBlckBnd|MinMax now has the inactive finite-only CPU-qualified candidate; Bnd remains missing and translated. Bnd needs loop-dependent bound plus exact TransMinMax/PSVECAdd state and rollback. No full-item completion claim.|
|21 quaternion|80301150 JMAEulerToQuat|Now has a separate inactive CPU-qualified quaternion candidate (native-quaternion.patch). One50-cycle block uses actual SDA sine/cosine tables, three signed-angle indices,14fmuls and4sums. No live module or native acceptance is implied.|
|22 game atan|802460D0 cM_atan2s with U_GetAtanTable|Missing. Preserve octants/signed zero,1025-entry bounds, exact division/fctiwz and guest FP state; do not substitute atan2/libm. Donor cM_atan2f was deliberately dropped for failing repeatable1.25x benefit; cLib_addCalc/addCalc2 are rewritten60Hz simulation sites and remain translated.|
|23 geometry/wind|8024A6F0 cM3d_CalcPla;80254214 cSPolar::Val;8008A230 dKyw_pntwind_get_info|All3 missing. Existing xyz arithmetic/box/cylinder/clip entries differ. Need callee closure, vector normalization exceptions, polar angle conversion and30-slot wind iteration; no safe blanket import.|
|24 JAS|8028DF2C TOscillator::getOffset;8028E238 TOscillator::calc;8028C3A8 TChannel::updateEffectorParam|All3 missing. Needs oscillator mutable state/table, interval/fp2unsigned, mixer/effect/pan/autosine and DSP buffer callee/alias/budget closure. Host preview/audio-device tests are irrelevant; future oracle may call authored data only.|

All six donor groups rely on generated leaf bodies and native_leaf_run.h.
That runner's lf_silent lines111–114 caches bw_host_quiet once for the entire run,
then only consults the static watch list. It cannot be copied into BlueWake's
versioned dynamic observation contract. A safe replay must query the actual
current host predicate at every skipped observable boundary with the CPU state
the translation would expose, or decline; CPU rollback alone is insufficient.
This is why the broad donor leaf import is declined as written. It is an
implementation gap, not evidence that these optimizations are impossible or that
their donor speed claims have been reproduced here.

Current useful test routes: tests/native_game_math_test.c (original-module,
protected authored RAM/full CPU), tests/native_game_math_guard_test.c (guarded
decline), tests/native_game_math_source_test.py + actual native_game_math.py
certify/fragment machinery (changed-body/mod/watch rejection). The isolated
item20 tests use those actual CPU/contract/certifier paths and do not rewrite
the original translation oracle. No native game/GPU/audio/controller execution.
