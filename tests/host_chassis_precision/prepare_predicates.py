"""Extract reviewed production chassis predicates; reject unreviewed drift."""
from pathlib import Path
import argparse
import hashlib
import re


def function(text, name):
    match = re.search(r'^(?:__attribute__\(\(noinline\)\) )?static (?:inline )?'
                      r'(?:bool|u32) ' + re.escape(name) + r'\([^;]*?\)\s*\{',
                      text, re.M)
    if match is None:
        raise RuntimeError('Missing production function: ' + name)
    end, depth = match.end(), 1
    while depth:
        if end >= len(text):
            raise RuntimeError('Unterminated production function: ' + name)
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]


def prepare(repo_root, output_dir):
    main = (repo_root / 'runtime/host/src/main.c').read_text(encoding='utf-8')
    reference = Path(__file__).with_name('predicates.inc').read_text(encoding='utf-8')
    actual = function(main, 'host_chassis_requires_full').replace(
        'host_chassis_requires_full', 'new_requires_full', 1)
    if actual != function(reference, 'new_requires_full'):
        raise RuntimeError('Actual chassis predicate changed: review and refresh the oracle')
    guards = {
        'host_chassis_edge_service_body': '54fa2ea526df0780a4a4c6e41d7fe37546c49c96df0a4e331a087466426f8228',
        'host_chassis_edge_service_full': 'b13929c87cabd5e85e42d9ccc76cf5d060c4d8bb8779a64664c89119188e9fc3',
        'host_canonical_linked_pc': 'b58b3264028f2db624fec829487f816ab1ff83d3659ff41bf89627df7b4377a1',
    }
    for name, expected in guards.items():
        if hashlib.sha256(function(main, name).encode()).hexdigest() != expected:
            raise RuntimeError('Actual service/canonicalization changed: review ' + name)
    edge_guard = ('if (!scheduler_requires && address == 0x80328F84u &&\n'
                  '        cpu->lr == 0x80246A04u)\n'
                  '        host_observe_ground_cross_return(cpu, 0u);')
    turn_guard = ('if (cpu.pc == 0x80328F84u && cpu.lr == 0x80246A04u)\n'
                  '            host_observe_ground_cross_return(&cpu, blocks);')
    if (edge_guard not in function(main, 'host_chassis_edge_service_body') or
            main.count(turn_guard) != 1 or
            len(re.findall(r'\bhost_observe_ground_cross_return\s*\(', main)) != 4):
        raise RuntimeError('GroundCross service/per-turn call or raw-PC/LR guard changed')
    edges = (repo_root / 'runtime/host/src/edge_intercepts.c').read_text(encoding='utf-8')
    if not re.search(r'case 0x80328F84u:\s*return false;', function(
            edges.replace('bool bluewake_', 'static bool bluewake_'),
            'bluewake_edge_observation_requires_host')):
        raise RuntimeError('GroundCross edge-return predicate changed')
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / 'host_chassis_predicates.inc').write_text(
        function(reference, 'old_requires_full') + '\n' + actual + '\n',
        encoding='utf-8', newline='\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo-root', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.repo_root, args.output_dir)
