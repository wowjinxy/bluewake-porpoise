"""Read-only hash/allowlist verification; never compile, install or launch."""
from pathlib import Path
import hashlib, json
root = Path(__file__).resolve().parent.parent
manifest = json.loads((root/'manifest.json').read_text(encoding='utf-8'))
allowed = {row['path'] for row in manifest['files']} | {'manifest.json'}
actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()}
assert actual == allowed, ('Unexpected or missing files', actual ^ allowed)
for row in manifest['files']:
    p = root / row['path']
    assert p.resolve().is_relative_to(root.resolve())
    data = p.read_bytes()
    assert len(data) == row['bytes'] and hashlib.sha256(data).hexdigest() == row['sha256'], row['path']
    assert p.suffix.lower() in {'.md','.json','.py','.c','.cpp','.h','.hpp','.patch'}
    assert not data.startswith((b'MZ', b'!<arch>'))
print(json.dumps({'status':'PASS_SOURCE_ONLY_HANDOFF', 'files':len(manifest['files']),
                  'native_game_qualified':False, 'performance_qualified':False}))
