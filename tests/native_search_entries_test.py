#!/usr/bin/env python3
"""Qualify five private prepared search chunks through their actual native hooks.

Pass a copied composite prepared by the normal Windows builder with
--native-entries, and a private unhooked module. No generated source is saved
outside the ignored build folder; the original module/data remain read-only.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]


def run(args):
    out = ROOT / 'build/native-search-routed/oracle'; out.mkdir(parents=True, exist_ok=True)
    runtime = args.runtime.resolve(); composite = args.composite.resolve(); module = args.module.resolve()
    if not (composite / 'native_entries.json').is_file() or not module.is_file():
        raise ValueError('Expected a privately prepared native-entry composite and original module')
    spec = importlib.util.spec_from_file_location('windows_builder_fixture', ROOT / 'scripts/windows/build.py')
    builder = importlib.util.module_from_spec(spec); spec.loader.exec_module(builder)
    build = builder.Builder(SimpleNamespace(out=out, jobs=2, jobs_auto=False, march='x86-64-v3'))
    build.check_tools()
    abi = runtime.parent / 'Source/Core/Core/PowerPC/StaticRecomp'
    common = [build.clang, '-std=gnu11', '-O2', '-march=x86-64-v3', '-ffp-contract=off',
              '-fno-slp-vectorize', '-mllvm', '-large-interval-freq-threshold=10',
              '-Wno-dll-attribute-on-redeclaration', '-DMODULE_GAME_ID="GZLE01"',
              '-DDOLRECOMP_CPU_HEADER="core/cpu.h"', '-DBW_GUEST_MEM1=bw_guest_mem1',
              '-DBW_GUEST_MEM1_SIZE=0x02000000u', '-DBLUEWAKE_EDGE_FILTER=1',
              '-DBLUEWAKE_GATHER_PIPE=1', '-DBLUEWAKE_GATHER_PIPE_BATCH=1',
              f'-I{ROOT / "cmake/composite"}', f'-I{composite}', f'-I{runtime / "include"}', f'-I{abi}']
    chunks = []
    for chunk in ('800256E0', '8003D6E0', '802416E0', '803256E0', '8032D6E0'):
        found = list((composite / 'chunks_dol').glob(f'*_{chunk}.c'))
        if len(found) != 1: raise ValueError(f'Missing or duplicate fixture chunk {chunk}')
        chunks.extend(found)
    def compile_chunk(source):
        obj = out / f'{source.stem}.obj'
        stamp = out / f'{source.stem}.json'
        command = common + ['-c', str(source), '-o', str(obj)]
        inputs = [source, composite / 'generated.h', composite / 'generated_composite.h', abi / 'StaticRecompABI.h']
        inputs += list((ROOT / 'cmake/composite').glob('*.h')) + list((runtime / 'include').rglob('*.h'))
        digest = hashlib.sha256(json.dumps({'command': command, 'compiler': build.clang_version, 'inputs': {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(inputs)}}, sort_keys=True).encode()).hexdigest()
        if obj.is_file() and stamp.is_file() and json.loads(stamp.read_text()).get('digest') == digest:
            return obj
        print(f'Compiling routed fixture {source.name}', flush=True)
        subprocess.run(command, env=build.env, check=True)
        stamp.write_text(json.dumps({'digest': digest}) + '\n')
        return obj
    with ThreadPoolExecutor(max_workers=2) as pool:
        objects = list(pool.map(compile_chunk, chunks))
    helper_names = ('native_entries', 'native_fifo', 'native_bg', 'native_vec', 'native_mtxcalc',
                    'native_search', 'gather_pipe', 'direct_calls', 'guest_cpu')
    helpers = [ROOT / f'cmake/composite/{name}.c' for name in helper_names]
    source = ROOT / 'tests/native_search_entries_test.c'
    core = sorted((runtime / 'src/core').glob('cpu*.c'))
    executable = out / 'native_search_entries_test.exe'
    linked = subprocess.run(common + list(map(str, [source] + objects + helpers + core)) + ['-o', str(executable)],
                            env=build.env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / 'link.log').write_text(linked.stdout)
    if linked.returncode:
        print(linked.stdout)
        raise RuntimeError('Routed fixture compilation/link failed')
    completed = subprocess.run([str(executable), str(module), str(args.cases), str(args.turn_cases)],
                               text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(completed.stdout, end='', flush=True)
    (out / 'comparison.log').write_text(completed.stdout)
    if completed.returncode: raise RuntimeError('Routed full CPU/RAM comparison failed')
    inputs = set(chunks + helpers + core + [source, Path(__file__).resolve()])
    inputs.update(ROOT / f'cmake/composite/{name}.h' for name in helper_names if (ROOT / f'cmake/composite/{name}.h').is_file())
    inputs.update(composite / name for name in ('generated.h', 'native_entries.json', 'bw_edge_watch.inc'))
    receipt = {'module': str(module), 'module_sha256': hashlib.sha256(module.read_bytes()).hexdigest(),
               'cases_per_function': args.cases, 'turn_cases_per_workload': args.turn_cases,
               'compiler': build.clang_version,
               'source_sha256': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(inputs)},
               'output': completed.stdout,
               'scope': 'Actual five prepared chunks, hooks enabled/disabled, full CPU/RAM and every scheduler turn; no game FPS claim.'}
    (out / 'report.json').write_text(json.dumps(receipt, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--composite', type=Path, required=True)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, default=ROOT / 'ref/recompcore/GXRuntime')
    parser.add_argument('--cases', type=int, default=20000)
    parser.add_argument('--turn-cases', type=int, default=3000)
    args = parser.parse_args()
    if args.cases < 1 or args.turn_cases < 1: parser.error('Case counts must be positive')
    run(args)
