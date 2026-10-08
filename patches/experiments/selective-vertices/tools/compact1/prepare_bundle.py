from pathlib import Path
import json,hashlib,difflib,subprocess,datetime
H=Path(__file__).resolve().parent;R=H.parents[2];SDK=R/'ref/recompcore'
d=json.loads((H/'source-receipt.json').read_text());patches={'sdk':[],'root':[]}
for r in d['files']:
 a=Path(r['source']);b=Path(r['candidate'])
 assert hashlib.sha256(a.read_bytes()).hexdigest()==r['source_sha256']
 assert hashlib.sha256(b.read_bytes()).hexdigest()==r['candidate_sha256']
 rel=r['relative'];part='root'if r['root']else'sdk'
 old=a.read_text(encoding='utf-8').splitlines(keepends=True);new=b.read_text(encoding='utf-8').splitlines(keepends=True)
 patches[part].append('diff --git a/'+rel+' b/'+rel+'\n'+''.join(difflib.unified_diff(old,new,fromfile='a/'+rel,tofile='b/'+rel,n=3)))
for part,rows in patches.items():(H/(part+'-selective-vertices.patch')).write_text(''.join(rows),encoding='utf-8',newline='\n')
def rec(p):return dict(path=str(p),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
checks=[]
for part,p in [('sdk',SDK),('root',R)]:
 r=subprocess.run(['git','-C',str(p),'apply','--check',str(H/(part+'-selective-vertices.patch'))],capture_output=True,text=True)
 checks.append(dict(part=part,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr));assert r.returncode==0
bindings=dict(created_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),status='PRIVATE_INACTIVE_APPLYCHECK_PASS',sdk_head=subprocess.check_output(['git','-C',str(SDK),'rev-parse','HEAD'],text=True).strip(),root_head=subprocess.check_output(['git','-C',str(R),'rev-parse','HEAD'],text=True).strip(),active_manifest=rec(R/'patches/recompcore/active.json'),original_compact_draft=rec(R/'patches/recompcore/drafts/compact-vertices.patch'),source_receipt=rec(H/'source-receipt.json'),patches=[rec(H/(p+'-selective-vertices.patch'))for p in patches],applychecks=checks,negative_experiments_unchanged=True,installed_game_changed=False,game_module_changed=False)
(H/'baseline-bindings.json').write_text(json.dumps(bindings,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(status=bindings['status'],source_receipt=bindings['source_receipt'],patches=bindings['patches'])))
