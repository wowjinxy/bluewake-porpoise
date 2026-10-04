#!/usr/bin/env python3
"""Build and run the invented native-search versioned observation fixture."""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(compiler, runtime):
    out = ROOT / 'build/native-search-oracle'; out.mkdir(parents=True, exist_ok=True)
    executable = out / 'native_search_guard_test.exe'
    helpers = ('native_entries', 'native_fifo', 'native_bg', 'native_vec', 'native_mtxcalc',
               'native_search', 'gather_pipe', 'direct_calls')
    command = [compiler, '-std=c11', '-O2', '-ffp-contract=off', '-Wno-dll-attribute-on-redeclaration',
               f'-I{ROOT / "cmake/composite"}', f'-I{runtime / "include"}',
               str(ROOT / 'tests/native_search_guard_test.c')]
    command += [str(ROOT / f'cmake/composite/{name}.c') for name in helpers]
    command += [str(p) for p in sorted((runtime / 'src/core').glob('cpu*.c'))]
    subprocess.run(command + ['-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='clang')
    parser.add_argument('--runtime', type=Path, default=ROOT / 'ref/recompcore/GXRuntime')
    args = parser.parse_args()
    run(args.compiler, args.runtime.resolve())
