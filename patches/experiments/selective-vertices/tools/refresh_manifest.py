"""Refresh hashes for the fixed source-only allowlist; never build or launch."""
from pathlib import Path
import datetime
import hashlib
import json
ROOT=Path(__file__).resolve().parent.parent
allowed=json.loads((ROOT/'allowlist.json').read_text())['files']
assert {p.relative_to(ROOT).as_posix() for p in ROOT.rglob('*') if p.is_file()}==set(allowed)
rows=[]
for name in sorted(set(allowed)-{'manifest.json'}):
    p=ROOT/name;data=p.read_bytes()
    assert p.resolve().is_relative_to(ROOT.resolve()) and not p.is_symlink()
    assert p.suffix.lower() in {'.md','.json','.py','.c','.cpp','.h','.hpp','.inc','.patch'} or name.startswith('licenses/')
    assert not data.startswith((b'MZ',b'!<arch>',b'DXBC',b'DXIL',b'PK\x03\x04'))
    data.decode('utf-8-sig')
    rows.append({'path':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
native=json.loads((ROOT/'evidence/native-results.json').read_text())
value={'status':'INACTIVE_UNQUALIFIED_SOURCE_ONLY_HANDOFF','snapshot_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
       'source_receipt_sha256':'996b05bee581cbbc9b8e031101b246433f1f89f00e121e5ef2a38202981038ed',
       'native_results_status':native['status'],'files':rows}
(ROOT/'manifest.json').write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'manifest_sha256':hashlib.sha256((ROOT/'manifest.json').read_bytes()).hexdigest(),'files':len(rows)}))
