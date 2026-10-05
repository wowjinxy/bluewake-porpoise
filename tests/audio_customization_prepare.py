"""Reconstruct original/fixed/gain fixtures in shadow copies of a pinned donor.

The real source may have neither, either or both audited patches. Every removal
and application requires exact unique markers and git's full patch check; drift
fails closed. Never edit the real tree or infer success from a single marker.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--patch', type=Path, action='append', default=[])
parser.add_argument('--completion-test-api', action='store_true')
args = parser.parse_args()
source, output = args.source.resolve(), args.output.resolve()
if source == output or source in output.parents or output in source.parents:
    raise SystemExit('Fixture donor must be separate from the real pinned tree')
for name in ['Zelda.cpp', 'Zelda.h']:
    relative = Path('Source/Core/Core/HW/DSPHLE/UCodes') / name
    destination = output / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source / relative, destination)
root = Path(__file__).resolve().parents[1]
gain_patch = root / 'patches/recompcore/drafts/audio-customization-premix.patch'
completion_patch = root / 'patches/recompcore/drafts/zelda-mram-partial-completion.patch'
cpp = output / 'Source/Core/Core/HW/DSPHLE/UCodes/Zelda.cpp'
header = output / 'Source/Core/Core/HW/DSPHLE/UCodes/Zelda.h'

def apply(patch, reverse=False, check_only=False):
    command = ['git', 'apply', '--unsafe-paths', '--directory=' + output.as_posix()]
    if reverse:
        command.append('--reverse')
    command.append(str(patch.resolve()))
    for invocation in [command[:2] + ['--check'] + command[2:], command]:
        # git apply filters paths relative to a repository subdirectory even
        # with --directory. CTest runs below build/, so anchor the command at
        # the repository root; all targets remain explicit shadow paths.
        result = subprocess.run(invocation, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if result.returncode:
            raise SystemExit(result.stderr or 'Cannot apply isolated donor draft')
        if check_only:
            break

def markers():
    c, h = cpp.read_text(encoding='utf-8'), header.read_text(encoding='utf-8')
    original = '    for (u16 i = remaining_length; i < requested_samples_count; ++i)\n      *dst++ = last_sample;'
    fixed = '    for (u16 i = remaining_length; i < requested_samples_count; ++i)\n      dst[i] = last_sample;'
    if (c.count(original), c.count(fixed)) not in [(1, 0), (0, 1)]:
        raise SystemExit('Partial completion marker missing, duplicate or changed')
    gain_counts = (c.count('#ifdef BLUEWAKE_DSP_AUDIO_CUSTOMIZATION'),
                   h.count('#ifdef BLUEWAKE_DSP_AUDIO_CUSTOMIZATION'))
    if gain_counts not in [(0, 0), (4, 1)]:
        raise SystemExit('Gain callback markers missing, duplicate or changed')
    return {'completion_fixed': c.count(fixed) == 1, 'gain': gain_counts == (4, 1)}

initial = markers()
if initial['gain']:
    apply(gain_patch, reverse=True)
if initial['completion_fixed']:
    apply(completion_patch, reverse=True)
assert markers() == {'completion_fixed': False, 'gain': False}
# Exact forward checks confirm the entire known original context after reverse.
apply(gain_patch, check_only=True)
apply(completion_patch, check_only=True)
desired = {'completion_fixed': False, 'gain': False}
for patch in args.patch:
    content = patch.read_bytes()
    if content == gain_patch.read_bytes():
        field = 'gain'
    elif content == completion_patch.read_bytes():
        field = 'completion_fixed'
    else:
        raise SystemExit('Only the two audited audio patches are supported')
    if desired[field]:
        raise SystemExit('Duplicate requested audio patch')
    apply(patch)
    desired[field] = True
assert markers() == desired
if args.completion_test_api:
    # This seam invokes the actual private implementation and native VPB
    # serializer. It exists only in fixture copies, never in either patch or
    # the production tree, and adds no class data or production ABI change.
    text = header.read_text(encoding='utf-8')
    point = '  void FinalizeFrame();'
    assert text.count(point) == 1
    text = text.replace(point, point + '''
#ifdef BLUEWAKE_DSP_COMPLETION_TEST_API
  void TestDownloadRawSamples(u16 voice, s16* destination, u16 count);
#endif''')
    header.write_text(text, encoding='utf-8')
    implementation = cpp
    text = implementation.read_text(encoding='utf-8')
    point = '}  // namespace DSP::HLE'
    assert text.count(point) == 1
    text = text.replace(point, '''#ifdef BLUEWAKE_DSP_COMPLETION_TEST_API
void ZeldaAudioRenderer::TestDownloadRawSamples(u16 voice, s16* destination, u16 count)
{
  VPB vpb;
  FetchVPB(voice, &vpb);
  DownloadRawSamplesFromMRAM(destination, &vpb, count);
  StoreVPB(voice, &vpb);
}
#endif

''' + point)
    implementation.write_text(text, encoding='utf-8')
(output / 'audio-shadow-receipt.json').write_text(json.dumps({
    'source': str(source), 'source_state': initial, 'shadow_state': desired,
    'completion_test_api': args.completion_test_api,
    'files_sha256': {name: hashlib.sha256((output / 'Source/Core/Core/HW/DSPHLE/UCodes' / name).read_bytes()).hexdigest()
                     for name in ['Zelda.cpp', 'Zelda.h']}
}, indent=2) + '\n', encoding='utf-8')
