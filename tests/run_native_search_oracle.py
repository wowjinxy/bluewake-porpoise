#!/usr/bin/env python3
"""Compare native search helpers with an unhooked private Windows game module.

The module and original game data remain read-only. Randomized protected RAM,
guest aliases and every CPU byte are compared, including observation suffixes
and partial searches resumed through the translated dispatcher. A report
records source/module hashes. This does not establish a game FPS improvement.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args):
    out = ROOT / 'build/native-search-oracle'; out.mkdir(parents=True, exist_ok=True)
    runtime = args.runtime.resolve(); module = args.module.resolve()
    core = sorted((runtime / 'src/core').glob('cpu*.c'))
    if len(core) != 6 or not module.is_file():
        raise ValueError('Expected six pinned CPU sources and an original private module')
    abi = runtime.parent / 'Source/Core/Core/PowerPC/StaticRecomp'
    common = [args.compiler, '-std=c11', '-O2', '-march=x86-64-v3', '-ffp-contract=off',
              '-Wno-dll-attribute-on-redeclaration', f'-I{ROOT / "cmake/composite"}',
              f'-I{runtime / "include"}', f'-I{abi}']
    helpers = [ROOT / f'cmake/composite/{name}.c' for name in ('native_search', 'direct_calls')]
    results = {}; sources = set(core + helpers + [Path(__file__).resolve()])
    sources.update(ROOT / f'cmake/composite/{name}.h' for name in ('native_search', 'direct_calls', 'native_entries'))
    sources.update((runtime / 'include/core').glob('*.h'))
    sources.update((runtime / 'src/core').glob('cpu*.h'))
    sources.add(abi / 'StaticRecompABI.h')
    for name in ('native_search', 'native_search_judge'):
        source = ROOT / f'tests/{name}_test.c'; sources.add(source)
        executable = out / f'{name}_test.exe'
        subprocess.run(common + list(map(str, [source] + helpers + core)) + ['-o', str(executable)], check=True)
        completed = subprocess.run([str(executable), str(module), str(args.cases), '0'],
                                   text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(completed.stdout, end='', flush=True)
        (out / f'{name}.log').write_text(completed.stdout)
        if completed.returncode:
            raise RuntimeError(f'{name}: full CPU/RAM oracle failed')
        if '0 mismatches' not in completed.stdout:
            raise RuntimeError(f'{name}: missing complete comparison output')
        results[name] = completed.stdout
    labels = lambda p: str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p)
    receipt = {'module': str(module), 'module_sha256': sha(module), 'cases_per_function': args.cases,
               'runtime_revision': subprocess.check_output(['git', '-C', str(runtime.parent), 'rev-parse', 'HEAD'], text=True).strip(),
               'compiler': subprocess.check_output([args.compiler, '--version'], text=True).splitlines()[0],
               'source_sha256': {labels(p): sha(p) for p in sorted(sources)}, 'results': results,
               'scope': 'Protected randomized native helper CPU/RAM equivalence; no game benchmark or routed module qualification.'}
    destination = args.report or out / 'report.json'
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'Oracle report: {destination}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, default=ROOT / 'ref/recompcore/GXRuntime')
    parser.add_argument('--compiler', default='clang')
    parser.add_argument('--cases', type=int, default=60000)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.cases < 1: parser.error('--cases must be positive')
    run(args)
