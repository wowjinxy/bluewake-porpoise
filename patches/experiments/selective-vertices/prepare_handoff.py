"""Stage a strictly source-only private handoff. Never compile or run a game."""
from pathlib import Path
import datetime
import hashlib
import json

OUT = Path(__file__).resolve().parent
PRIVATE = OUT.parent
ROOT = PRIVATE.parents[1]
FREEZE = '996b05bee581cbbc9b8e031101b246433f1f89f00e121e5ef2a38202981038ed'
files = {'prepare_handoff.py'}
copies = []

def rec(path):
    path = Path(path).resolve()
    return {'path': str(path), 'bytes': path.stat().st_size,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

def write(name, data):
    target = OUT / name
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        assert target.read_bytes() == data, 'Refuse to overwrite changed handoff file: ' + name
    else:
        target.write_bytes(data)
    files.add(name)

def text(name, data):
    write(name, data.encode('utf-8'))

def js(name, data):
    text(name, json.dumps(data, indent=2) + '\n')

def copy(source, name):
    source = Path(source)
    write(name, source.read_bytes())
    copies.append({'source': rec(source), 'handoff': name})

receipt_path = PRIVATE / 'compact1/source-receipt.json'
assert rec(receipt_path)['sha256'] == FREEZE
receipt = json.loads(receipt_path.read_text())
assert len(receipt['files']) == 11
for row in receipt['files']:
    candidate = Path(row['candidate'])
    assert rec(candidate)['sha256'] == row['candidate_sha256']
    assert rec(row['source'])['sha256'] == row['source_sha256']
    name = 'sources/' + ('' if row['root'] else 'ref/recompcore/') + row['relative']
    copy(candidate, name)

for source, name in [
    ('compact1/sdk-selective-vertices.patch', 'patches/01-sdk-selective-vertices.patch'),
    ('compact1/root-selective-vertices.patch', 'patches/02-root-selective-vertices.patch'),
    ('compact1/baseline-bindings.json', 'bindings/baseline-bindings.json'),
    ('compact1/source-receipt.json', 'bindings/source-receipt.json'),
    ('compact1/test-source-receipt.json', 'bindings/test-source-receipt.json'),
    ('compact1/interp-production-flags.json', 'bindings/interp-production-flags.json'),
    ('compact1/DESIGN.md', 'DESIGN.md'),
    ('compact1/selective_decode_test.cpp', 'fixtures/compact1/selective_decode_test.cpp'),
    ('compact1/selective_interp_test.cpp', 'fixtures/compact1/selective_interp_test.cpp'),
    ('pixels1/compact_pixels_test.cpp', 'fixtures/pixels1/compact_pixels_test.cpp'),
    ('pixels1/cases.cpp.inc', 'fixtures/pixels1/cases.cpp.inc'),
    ('pixels1/prepare_fixture.py', 'tools/pixels1/prepare_fixture.py'),
    ('pixels1/run_fixture.py', 'tools/pixels1/run_fixture.py'),
    ('compact1/prepare.py', 'tools/compact1/prepare.py'),
    ('compact1/prepare_tests.py', 'tools/compact1/prepare_tests.py'),
    ('compact1/prepare_bundle.py', 'tools/compact1/prepare_bundle.py'),
    ('compact1/run_decode.py', 'tools/compact1/run_decode.py'),
    ('compact1/run_interp.py', 'tools/compact1/run_interp.py'),
    ('state-inputs1/efb_input_diagnostic.h', 'fixtures/state-inputs1/efb_input_diagnostic.h'),
    ('state-inputs1/fixture.c', 'fixtures/state-inputs1/fixture.c'),
    ('state-inputs1/prepare.py', 'tools/state-inputs1/prepare.py'),
    ('state-inputs1/run_tests.py', 'tools/state-inputs1/run_tests.py'),
    ('state-inputs1/source-receipt.json', 'bindings/efb-source-receipt.json'),
]:
    copy(PRIVATE / source, name)

for source, name in [
    ('patches/experiments/gpu-raw-stripecross/fixtures/raw_pixels_test.cpp', 'fixtures/retained/raw_pixels_test.cpp'),
    ('build/performance-02-compact-vertices-20261006/candidate/GXRuntime/graphics/gxcore/tests/gxcore_vertex_layout_test.cpp', 'fixtures/retained/gxcore_vertex_layout_test.cpp'),
    ('patches/recompcore/drafts/compact-vertices.md', 'bindings/retained-compact-vertices.md'),
    ('LICENSE', 'licenses/root-LICENSE'),
    ('ref/recompcore/COPYING', 'licenses/recompcore-COPYING'),
    ('ref/recompcore/GXRuntime/LICENSE', 'licenses/GXRuntime-LICENSE'),
    ('ref/recompcore/GXRuntime/graphics/aurora/LICENSE', 'licenses/aurora-LICENSE'),
]:
    copy(ROOT / source, name)

reference_paths = [
    ROOT / 'build/performance-02-compact-vertices-20261006/run_tests_v4.py',
    ROOT / 'build/performance-02-compact-vertices-20261006/test-inputs-v4.json',
    PRIVATE / 'build1/prepare.py',
    PRIVATE / 'build1/baseline-audit1.json',
    ROOT / 'build/slowdown-marker-20261007/host1/work/commands.json',
    ROOT / 'build/core-efb-gpu-fixture-build-20261006-attempt2/work/commands.json',
    ROOT / 'ref/recompcore/GXRuntime/graphics/gxcore/src/gxcore_shader.cpp',
    ROOT / 'patches/recompcore/active.json',
    ROOT / 'patches/recompcore/drafts/compact-vertices.patch',
]
js('bindings/private-prerequisite-references.json', {
    'status': 'REFERENCES_ONLY_NOT_BUNDLED',
    'limits': 'Exact retained private prerequisites, not a portable runner installation. Toolchains, runtime DLLs, input traces and game assets are intentionally absent.',
    'files': [rec(p) for p in reference_paths],
})

receipt_names = {
    'decode_o3': 'compact1/decode-attempt3/result.json',
    'interpolation_o3': 'compact1/interp-attempt2/result.json',
    'pixels_d3d12': 'pixels1/attempt2/result.json',
    'host_18_tu': 'build1/work4/result.json',
    'efb_helper_contract': 'state-inputs1/tests1/result.json',
}
results = {key: json.loads((PRIVATE / name).read_text()) for key, name in receipt_names.items()}
summary = {'status': 'STATIC_QUALIFICATION_NATIVE_AND_TIMING_PENDING',
           'source_receipt_sha256': FREEZE, 'receipts': {}, 'results': {},
           'native_game_qualified': False, 'performance_qualified': False}
for key, name in receipt_names.items():
    summary['receipts'][key] = rec(PRIVATE / name)
    assert results[key]['status'].startswith('PASS_')
for key in ('decode_o3', 'interpolation_o3'):
    summary['results'][key] = {'status': results[key]['status'], 'profiles': [
        {'profile': p['profile'], 'outputs': p['outputs']} for p in results[key]['profiles']]}
pixels = results['pixels_d3d12']
summary['results']['pixels_d3d12'] = {
    'status': pixels['status'], **pixels['pixels'],
    'byte_identical_shader_pairs': len(pixels['shader_pairs']),
    'actual_dependency_counts': pixels['actual_dependencies'],
    'actual_link_input_count': len(pixels['actual_link_inputs']), 'limits': pixels['limits'],
    'adapter': 'NVIDIA GeForce RTX 3070; D3D12 driver 32.0.16.1088',
}
host = results['host_18_tu']
summary['results']['host_18_tu'] = {
    'status': host['status'], 'compiled_tus': len(host['compiled']),
    'host_reference_only': host['host'], 'baseline_reference_only': host['baseline_host'],
    'game_executed': host['game_executed'], 'module_rebuilt': host['module_rebuilt'],
    'public_source_edits': host['public_source_edits'],
}
efb = results['efb_helper_contract']
summary['results']['efb_helper_contract'] = {
    'status': efb['status'], 'checks': efb['checks'], 'timing_eligible': efb['timing_eligible'],
}
summary['retained_failure_references'] = [rec(PRIVATE / 'pixels1/attempt1/result.json')]
js('evidence/static-qualification.json', summary)
js('evidence/native-results.json', {
    'status': 'PENDING_NATIVE_EXPERIMENTS', 'source_receipt_sha256': FREEZE,
    'native_game_qualified': False, 'performance_qualified': False,
    'results': [], 'note': 'Root will fill after native state/capture and matched timing tests. No speedup or gameplay equivalence claimed by this placeholder.',
})
js('evidence/asan-results.json', {
    'status': 'PENDING_FOCUSED_ASAN', 'source_receipt_sha256': FREEZE,
    'results': [], 'broad_interpolation_asan_qualified': False,
    'note': 'Focused decode/capture results will be filled separately. Earlier broad interpolation ASan baseline limitation remains preserved.',
})

recipes = []
for key in ('decode_o3', 'interpolation_o3', 'pixels_d3d12', 'efb_helper_contract'):
    selected = []
    for row in results[key]['rows']:
        role = row['role']
        if key in ('decode_o3', 'interpolation_o3'):
            keep = role.startswith('candidate-o3-') and ('-compile-' in role or role.endswith('-link'))
        else:
            keep = role in ('compile', 'compile0', 'compile1', 'link')
        if keep:
            selected.append({'role': role, 'argv': row['argv'],
                             'returncode': row.get('returncode', row.get('exit_code'))})
    recipes.append({'qualification': key, 'receipt': summary['receipts'][key], 'commands': selected})
js('fixtures/qualified-command-recipes.json', {
    'status': 'RECORDED_ORIGINAL_PRIVATE_COMMANDS_NOT_PORTABLE_EXECUTION',
    'recipes': recipes,
    'note': 'Commands retain original paths and flags. Resolve sources/includes/libraries into a fresh private build before running; never execute them blindly after moving this bundle.',
})
js('bindings/copied-source-provenance.json', {'files': copies})

text('README.md', '''# Selective vertex input source handoff

This is an inactive, source-only staging bundle. Native experiments and matched
timing are still pending. It contains no precompiled tester build and claims no
gameplay equivalence or performance gain.

The eleven files under `sources/` exactly match frozen source receipt
`996b05bee581cbbc9b8e031101b246433f1f89f00e121e5ef2a38202981038ed`.
SDK paths are rooted at `sources/ref/recompcore/`; the host change is rooted at
`sources/runtime/`. The two patches remain inactive; apply them only against the
exact originals recorded in `bindings/source-receipt.json` and verified SDK/root
HEADs in `bindings/baseline-bindings.json`. This bundle never modifies active.json.

`DOL_GXCORE_SELECTIVE_VERTICES=1` alone selects packed inputs; unset, empty, `0`
and `11` retain canonical 132-byte inputs. The decoder writes packed data directly
using cached recipes, preserves both texture-matrix words and NBT cache updates,
and reconstructs canonical vertices before a cold ubershader or legacy filter.
Generated shader bodies remain unchanged. The canonical default stream is bound
once per pass only when declared inputs are absent; that cleanup is already in
this frozen source. Pipeline cache version is 15.

`evidence/static-qualification.json` gives bounded CPU/GPU/host/helper results and
exact hashes of retained receipts. `evidence/native-results.json` remains an
explicit pending placeholder. `evidence/asan-results.json` records the completed
focused decode/capture passes; the earlier broad interpolation limit remains.
The three principal fixtures are decoder, production interpolation capture and
offscreen D3D12 pixels. The EFB record/replay helper is correctness-only and must
remain excluded from timings. No input recordings or game output are bundled.

Run `python -B -S tools/verify_handoff.py` for read-only allowlist and hash checks.
After editing evidence, run `python -B -S tools/refresh_manifest.py` and verify
again. Neither tool compiles, installs, launches a game or accesses a network.

Original preparers/runners are included byte-for-byte for provenance. They use
private historical paths and are not portable entry points from this directory.
See `FIXTURES.md` and the pinned prerequisite references before rebuilding.
Killer7 shader binaries, cache data and retail shader source are absent. This
adapts the general idea of transporting only relevant inputs, not retail code.
''')
text('FIXTURES.md', '''# Rebuilding fixtures in an isolated source checkout

First verify all eleven originals and the SDK/root commits in the bindings, then
apply the inactive patches in a new checkout. Refresh every consumer of shader.hpp,
DrawPlan and DrawData; a partial object replacement is not a qualified host build.

Use the exact original commands in `fixtures/qualified-command-recipes.json` as
auditable recipes. They show real tested flags and input paths, not a relocated
installation. Replace paths deliberately with your own pinned source/toolchain
locations and record actual compiler -M/-MD closures and linker inputs again.
No compiler, system libraries, Dawn DLLs or game assets are supplied here.

- Decoder: compile `selective_decode_test.cpp` with render_sink.cpp, gxcore.cpp,
  gxcore_shader.cpp, gxcore_uber.cpp, texture_decode.cpp, texture_encode.cpp and
  guest_memory.c plus required GXRuntime support. Run a current full-reference
  build using COMPACT_TEST_REFERENCE, then candidate unset/0/11/empty/1 controls.
  Compare canonical semantic digests, not packed byte lengths.
- Interpolation: compile `selective_interp_test.cpp` with actual patched production
  frame_interp.cpp using the recorded production definitions/includes and needed
  support libraries. This is capture qualification, not a broad blend-suite pass.
- GPU: compile `compact_pixels_test.cpp` with unchanged gxcore_shader.cpp and the
  patched shader.hpp include first, link Dawn, then use compatible Dawn/dxcompiler/
  dxil runtimes. It creates offscreen D3D12 targets without a window or input. It
  requires dual-source blending and compares exact RGBA8 plus Depth32Float bytes.
  Both routes generate the same shader text. Its manually authored canonical
  vertices do not execute the production decoder or backend pipeline factory.
  Sparse texture slots share one authored image; early-depth multipass and Smooth
  Motion integration are outside this fixture.
- EFB helper: compile fixture.c beside efb_input_diagnostic.h as one C17 TU with
  the recorded Windows runtime recipe. The retained run_tests.py documents 25
  off/record/replay/refusal/malformed-input scenarios. Never use EFB replay for
  performance timings. Its source preparer performs a byte-exact inverse check.

Exact authored pixel/CPU preparers and predecessor fixtures are retained. Paths
inside those programs still refer to their original private build directories;
the pinned helper and metadata dependencies are references, not bundled payloads.
Run any rebuilt fixture from a fresh private output directory and preserve failed
attempts. This bundle contains no executable and no captured native input trace.
''')
text('CREDITS.md', '''# Credits and preserved source notices

The selective layout extends the existing BlueWake/GXRuntime renderer and its
repository fixtures. The earlier compact-vertex work and decoder fixture derive
from the retained compact-vertices history. Preserve donor
elliotttate <elliotttate@gmail.com>, commit
6f52a68d14b4375afe55314ac0a66c490aa6f183
(https://github.com/elliotttate/RecompCore/commit/6f52a68d14b4375afe55314ac0a66c490aa6f183).
The original retained draft attribution is copied in
`bindings/retained-compact-vertices.md`.

Existing source copyright and SPDX notices are preserved byte-for-byte. Relevant
root, RecompCore, GXRuntime and Aurora license texts are copied under `licenses/`;
Aurora's MIT notice credits Luke Street (2022). No proprietary Killer7 shader
bytecode, source template or cache file is included in this bundle.
''')

verify = '''"""Read-only exact allowlist/hash verification; no build, install or launch."""
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
    assert not data.startswith((b'MZ',b'!<arch>',b'DXBC',b'DXIL',b'PK\\x03\\x04'))
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
'''
text('tools/verify_handoff.py', verify)
refresh = '''"""Refresh hashes for the fixed source-only allowlist; never build or launch."""
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
    assert not data.startswith((b'MZ',b'!<arch>',b'DXBC',b'DXIL',b'PK\\x03\\x04'))
    data.decode('utf-8-sig')
    rows.append({'path':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
native=json.loads((ROOT/'evidence/native-results.json').read_text())
value={'status':'INACTIVE_SOURCE_ONLY_HANDOFF','snapshot_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
       'source_receipt_sha256':'996b05bee581cbbc9b8e031101b246433f1f89f00e121e5ef2a38202981038ed',
       'native_results_status':native['status'],'files':rows}
(ROOT/'manifest.json').write_text(json.dumps(value,indent=2)+'\\n',encoding='utf-8')
print(json.dumps({'manifest_sha256':hashlib.sha256((ROOT/'manifest.json').read_bytes()).hexdigest(),'files':len(rows)}))
'''
text('tools/refresh_manifest.py', refresh)
files.update({'allowlist.json', 'manifest.json'})
js('allowlist.json', {'status': 'EXACT_SOURCE_ONLY_ALLOWLIST', 'files': sorted(files)})
js('manifest.json', {
    'status': 'INACTIVE_SOURCE_ONLY_HANDOFF',
    'snapshot_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'source_receipt_sha256': FREEZE, 'native_results_status': 'PENDING_NATIVE_EXPERIMENTS',
    'files': [{'path': name, 'bytes': (OUT/name).stat().st_size,
               'sha256': hashlib.sha256((OUT/name).read_bytes()).hexdigest()}
              for name in sorted(files - {'manifest.json'})],
})
print(json.dumps({'status': 'STAGED_SOURCE_ONLY_NATIVE_RESULTS_PENDING',
                  'files': len(files)-1, 'manifest': rec(OUT/'manifest.json')}))
