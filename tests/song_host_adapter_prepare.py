"""Read-only extraction of production host seams for public synthetic tests.

No ROM/module/state/card input. The optional privately generated Hr caller is
never extracted or copied by this tool and is excluded from public defaults.
"""
from pathlib import Path
import argparse, hashlib, json, re
p=argparse.ArgumentParser();p.add_argument('--main',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
source=a.main.read_text();a.output.mkdir(parents=True,exist_ok=True)
def function(name):
    match=re.search(r'^static [^\n]*\b'+re.escape(name)+r'\s*\(',source,re.M)
    if not match:raise ValueError('Required production seam missing: '+name)
    opening=source.index('{',match.start());at=opening+1;depth=1
    while depth:
        depth+=(source[at]=='{')-(source[at]=='}');at+=1
    return source[match.start():at]+'\n'
files={}
files['song_host_bridge.inc']='static BwSongHostAdapter g_song_host;\n'+'\n'.join(function(n) for n in
    ['host_song_alias_generation','host_song_fixed_backing','host_song_project_slots','host_song_owner_query','host_song_owner_revoke','host_song_owner_bind'])
files['song_native_loader.inc']='\n'.join(function(n) for n in
    ['host_remove_rel_aliases','host_rel_slot_is_live','host_find_rel_scratch','host_materialize_rel','host_zero_rel_bss','host_rel_section_linked_start','host_register_rel_alias'])
start=source.index('        if (cpu.pc == 0x80240744u) {');end=source.index('        if (cpu.pc == 0x81E000D4u',start)
files['song_native_materializer_hle.inc']=source[start:end]
reset=function('host_enhancement_reset');start=reset.index('    // The event epoch already cancels pending native calls on CARD load.');end=reset.index('    bluewake_quick_items_reset',start)
files['song_native_reset.inc']=reset[start:end]
load=function('host_state_load');start=load.index('    // FORCE may load a different game translation;');end=load.index('    g_overlap_cached_alias_state',start)
files['song_native_STATE_eligibility.inc']=load[start:end]
for name,text in files.items():(a.output/name).write_text(text)
receipt=dict(main=str(a.main.resolve()),main_sha256=hashlib.sha256(a.main.read_bytes()).hexdigest(),no_private_inputs=True,
    seams={n:hashlib.sha256(t.encode()).hexdigest() for n,t in files.items()})
(a.output/'source.json').write_text(json.dumps(receipt,indent=2)+'\n')
