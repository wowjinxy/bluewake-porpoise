#!/usr/bin/env python3
"""Compile invented translations through the real preparer and entry router.

No disc/generated game code is needed. All nine injected C entry hooks run
with enabled, disabled and observing hosts; fallback execution is counted and
every CPU/RAM byte is checked when a hook declines. Real translated-function
comparison remains in native_{fifo,bg,vec_sr,mtxcalc}_test.c.
"""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native_entries_prepare_hooks', ROOT / 'scripts/windows/native_entries.py')
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


def run(compiler, runtime):
    with tempfile.TemporaryDirectory(prefix='bluewake-native-hooks-') as tmp:
        root = Path(tmp); folder = root / 'chunks_dol'; folder.mkdir()
        (root / 'generated.h').write_text('#include "core/cpu.h"\nextern unsigned fallback_calls;\n')
        fragments = {}; entries = {}
        # Standalone invented functions retain the donor entry addresses so
        # their generated hooks exercise the actual runtime routing switch.
        for entry, required in prepare.ENTRIES.items():
            chunk = prepare.FRAGMENTS[required[0]][0]
            name = f'entry_{entry:08X}'
            body = f'\nlabel_{entry:08X}:\n    fallback_calls++;\n    return;\n'
            digest = hashlib.sha256(' '.join(body.split()).encode()).hexdigest()
            fragments[name] = (chunk, entry, entry + 4, digest); entries[entry] = (name,)
            source = (prepare.INCLUDE + f'void hook_{entry:08X}(CPUState* ctx) {{' + body +
                      f'\nlabel_{entry + 4:08X}:\n    return;\n\nreturn_dispatch_{chunk:08X}:\n    return;\n}}\n')
            # Multiple entry functions sharing one translator chunk need
            # distinct filenames that still end with its real chunk address.
            path = folder / f'chunk_{entry:08X}_{chunk:08X}.c'
            path.write_text(source)
        # The real preparer requires all entry labels in every chunk variant.
        # Merge the functions belonging to each chunk into one synthetic unit.
        for chunk in {v[0] for v in fragments.values()}:
            paths = sorted(folder.glob(f'*_{chunk:08X}.c'))
            combined = ''.join(p.read_text() for p in paths)
            for p in paths: p.unlink()
            (folder / f'chunk_synthetic_{chunk:08X}.c').write_text(combined)
        with patch.object(prepare, 'FRAGMENTS', fragments), patch.object(prepare, 'ENTRIES', entries), \
                patch.object(prepare, 'watched_addresses', return_value=set()):
            prepare.prepare(root)
            prepare.prepare(root)
        driver = root / 'driver.c'
        declarations = '\n'.join(f'void hook_{entry:08X}(CPUState*);' for entry in entries)
        functions = ', '.join(f'hook_{entry:08X}' for entry in entries)
        fixture = (ROOT / 'tests/native_entries_guard_test.c').as_posix()
        driver.write_text(f'''
#define main guard_fixture_main
#include "{fixture}"
#undef main
unsigned fallback_calls;
{declarations}
static void (*const hooks[])(CPUState*) = {{{functions}}};
int main(void) {{
    for (unsigned i = 0; i < sizeof entries / sizeof entries[0]; ++i) {{
        const u32 entry = entries[i];
        CPUState cpu = state(entry), before = cpu;
        memcpy(saved, ram, sizeof ram); fallback_calls = 0;
        bluewake_composite_native_entries_v1(false, ready, &allow);
        hooks[i](&cpu);
        assert(fallback_calls == 1 && !memcmp(&cpu, &before, sizeof cpu));
        assert(!memcmp(saved, ram, sizeof ram));
        cpu = state(entry); fallback_calls = 0;
        bluewake_composite_native_entries_v1(true, ready, &allow);
        hooks[i](&cpu);
        assert(fallback_calls == 0); check_result(&cpu, entry);
        cpu = state(entry); before = cpu; allow = false;
        memcpy(saved, ram, sizeof ram); fallback_calls = 0;
        hooks[i](&cpu);
        assert(fallback_calls == 1 && !memcmp(&cpu, &before, sizeof cpu));
        assert(!memcmp(saved, ram, sizeof ram));
    }}
    puts("Prepared native hooks: all nine compile, route and preserve observed fallback state");
    return 0;
}}
''')
        helpers = ('native_entries', 'native_fifo', 'native_bg', 'native_vec', 'native_mtxcalc', 'native_search', 'gather_pipe', 'direct_calls')
        executable = root / 'native_entries_hook_test.exe'
        command = [compiler, '-std=c11', '-O2', '-Wno-dll-attribute-on-redeclaration', '-ffp-contract=off',
                   f'-I{ROOT / "cmake/composite"}', f'-I{runtime / "include"}', str(driver)]
        command += [str(p) for p in sorted(folder.glob('*.c'))]
        command += [str(ROOT / f'cmake/composite/{name}.c') for name in helpers]
        command += [str(p) for p in sorted((runtime / 'src/core').glob('cpu*.c'))]
        command += ['-o', str(executable)]
        subprocess.run(command, check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='clang')
    parser.add_argument('--runtime', type=Path, default=ROOT / 'ref/recompcore/GXRuntime')
    args = parser.parse_args()
    run(args.compiler, args.runtime.resolve())
