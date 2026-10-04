"""Compile original/fast/lean synthetic chunks against the real memory ABI.

No game source is needed. The fixture compares complete CPU, RAM, aliases and
observer traces, including refunds that resume inside the original block.
"""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts/windows" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


fast = load("fast_blocks")
lean = load("lean_memory")


def source(bits=32, loop=False, early=False, omit_pc=False):
    bodies = [
        f"    ctx->gpr[3] = (u32)mem_read{bits}(ctx, ctx->gpr[4]);",
        "    ctx->gpr[3] += ctx->gpr[8];",
        f"    mem_write{bits}(ctx, ctx->gpr[5], ctx->gpr[3]);",
        "    ctx->gpr[8] ^= ctx->gpr[3];",
        f"    ctx->gpr[7] = (u32)mem_read{bits}(ctx, ctx->gpr[4]) ^ ctx->pc ^ ctx->cycle_observation_suffix;",
        "    observe(ctx);",
        f"    mem_write{bits}(ctx, ctx->gpr[6], ctx->gpr[7]);",
        "    ctx->gpr[9] ^= ctx->gpr[3];",
    ]
    if early:
        bodies[3] += "\n    if (ctx->gpr[8] & 1u) return;"
    lines = [fast.INCLUDE.rstrip(), "void test_chunk(CPUState* ctx) {",
             "    bool cycle_block_prepaid;", "    ctx->pc = 0x80004000u;",
             "    cycle_block_prepaid = dolrecomp_block_can_precharge(ctx, 8u);"]
    if loop:
        lines += ["    if (cycle_block_prepaid) {",
                  "        if (ctx->downcount <= -(s64)DOLRECOMP_C_LOOP_CYCLE_BUDGET) {",
                  "            ctx->pc = 0x80004000u;", "            return;", "        }",
                  "        ctx->downcount -= 8;", "    }"]
    else:
        lines += ["    if (ctx->downcount <= -(s64)DOLRECOMP_C_LOOP_CYCLE_BUDGET) {",
                  "        ctx->pc = 0x80004000u;", "        return;", "    }",
                  "    ctx->downcount -= cycle_block_prepaid ? 8u : 1u;"]
    for i, body in enumerate(bodies):
        pc = 0x80004000 + 4 * i
        if i or loop:
            lines += [f"label_{pc:08X}:"]
            if not omit_pc:
                lines += [f"    ctx->pc = 0x{pc:08X}u;"]
            lines += [f"    if (!cycle_block_prepaid && !dolrecomp_charge_precise(ctx, 1u, 0x{pc:08X}u)) return;"]
        if "mem_" in body:
            lines += [f"    ctx->cycle_observation_suffix = cycle_block_prepaid ? {7 - i}u : 0u;"]
        lines += [f"    // {pc:08X}: synthetic instruction", body]
        if "mem_" in body:
            lines += fast.REFUND
    return "\n".join(lines + ["}", ""])


class Preparation(unittest.TestCase):
    def test_refunds_pc_deferral_and_complete_state_flush(self):
        copied, count = fast.transform(source())
        self.assertEqual(count, 1)
        self.assertIn("goto bwslow_0_1;", copied)
        self.assertIn("goto bwslow_0_7;", copied)
        prepared, accesses = lean.transform(copied)
        self.assertEqual(accesses, 3)  # metadata-reading instruction keeps ordinary path
        self.assertIn("bw_read32_at_observed(ctx, (ctx->pc), 7u", prepared)
        self.assertIn("ctx->cycle_observation_suffix = 1u;", prepared)
        self.assertEqual(fast.transform(copied), (copied, 0))
        self.assertEqual(lean.transform(prepared), (prepared, 0))

    def test_unknown_prepaid_state_and_mid_instruction_refunds_decline(self):
        for changed in (source().replace("    observe(ctx);", "    cycle_block_prepaid = false;"),
                        source().replace("    }\nlabel_80004004", "    }\n    observe(ctx);\nlabel_80004004", 1)):
            self.assertEqual(fast.transform(changed), (changed, 0))

    def test_unknown_builtin_and_metadata_read_are_observations(self):
        changed = source().replace("    ctx->gpr[3] += ctx->gpr[8];",
                                   "    ctx->gpr[3] = __builtin_unknown_observer(ctx);")
        copied, _ = fast.transform(changed)
        self.assertIn("ctx->pc = 0x80004004u;\n    // 80004004", copied[copied.index("bwfast_0:"):])
        prepared, _ = lean.transform(copied)
        self.assertIn("cycle_observation_suffix = 7u;", prepared[prepared.index("bwfast_0:"):])
        self.assertIn("(u32)mem_read32(ctx, ctx->gpr[4]) ^ ctx->pc", prepared)

    def test_reserved_ranges_and_unrecognized_copy(self):
        changed = source().replace("80004000", "8030D0C8")
        self.assertEqual(fast.transform(changed), (changed, 0))
        with self.assertRaisesRegex(ValueError, "no end"):
            lean.transform(fast.MARK + "bwfast_0:\n")

    def test_compiled_equivalence(self):
        runtime = ROOT / "ref/recompcore/GXRuntime"
        if not (runtime / "include/core/cpu.h").is_file():
            self.skipTest("compiled memory ABI fixture requires pinned ref/recompcore")
        compiler = os.environ.get("BLUEWAKE_TEST_CC") or shutil.which("clang") or shutil.which("cc")
        if not compiler:
            self.fail("clang or cc required; set BLUEWAKE_TEST_CC")
        cases, table = [], []
        for bits in (8, 16, 32, 64):
            for loop in (False, True):
                for early in (False, True):
                    for omit_pc in (False, True):
                        original = source(bits, loop, early, omit_pc)
                        copied, count = fast.transform(original)
                        self.assertEqual(count, 1)
                        prepared, accesses = lean.transform(copied)
                        self.assertEqual(accesses, 3)
                        names = []
                        for kind, text in (("original", original), ("fast", copied), ("lean", prepared)):
                            name = f"chunk_{bits}_{int(loop)}_{int(early)}_{int(omit_pc)}_{kind}"
                            names.append(name)
                            cases.append(text.replace(fast.INCLUDE, "").replace("test_chunk", name))
                        table.append("{" + ", ".join(names) + "}")
        with tempfile.TemporaryDirectory(prefix="bluewake-prepaid-") as tmp:
            directory = Path(tmp)
            (directory / "rewritten_cases.h").write_text("\n".join(cases) +
                "\nstatic Run cases[][3] = {" + ",\n".join(table) + "};\n", encoding="utf-8")
            exe = directory / ("fixture.exe" if os.name == "nt" else "fixture")
            command = [compiler, "-std=c11", "-O2", "-I" + str(directory),
                       "-I" + str(runtime / "include"), "-I" + str(ROOT / "cmake/composite"),
                       str(ROOT / "tests/prepaid_rewrite_fixture.c"),
                       str(ROOT / "cmake/composite/gather_pipe.c"),
                       str(runtime / "src/core/cpu.c"), str(runtime / "src/core/cpu_exception.c"),
                       "-o", str(exe)]
            if os.name != "nt":
                command += ["-lm"]
            compiled = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("96000 equivalent runs", result.stdout)


if __name__ == "__main__":
    unittest.main()
