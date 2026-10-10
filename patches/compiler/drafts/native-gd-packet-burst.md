# Guarded display-list packet burst experiment, October 9

Status: inactive archive. Fixture qualification: PASS. Runtime test: PASS for the controlled title route below. Performance: strict FAIL; closed and inactive. This is experimental source, with no production CMake target or automatic installation.

The patch adds only `experiments/gd-packet-burst/native_gd.c`, `native_gd.h`, and `native_gd_test.c`. It contains no translated game source or generated chunk. Applying it does not activate the optimization. The existing negative GroundCross LLVM experiment remains inactive and separate.

## Scope and behavior

The helper replaces the unpaid 64-cycle suffix at guest address `0x802D82C8` in GZLE01's TevKColor routine. The private routing experiment preserves the original prologue, color loads/packing, overflow check and optional overflow callback before reaching that suffix. The original precise and prepaid bodies remain the fallback. A successful helper rejoins the original chunk's central return dispatcher; it does not replace the surrounding dispatcher or its budget checks.

The suffix emits ten bytes: `61`, the big-endian word in r31, `61`, then the big-endian word in r30. The guarded implementation removes repeated cursor reload/publication and byte-access checks. It accepts only the qualified fixed 32 MiB MEM1 policy, with fresh alias-overlap, journal, reservation, exception, cycle-budget and deadline checks. The SDA pointer cell, object, saved-register frame and complete output span must be in raw MEM1 and pairwise disjoint. Mirrored, aliased, MMIO, overlapping, journaled and reserved cases decline. Failed helper admission leaves the CPU and guest memory unchanged; diagnostic decline counters can increase.

Success charges exactly 64 guest cycles and reconstructs every modified GPR, LR, PC and observation suffix, including the saved-register restores and final suffix value 2. CR, XER, CTR and floating-point state are unchanged. Packet formation uses integer byte operations and preserves the host floating-point environment. The helper uses the fixed global MEM1 policy even if `CPUState.ram` has another value.

The caller must first obtain approval from the existing versioned native-entry observation predicate. `bluewake_native_gd` does not itself call the host observer. The only private routed entry is `0x802D82C8`, before its original PC metadata and precharge. Do not also place the hook inside the already-paid copy: that would charge twice. Future integration must retain the original callback approval and certify the exact skipped observer/source domain.

## Actual differential qualification

The frozen fixture completed 60,000 cases against the actual original C2 module:

- 30,010 accepted cases matched the original module dispatcher.
- 29,990 declined cases preserved their input state and writable memory.
- All 3,552 CPU-state bytes were compared for accepted cases; only the distinct RAM-image owner pointer was normalized.
- Every writable byte was compared per case: seven pages totaling 28,672 bytes. All other MEM1 pages were read-only in both images, and initial/final checks compared the complete 32 MiB images.
- Alias, alternate-owner and small MEM2 test buffers were checked for unintended changes. Host exception flags and MXCSR were checked. Boundary cases include unaligned/end-of-MEM1 spans, overlap, aliases, journals, reservations, budgets/deadlines, null/wrong entry and signed underflow limits.
- Nine C translation units, one link and one fixture execution completed with zero exit codes and owned processes drained. No game or performance measurement was part of this fixture.

The terminal marker was:

```text
GD_PACKET_DIFFERENTIAL_PASS cases=60000 accepted=30010 declined=29990 full_cpu_bytes=3552 mem1_bytes=33554432 writable_bytes=28672 readonly_outside=1 full_image_checks=2 timing_eligible=0
```

Original C2 DLL SHA-256: `541191776a1b8b7289606d98f34b294a62e71ec23a4ee9008547a1ba77ce89b2`.

Private fixture receipt SHA-256: `1b87e65a3b8213cfdae5e0b0175506aad8776d23311a3d00d3b8c92feeef2c4f`.

## Actual private runtime qualification

The linked private candidate module is 410,186,240 bytes, SHA-256 `e2cb244cc97c207dc099378726b9e5cef040406626fe967773214f4e5b4430f3`. It replaces the original C2 chunk0181 and native-entry router objects and appends one native packet provider: 836 direct objects, 833 retained existing objects, 15 physical providers and 853 LINKREPRO members. All five existing C2 replacement objects remain retained. The actual compiler dependency closures, link inputs and PE surface passed their existing gates.

The controlled title comparison completed normally for both original C2 and the private GD candidate on the same qualified host. All six complete logical CPU/MEM1/MEM2/ordered-alias checkpoint hashes and the complete captured P6 image matched. These are source-qualified logical state hashes, not retained byte-for-byte RAM dumps. The route retained its exact EFB replay and original physical reads. This is controlled-route correctness, not ordinary determinism, arbitrary-gameplay proof or every-instruction equivalence.

The candidate's actual terminal report recorded `native-gd` TevKColor admission: **367,052 accepted and 2,044 declined**. This establishes that the routed helper was exercised; it is not a hit-rate profile for another route or a speed measurement. The subsequent fixed-eight timing protocol requires positive acceptance from the owned candidate parity log before its first timing case.

| Private evidence | SHA-256 |
| --- | --- |
| Differential fixture, `gd-packet-burst1/attempt1/result.json` | `1b87e65a3b8213cfdae5e0b0175506aad8776d23311a3d00d3b8c92feeef2c4f` |
| Three-TU compile, `gd-packet-burst1/module-plan1/attempt2/result.json` | `88867b8ae8cb52ecd77360a5c2465387dc93cb5c1b7c9d3b30e4f5b81acb98a4` |
| Private module link, `gd-packet-burst1/module-plan1/link-attempt1/result.json` | `b1cef2d6f2758e2b5f23249a88b1f2abbe730b5d100c4560ce7e978b67d2209b` |
| Correctness batch, `gd-packet-runtime1/correctness-batch1.json` | `05fd476b6813c3c6c9c311f9c97ea7c3961a05dfbd8105624fd4907589366e94` |
| Six-checkpoint/P6 comparison, `gd-packet-runtime1/native-efb1/gd-packet-per-vi-input-parity1.json` | `6e3befccbe46e991d9098483fbab51a94db065116dd40d4071b738cf8f177473` |
| Independent correctness audit | `54cbf54e2f01d159361e00e2608687fe6af1f4bc7e75a7ad33c74d56bc610fc1` |
| `gd-packet-runtime1/independent-timing-audit1.json` | `7a991939ba5e1bc6de9c529d2e1c113a8976a8690545cc22873a83f0b49e254c` |

## Completed controlled timing: strict FAIL

The preregistered eight-run order was `A-G-G-A, G-A-A-G`, using four independent pairs and primary VI 750?1,500 with five 150-VI segments. All eight native cases completed and drained, passing all 34 runtime checks. The independent raw audit checked all 1,801 VI records and 1,025 calibration records per case, retaining route/input/zero-pipeline gates. Primary and all segment work and absolute endpoint cursors matched exactly: primary work was 547,802 dispatcher blocks, 6,075,000,001 guest cycles and 99 EFB replay entries. Complete terminal GX, clock and dispatch gates also matched. All eight raw cases and 1,801 per-VI records per case remain stored.

| Primary mean | Original C2 | GD candidate | Candidate change |
| --- | ---: | ---: | ---: |
| Main-thread CPU seconds | 17.87890625 | 17.59375000 | -1.594931% |
| Elapsed seconds | 18.070748800 | 17.857096575 | -1.182310% |
| Main-thread cycles | 63961519176.00 | 62883438417.25 | -1.685515% |

| Pair (control / candidate) | CPU change | Wall change | Thread-cycle change |
| --- | ---: | ---: | ---: |
| a1 / g1 | +11.881188% | +13.599754% | +11.404126% |
| a2 / g2 | -7.778738% | -7.753242% | -7.564392% |
| a3 / g3 | -2.510460% | -2.208340% | -2.503055% |
| a4 / g4 | -7.630162% | -8.023300% | -7.771786% |

Both mean reductions exceed one percent, but the first pair regresses in both CPU and wall measurements. The unchanged all-pairs consistency requirement therefore **FAILS**. The experiment is closed and inactive; there is no promotion, retry, discarded pair or rescue batch. Lower means are not a demonstrated reliable gain, and this mixed result does not establish a causal regression either.

Current background load and unresolved coarse CPU timer/calibration materiality remain limitations. No overhead was subtracted. Correlated per-VI rows are not independent trials. This controlled, unpaced, EFB-replayed title workload is not ordinary native throughput, displayed FPS, whole-game/GPU timing or shipping qualification.

Timing report: `gd-packet-runtime1/controlled-input1/gd-packet-per-vi-eight-run1.json`, SHA-256 `61794021bfef10cbc96922bab27d8ae3ad9b9ba935d818c701c5da4ce1ecf187` (7,814,901 bytes), status `FAIL_CONTROLLED_INPUT_GATE_PRESERVED`.

Enclosing batch: `gd-packet-runtime1/controlled-batch1.json`, SHA-256 `77b61d1b21ae0e27e00e5c82da306b583af204df9528490b9c73447cf5545bc4`, status `FAIL_PRESERVED`. The earlier pending and runtime-only notes remain unchanged historical snapshots.

## Manual build requirements and limits

This archive is not a standalone build system. The fixture needs the matching canonical `core/cpu.h` and `core/types.h`, `gather_pipe.h`, `module_cpu_contract.h`, `StaticRecompABI.h`, and the original runtime/header closure. Module interface ABI 5 and canonical CPU storage ABI 6 are distinct contracts.

The qualified Windows build used x64 Clang 19 with GNU C11, `-O2 -march=x86-64-v3 -ffp-contract=off`, the MSVC dynamic CRT, and these fixed-memory defines:

```text
BW_GUEST_MEM1=bw_guest_mem1
BW_GUEST_MEM1_SIZE=0x02000000u
DOLRECOMP_CPU_HEADER="core/cpu.h"
```

Compile the two C files in this archive with the six matching CPU units (`cpu.c`, `cpu_exception.c`, `cpu_interpreter.c`, `cpu_interpreter_float.c`, `cpu_interpreter_integer.c`, `cpu_interpreter_table.c`) and the real `gather_pipe.c`. Do not add `guest_cpu.c`: the fixture defines its own aligned fixed MEM1 array. Include the archive directory, the matching composite headers, GXRuntime include directory and StaticRecomp ABI directory. Link the real MSVC runtime and Windows libraries. No dummy callbacks or success stubs replace canonical runtime providers.

After producing the fixture executable, its interface is:

```text
native_gd_test.exe ORIGINAL_C2_MODULE.dll 60000
```

It loads the original module only inside the bounded fixture process. Use the exact original DLL hash above. A manually rebuilt or changed header/runtime closure needs its own qualification; this document does not claim interchangeable binaries. The private owned runner, generated game code and module routing recipe are intentionally outside this archive.

## Preservation and next gate

Inspect with `git apply --stat native-gd-packet-burst.patch` and `git apply --check native-gd-packet-burst.patch`. Apply with `git apply native-gd-packet-burst.patch`; reverse with `git apply -R native-gd-packet-burst.patch`. The patch adds three inactive files and changes no existing production file.

Controlled title runtime test: PASS as bounded above. Measured performance: strict FAIL. This version remains inactive. Any future pipeline integration requires a distinct justified experiment and a measured gain under a preregistered comparison suitable for that change. Fixture success alone is not gameplay, performance or production-promotion evidence. Preserve this experiment and any negative result rather than silently enabling it or discarding failed measurements.

## Archived source identity

| File | SHA-256 |
| --- | --- |
| `native_gd.c` | `7b67ac7f78f3975ac85a8e32a27f8a0fca88bc694af7b1d0248fa944809b85e0` |
| `native_gd.h` | `9ffe041749b85e12fe345c64ffbc4c7c35235b067aadc02f033cd6865de03834` |
| `native_gd_test.c` | `7fb34402ebb0fc6450f49e9f9508944e34b0f7701be599555cc8c78bc1eb0ece` |
