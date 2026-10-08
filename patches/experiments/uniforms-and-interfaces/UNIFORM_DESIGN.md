# Private sparse vertex uniforms on current selective vertices

This adaptation is inactive. `DOL_GX_SPARSE_UNIFORMS=1` is the sole opt-in;
unset, empty, 0 and 11 keep the full uniform binding. Current selective
vertex storage, HUD certification, integer TEV/fog semantics and shader
interfaces remain intact. Pixel constants are unchanged.

The current `VertexShaderConstants` still contains every logical field and
2832 bytes, reordered into a 656-byte draw bank, 1536-byte XF matrix bank and
640-byte light bank. Specialized shaders upload only leading consumed bank
prefixes; canonical ubershaders require all three complete banks. The sparse
group-1 layout has three dynamic uniform bindings, while full control has
one. Config version 16 and `sparseUniforms` distinguish both persisted
pipeline layouts, including four destination-alpha/ubershader identities.
Early selective-pipeline admission includes the same uniform-layout bit.

The actual shared `stage_vertex_uniforms` helper handles ordinary and
interpolated staging. It de-duplicates each bank against its previous owned
packet range, grows/shrinks prefix requirements conservatively, and clears
repeat-match evidence after partial failure. Whole range equality compares
all three offsets and the presence marker. Prefix shape is determined by
the same pipeline already required for draw fusion. Capacity includes two
additional alignment gaps and existing finish headroom. A staging segment
refreshes the packet ID before new bank cache lookup and canonical identity
commit; ranges never inherit the old packet ID.

`VertexConstantsIdentity` preserves full canonical equality for interpolation.
Without interpolation, only nonzero stable constants IDs authorize repeat
shortcuts; bank caches otherwise compare consumed prefixes. Off-mode draws
invalidate snapshot admission without copying or comparing the complete
2832-byte snapshot. Enabling interpolation requires a new canonical snapshot;
only successful staging commits identity. The authored fixture checks these
off/on, id-zero/different/repeated and failure transitions using the actual
production helper.

`common.hpp` changes its `InterpRanges` uniform ABI. This requires the honest
expanded consumer rebuild; borrowing the former 18-TU subset would be wrong.
No parallel sidecar storage was introduced to hide that dependency.

Preparation is chronological: `prepare_initial.py` applies matching v4
hunks and retains all rejected contexts; `adapt.py` rebases the contexts to
the current selective ABI, followed by the documented packet-ID correction.
The final `sparse-uniforms-current.patch` and source receipt are authoritative.
Do not claim the chronological initial scripts independently regenerate the
final overlay. Final source reproduction can apply the final patch to the
pinned originals; no older fusion or fixed-60-byte decoder source is imported.

The implementation retains elliotttate's human donor credit for commit
`160224821dbef6d57fd1df4536a92b050885be10`, adapted through the existing
GPL-3.0-or-later sparse v4 draft. Existing GPL/SPDX source notices are retained.
The K7 templates supplied architectural ideas only; no retail shader source,
DXBC, game assets or proprietary executable code is copied here.

Qualification is pending. Prior v4 fixture/GPU results guide coverage, but
are not current-overlay results. `uniform_use` still scans configured texgens
per draw; caching that recipe is an unmeasured follow-up. Reduced staged bytes
do not establish lower CPU time, better frame rate or full gameplay parity.


Source2 repairs a source1 audit defect: a successful VS-bank stage followed
by failed PS staging must invalidate bank repeat shortcuts before returning.
Otherwise canonical identity still names A while bank caches hold B, and a
subsequent repeated A could reuse B. The actual invalidation helper now serves
both partial VS failure and the post-VS PS guard; fixtures exercise A/B/fail/A
with both private-byte and staged-byte cache comparison. Source1 is retained
as a negative candidate, including its first authored runtime failure.
That runtime failure used an authored TEV count up to16 against the current
8-entry key array. Source2's fixture caps the shape count with the actual
`kMaxTevStages`; it does not broaden production limits.
