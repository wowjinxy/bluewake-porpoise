# Version 3 cached native restart regression

The first native warmup of renderer v2 ran 1,800 VI with 39829 raw submissions and
zero GX plan rejections. A subsequent startup from its newly written v14 cache
crashed in D3D12 `CreateRenderPipeline` with `E_INVALIDARG` before guest execution.
The same crash occurred with an unmerged candidate cache, so combining legacy
and candidate Dawn keys is not required to trigger it. Both failed cases remain
preserved under native1/both-checkpoint1 and both-checkpoint-unmerged1.

Startup reconstructs the same raw shader/layout as live pipeline creation, but
pipeline_cache.cpp also queues an ubershader sibling for each loaded color
pipeline. It copied `rawPosUv`=1 into `depthOnly`=2, producing decoded ubershader
WGSL with zero vertex-buffer layouts. Its `CHECK` is compiled out under `NDEBUG`.
The ordinary live find_pipeline path does not queue this sibling, explaining
why the initial warmup succeeded. D3D12 defaults to ubershader mode 1.

Version 3 adds only `|| config.rawPosUv != 0u` to `queue_ubershader_state`'s early
return condition. All six renderer v2 sources and the `PipelineConfig` version 14 ABI remain
unchanged. Runtime already materializes raw plans into decoded vertices before
any forced/unready ubershader fallback; its uber block additionally requires
!plan.gpu_raw_pos_uv. Thus raw cached pipelines must not queue decoded siblings.

Version 3's actual cached native restart passed (`both-checkpoint2`, host `50865627...`).
It rendered 39,926 raw plans with 0 fallback and an exact captured P6 match.
Normal guest-state differences remain unqualified; see RESULTS.md. No trivial
predicate-mirroring fixture or performance claim substitutes for those checks. Version 2 source/receipts/`handoff1` and the first
CRLF-heavy v3 patch remain preserved. The compact patch reuses exact v2 bytes
plus one normalized pipeline_cache hunk; it does not alter frozen source bytes.
