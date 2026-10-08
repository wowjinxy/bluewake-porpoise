"""Fail-closed private chunk overlay and literal frozen-body fixture oracle."""
from pathlib import Path
import hashlib, json, re

import argparse
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--frozen-composite', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
FROZEN = args.frozen_composite.resolve()
HERE = args.output.resolve()
assert not HERE.exists(), 'Fresh output directory required; preserve attempts'
HERE.mkdir(parents=True)
CHUNK = FROZEN / 'chunks_dol/chunk_0152_text1_802616E0.c'
PINS = {
    CHUNK: '58b9345972121b76e8b82b7dcd1159fc7c40d493fa6991fce06c2d7325be9c61',
    FROZEN / 'generated.h': 'ce68d056edc5868b9214ab8c228506510cc73c3895f8ac62a1ff563f4f70a815',
}
for path, expected in PINS.items():
    assert hashlib.sha256(path.read_bytes()).hexdigest() == expected, str(path)
original = CHUNK.read_bytes().decode('utf-8')
assert '\r' not in original, 'unexpected newline style'
overlay = original
include = '#include "native_stripe.h"\n'
assert original.count('#include "gather_pipe.h"\n') == 1
overlay = overlay.replace('#include "gather_pipe.h"\n', '#include "gather_pipe.h"\n' + include, 1)
oracles, edits, spans = [], [], []
for block, entry, resume, name in [(200, '80263E7C', '80263F3C', 'FIRST'), (224, '80264260', '80264320', 'SECOND')]:
    base = original.index(f'\nbwfast_{block}:\n')
    start = original.index(f'    ctx->pc = 0x{entry}u;\n', base)
    stop = original.index(f'    // {resume}: ', start)
    body = original[start:stop]
    instructions = re.findall(r'^    // ([0-9A-F]{8}):', body, re.M)
    assert len(instructions) == 48 and instructions[0] == entry
    assert len(re.findall(r'// [0-9A-F]+: stfs ', body)) == 10
    labels = sorted(set(re.findall(r'goto (bwslow_\d+_\d+);', body)))
    oracles.append(f'void bluewake_stripe_oracle_{name.lower()}(CPUState* ctx) {{\n'
                   '    bool cycle_block_prepaid = true;\n' + body + '\n    return;\n' +
                   ''.join(f'{label}: ;\n' for label in labels) +
                   '    (void)cycle_block_prepaid; abort();\n}\n')
    spans.append(dict(block=block, entry=entry, resume=resume, literal_body_sha256=hashlib.sha256(body.encode()).hexdigest(),
                      literal_body_bytes=len(body), instructions=48, stores=10))
    # Insert the normal-copy guard after its label: the precise path always
    # retains its original code; only an already-prepaid entry could admit.
    normal_anchor = f'label_{entry}:\n'
    assert overlay.count(normal_anchor) == 1
    normal_add = (f'    if (cycle_block_prepaid && bluewake_native_stripe_tail(ctx, BLUEWAKE_GPU_STRIPE_{name}))\n'
                  f'        goto label_{resume};\n')
    overlay = overlay.replace(normal_anchor, normal_anchor + normal_add, 1)
    # The literal prepaid copy has no instruction labels; use a private resume.
    fastbase = overlay.index(f'\nbwfast_{block}:\n')
    faststart = overlay.index(f'    ctx->pc = 0x{entry}u;\n', fastbase)
    fast_add = (f'    if (bluewake_native_stripe_tail(ctx, BLUEWAKE_GPU_STRIPE_{name}))\n'
                f'        goto bw_gpu_stripe_resume_{block};\n')
    overlay = overlay[:faststart] + fast_add + overlay[faststart:]
    faststop = overlay.index(f'    // {resume}: ', faststart + len(fast_add))
    resume_add = f'bw_gpu_stripe_resume_{block}: ;\n'
    overlay = overlay[:faststop] + resume_add + overlay[faststop:]
    edits.extend([normal_add, fast_add, resume_add])
restored = overlay.replace(include, '', 1)
for addition in edits:
    assert restored.count(addition) == 1
    restored = restored.replace(addition, '', 1)
assert restored == original, 'overlay changes other source bytes'

header = ('/* Literal prepaid bodies from the exact module976 input; only wrapped\n'
          ' * as independently callable fixture functions. No arithmetic rewrite. */\n'
          '#include "gather_pipe.h"\n#define DOLRECOMP_CPU_HEADER "core/cpu.h"\n'
          f'#include "{(FROZEN / "generated.h").as_posix()}"\n'
          '#include "inline_fp.h"\n#include <stdlib.h>\n')
(HERE / 'stripe_oracle.c').write_bytes((header + '\n'.join(oracles)).encode())
work = HERE / 'work'
work.mkdir(exist_ok=True)
(work / CHUNK.name).write_bytes(overlay.encode())
receipt = dict(status='PRIVATE_LITERAL_ORACLE_AND_FOUR_INSERTIONS_READY_NOT_COMPILED',
               frozen_inputs=[dict(path=str(p), sha256=h) for p,h in PINS.items()], spans=spans,
               overlay=dict(path=str(work / CHUNK.name), sha256=hashlib.sha256(overlay.encode()).hexdigest()),
               original_other_bytes_restored_exactly=True, game_executed=False)
(HERE / 'prepare-receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
print(json.dumps(receipt, indent=2))
