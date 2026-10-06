"""Check float-widening target flags and prerequisite failures without game data."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(all(shutil.which(x) for x in ("cmake", "ninja", "clang")), "requires CMake, Ninja and Clang")
class FloatWideningCMakeTest(unittest.TestCase):
    def test_target_definition_and_prerequisites(self):
        with tempfile.TemporaryDirectory(prefix="f32 cmake ") as tmp:
            root = Path(tmp)
            composite = root / "source"
            (composite / "chunks_dol").mkdir(parents=True)
            for name in ("generated.h", "generated_composite.h", "module_tables.inc", "rel_modules.inc", "rel_data.inc"):
                (composite / name).write_text("/* synthetic configuration input */\n")
            (composite / "chunks_dol/example.c").write_text(
                '#include "gather_pipe.h"\n#include "../generated.h"\n#include "inline_fp.h"\n')
            sdk = root / "sdk"
            (sdk / "src/core").mkdir(parents=True)
            for name in ("cpu", "cpu_exception", "cpu_interpreter", "cpu_interpreter_table",
                         "cpu_interpreter_float", "cpu_interpreter_integer"):
                (sdk / ("src/core/" + name + ".c")).write_text("/* not compiled */\n")
            abi = root / "abi"; abi.mkdir()
            (abi / "StaticRecompABI.h").write_text("/* synthetic ABI */\n")
            cases = ((False, False, False), (False, True, True), (True, True, True),
                     (True, False, False), (True, True, False), (True, False, True))
            for i, (enabled, inline, gather) in enumerate(cases):
                with self.subTest(enabled=enabled, inline=inline, gather=gather):
                    output = root / str(i)
                    args = ["cmake", "-G", "Ninja", "-S", str(ROOT / "cmake/composite"), "-B", str(output),
                            "-DCMAKE_C_COMPILER=" + shutil.which("clang"), "-DCMAKE_C_COMPILER_FORCED=TRUE",
                            "-DCOMPOSITE_OPTIMIZATION_LEVEL=1", "-DCOMPOSITE_DIR=" + str(composite),
                            "-DGXRUNTIME_DIR=" + str(sdk), "-DABI_DIR=" + str(abi),
                            "-DBLUEWAKE_F32_HW_WIDEN=" + ("ON" if enabled else "OFF"),
                            "-DBLUEWAKE_INLINE_FP=" + ("ON" if inline else "OFF"),
                            "-DBLUEWAKE_GATHER_PIPE=" + ("ON" if gather else "OFF")]
                    result = subprocess.run(args, capture_output=True, text=True, timeout=60,
                                            creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
                    if enabled and not (inline and gather):
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn("Float widening requires BLUEWAKE_INLINE_FP and BLUEWAKE_GATHER_PIPE",
                                      result.stdout + result.stderr)
                    else:
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        definitions = (output / "build.ninja").read_text()
                        self.assertEqual("-DBW_F32_LOAD_HW_WIDEN=1" in definitions, enabled)


if __name__ == "__main__":
    unittest.main()
