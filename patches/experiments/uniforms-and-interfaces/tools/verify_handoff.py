"""Read-only source/binding integrity verification; no compiler or game."""
from pathlib import Path
import hashlib, json

ROOT = Path(__file__).resolve().parent.parent
allowed = json.loads((ROOT/'allowlist.json').read_text(encoding='utf-8'))['files']
manifest = json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
assert len(allowed) == len(set(allowed))
assert {r['path'] for r in manifest['files']} == set(allowed)-{'manifest.json'}
assert {p.relative_to(ROOT).as_posix() for p in ROOT.rglob('*') if p.is_file()} == set(allowed)
for row in manifest['files']:
    path = ROOT/row['path']
    assert path.resolve().is_relative_to(ROOT.resolve()) and not path.is_symlink()
    data = path.read_bytes()
    assert len(data) == row['bytes'] and hashlib.sha256(data).hexdigest() == row['sha256'], row['path']
    assert path.suffix.lower() in {'.md','.json','.py','.c','.cpp','.h','.hpp','.inc','.patch'} or row['path'].startswith('licenses/')
    assert not data.startswith((b'MZ',b'!<arch>',b'DXBC',b'DXIL',b'PK\x03\x04'))
    data.decode('utf-8-sig')
uniform = json.loads((ROOT/'bindings/uniform-source.json').read_text(encoding='utf-8'))
assert len(uniform['files']) == 10 and uniform['config_version'] == 16
for row in uniform['files']:
    data = (ROOT/'sources/uniforms'/row['relative_path']).read_bytes()
    assert len(data) == row['overlay']['bytes'] and hashlib.sha256(data).hexdigest() == row['overlay']['sha256']
assert hashlib.sha256((ROOT/'patches/01-sparse-uniforms.patch').read_bytes()).hexdigest() == uniform['patch']['sha256']
interface = json.loads((ROOT/'bindings/interface-source.json').read_text(encoding='utf-8'))
assert len(interface['changes']) == 4
for name, row in interface['changes'].items():
    before = (ROOT/'sources/uniforms'/name).read_bytes()
    after = (ROOT/'sources/interfaces'/name).read_bytes()
    assert hashlib.sha256(before).hexdigest() == row['before']
    assert len(after) == row['bytes'] and hashlib.sha256(after).hexdigest() == row['after']
assert hashlib.sha256((ROOT/'patches/02-specialized-interfaces.patch').read_bytes()).hexdigest() == interface['patch_sha256']
evidence = json.loads((ROOT/'evidence/qualification.json').read_text(encoding='utf-8'))
print(json.dumps({'status':'PASS_SOURCE_ONLY_HANDOFF','files':len(manifest['files']),
                  'qualification_status':evidence['status'],'performance_qualified':evidence['performance_qualified']}))
