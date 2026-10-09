"""Compare whole-frame eligibility with the previous real-resolver predicate."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class InlineGprFrameTests(unittest.TestCase):
    def test_generic_and_fixed_ram_owners(self):
        compiler = os.environ.get('BLUEWAKE_TEST_CC') or shutil.which('clang') or shutil.which('cc')
        self.assertIsNotNone(compiler, 'clang or cc required; set BLUEWAKE_TEST_CC')
        runtime = ROOT / 'ref/recompcore/GXRuntime'
        self.assertTrue((runtime / 'include/core/cpu.h').is_file(), 'Pinned runtime required')
        with tempfile.TemporaryDirectory(prefix='bluewake-gpr-frame-') as temp:
            directory = Path(temp)
            for fixed, experimental in ((False, False), (True, False), (False, True), (True, True)):
                with self.subTest(fixed_memory=fixed, experimental_preflight=experimental):
                    output = directory / (('fixed' if fixed else 'generic') + ('-preflight' if experimental else '-default'))
                    if os.name == 'nt':
                        output = output.with_suffix('.exe')
                    argv = [compiler, '-std=c11', '-O3', '-ffp-contract=off',
                            '-I' + str(runtime / 'include'), '-I' + str(ROOT / 'cmake/composite'),
                            str(ROOT / 'tests/inline_gpr_frame_test.c'),
                            str(runtime / 'src/core/cpu.c'), str(runtime / 'src/core/cpu_exception.c'),
                            '-o', str(output)]
                    if fixed:
                        argv += ['-DBW_GUEST_MEM1=fixture_mem1', '-DBW_GUEST_MEM1_SIZE=1024u']
                    if experimental:
                        argv += ['-DBLUEWAKE_EXPERIMENTAL_GPR_FRAME_PREFLIGHT=1']
                    if os.name == 'nt':
                        argv += ['-fms-runtime-lib=dll', '-fuse-ld=lld',
                                 '-Xlinker', '/NODEFAULTLIB:libcmt',
                                 '-Xlinker', '/NODEFAULTLIB:libucrt',
                                 '-lmsvcrt', '-lucrt', '-lvcruntime']
                    else:
                        argv += ['-lm']
                    built = subprocess.run(argv, capture_output=True, text=True)
                    self.assertEqual(built.returncode, 0, built.stderr)
                    result = subprocess.run([str(output)], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn('equivalent readiness queries', result.stdout)


if __name__ == '__main__':
    unittest.main()
