"""Compile the real native FP users with the target-wide load option enabled."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPOSITE = ROOT / "cmake/composite"
SDK = Path(os.environ.get("BLUEWAKE_TEST_CPU_INCLUDE", ROOT / "ref/recompcore/GXRuntime/include"))
CLANG = os.environ.get("BLUEWAKE_TEST_CLANG") or shutil.which("clang")
NATIVE = ("native_j3d.c", "native_game_math.c", "native_skin.c", "native_vec.c", "native_mtxcalc.c")


@unittest.skipUnless(CLANG and (SDK / "core/cpu.h").is_file(), "requires Clang and GXRuntime headers")
class NativeInlineFPCompileTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        output = os.environ.get("BLUEWAKE_TEST_OUTPUT_DIR")
        cls.temp = None if output else tempfile.TemporaryDirectory(prefix="native inline fp ")
        cls.output = Path(output if output else cls.temp.name)
        if output:
            cls.output.mkdir(parents=True, exist_ok=False)
        cls.flags = ["-std=gnu11", "-O2", "-ffp-contract=off", "-DNDEBUG",
                     "-I" + str(COMPOSITE), "-I" + str(SDK),
                     "-DBLUEWAKE_DIRECT_CALLS=1", "-DBLUEWAKE_EDGE_FILTER=1",
                     "-DBLUEWAKE_FIXED_CPU=1", "-DBW_GUEST_MEM1=bw_guest_mem1",
                     "-DBW_GUEST_MEM1_SIZE=0x02000000u", "-DBLUEWAKE_LIBPORPOISE=1",
                     "-DBLUEWAKE_NATIVE_BG_MINMAX=1", "-DBLUEWAKE_NATIVE_QUATERNION=1",
                     "-DBLUEWAKE_NATIVE_GAME_ATAN=1"]

    @classmethod
    def tearDownClass(cls):
        if cls.temp:
            cls.temp.cleanup()

    def compile(self, role, source, extra=(), success=True):
        folder = self.output / role
        folder.mkdir()
        obj = folder / "result.obj"
        argv = [CLANG, *self.flags, *extra, "-c", str(source), "-o", str(obj)]
        (folder / "command.json").write_text(json.dumps(argv, indent=2) + "\n")
        result = subprocess.run(argv, capture_output=True, timeout=90,
                                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        (folder / "stdout.log").write_bytes(result.stdout)
        (folder / "stderr.log").write_bytes(result.stderr)
        diagnostics = result.stderr.decode(errors="replace")
        if success:
            self.assertEqual(result.returncode, 0, diagnostics)
            return obj.read_bytes()
        self.assertNotEqual(result.returncode, 0, diagnostics)
        self.assertFalse(obj.exists())
        return diagnostics

    def source(self, name, text):
        path = self.output / name
        path.write_text(text)
        return path

    def object_payload(self, data):
        if os.name == "nt":
            # Clang's ordinary x64 COFF header records wall-clock seconds.
            # Ignore only that four-byte field, retaining every section,
            # relocation, symbol, constant and export directive byte.
            self.assertGreaterEqual(len(data), 20)
            self.assertEqual(int.from_bytes(data[:2], "little"), 0x8664)
            self.assertGreater(int.from_bytes(data[2:4], "little"), 0)
            self.assertEqual(int.from_bytes(data[16:18], "little"), 0)
            return data[:4] + bytes(4) + data[8:]
        return data

    def test_actual_native_objects_match_disabled_and_old_include(self):
        old = self.output / "old-include"
        old.mkdir()
        for name in NATIVE:
            with self.subTest(source=name):
                source = COMPOSITE / name
                text = source.read_text()
                self.assertEqual(text.count('#include "native_inline_fp.h"'), 1)
                legacy = old / name
                legacy.write_text(text.replace('#include "native_inline_fp.h"', '#include "inline_fp.h"'))
                previous = self.compile(name + "-previous", legacy)
                disabled = self.compile(name + "-disabled", source, ["-DBW_F32_LOAD_HW_WIDEN=0"])
                enabled = self.compile(name + "-enabled", source, ["-DBW_F32_LOAD_HW_WIDEN=1"])
                self.assertEqual(self.object_payload(previous), self.object_payload(disabled))
                self.assertEqual(self.object_payload(disabled), self.object_payload(enabled))

    def test_target_option_is_restored(self):
        for value in (None, "0", "1", "17"):
            with self.subTest(value=value):
                check = ("#ifdef BW_F32_LOAD_HW_WIDEN\n#error option leaked\n#endif\n" if value is None else
                         "#if !defined(BW_F32_LOAD_HW_WIDEN) || BW_F32_LOAD_HW_WIDEN != " + value +
                         "\n#error option changed\n#endif\n")
                source = self.source("restore-" + str(value) + ".c", '#include "native_inline_fp.h"\n' + check)
                self.compile("restore-" + str(value), source,
                             [] if value is None else ["-DBW_F32_LOAD_HW_WIDEN=" + value])

    def test_translated_missing_prerequisites_still_fail(self):
        source = self.source("missing-generated.c", '#include "inline_fp.h"\n')
        error = self.compile("missing-generated", source, ["-DBW_F32_LOAD_HW_WIDEN=1"], success=False)
        self.assertIn("float widening requires gather_pipe.h before the generated header", error)

    def test_native_wrapper_rejects_generated_context(self):
        for macro in ("RECOMP_COMPOSITE_H", "dolrecomp_f32_from_bits"):
            source = self.source(macro + ".c", '#include "native_inline_fp.h"\n')
            error = self.compile(macro, source, ["-D" + macro + "=1"], success=False)
            self.assertIn("only for native arithmetic translation units", error)

    def test_translated_load_include_order_still_compiles(self):
        # Synthetic generated declaration; this checks include/interposition
        # integration, not the already separately tested conversion arithmetic.
        generated = self.source("generated.h", "#ifndef RECOMP_COMPOSITE_H\n#define RECOMP_COMPOSITE_H\n"
                                "static inline f64 dolrecomp_f32_from_bits(u32 bits) { return (f64)bits; }\n#endif\n")
        source = self.source("translated.c", '#include "gather_pipe.h"\n#include "generated.h"\n'
                             '#include "inline_fp.h"\nf64 read_float(u32 bits) { return dolrecomp_f32_from_bits(bits); }\n')
        self.assertTrue(generated.is_file())
        self.compile("translated", source, ["-DBW_F32_LOAD_HW_WIDEN=1"])


if __name__ == "__main__":
    unittest.main()
