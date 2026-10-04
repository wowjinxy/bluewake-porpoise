#!/usr/bin/env python3
"""Compare the second native batch with a personal translated Windows module.

Usage: python tests/run_native_entries_oracle.py --module ORIGINAL_MODULE.dll
       [--cases 60000] [--compiler clang] [--report PATH]

The original module must omit the second native batch. It is read only;
the harnesses own their fixture CPU/RAM and protect unrelated MEM1 pages.
The report records hashes of the module and tested source, plus complete
per-entry comparison counts. This is equivalence qualification, not a game
performance benchmark or certification of an untested routed release build.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
GROUPS = {'native_fifo': ('native_fifo', 'gather_pipe'),
          'native_bg': ('native_bg', 'direct_calls'),
          'native_vec_sr': ('native_vec',),
          'native_mtxcalc': ('native_mtxcalc', 'direct_calls')}


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as file:
        for data in iter(lambda: file.read(1024 * 1024), b''): digest.update(data)
    return digest.hexdigest()


def run(args):
    output = ROOT / 'build/native-entries-oracle'; output.mkdir(parents=True, exist_ok=True)
    runtime = args.runtime.resolve(); module = args.module.resolve()
    if not module.is_file(): raise ValueError(f'missing personal module: {module}')
    core = sorted((runtime / 'src/core').glob('cpu*.c'))
    if len(core) != 6: raise ValueError('expected the six pinned runtime CPU translation units')
    abi = runtime.parent / 'Source/Core/Core/PowerPC/StaticRecomp'
    common = [args.compiler, '-std=c11', '-O2', '-Wno-dll-attribute-on-redeclaration', '-ffp-contract=off',
              f'-I{ROOT / "cmake/composite"}', f'-I{runtime / "include"}', f'-I{abi}']
    results = {}; sources = set(core + [abi / 'StaticRecompABI.h'])
    sources.update((runtime / 'include/core').glob('*.h'))
    sources.update((runtime / 'src/core').glob('cpu*.h'))
    for name, helpers in GROUPS.items():
        source = ROOT / f'tests/{name}_test.c'; sources.add(source)
        units = [ROOT / f'cmake/composite/{helper}.c' for helper in helpers]; sources.update(units)
        sources.update(ROOT / f'cmake/composite/{helper}.h' for helper in helpers)
        executable = output / f'{name}_test.exe'
        subprocess.run(common + [str(source)] + list(map(str, units + core)) + ['-o', str(executable)], check=True)
        completed = subprocess.run([str(executable), str(module), str(args.cases), '0'],
                                   text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(completed.stdout, end='')
        if completed.returncode: raise RuntimeError(f'{name}: oracle comparison failed')
        compared = re.findall(r'^([0-9A-F]{8}).*?: (\d+) cases, (\d+) identical.*?, (\d+) declined unchanged, 0 mismatches$',
                              completed.stdout, re.M)
        if len(compared) != (3 if name in ('native_fifo', 'native_mtxcalc') else 2 if name == 'native_bg' else 1):
            raise RuntimeError(f'{name}: incomplete comparison output')
        results[name] = {'output': completed.stdout,
                         'entries': {entry: {'cases': int(cases), 'compared': int(accepted), 'declined': int(declined)}
                                     for entry, cases, accepted, declined in compared}}
    sources.update(ROOT / 'cmake/composite' / name for name in ('inline_fp.h', 'gather_pipe_batch.h',
                                                               'native_entries.c', 'native_entries.h'))
    sources.add(Path(__file__).resolve()); sources.add(ROOT / 'scripts/windows/native_entries.py')
    labels = lambda path: str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)
    report = {'module': str(module), 'module_sha256': sha(module), 'cases_per_entry': args.cases,
              'runtime_revision': subprocess.check_output(['git', '-C', str(runtime.parent), 'rev-parse', 'HEAD'], text=True).strip(),
              'compiler': subprocess.check_output([args.compiler, '--version'], text=True).splitlines()[0],
              'source_sha256': {labels(path): sha(path) for path in sorted(sources)}, 'results': results,
              'scope': 'Native helper equivalence with the supplied original personal module; no routed game/play benchmark.'}
    destination = args.report or output / 'report.json'; destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + '\n')
    print(f'Oracle report: {destination}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--cases', type=int, default=60000)
    parser.add_argument('--compiler', default='clang')
    parser.add_argument('--runtime', type=Path, default=ROOT / 'ref/recompcore/GXRuntime')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.cases < 1: parser.error('--cases must be positive')
    run(args)
