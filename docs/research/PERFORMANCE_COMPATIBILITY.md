# Larger performance leads: compatibility findings

Source review on October 6, 2026. These findings supplement items 25–34 in
[the checklist](PERFORMANCE_TEST_CHECKLIST.md). They are not completed native
performance qualifications. Existing source, failed experiments and diagnostic
receipts remain preserved. The checklist separately records an older debug-PDB
overwrite; that lost file is an exception to earlier preservation statements.
No backend, presentation path or simulation timing was changed by this review.

| Items | Finding | Work required before promotion |
| --- | --- | --- |
| 25–26 | Persistent GPU geometry is a distinct mechanism from the existing inactive CPU geometry cache. The latter reused about 57% of vertices but increased median CPU about 3%, with no reliable rendered gain. Donor sampled list/pointer identities cannot establish unchanged guest inputs. | Track the complete display-list and array read footprint, every alias/write, retained buffer ownership, eviction, queued-frame readers and reset. Measure a changed mechanism rather than repeating the same CPU cache. Transform batching depends on this lifetime and identity work. |
| 27 | The retail backend installs `ConsumingAuroraRenderSink` and consumes translated FIFO packets. The donor descriptor-setter optimization targets native SDK setters. | Establish that those setters are called and costly on the selected route. Preserve shadow/decoded agreement, direct CP writes, NRM/NBT aliases and recording boundaries. Existing Swiss maps provide no separate new import. |
| 28 | A narrow current-C projection has a real machine-code effect. For an authored 16-ADDI block, CPU-r3 memory operands fall 19→2, while O3 object size grows 2,217→2,315 bytes. O3/ASan each passed 264,800 checks over 262,144 full-CPU cases, including 1,632 resumed calls. | Extend the effect/flush model to callbacks, memory aliases, FP faults, native/mod hooks and observation boundaries; measure runtime cost. This projection does not import the donor scheduler and is not a general residency implementation. |
| 30 | The donor pass is explicit offline block-local DolIR tooling. Normal C/LLVM generation does not call it. Its effect model informs the current-C projection in 28. | Integrate with a selected generation path and demonstrate improved machine code plus complete state/observer parity. Fewer IR operations alone do not establish a gain. |
| 31 | The newer ModernGekko work changes generated entry/runtime types, reload-state masks, wrappers, memory services and patch ABI. The frozen commit changes 33 files. | Treat this as a separate backend migration. Establish scheduler, CPU layout, memory/FP, module exports and current versioned hook compatibility before comparing the same game routes. No current prepared-C option can safely select it as a drop-in patch. |
| 32 | The frozen “single-pass” implementation is stereo/VR: `GXSetStereo`, eye projection/view records, side-by-side render targets and per-eye replay. Its EFB screenshot branch explicitly selects the left half. | It needs a stereo integration and retail frontend mapping. No mono Wind Waker gain was established. Preserve required mono EFB/depth copies. |
| 33 | The donor finishes/submits scene commands before acquiring a surface image in Fifo/FifoRelaxed modes, then creates a presentation encoder. Our hidden qualification route deliberately acquires no surface image. | Measure visible acquire waits and the extra encoder/submit cost, then verify callback retirement, queue ordering, resize/occlusion and each present mode. Offscreen results cannot qualify this benefit. |
| 34 | The donor combines a bounded 1 ms `WaitAny` retirement path with native command-processor vertex-layout precomputation. Our adapter already requests TimedWaitAny, but the worker/readback organization differs and the donor frame/map-tracker files are absent. | Identify a corresponding hot completion or native-layout path before adapting code. Verify callback completion, teardown/join order, FIFO layout and retained buffer readers. Feature availability alone is not a completed optimization. |

The item 28/30 source and machine-code assessment is preserved in
`build/performance-28-30-assessment-20261006/`. Its first assessment receipt is
`result.json`, SHA-256
`9845ada4dc55cfdea17d3bd46afb0e74f1217f846ecd09788eb70120113f8a88`.
The later pure-block parity receipt is
`build/performance-13-known-single-20261006/residency-proof-v1/result.json`,
SHA-256 `5c9d81e69745f83017767e024af322c2ebfc76710d3a430f5f7f70ddd30d07d3`.
The initial assessment's “not executed yet” limitation remains historical;
the later bounded proof does not qualify the broader observer cases.

Primary frozen sources:

- [Offline state-access pass](https://github.com/chrissotraidis/DolRecomp/blob/acb8e7b472f9a000ee98fbc52033f536c5e4bf4c/docs/STATE-ACCESS-PASS.md).
- [ModernGekko execution/patch migration](https://github.com/ExpansionPak/DolRecomp/commit/c876e2b9e022e084aa9a690fe15e2ad79a0706cd).
- [Stereo single-pass implementation](https://github.com/danieltobey/aurora/commit/dc9d1841fb2eb5370f43036bb5049a6570f0ce23).
- [Scene submission before acquisition](https://github.com/shibbo/aurora/commit/6d88d468985bba3aeba3dce92acdcff87ad9d227).
- [Completion and vertex-format changes](https://github.com/Frityet/aurora/commit/870f700098ae5dd381623a9bcff44205cb7ba003).

The implementation records for 19/23/24 are separate from this backend review.
Finite fmod and oscillator calc now have exact optimized/ASan candidate proofs.
Plane has optional builder wiring, but its original-stage certificate needed
correction and permanent watched interior PC 80328F40 still prevents admission.
Oscillator's permanent save/restore interior watches also prevent admission.
These results do not authorize removing observers or substituting a later-stage
source certificate. Ordinary tester builds retain those translated paths;
none of this backend review enables them.
