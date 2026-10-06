from pathlib import Path
import tempfile
import unittest
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts/windows"))
import cache_callbacks as preparation

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
CPU = ROOT / "ref/recompcore/GXRuntime/src/core/cpu.c"
GATHER = ROOT / "cmake/composite/gather_pipe.h"


def site(xo, cia=0x80001000, name="ppc_fallback_instruction"):
    raw = (31 << 26) | (3 << 21) | (4 << 16) | (5 << 11) | (xo << 1)
    return f"    {name}(ctx, 0x{raw:08X}u, 0x{cia:08X}u);\n    return;\n"


class PreparationTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="cache callback ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "chunks_0").mkdir()
        (self.root / "gather_pipe.h").write_bytes(GATHER.read_bytes())
        self.path = self.root / "chunks_0/test.c"
        self.original = '#include "../generated.h"\n' + ''.join(site(xo) for xo in (54, 86, 470, 982, 467))
        self.path.write_bytes(self.original.encode())

    def prepare(self, enabled=True, cpu=CPU):
        return preparation.prepare(self.root, cpu, ROOT / "cmake/composite/cache_fallback.h", enabled)

    def test_disabled_has_zero_source_or_helper_changes(self):
        self.assertEqual(self.prepare(False), (0, 0))
        self.assertEqual(self.path.read_text(), self.original)
        self.assertFalse((self.root / "cache_fallback.h").exists())

    def test_four_only_whole_inverse_and_idempotent_metadata(self):
        self.assertEqual(self.prepare(), (4, 1))
        actual = self.path.read_text()
        self.assertEqual(actual.count("bw_cache_fallback_instruction"), 4)
        self.assertIn(site(467), actual)
        self.assertEqual(actual.replace(preparation.HELPER_INCLUDE, "").replace(
            "bw_cache_fallback_instruction", "ppc_fallback_instruction"), self.original)
        times = [p.stat().st_mtime_ns for p in (self.path, self.root / "cache_fallback.h")]
        self.assertEqual(self.prepare(), (0, 0))
        self.assertEqual(times, [p.stat().st_mtime_ns for p in (self.path, self.root / "cache_fallback.h")])

    def test_bad_second_chunk_is_transactional_decline(self):
        bad = self.root / "chunks_0/bad.c"
        bad.write_bytes(('#include "../generated.h"\n' + site(54).replace("    return;", "    goto next;")).encode())
        with self.assertRaisesRegex(ValueError, "template"):
            self.prepare()
        self.assertEqual(self.path.read_text(), self.original)
        self.assertFalse((self.root / "cache_fallback.h").exists())

    def test_unknown_runtime_declines_before_write(self):
        bad = self.root / "cpu.c"
        bad.write_bytes(CPU.read_bytes() + b"\n/* another runtime */\n")
        with self.assertRaisesRegex(ValueError, "CPU fallback"):
            self.prepare(cpu=bad)
        self.assertEqual(self.path.read_text(), self.original)

    def test_changed_gather_declines(self):
        (self.root / "gather_pipe.h").write_bytes(b"no drain")
        with self.assertRaisesRegex(ValueError, "gather-pipe"):
            self.prepare()

    def test_prepared_helper_cannot_be_replaced(self):
        self.prepare()
        (self.root / "cache_fallback.h").write_bytes(b"changed")
        before = self.path.read_bytes()
        with self.assertRaisesRegex(ValueError, "helper differs"):
            self.prepare()
        self.assertEqual(self.path.read_bytes(), before)

    def test_unknown_helper_and_noncache_prepared_site_decline(self):
        for extra in ('#define bw_cache_fallback_instruction fake\n', site(467, name="bw_cache_fallback_instruction")):
            with self.subTest(extra=extra):
                with self.assertRaises(ValueError):
                    preparation.convert(self.original + extra)

    def test_source_macro_overrides_decline_before_mutation(self):
        for name in ('ppc_fallback_instruction', 'ppc_program_exception', 'bw_fallback_instruction', 'bw_gather_pipe_drain'):
            with self.subTest(name=name):
                with self.assertRaisesRegex(ValueError, "source overrides"):
                    preparation.convert(f'#define {name} other\n' + self.original)

    def test_pc_overflow_primitive_privileged_bits_other_raw_untouched(self):
        value = '#include "../generated.h"\n' + site(470, 0xFFFFFFFC)
        actual, count = preparation.convert(value)
        self.assertEqual(count, 1)
        self.assertIn("0xFFFFFFFCu", actual)
        self.assertNotIn("ctx->pc =", actual)
        self.assertEqual(preparation.convert('#include "../generated.h"\n' + site(470).replace(
            '0x7C642BACu', '0x00642BACu'))[1], 0)


if __name__ == "__main__":
    unittest.main()
