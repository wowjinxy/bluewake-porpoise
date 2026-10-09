"""Compile/run the source-only observation contract; preserve every attempt."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess


def record(path):
    path = Path(path).resolve()
    return {'path': str(path), 'bytes': path.stat().st_size,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--environment-json', type=Path)
    parser.add_argument('--watch', type=Path)
    args = parser.parse_args()
    root, out = args.repo.resolve(), args.output.resolve()
    spec = importlib.util.spec_from_file_location('facts_prepare', Path(__file__).with_name('prepare.py'))
    prepare = importlib.util.module_from_spec(spec); spec.loader.exec_module(prepare)
    prepare.prepare(root, out, args.watch)
    # Existing Make parser handles Windows literal backslashes/escaped spaces.
    spec = importlib.util.spec_from_file_location('dependency_parser', root / 'tests/test_quantized_psq_runtime.py')
    helper = importlib.util.module_from_spec(spec); spec.loader.exec_module(helper)
    environment = dict(os.environ)
    if args.environment_json:
        loaded = json.loads(args.environment_json.read_text())
        environment = loaded.get('environment', loaded)
    environment.update(TEMP=str(out), TMP=str(out), PYTHONDONTWRITEBYTECODE='1')
    pins, rows = {}, []
    def pin(path):
        value = record(path); key = os.path.normcase(value['path'])
        if key in pins and pins[key] != value:
            raise RuntimeError('Input changed: ' + value['path'])
        pins[key] = value
        return value
    def run(role, argv):
        log = out / (str(len(rows)) + '-' + role + '.log')
        row = {'role': role, 'argv': list(map(str, argv))}; rows.append(row)
        with log.open('xb') as stream:
            value = subprocess.run(row['argv'], cwd=out, env=environment,
                stdin=subprocess.DEVNULL, stdout=stream, stderr=subprocess.STDOUT, timeout=120,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        row.update(exit_code=value.returncode, log=record(log))
        if value.returncode:
            raise RuntimeError('Failed ' + role + ': ' + str(log))
        return log.read_text(errors='replace')
    result = {'status': 'RUNNING', 'rows': rows}
    try:
        for path in [Path(__file__), Path(prepare.__file__), Path(helper.__file__), args.compiler]:
            pin(path)
        if args.environment_json: pin(args.environment_json)
        inputs = json.loads((out / 'source-receipt.json').read_text())['inputs']
        for value in inputs:
            if pin(value['path']) != value: raise RuntimeError('Prepared input drift')
        for relative in ['cmake/composite/direct_calls.c','cmake/composite/direct_calls.h',
                         'cmake/composite/dispatch_loop.h','cmake/composite/observation_facts.h',
                         'cmake/composite/edge_intercept_abi.h','runtime/host/src/health_return_observer.h']:
            source = root / relative; destination = out / 'tree' / relative
            pin(source); destination.parent.mkdir(parents=True, exist_ok=True); shutil.copyfile(source,destination)
        shutil.copyfile(Path(__file__).with_name('oracle.c'),out / 'oracle.c')
        sdk = root / 'ref/recompcore/GXRuntime/include'
        includes = [out,out / 'tree/cmake/composite',root / 'runtime/host/src',sdk,
            root / 'ref/recompcore/Source/Core/Core/PowerPC/StaticRecomp']
        common = ['-std=gnu11','-fms-runtime-lib=dll','-DBLUEWAKE_DIRECT_CALLS=1','-DBLUEWAKE_EDGE_FILTER=1',
            '-DBW_EDGE_WATCH_INCLUDE="fixture_watch.inc"','-D_DLL','-D_MT',
            '-Xclang','--dependent-lib=msvcrt','-ffp-contract=off',
            *['-I' + str(path).replace('\\','/') for path in includes]]
        profiles = [('o3',['-O3']),('asan',['-O2','-fsanitize=address','-fno-omit-frame-pointer']),
                    ('tracing',['-O3','-DBLUEWAKE_ENABLE_DEVELOPER_TRACING=1']),
                    ('census',['-O3','-DBLUEWAKE_EDGE_CENSUS=1'])]
        outputs = []
        for profile, flags in profiles:
            objects = []
            for source in [out / 'oracle.c',out / 'tree/cmake/composite/direct_calls.c']:
                obj = out / (profile + '-' + source.stem + '.obj')
                dep, pre = Path(str(obj)+'.d'),Path(str(obj)+'.pre.d')
                run(profile+'-M-'+source.stem,[args.compiler,*common,*flags,'-M','-MF',pre,'-MT',obj,source])
                before = {os.path.normcase(str(p.resolve())): pin(p) for p in helper.dependencies(pre.read_text())}
                run(profile+'-compile-'+source.stem,[args.compiler,*common,*flags,'-MD','-MF',dep,'-MT',obj,'-c',source,'-o',obj])
                after = {os.path.normcase(str(p.resolve())) for p in helper.dependencies(dep.read_text())}
                if set(before)!=after: raise RuntimeError('Actual -M/-MD mismatch')
                for value in before.values():
                    if record(value['path'])!=value: raise RuntimeError('Dependency changed during compilation')
                rows[-1]['dependencies'] = list(before.values()); rows[-1]['object'] = record(obj)
                objects.append(obj)
            exe = out / (profile+'.exe')
            link_flags = ['-shared-libasan'] if profile == 'asan' else []
            run(profile+'-link',[args.compiler,*flags,*link_flags,'-fms-runtime-lib=dll','-fuse-ld=lld','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt',*objects,'-o',exe])
            # Clang's ASan DLL is beside its compiler on this Windows toolchain.
            environment['PATH'] = str(args.compiler.parent) + os.pathsep + environment.get('PATH','')
            if profile == 'asan':
                resource = run('compiler-resource-dir',[args.compiler,'-print-resource-dir']).strip()
                runtime = Path(resource) / 'lib/windows/clang_rt.asan_dynamic-x86_64.dll'
                pin(runtime); shutil.copyfile(runtime,out/runtime.name)
            text = run(profile+'-execute',[exe])
            if 'observation facts PASS' not in text: raise RuntimeError('Missing terminal PASS')
            outputs.append({'profile':profile,'text':text.strip(),'executable':record(exe)})
        for value in pins.values():
            if record(value['path']) != value: raise RuntimeError('Final input preservation failed')
        result.update(status='PASS_SOURCE_CONTRACT_ONLY',profiles=outputs,inputs_preserved=True)
    except Exception as error:
        result.update(status='FAIL_PRESERVED',error=str(error))
        raise
    finally:
        result['inputs'] = list(pins.values())
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')


if __name__ == '__main__':
    main()
