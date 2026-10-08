"""Extract the exact production decoder prefix; no game source is needed."""
from pathlib import Path
import argparse, hashlib, json
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--gxcore-source', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
data = a.gxcore_source.read_bytes()
assert hashlib.sha256(data).hexdigest() == 'f641231235b33f191f0a50507b9efe8bafc4a81a97d6378099b513260f2cf49f'
source = data.decode('utf-8')
assert source.count('// --- Sink') == 1
assert not a.output.exists(), 'Fresh output required'
output = (source[:source.index('// --- Sink')] + '\n} // namespace gxruntime::gxcore\n').encode('utf-8')
a.output.write_bytes(output)
print(json.dumps({'output': str(a.output), 'sha256': hashlib.sha256(output).hexdigest()}))
