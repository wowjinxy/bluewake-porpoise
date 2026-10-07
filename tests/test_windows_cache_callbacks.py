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

    def test_lf_and_crlf_runtime_contracts_accept_without_rewriting_inputs(self):
        cpu_lf = CPU.read_bytes().replace(b"\r\n", b"\n")
        gather_lf = GATHER.read_bytes().replace(b"\r\n", b"\n")
        cpu = self.root / "cpu.c"
        gather = self.root / "gather_pipe.h"
        for cpu_eol in (b"\n", b"\r\n"):
            for gather_eol in (b"\n", b"\r\n"):
                with self.subTest(cpu_eol=cpu_eol, gather_eol=gather_eol):
                    cpu_bytes = cpu_lf.replace(b"\n", cpu_eol)
                    gather_bytes = gather_lf.replace(b"\n", gather_eol)
                    cpu.write_bytes(cpu_bytes)
                    gather.write_bytes(gather_bytes)
                    self.path.write_bytes(self.original.encode())
                    self.assertEqual(self.prepare(cpu=cpu), (4, 1))
                    self.assertEqual(cpu.read_bytes(), cpu_bytes)
                    self.assertEqual(gather.read_bytes(), gather_bytes)
                    self.assertEqual(self.path.read_text().replace(preparation.HELPER_INCLUDE, "").replace(
                        "bw_cache_fallback_instruction", "ppc_fallback_instruction"), self.original)

    def test_mutated_runtime_tokens_decline_with_either_line_ending(self):
        cpu_lf = CPU.read_bytes().replace(b"\r\n", b"\n")
        gather_lf = GATHER.read_bytes().replace(b"\r\n", b"\n")
        cpu = self.root / "cpu.c"
        gather = self.root / "gather_pipe.h"
        for eol in (b"\n", b"\r\n"):
            for contract in ("CPU fallback", "gather-pipe"):
                with self.subTest(eol=eol, contract=contract):
                    cpu_bytes = cpu_lf
                    gather_bytes = gather_lf
                    if contract == "CPU fallback":
                        self.assertIn(b"PPC_PROGRAM_ILLEGAL", cpu_bytes)
                        cpu_bytes = cpu_bytes.replace(b"PPC_PROGRAM_ILLEGAL", b"PPC_PROGRAM_PRIV", 1)
                    else:
                        self.assertIn(b"    bw_gather_pipe_drain();", gather_bytes)
                        gather_bytes = gather_bytes.replace(b"    bw_gather_pipe_drain();", b"    /* no drain */", 1)
                    cpu.write_bytes(cpu_bytes.replace(b"\n", eol))
                    gather.write_bytes(gather_bytes.replace(b"\n", eol))
                    with self.assertRaisesRegex(ValueError, contract):
                        self.prepare(cpu=cpu)
                    self.assertEqual(self.path.read_bytes(), self.original.encode())
                    self.assertFalse((self.root / "cache_fallback.h").exists())

    def test_lone_cr_contracts_still_decline(self):
        cpu = self.root / "cpu.c"
        gather = self.root / "gather_pipe.h"
        for contract in ("CPU fallback", "gather-pipe"):
            with self.subTest(contract=contract):
                cpu_bytes = CPU.read_bytes().replace(b"\r\n", b"\n")
                gather_bytes = GATHER.read_bytes().replace(b"\r\n", b"\n")
                if contract == "CPU fallback":
                    cpu_bytes = cpu_bytes.replace(b"\n", b"\r", 1)
                else:
                    gather_bytes = gather_bytes.replace(b"\n", b"\r", 1)
                cpu.write_bytes(cpu_bytes)
                gather.write_bytes(gather_bytes)
                with self.assertRaisesRegex(ValueError, contract):
                    self.prepare(cpu=cpu)
                self.assertEqual(self.path.read_bytes(), self.original.encode())
                self.assertFalse((self.root / "cache_fallback.h").exists())

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

    def test_explicit_compiler_include_header_needs_no_generated_copy(self):
        (self.root / "gather_pipe.h").unlink()
        self.assertEqual(preparation.prepare(
            self.root, CPU, ROOT / "cmake/composite/cache_fallback.h", True,
            gather_header=GATHER), (4, 1))
        self.assertFalse((self.root / "gather_pipe.h").exists())

    def test_explicit_changed_gather_declines_before_publication(self):
        bad = self.root / "different-gather.h"
        bad.write_bytes(GATHER.read_bytes() + b"/* changed contract */\n")
        with self.assertRaisesRegex(ValueError, "gather-pipe"):
            preparation.prepare(self.root, CPU, ROOT / "cmake/composite/cache_fallback.h",
                                True, gather_header=bad)
        self.assertEqual(self.path.read_text(), self.original)
        self.assertFalse((self.root / "cache_fallback.h").exists())

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
