"""Summarize retained receipts only. Never compile, launch or copy captures."""
from pathlib import Path
import hashlib
import json
import re

OUT=Path(__file__).resolve().parent.parent
PRIVATE=OUT.parent
NATIVE=PRIVATE/'native1'
FREEZE='996b05bee581cbbc9b8e031101b246433f1f89f00e121e5ef2a38202981038ed'

def pin(path):
    path=Path(path)
    return {'path':str(path.resolve()),'bytes':path.stat().st_size,
            'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}

def write(name,value):
    (OUT/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')

names=['abba-net1.json','strict-cache-abba1.json','normal-comparison1.json',
       'efb-full-repeat1.json','efb-selective-comparison1.json','normal-selective/result.json']
receipts={name:pin(NATIVE/name) for name in names}
data={name:json.loads((NATIVE/name).read_text()) for name in names}
normal=data['normal-comparison1.json']
assert normal['matching_checkpoints']==1 and normal['full_p6_equal']
assert normal['status']=='FAIL_PRESERVED_NATIVE_STATE_OR_PIXEL_DIFFERENCES'
controlled=[]
for name in ['efb-full-repeat1.json','efb-selective-comparison1.json']:
    value=data[name]
    assert value['matching_checkpoints']==6 and value['full_p6_equal'] and not value['timing_eligible']
    controlled.append({'receipt':receipts[name],'status':value['status'],
                       'matching_checkpoints':6,'total_checkpoints':6,'full_p6_equal':True,
                       'timing_eligible':False,'normal_gameplay_qualification':False,
                       'efb_reports':[{'case':r['case'],**r['efb_report']} for r in value['rows']],
                       'limits':value['limits']})
abba=data['abba-net1.json'];cache=data['strict-cache-abba1.json']
assert abba['status']=='FAIL_ANALYSIS_GATE_PRESERVED' and not abba['promotion_qualified']
assert cache['status']=='PASS_STRICT_WARM_CACHE_GATE'
assert not abba['abba']['meets_timing_threshold']
log_path=NATIVE/'normal-selective/logs/0.log'
log=log_path.read_text(encoding='utf-8')
matches=re.findall(r'^\[gx-selective\] decoded_draws=.*$',log,re.M)
assert len(matches)==1
stats={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',matches[0])}
assert stats['decoded_full_bytes']==stats['submitted_full_bytes']==4978336440
assert stats['decoded_bytes']==stats['submitted_bytes']==1458918624
stride_line=re.search(r'^\[gx-selective\] strides (.+)$',log,re.M)
assert stride_line
strides={k:int(v) for k,v in re.findall(r'(\d+)=(\d+)',stride_line[1])}
saved=100*(1-stats['decoded_bytes']/stats['decoded_full_bytes'])
assert abs(saved-70.69465590397101)<1e-12
native={
 'status':'INACTIVE_UNQUALIFIED_V1_NATIVE_STATE_AND_TIMING_GATES_FAILED',
 'source_receipt_sha256':FREEZE,'promotion_qualified':False,
 'native_game_qualified':False,'performance_qualified':False,
 'receipts':receipts,
 'scope':{'route':'title intro','max_retraces':1800,'checkpoint_retraces':[300,600,900,1200,1500,1800],
          'hidden_noninteractive':True,'gameplay_scene_tested':False,
          'mods':['widescreen','betterww'],'unpaced':True},
 'ordinary_native':{'status':normal['status'],'matching_checkpoints':1,'total_checkpoints':6,
                    'full_p6_equal':True,'differing_retraces':[d['retrace'] for d in normal['differences']],
                    'different_fields':{str(d['retrace']):sorted(d['fields']) for d in normal['differences']},
                    'state_qualification':False,'timing_eligible':False,
                    'limits':normal['limits']},
 'controlled_actual_efb_inputs':controlled,
 'loaded_abba':{'status':abba['status'],'failure':abba['error'],
                **abba['abba'],'strict_warm_cache_status':cache['status'],
                'terminal_pipeline_creations':{r['case']:r['terminal_pipelines_made'] for r in cache['rows']},
                'new_semantic_config_counts':{r['case']:len(r['new_semantic_configs']) for r in cache['rows']},
                'cached_startup_compilations':{r['case']:r['cached_startup_compilations'] for r in cache['rows']},
                'hardware_gpu_timing_measured':False,'displayed_fps_measured':False,
                'descriptive_only':True,'limits':abba['limits']},
 'logical_vertex_accounting':{'receipt':receipts['normal-selective/result.json'],
                              'log_reference':pin(log_path),'counters':stats,'stride_draw_counts':strides,
                              'reduction_percent':saved,'timing_eligible':False,
                              'actual_gpu_bandwidth_measured':False,
                              'limits':'Logical decoder/submission accounting only; no hardware draw, bandwidth or GPU time measurement. Byte reduction is not a speedup result.'},
 'interpretation':'Ordinary native state qualification failed despite exact captured pixels. Controlled replay of recorded actual EFB read values matched six checkpoints and pixels, but does not replace the ordinary-state gate. Loaded ABBA failed exact workload equality and the timing threshold. No promotion.',
 'retained_limits':['Only title-route checkpoints and one captured frame were compared, not every instruction or whole-game gameplay.',
                    'Physical EFB readbacks still execute in controlled diagnostics; recorded values are substituted only after metadata validation. Those runs are excluded from timings.',
                    'Earlier broad interpolation ASan baseline failure remains unresolved; focused capture passes do not resolve it.',
                    'No hardware GPU execution timing or displayed FPS was measured.'],
}
preservation_path=PRIVATE/'final-preservation1.json'
preservation=json.loads(preservation_path.read_text())
assert pin(preservation_path)['sha256']=='fde73b24ae6331c6ef5145697fc41a7bf836cbc47ff8ec984bbac708fa4691b6'
assert all(preservation['originals_preserved'].values())
native['preservation']={
 'receipt':pin(preservation_path),'status':preservation['status'],
 'originals_preserved':preservation['originals_preserved'],
 'inventory_file_counts':{k:len(preservation['fresh_originals'][k]['files']) for k in ['installed','player','k7']},
 'foreign_file_count':len(preservation['fresh_originals']['foreign']),
 'independently_verified_evidence_files':len(preservation['verified_evidence_files']),
 'source_commit':preservation['source_commit'],
 'source_commit_unchanged':preservation['source_commit_unchanged'],
 'git_status_equal_to_baseline':preservation['git_status_equal_to_baseline'],
 'promotion_qualified':False,
 'limits':'Preservation audit only. Original private file inventories are referenced by receipt hash, not copied into this bundle.',
}
write('evidence/native-results.json',native)
static=json.loads((OUT/'evidence/static-qualification.json').read_text())
static['status']='PASS_BOUNDED_STATIC_CPU_GPU_HOST_AND_HELPER_QUALIFICATION'
static['native_game_qualified']=False;static['performance_qualified']=False
static['qualification_boundary']='Static and synthetic passes remain valid; completed native/timing results are separately inactive and unqualified.'
write('evidence/static-qualification.json',static)
print(json.dumps({'status':native['status'],'logical_vertex_reduction_percent':saved,'native_summary':pin(OUT/'evidence/native-results.json')}))
