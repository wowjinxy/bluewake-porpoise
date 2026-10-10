# Cold precise twin RAM experiment, October 9

Status: inactive source archive. Synthetic differential qualification PASS;
one-owner controlled-route correctness PASS. Broader gameplay and performance
remain unqualified. Nothing here is
registered in the production builder, CMake or workflows. Applying the patch
does not enable this transform. The older negative lean-memory experiment and
the closed LLVM/GD experiments remain inactive.

## Mechanism

The transform recognizes only translated integer 8/16/32-bit accesses with a
pure effective-address temporary, one access, and an optional update-register
write. It changes the existing paid fast copy. Each direct RAM operation uses
the original fresh gather guard; stores also require no journal and no valid
reservation. A failed guard publishes the exact committed PC/suffix and jumps
to the original current precise part while the block remains prepaid. That
tail executes the original access and remainder once. There is no slow-helper
rejoin into the fast copy, no runtime observed flag, no cached alias admission,
and no current-access or update-register write before the guard. Earlier pure
instructions may already have committed their CPU writes. Source-pending
metadata is flushed before
observers/exits. Strict deadline refunds retain the original NEXT-part target
and set prepaid false only after publishing/refunding the exact suffix.

First-part resume aliases are after the original prepaid-to-fast jump, so a
cold handoff never recharges the leader. Unknown source forms are preserved.
FP/PSQ, 64-bit, atomic and multiple-access bodies are outside this experiment.
The RAM helper requires the exact fixed 32 MiB MEM1 policy and uses the fixed
global even when CPUState.ram differs. It is not a generic RAM helper.

## Reproducing synthetic sources

From the matching BlueWake checkout after applying this patch:

```text
python -B -S experiments/cold-twin-memory/generate_fixture.py --output cold-twin-generated
```

This command generates source only. It uses the repository's synthetic source
shapes and original fast-block generator, verifies their qualified hashes after CRLF-to-LF normalization,
and emits 100 synthetic original/paid-copy/cold-twin triplets plus two mutants.
It imports only the parser from lean_memory.py; it never runs the old lean
transform. Every fast-to-cold source inverse is retained in the output. No
translated game source, player data, binary or private path is in this patch.

`transform.py` exports `transform_source(text)` and `transform_function(lines)`.
It has no command-line mutation or automatic integration. Its recognized
transformation function bodies are unchanged from the qualified prototype;
only repository discovery and the opening description were adapted.

## Actual bounded qualification

Windows x64 Clang 19, MSVC dynamic CRT, GNU C11, final O2, fixed 32 MiB MEM1;
pointer CPU and fixed CPU, each normal and ASan. All four profiles passed:

```text
COLD_TWIN_DIFFERENTIAL_PASS shapes=100 scenarios=1024 runs=307200 directed=2400 mutants=2 CPU=3552 MEM1=33554432 writable=12288 readonly=1 traces=full fenv=1 timing=0
```

Across four profiles: 1,228,800 executions, 819,200 original-reference
comparisons and eight required mutant detections. All 40 owned child rows
exited zero and drained. The real CPU/gather services were compiled into each
fixture; no memory-service simulator was substituted. Callback traces compare
complete CPU state in order, alongside final 3,552-byte CPU state, every
writable MEM1 byte, alias/MEM2/alternate-owner buffers, gather state, journal,
host fenv and MXCSR. The rest of MEM1 is read-only with full before/after scans.

Cases include guard misses after a committed store, inherited/prior-pure PC,
deadlines/refunds, update-register aliases, mutable callbacks, journals,
reservations, raw/mirrored/aliased RAM, external MMIO, and unsupported shapes.
Two setup defects were repaired before execution: store-value-as-address
shapes now constrain their seed to writable memory, and null/128-byte alternate
RAM owners have truthful sizes. Operation bodies were not weakened.

Evidence SHA-256:

- Four-profile terminal receipt: `1512932084da900318b0089a0785aed51bbc7d0660dc7ba4e30ae23a0dead313`.
- Independent actual audit: `7ec45855e755690f8256961c6055c12feea8ef64697ca68d4880699aec0cc341`.
- Final source preparation: `53da1fccd96b30e315b6f1c2eb1d019714bf99ab280deeaa4f1fef46d8f24ad4`.
- Original canonical CPU header: `96304098a5ba52708eb760c96a8f9fe3b0a83e83f3c212f691b961f3e7ee29af`.
- Original gather header: `465903449d7e1a651aeff335ffadc32aa380bb05cff5cc42415b767a5092f57b`.

The synthetic result alone does not establish actual translated-chunk execution,
concurrent observer safety, all alias priorities, FIFO coverage or gameplay.
The tested MMIO address is external MMIO, not the GX FIFO address. Loop-first
forms with unrecognized entry layout decline. Budgets avoid undefined signed
overflow. ASan covers the C fixture and services. No timing was measured.

One private exact-source compile showed fewer instructions/reloads at a
three-load anchor but increased whole-chunk text by 7.19%; another two-load
anchor was nearly unchanged. Static counts establish no speedup. Runtime
correctness for broader source scopes, actual guard admission and strict paired
timing are required before any production proposal.

## Actual private one-owner module and controlled-route correctness

The separate private candidate replaces only original C2 chunk0145 and retains
834 existing direct objects, all five prior C2 modified objects, the dispatcher,
native/mod selection, exports and 15 physical providers. It has 835 direct
objects and 852 LINKREPRO members. Actual original-policy M/MD had 49 inputs:
the original 48 plus the experiment header. The link passed exact PE imports,
exports, resources and provider checks. No translated source or module is
included in this public patch.

The private candidate is 410,236,416 bytes, SHA-256
`f4991c5b3043fb1563ff525a2b99a3e51a7c459ac11989ae185b46b59eed8ad6`;
original C2 is
`541191776a1b8b7289606d98f34b294a62e71ec23a4ee9008547a1ba77ce89b2`.
Both controlled title-route cases used the same diagnostic host with v3 off,
1,800 VI and the same 1,134 recorded physical EFB inputs. All 32 route checks
passed in each arm, both owned game children completed and drained, and the
comparison passed. All six complete logical CPU/MEM1/MEM2/ordered-alias
checkpoint hashes matched, as did the full 1,228,335-byte P6 image. Raw 1,801-VI
work cursors and terminal GX/clock/dispatch matched; terminal PIPE was zero.
These are source-qualified complete logical state hashes, not retained RAM
byte dumps. The physical EFB reads remained part of the protocol.

| Private evidence | SHA-256 |
| --- | --- |
| One-owner compile | `d39bd4d69f23fbc3b6dc12feb50ca8dd934edc905c86b863e1d66f88092f1f95` |
| One-owner module link | `240718dde2914a11cd3d75e63009220876baad58ef61b3c213982c14d13a5cd7` |
| Correctness batch | `a197320d522111398681f149a62918a18f6983c3f620b6aa7babe9cf965e9a71` |
| Six-state/P6 comparison | `579434621f8fbe990d88283584dc686db597c1d8fa24b4fb4fd273299d304416` |
| Independent raw correctness audit | `cb6a2449a77df3c92878e03b410050450ef80a1fbce0def187145a705deb8045` |

This proves controlled-route correctness for the one-owner candidate only.
It does not dynamically identify every RAM guard, qualify a ten-owner expansion,
establish ordinary determinism, arbitrary gameplay, displayed FPS or speedup.
No performance measurement has qualified this archive and no production
activation is included. Older archival snapshots remain unchanged.

## Manual compilation requirements

This is not a standalone build system. Compile generated fixture.c with the
matching canonical core/cpu.h and core/types.h, gather_pipe.h and its original
header closure, plus real cpu.c, cpu_exception.c and gather_pipe.c. Do not link
a second guest_cpu provider: the fixture defines bw_guest_cpu and bw_guest_mem1.
Canonical CPU storage ABI 6 (3552 bytes) is distinct from module interface ABI.
Changed public runtime headers/providers require requalification.

The qualified compile defines were:

```text
BW_GUEST_MEM1=bw_guest_mem1
BW_GUEST_MEM1_SIZE=0x02000000u
```

The fixed-CPU arms require both `FIXTURE_FIXED_CPU=1` and
`BLUEWAKE_FIXED_CPU=1`; omit both for the pointer-CPU arms. The matching ordered
compiler policy used `-march=x86-64-v3 -fno-slp-vectorize -mllvm
-large-interval-freq-threshold=10 -O3 -DNDEBUG -std=gnu11`, final `-O2`, and
`-ffp-contract=off`; no effective no-SLP claim is made from that ordering.
The fixture undefines NDEBUG so its checks remain active. ASan arms add
`-fsanitize=address -fno-omit-frame-pointer` and matching runtime libraries.
Windows VirtualProtect is required; use the real platform/CRT include and
library environment. This archive intentionally supplies no automatic
compiler/launcher/provider selection or production activation.
