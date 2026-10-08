"""Refresh the fixed text-only allowlist; never compile, launch or install."""
from pathlib import Path
import datetime, hashlib, json

ROOT = Path(__file__).resolve().parent.parent
allowed = json.loads((ROOT/'allowlist.json').read_text(encoding='utf-8'))['files']
assert len(set(allowed)) == len(allowed)
assert {p.relative_to(ROOT).as_posix() for p in ROOT.rglob('*') if p.is_file()} == set(allowed)
rows = []
for name in sorted(set(allowed)-{'manifest.json'}):
    path = ROOT/name
    assert path.resolve().is_relative_to(ROOT.resolve()) and not path.is_symlink()
    data = path.read_bytes()
    assert path.suffix.lower() in {'.md','.json','.py','.c','.cpp','.h','.hpp','.inc','.patch'} or name.startswith('licenses/')
    assert not data.startswith((b'MZ',b'!<arch>',b'DXBC',b'DXIL',b'PK\x03\x04'))
    data.decode('utf-8-sig')
    rows.append({'path':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
value = {'status':'EXPERIMENTAL_SOURCE_ONLY_HANDOFF',
         'snapshot_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'files':rows}
(ROOT/'manifest.json').write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'files':len(rows),'manifest_sha256':hashlib.sha256((ROOT/'manifest.json').read_bytes()).hexdigest()}))
