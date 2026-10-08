"""Read-only exact allowlist/hash verification; no build, install or launch."""
from pathlib import Path
import hashlib
import json
ROOT=Path(__file__).resolve().parent.parent
manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
allowed=json.loads((ROOT/'allowlist.json').read_text(encoding='utf-8'))['files']
assert len(allowed)==len(set(allowed))
assert {r['path'] for r in manifest['files']}==set(allowed)-{'manifest.json'}
actual={p.relative_to(ROOT).as_posix() for p in ROOT.rglob('*') if p.is_file()}
assert actual==set(allowed),('Unexpected or missing files',actual^set(allowed))
for row in manifest['files']:
    p=ROOT/row['path']
    assert p.resolve().is_relative_to(ROOT.resolve()) and not p.is_symlink()
    data=p.read_bytes()
    assert len(data)==row['bytes'] and hashlib.sha256(data).hexdigest()==row['sha256'],row['path']
    assert p.suffix.lower() in {'.md','.json','.py','.c','.cpp','.h','.hpp','.inc','.patch'} or row['path'].startswith('licenses/')
    assert not data.startswith((b'MZ',b'!<arch>',b'DXBC',b'DXIL',b'PK\x03\x04'))
    data.decode('utf-8-sig')
receipt=json.loads((ROOT/'bindings/source-receipt.json').read_text())
assert len(receipt['files'])==11
assert hashlib.sha256((ROOT/'bindings/source-receipt.json').read_bytes()).hexdigest()==manifest['source_receipt_sha256']
for row in receipt['files']:
    p=ROOT/'sources'/('' if row['root'] else 'ref/recompcore')/row['relative']
    assert len(p.read_bytes())==row['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest()==row['candidate_sha256']
bindings=json.loads((ROOT/'bindings/baseline-bindings.json').read_text())
for name,row in zip(['01-sdk-selective-vertices.patch','02-root-selective-vertices.patch'],bindings['patches']):
    p=ROOT/'patches'/name
    assert len(p.read_bytes())==row['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256']
native=json.loads((ROOT/'evidence/native-results.json').read_text())
print(json.dumps({'status':'PASS_SOURCE_ONLY_HANDOFF','files':len(manifest['files']),
                  'native_results_status':native['status'],
                  'native_game_qualified':native['native_game_qualified'],
                  'performance_qualified':native['performance_qualified']}))
