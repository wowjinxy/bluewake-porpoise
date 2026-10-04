#!/usr/bin/env python3
"""Build the current frontend and pinned full-copy baseline; compare outputs.

Run inside a Visual Studio developer shell on Windows. No game assets needed.
The ignored build fixture obtains the three baseline files directly from Git.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = 'e280c788dadabd18b085af0085f1558fc9ff5ecc'
FILES = [
    'GXRuntime/graphics/frontend/include/gxruntime/aurora_recomp/retail_gx_frontend.hpp',
    'GXRuntime/graphics/frontend/src/retail_gx_frontend.cpp',
    'GXRuntime/graphics/frontend/src/render_sink.cpp',
]


def run(argv, *, env=None):
    completed = subprocess.run([str(x) for x in argv], check=False, text=True,
                               encoding='utf8', errors='replace', capture_output=True,
                               env=env)
    output = completed.stdout + completed.stderr
    if completed.returncode:
        print(output, flush=True)
        completed.check_returncode()
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, default=ROOT / 'ref/recompcore')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/transform-snapshot-reuse')
    parser.add_argument('--cc', default=None)
    parser.add_argument('--cxx', default=None)
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--ninja', default=None)
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    baseline = out / 'baseline'
    hashes = {}
    for name in FILES:
        data = subprocess.check_output(['git', '-C', str(runtime), 'show', BASE + ':' + name])
        dest = baseline / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
        hashes[name] = {'baseline_sha256': hashlib.sha256(data).hexdigest(),
                        'current_sha256': hashlib.sha256((runtime / name).read_bytes()).hexdigest()}
    fixture = ROOT / 'tests/transform_snapshot_reuse_test.cpp'
    cmake = f'''cmake_minimum_required(VERSION 3.24)
project(transform_snapshot_reuse LANGUAGES C CXX)
include(CTest)
set(BUILD_TESTING ON CACHE BOOL "" FORCE)
set(GXRUNTIME_ENABLE_AURORA OFF CACHE BOOL "" FORCE)
set(GXRUNTIME_ENABLE_AURORA_RECOMP ON CACHE BOOL "" FORCE)
add_subdirectory("{runtime.as_posix()}/GXRuntime" gxruntime)
if(WIN32)
  add_library(transform_test_posix STATIC "{ROOT.as_posix()}/windows/compat/bw_posix_compat.c")
  target_include_directories(transform_test_posix PRIVATE "{ROOT.as_posix()}/windows/compat")
  target_compile_options(gxruntime PRIVATE "SHELL:-include {ROOT.as_posix()}/windows/compat/bw_posix_compat.h")
  target_link_libraries(gxruntime PUBLIC transform_test_posix winmm)
endif()
add_executable(transform_snapshot_reuse "{fixture.as_posix()}")
target_link_libraries(transform_snapshot_reuse PRIVATE GXRuntime::retail_gx_frontend)
add_library(transform_baseline_sink STATIC "{baseline.as_posix()}/GXRuntime/graphics/frontend/src/render_sink.cpp")
target_include_directories(transform_baseline_sink PUBLIC "{baseline.as_posix()}/GXRuntime/graphics/frontend/include" "{runtime.as_posix()}/GXRuntime/graphics/frontend/include")
target_compile_features(transform_baseline_sink PUBLIC cxx_std_20)
target_link_libraries(transform_baseline_sink PUBLIC GXRuntime::runtime)
add_library(transform_baseline_frontend STATIC "{baseline.as_posix()}/GXRuntime/graphics/frontend/src/retail_gx_frontend.cpp")
target_link_libraries(transform_baseline_frontend PUBLIC transform_baseline_sink)
add_executable(transform_snapshot_baseline "{fixture.as_posix()}")
target_link_libraries(transform_snapshot_baseline PRIVATE transform_baseline_frontend)
if(WIN32)
  # Match the shipping host: the existing frontend fixture puts megabyte-sized
  # register states on its stack. MSVC's default1MiB reserve is insufficient.
  foreach(t aurora_recomp_frontend_tests replay_digest_tests transform_snapshot_reuse transform_snapshot_baseline)
    if(MINGW)
      target_link_options(${{t}} PRIVATE "LINKER:--stack,67108864")
    else()
      target_link_options(${{t}} PRIVATE "LINKER:/STACK:67108864")
    endif()
  endforeach()
endif()
'''
    (out / 'CMakeLists.txt').write_text(cmake, encoding='utf8')
    config = [args.cmake, '-S', out, '-B', out/'cmake', '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release']
    for name, value in [('CMAKE_C_COMPILER', args.cc), ('CMAKE_CXX_COMPILER', args.cxx),
                        ('CMAKE_MAKE_PROGRAM', args.ninja)]:
        if value:
            config.append('-D' + name + '=' + value)
    (out / 'configure.log').write_text(run(config), encoding='utf8')
    print('Configured headless fixture', flush=True)
    targets = ['transform_snapshot_reuse', 'transform_snapshot_baseline',
               'aurora_recomp_frontend_tests', 'replay_digest_tests']
    build = run([args.cmake, '--build', out/'cmake', '--parallel', '4', '--target', *targets])
    (out / 'build.log').write_text(build, encoding='utf8')
    print('Built current and full-copy baseline', flush=True)
    suffix = '.exe' if os.name == 'nt' else ''
    receipts = {}
    for mode, exe, verify in [('baseline', 'transform_snapshot_baseline', False),
                              ('reuse', 'transform_snapshot_reuse', False),
                              ('verify', 'transform_snapshot_reuse', True)]:
        env = os.environ.copy()
        env['DOL_GX_TRANSFORM_VERIFY'] = '1' if verify else '0'
        result = run([out/'cmake'/(exe+suffix)], env=env)
        (out / (mode + '.log')).write_text(result, encoding='utf8')
        match = re.search(r'draws=(\d+) transform_digest=([0-9a-f]+)', result)
        assert match, result
        receipts[mode] = {'draws': int(match[1]), 'digest': match[2]}
        print(mode, match[0], flush=True)
    assert receipts['baseline'] == receipts['reuse'] == receipts['verify'], receipts
    env = os.environ.copy()
    env['DOL_GX_TRANSFORM_VERIFY'] = '1'
    long_result = run([out/'cmake'/('transform_snapshot_reuse'+suffix), '--long-verify'], env=env)
    (out / 'long-verify.log').write_text(long_result, encoding='utf8')
    assert '[gx-transform]' in long_result
    assert not re.search(r'mismatches [1-9]|[1-9]\d* differ', long_result), long_result
    print(long_result.strip(), flush=True)
    suite = run([args.cmake.replace('cmake', 'ctest'), '--test-dir', out/'cmake',
                 '-R', '^(aurora_recomp_frontend_tests|replay_digest_tests)$', '--output-on-failure',
                 '--no-tests=error'], env=env)
    (out / 'existing-tests.log').write_text(suite, encoding='utf8')
    receipt = {'base_commit': BASE, 'files': hashes,
               'fixture_sha256': hashlib.sha256(fixture.read_bytes()).hexdigest(),
               'result': receipts, 'verification': long_result,
               'existing_tests': suite, 'compiler': {'cc': args.cc, 'cxx': args.cxx},
               'claim': 'Functional equality and zero reported reuse mismatches; no FPS or timing benchmark.'}
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf8')
    print('PASS: full-copy baseline equals optimized and verification paths', flush=True)


if __name__ == '__main__':
    main()
