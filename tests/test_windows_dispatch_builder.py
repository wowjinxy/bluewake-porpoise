"""Builder/cache integration with authored chunks and actual source preparers."""
import ast
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import runpy
import shutil
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[1]
SOURCE = REPO / "scripts/windows/build.py"
spec = importlib.util.spec_from_file_location("dispatch_builder", SOURCE)
bw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bw)
OPTIONS = ("dispatch_slots", "return_ranges", "inline_cache_callbacks")
CHUNK = '''#include "../generated.h"
void func_80001000(CPUState* ctx) {
    switch (ctx->pc) {
    case 0x80001000u: goto label_80001000;
    case 0x80001010u: goto label_80001010;
    default: return;
    }
label_80001000:
    ppc_fallback_instruction(ctx, 0x7C00006Cu, 0x80001000u);
    return;
label_80001010:
    goto return_dispatch_80001000;
return_dispatch_80001000:
    if (ctx->downcount <= -(s64)DOLRECOMP_C_LOOP_CYCLE_BUDGET) return;
    switch (ctx->pc) {
    case 0x80001000u: goto label_80001000;
    case 0x80001010u: goto label_80001010;
    default: return;
    }
}
'''


def parser_prefix():
    """Execute the real parser/default/dependency code, stopping before I/O."""
    main = next(n for n in ast.parse(SOURCE.read_text()).body
                if isinstance(n, ast.FunctionDef) and n.name == "main")
    body = []
    for n in main.body:
        if (isinstance(n, ast.Assign) and any(isinstance(t, ast.Attribute)
                and t.attr == "jobs_auto" for t in n.targets)):
            break
        body.append(n)
    body.append(ast.Return(ast.Name("args", ast.Load())))
    fn = ast.FunctionDef("parse_selection", main.args, body, [], None, None)
    scope = dict(bw.__dict__)
    exec(compile(ast.fix_missing_locations(ast.Module([fn], [])), str(SOURCE), "exec"), scope)
    return scope["parse_selection"]


class DispatchBuilderTest(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="dispatch builder ")
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        for name in ("scripts/ios/composite_manifest.py", "scripts/windows/fast_blocks.py",
                     "scripts/windows/global_guest_cpu.py", "scripts/windows/chunk_headers.py",
                     "scripts/windows/dispatch_prepare.py", "scripts/windows/cache_callbacks.py",
                     "cmake/composite/cache_fallback.h", "cmake/composite/inline_fp.h",
                     "cmake/composite/gather_pipe.h", "cmake/composite/gather_pipe.c",
                     "cmake/composite/gather_pipe_batch.h"):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(REPO / name, target)
        self.base = self.root / "base"
        (self.base / "chunks_dol").mkdir(parents=True)
        (self.base / "generated.h").write_bytes(b"/* authored header */\n")
        (self.base / "chunks_dol/a.c").write_bytes(CHUNK.encode())
        self.out = self.root / "out"
        self.out.mkdir()
        names = ("prepared_blocks", "fixed_cpu", "fixed_mem1", "inline_fp", "gather_pipe",
                 "direct_calls", "inline_gpr", "native_j3d", "native_vec", "native_math",
                 "native_skin", "native_game_math", "native_entries", "lean_memory",
                 "libporpoise", "f32_hw_widen", "module_thinlto", *OPTIONS)
        args = SimpleNamespace(out=self.out, accept_new_composite=False, march="x86-64-v3",
                               **dict.fromkeys(names, False))
        self.b = bw.Builder(args)
        self.b.recompcore = self.root / "runtime"
        self.b.mods = False
        self.b.clang_version = "authored compiler"
        self.b.git = lambda *a: "authored revision"
        self.b.runtime_patch_receipt = {"authored": True}
        self.b.composite = lambda *a: shutil.copytree(self.base, a[-2])
        self.cpu = self.b.recompcore / "GXRuntime/src/core/cpu.c"
        self.cpu.parent.mkdir(parents=True)
        self.cpu.write_bytes(b"/* authored CPU contract; no machine execution */\n")
        self.cpu_pin = bw.sha256_file(self.cpu)
        self.calls = []
        self.b.source_step = self.source_step
        self.addCleanup(patch.stopall)
        patch.object(bw, "ROOT", self.root).start()
        patch.object(bw, "profile_value", return_value=bw.tree_digest(self.base)).start()

    def source_step(self, name, script, root, *options):
        self.calls.append(name)
        script = self.root / script
        with patch.object(sys, "argv", [str(script), str(root), *map(str, options)]):
            if name == "cache-callbacks":
                spec = importlib.util.spec_from_file_location("authored_cache_preparer", script)
                module = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(module)
                # Only the input SHA pin names authored CPU bytes. The actual
                # preparer/CLI and gather/helper bytes are unchanged. The real
                # CPU contract has separate C differential qualification.
                module.CPU_SHA = self.cpu_pin
                module.main()
            else:
                runpy.run_path(str(script), run_name="__main__")

    def cycle(self):
        with contextlib.redirect_stdout(io.StringIO()):
            self.b.generate()
            self.b.prepare_blocks()
        return json.loads((self.out / "prepared-blocks.json").read_text())

    def chunk(self):
        return self.out / "composite-src/chunks_dol/a.c"

    def test_parser_defaults_reversal_and_gather_dependency(self):
        parse = parser_prefix()
        for argv in (["authored.iso"], ["authored.iso", "--conservative"]):
            with patch.object(sys, "argv", ["builder", *argv]):
                args = parse()
            self.assertTrue(all(not getattr(args, n) for n in OPTIONS))
        for option in OPTIONS:
            flags = ["--" + option.replace("_", "-")]
            with patch.object(sys, "argv", ["builder", "authored.iso", *flags]):
                self.assertTrue(getattr(parse(), option))
            flags.append("--no-" + option.replace("_", "-"))
            with patch.object(sys, "argv", ["builder", "authored.iso", *flags]):
                self.assertFalse(getattr(parse(), option))
        with patch.object(sys, "argv", ["builder", "authored.iso", "--conservative", "--inline-cache-callbacks"]), contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                parse()
        self.assertTrue(all(n not in bw.WINDOWS_DEFAULT_OPTIMIZATIONS for n in OPTIONS))

    def test_all_options_reuse_disable_and_tamper_regeneration(self):
        self.cycle()
        self.assertEqual(self.chunk().read_text(), CHUNK)
        self.b.args.gather_pipe = True
        for n in OPTIONS:
            setattr(self.b.args, n, True)
        selected = self.cycle()
        self.assertIn("bw_dispatch_pc_slot", self.chunk().read_text())
        self.assertIn("bw_cache_fallback_instruction", self.chunk().read_text())
        self.assertIn("ctx->pc - 0x80001000u", self.chunk().read_text())
        self.assertTrue(all(selected[n] for n in OPTIONS))
        # Production resolves gather_pipe.h through cmake/composite's include
        # directory. Source preparation must not assume a generated copy.
        self.assertFalse((self.out / "composite-src/gather_pipe.h").exists())
        self.assertEqual(self.calls[-3:], ["inline-helpers", "dispatch-preparation", "cache-callbacks"])
        before = self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns
        count = len(self.calls)
        self.cycle()
        self.assertTrue(self.b.preparation_current)
        self.assertEqual(count, len(self.calls))
        self.assertEqual(before, (self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns))
        self.chunk().write_bytes(b"/* interrupted/edited preparation */\n")
        self.cycle()
        self.assertEqual(before[0], self.chunk().read_bytes())
        for n in OPTIONS:
            setattr(self.b.args, n, False)
        self.b.args.gather_pipe = False
        off = self.cycle()
        self.assertEqual(self.chunk().read_text(), CHUNK)
        self.assertTrue(all(not off[n] for n in OPTIONS))
        self.assertFalse((self.out / "composite-src/cache_fallback.h").exists())

    def test_helper_and_runtime_hashes_invalidate_preparation_and_training(self):
        for n in OPTIONS:
            setattr(self.b.args, n, True)
        self.b.args.gather_pipe = True
        self.cycle()
        key = self.b.training_fingerprint()
        old = (self.out / "composite-inputs.digest").read_text()
        for name in ("scripts/windows/dispatch_prepare.py", "scripts/windows/cache_callbacks.py",
                     "cmake/composite/cache_fallback.h"):
            p = self.root / name
            p.write_bytes(p.read_bytes() + b"\n" + (b"# authored identity mutation\n" if p.suffix == ".py" else b"/* authored identity mutation */\n"))
            self.assertNotEqual(key, self.b.training_fingerprint())
            self.cycle()
            new = (self.out / "composite-inputs.digest").read_text()
            self.assertNotEqual(old, new)
            old, key = new, self.b.training_fingerprint()
        self.cpu.write_bytes(self.cpu.read_bytes() + b"/* changed contract */\n")
        self.assertNotEqual(key, self.b.training_fingerprint())
        with contextlib.redirect_stdout(io.StringIO()):
            self.b.generate()
            with self.assertRaisesRegex(ValueError, "CPU fallback contract"):
                self.b.prepare_blocks()
        self.assertNotIn("bw_cache_fallback_instruction", self.chunk().read_text())
        self.assertNotEqual(bw.tree_digest(self.out / "composite-src"),
                            (self.out / "composite-final.digest").read_text().strip())

    def test_each_selection_has_an_independent_generation_and_training_identity(self):
        self.b.args.gather_pipe = True
        self.cycle()
        baseline = self.chunk().read_bytes()
        previous = (self.out / "composite-inputs.digest").read_text()
        training = self.b.training_fingerprint()
        for name in OPTIONS:
            for enabled in (True, False):
                setattr(self.b.args, name, enabled)
                receipt = self.cycle()
                new = (self.out / "composite-inputs.digest").read_text()
                self.assertNotEqual(previous, new)
                self.assertNotEqual(training, self.b.training_fingerprint())
                self.assertIs(receipt[name], enabled)
                if not enabled:
                    self.assertEqual(baseline, self.chunk().read_bytes())
                previous, training = new, self.b.training_fingerprint()


if __name__ == "__main__":
    unittest.main()
