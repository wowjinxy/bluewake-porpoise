"""Private Builder option/cache proof; real file digest, no compiler/native call."""
import argparse
import ast
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[1]
SOURCE = REPO / "scripts/windows/build.py"
spec = importlib.util.spec_from_file_location("private_thinlto_builder", SOURCE)
bw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bw)
TREE = ast.parse(SOURCE.read_text())


class ThinLTOBuilderTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="module ThinLTO ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.repo = self.root / "repo"
        (self.repo / "cmake/composite").mkdir(parents=True)
        (self.repo / "cmake/composite/module_thinlto.cmake").write_bytes(
            (REPO / "cmake/composite/module_thinlto.cmake").read_bytes())
        manifest = self.repo / "scripts/ios/composite_manifest.py"
        manifest.parent.mkdir(parents=True)
        manifest.write_bytes((REPO / "scripts/ios/composite_manifest.py").read_bytes())
        self.old_root = bw.ROOT
        bw.ROOT = self.repo
        self.addCleanup(setattr, bw, "ROOT", self.old_root)
        self.builder = bw.Builder.__new__(bw.Builder)
        self.builder.args = SimpleNamespace(**{name: False for name in (
            "fixed_cpu", "fixed_mem1", "inline_fp", "gather_pipe", "direct_calls",
            "inline_gpr", "native_j3d", "native_vec", "native_math", "native_skin",
            "native_game_math", "native_entries", "lean_memory", "defer_pc",
            "indirect_tails", "libporpoise", "f32_hw_widen", "module_thinlto",
            "prepared_blocks", "accept_new_composite", "jobs_auto")})
        self.builder.args.march = "x86-64-v3"
        self.builder.args.jobs = 1
        self.builder.out = self.root / "out"
        (self.builder.out / "composite-src").mkdir(parents=True)
        (self.builder.out / "composite-src/chunk.c").write_text("int authored(void){return 7;}\n")
        self.builder.recompcore = self.root / "runtime"
        self.builder.libporpoise = self.root / "libporpoise"
        self.builder.mods = False
        self.builder.profile = None
        self.builder.clang_version = "authored compiler identity A"
        self.builder.runtime_patch_receipt = {"identity": "authored runtime"}
        self.builder.libporpoise_inputs = lambda: None
        self.builder.git = lambda *args, **kwargs: "authored revision"
        self.commands = []
        self.builder.run = lambda name, argv, **kwargs: self.commands.append(
            {"name": name, "argv": list(map(str, argv)), "kwargs": kwargs})

    def compile(self, enabled, flags=(), links=()):
        self.builder.args.module_thinlto = enabled
        build = self.root / ("thin" if enabled else "plain")
        build.mkdir(exist_ok=True)
        (build / bw.MODULE).write_bytes(b"authored captured-commands placeholder")
        self.builder.compile_composite(build, "2", list(flags), list(links), "captured")
        return self.commands[-2]["argv"]

    def test_parser_is_off_by_default_and_explicitly_reversible(self):
        main = next(n for n in TREE.body if isinstance(n, ast.FunctionDef) and n.name == "main")
        prefix = []
        for node in main.body:
            if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "args" for t in node.targets):
                break
            prefix.append(node)
        prefix.append(ast.Return(value=ast.Name(id="parser", ctx=ast.Load())))
        fn = ast.FunctionDef(name="make_parser", args=main.args, body=prefix,
                             decorator_list=[], returns=None, type_comment=None)
        scope = dict(bw.__dict__)
        exec(compile(ast.fix_missing_locations(ast.Module(body=[fn], type_ignores=[])), str(SOURCE), "exec"), scope)
        parser = scope["make_parser"]()
        self.assertFalse(parser.parse_args(["authored.iso"]).module_thinlto)
        self.assertTrue(parser.parse_args(["authored.iso", "--module-thinlto"]).module_thinlto)
        self.assertFalse(parser.parse_args(["authored.iso", "--module-thinlto", "--no-module-thinlto"]).module_thinlto)
        self.assertNotIn("module_thinlto", bw.WINDOWS_DEFAULT_OPTIMIZATIONS)

    def test_configure_uses_same_option_for_normal_and_training_builds(self):
        for enabled in (False, True):
            normal = self.compile(enabled)
            training = self.compile(enabled, ["-fprofile-instr-generate"], ["-fprofile-instr-generate"])
            expected = "-DBLUEWAKE_MODULE_THINLTO=" + ("ON" if enabled else "OFF")
            self.assertEqual(normal.count(expected), 1)
            self.assertEqual(training.count(expected), 1)
            self.assertIn("-fprofile-instr-generate", next(x for x in training if x.startswith("-DCMAKE_C_FLAGS=")))
            self.assertIn("-fprofile-instr-generate", next(x for x in training if x.startswith("-DCMAKE_SHARED_LINKER_FLAGS=")))
            self.assertIn("-ffp-contract=off", (REPO / "cmake/composite/CMakeLists.txt").read_text())

    def test_profile_use_and_training_extra_link_flags_are_preserved(self):
        argv = self.compile(True, ["-fprofile-instr-use=authored.profdata"], ["-Wl,/debug:none"])
        self.assertIn("-fprofile-instr-use=authored.profdata", next(x for x in argv if x.startswith("-DCMAKE_C_FLAGS=")))
        self.assertIn("-Wl,/debug:none", next(x for x in argv if x.startswith("-DCMAKE_SHARED_LINKER_FLAGS=")))
        self.assertIn("-fuse-ld=lld", next(x for x in argv if x.startswith("-DCMAKE_SHARED_LINKER_FLAGS=")))

    def test_training_key_binds_option_helper_source_compiler_and_prepared_bytes(self):
        b = self.builder
        off = b.training_fingerprint()
        self.assertEqual(off, b.training_fingerprint())
        b.args.module_thinlto = True
        thin = b.training_fingerprint()
        self.assertNotEqual(off, thin)
        b.args.module_thinlto = False
        self.assertEqual(off, b.training_fingerprint())
        b.clang_version += " changed"
        self.assertNotEqual(off, b.training_fingerprint())
        b.clang_version = "authored compiler identity A"
        (b.out / "composite-src/chunk.c").write_text("int authored(void){return 8;}\n")
        changed = b.training_fingerprint()
        self.assertNotEqual(off, changed)
        with (self.repo / "cmake/composite/module_thinlto.cmake").open("a") as f:
            f.write("# authored recipe mutation\n")
        self.assertNotEqual(changed, b.training_fingerprint())

    def test_generation_input_identity_binds_option_and_helper(self):
        b = self.builder
        def composite(*args):
            new = args[-2]
            new.mkdir()
            (new / "chunk.c").write_text("int authored(void){return 7;}\n")
        b.composite = composite
        b.preparation_current = False
        expected = bw.tree_digest(b.out / "composite-src")
        with patch.object(bw, "profile_value", return_value=expected):
            b.generate()
            key = (b.out / "composite-inputs.digest").read_text()
            b.generate()
            self.assertEqual(key, (b.out / "composite-inputs.digest").read_text())
            b.args.module_thinlto = True
            b.generate()
            thin = (b.out / "composite-inputs.digest").read_text()
            self.assertNotEqual(key, thin)
            with (self.repo / "cmake/composite/module_thinlto.cmake").open("a") as f:
                f.write("# authored helper mutation\n")
            b.generate()
            self.assertNotEqual(thin, (b.out / "composite-inputs.digest").read_text())

    def test_preparation_receipt_records_the_selected_value(self):
        # Only required receipt files are authored here; no preparer runs.
        for relative in ("cmake/composite/gather_pipe.h", "cmake/composite/gather_pipe.c",
                         "cmake/composite/gather_pipe_batch.h", "scripts/windows/chunk_headers.py",
                         "cmake/composite/inline_fp.h", "scripts/windows/global_guest_cpu.py",
                         "scripts/windows/fast_blocks.py"):
            p = self.repo / relative
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("authored receipt source\n")
        b = self.builder
        (b.out / "composite-src.digest").write_text(bw.tree_digest(b.out / "composite-src"))
        for enabled in (False, True):
            b.args.module_thinlto = enabled
            b.prepare_blocks()
            receipt = json.loads((b.out / "prepared-blocks.json").read_text())
            self.assertIs(receipt["module_thinlto"], enabled)

    def test_provenance_expression_records_the_selected_value(self):
        klass = next(n for n in TREE.body if isinstance(n, ast.ClassDef) and n.name == "Builder")
        package = next(n for n in klass.body if isinstance(n, ast.FunctionDef) and n.name == "package")
        expression = next(n.value for n in package.body if isinstance(n, ast.Assign) and
                          any(isinstance(t, ast.Name) and t.id == "provenance" for t in n.targets))
        app = self.root / "app"
        app.mkdir()
        (app / bw.MODULE).write_bytes(b"authored module bytes")
        (self.builder.out / "composite-src.digest").write_text("authored prepared digest")
        for enabled in (False, True):
            self.builder.args.module_thinlto = enabled
            value = eval(compile(ast.Expression(expression), str(SOURCE), "eval"),
                         bw.__dict__, {"self": self.builder, "dirty": False,
                                       "libporpoise": None, "app": app})
            self.assertIs(value["module_thinlto"], enabled)
            self.assertEqual(value["compiler"], self.builder.clang_version)



if __name__ == "__main__":
    unittest.main(verbosity=2)
