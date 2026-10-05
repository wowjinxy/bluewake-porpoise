"""Actual donor shadow reconstruction matrix; no active tree/build mutation."""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import subprocess
import sys
import uuid

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
prepare = root / 'tests/audio_customization_prepare.py'
gain = root / 'patches/recompcore/drafts/audio-customization-premix.patch'
completion = root / 'patches/recompcore/drafts/zelda-mram-partial-completion.patch'
output = args.output.resolve() / ('matrix-' + uuid.uuid4().hex[:12])
output.mkdir(parents=True)
relative = Path('Source/Core/Core/HW/DSPHLE/UCodes')

def fingerprints(path):
    return {name: hashlib.sha256((path / relative / name).read_bytes()).hexdigest()
            for name in ['Zelda.cpp', 'Zelda.h']}

def run(source, destination, fixed, customized, succeed=True):
    command = [sys.executable, str(prepare), '--source', str(source), '--output', str(destination)]
    if fixed:
        command += ['--patch', str(completion)]
    if customized:
        command += ['--patch', str(gain)]
    result = subprocess.run(command, capture_output=True, text=True)
    if (result.returncode == 0) != succeed:
        raise AssertionError(f'Unexpected shadow outcome: {command}\n{result.stdout}\n{result.stderr}')

before = fingerprints(args.source)
sources = {}
for fixed, customized in itertools.product([False, True], repeat=2):
    source = output / f'source-{int(fixed)}-{int(customized)}'
    run(args.source, source, fixed, customized)
    sources[(fixed, customized)] = source
expected = {state: fingerprints(source) for state, source in sources.items()}
cases = []
for source_state, source in sources.items():
    source_before = fingerprints(source)
    for target_state in sources:
        destination = output / ('from-%d-%d-to-%d-%d' % (*source_state, *target_state))
        run(source, destination, *target_state)
        assert fingerprints(destination) == expected[target_state]
        assert fingerprints(source) == source_before
        cases.append({'source': source_state, 'target': target_state,
                      'sha256': fingerprints(destination)})

# A known marker with changed context must fail its full reverse check.
bad = output / 'bad-context'
run(args.source, bad, True, True)
bad_cpp = bad / relative / 'Zelda.cpp'
text = bad_cpp.read_text(encoding='utf-8')
point = '    vpb->current_position_h += remaining_length;'
assert text.count(point) == 1
bad_cpp.write_text(text.replace(point, point + ' // changed context'), encoding='utf-8')
bad_before = fingerprints(bad)
run(bad, output / 'reject-context', False, False, succeed=False)
assert fingerprints(bad) == bad_before

# Incomplete callback ABI markers cannot be mistaken for an unpatched donor.
bad = output / 'bad-marker'
run(args.source, bad, False, True)
bad_h = bad / relative / 'Zelda.h'
text = bad_h.read_text(encoding='utf-8')
point = '#ifdef BLUEWAKE_DSP_AUDIO_CUSTOMIZATION'
assert text.count(point) == 1
bad_h.write_text(text.replace(point, '#ifdef UNKNOWN_CUSTOMIZATION'), encoding='utf-8')
bad_before = fingerprints(bad)
run(bad, output / 'reject-marker', False, False, succeed=False)
assert fingerprints(bad) == bad_before
assert fingerprints(args.source) == before
(output / 'receipt.json').write_text(json.dumps({'source_unchanged': True, 'source_sha256': before,
    'matrix': cases, 'changed_context_rejected': True, 'incomplete_markers_rejected': True}, indent=2)
    + '\n', encoding='utf-8')
print(f'16 donor shadow combinations + drift/marker rejection passed; evidence: {output}')
