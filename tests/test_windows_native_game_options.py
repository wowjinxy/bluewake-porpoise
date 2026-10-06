"""Build/cache wiring for separately qualified native game-math leaves."""
import ast
import contextlib
import importlib.util
import io
import itertools
import json
from pathlib import Path
import shutil
import subprocess
import sys
import unittest
from unittest.mock import patch

import test_windows_dispatch_builder as integration

bw = integration.bw
OPTIONS = ("native_bg_minmax", "native_quaternion", "native_game_atan")
ENTRIES = (0x80247C4C, 0x80301150, 0x802460D0)


class NativeGameOptionsTest(integration.DispatchBuilderTest):
    def setUp(self):
        super().setUp()
        for name in OPTIONS:
            setattr(self.b.args, name, False)
        for name in ("scripts/windows/native_game_math.py", "scripts/windows/direct_calls.py",
                     "cmake/composite/native_game_math.c", "cmake/composite/native_game_math.h"):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(integration.REPO / name, target)
        for chunk, entries in ((0x802456E0, (ENTRIES[0], ENTRIES[2])),
                               (0x802FD6E0, (ENTRIES[1],))):
            text = '#include "../generated.h"\nvoid authored(CPUState* ctx) {\n'
            text += "".join(f"\nlabel_{entry:08X}:\n    return;\n" for entry in entries)
            text += f"\nreturn_dispatch_{chunk:08X}:\n    return;\n}}\n"
            (self.base / f"chunks_dol/chunk_{chunk:08X}.c").write_text(text, newline="\n")
        patch.object(bw, "profile_value", return_value=bw.tree_digest(self.base)).start()

    def source_step(self, name, script, root, *options):
        if name != "native-game-math":
            return super().source_step(name, script, root, *options)
        self.calls.append((name, tuple(options)))
        spec = importlib.util.spec_from_file_location("authored_native_preparer", self.root / script)
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        # Wiring uses authored chunks. Only certificate admission is mocked;
        # actual selection/reset, hook transformation and publication run.
        # Original-fragment and whole-state proofs live in the native oracles.
        with patch.dict(m.FRAGMENTS, {}, clear=True), patch.dict(m.ENTRIES, {}, clear=True), \
             patch.object(m, "watched_addresses", return_value=set()), \
             patch.object(m, "certify", side_effect=lambda *_: set(m.ENTRIES)):
            m.prepare(root, "--enable-bg-minmax" in options,
                      "--enable-quaternion" in options, "--enable-game-atan" in options)

    def test_native_parser_defaults_reversal_and_parent_dependency(self):
        parse = integration.parser_prefix()
        for extra in ([], ["--conservative"]):
            with patch.object(sys, "argv", ["builder", "authored.iso", *extra]):
                args = parse()
            self.assertTrue(all(not getattr(args, name) for name in OPTIONS))
        for name in OPTIONS:
            flag = "--" + name.replace("_", "-")
            with patch.object(sys, "argv", ["builder", "authored.iso", flag]):
                self.assertTrue(getattr(parse(), name))
            with patch.object(sys, "argv", ["builder", "authored.iso", flag, "--no-" + name.replace("_", "-")]):
                self.assertFalse(getattr(parse(), name))
            with patch.object(sys, "argv", ["builder", "authored.iso", "--conservative", flag]), \
                 contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                parse()
        self.assertTrue(all(name not in bw.WINDOWS_DEFAULT_OPTIMIZATIONS for name in OPTIONS))

    def test_native_selections_reuse_disable_and_helper_identity(self):
        self.b.args.native_game_math = True
        previous = None
        for selected in itertools.product((False, True), repeat=3):
            for name, enabled in zip(OPTIONS, selected):
                setattr(self.b.args, name, enabled)
            receipt = self.cycle()
            expected = sorted(entry for entry, enabled in zip(ENTRIES, selected) if enabled)
            manifest = json.loads((self.out / "composite-src/native_game_math.json").read_text())
            self.assertEqual(manifest["entries"], expected)
            self.assertEqual(self.calls[-1], ("native-game-math", tuple(
                flag for name, _, flag in bw.NATIVE_GAME_EXPERIMENTS if getattr(self.b.args, name))))
            self.assertTrue(all(receipt[name] == enabled for name, enabled in zip(OPTIONS, selected)))
            key = (self.out / "composite-inputs.digest").read_text(), self.b.training_fingerprint()
            if previous is not None:
                self.assertNotEqual(previous[0], key[0])
                self.assertNotEqual(previous[1], key[1])
            count = len(self.calls)
            self.cycle()
            self.assertTrue(self.b.preparation_current)
            self.assertEqual(count, len(self.calls))
            previous = key
        for name in OPTIONS:
            setattr(self.b.args, name, False)
        self.cycle()
        for q in (self.out / "composite-src/chunks_dol").glob("*.c"):
            self.assertNotIn("bluewake_native_game_math_try", q.read_text())
        old = (self.out / "composite-inputs.digest").read_text(), self.b.training_fingerprint()
        q = self.root / "cmake/composite/native_game_math.c"
        q.write_bytes(q.read_bytes() + b"\n/* authored helper identity mutation */\n")
        self.cycle()
        self.assertNotEqual(old[0], (self.out / "composite-inputs.digest").read_text())
        self.assertNotEqual(old[1], self.b.training_fingerprint())
        self.b.args.native_game_atan = True
        self.cycle()
        old = (self.out / "composite-inputs.digest").read_text(), self.b.training_fingerprint()
        q = self.root / "scripts/windows/native_game_math.py"
        q.write_bytes(q.read_bytes() + b"\n# authored preparer identity mutation\n")
        self.cycle()
        self.assertNotEqual(old[0], (self.out / "composite-inputs.digest").read_text())
        self.assertNotEqual(old[1], self.b.training_fingerprint())

    def test_actual_preparer_rejects_uncertified_input_without_publication(self):
        spec = importlib.util.spec_from_file_location("negative_native_preparer", self.root / "scripts/windows/native_game_math.py")
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        before = {q.relative_to(self.base): q.read_bytes() for q in self.base.rglob("*") if q.is_file()}
        with patch.object(m, "watched_addresses", return_value=set()), \
             contextlib.redirect_stdout(io.StringIO()), self.assertRaisesRegex(ValueError, "uncertified"):
            m.prepare(self.base, True, True, True)
        after = {q.relative_to(self.base): q.read_bytes() for q in self.base.rglob("*") if q.is_file()}
        self.assertEqual(before, after)

    def test_normal_and_training_compile_selections(self):
        self.b.args.jobs = 1
        self.b.args.jobs_auto = False
        self.b.args.native_game_math = True
        self.b.libporpoise = self.root / "libporpoise"
        self.b.profile = None
        commands = []
        self.b.run = lambda name, argv, **kw: commands.append(list(map(str, argv)))
        build = self.root / "compile"
        build.mkdir()
        (build / bw.MODULE).write_bytes(b"authored command-capture placeholder")
        for selected in itertools.product((False, True), repeat=3):
            for name, enabled in zip(OPTIONS, selected):
                setattr(self.b.args, name, enabled)
            for flags in ([], ["-fprofile-instr-generate"]):
                self.b.compile_composite(build, "2", flags, flags, "capture")
                configure = commands[-2]
                for name, macro, _ in bw.NATIVE_GAME_EXPERIMENTS:
                    self.assertEqual(configure.count(f"-D{macro}={'ON' if getattr(self.b.args, name) else 'OFF'}"), 1)

    def test_package_provenance_records_each_selection(self):
        self.cycle()
        self.b.profile = None
        tree = ast.parse(integration.SOURCE.read_text())
        cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "Builder")
        package = next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name == "package")
        expr = next(n.value for n in package.body if isinstance(n, ast.Assign) and
                    any(isinstance(t, ast.Name) and t.id == "provenance" for t in n.targets))
        app = self.root / "app"
        app.mkdir()
        (app / bw.MODULE).write_bytes(b"authored provenance module input")
        for selected in itertools.product((False, True), repeat=3):
            for name, enabled in zip(OPTIONS, selected):
                setattr(self.b.args, name, enabled)
            result = eval(compile(ast.Expression(expr), str(integration.SOURCE), "eval"),
                          bw.__dict__, {"self": self.b, "dirty": False, "libporpoise": None, "app": app})
            self.assertEqual(tuple(result[name] for name in OPTIONS), selected)

    def test_real_cmake_certificate_and_parent_guards(self):
        cmake = shutil.which("cmake")
        ninja = shutil.which("ninja")
        compiler = shutil.which("clang")
        if not all((cmake, ninja, compiler)):
            self.skipTest("CMake/Ninja/Clang required for configuration guards")
        source = self.root / "cmake/composite"
        for name in ("CMakeLists.txt", "module_thinlto.cmake"):
            shutil.copy2(integration.REPO / "cmake/composite" / name, source / name)
        for name in ("module_export.c", "dispatch_loop.c"):
            (source / name).write_text("/* authored configuration input */\n")
        runtime = self.root / "runtime/GXRuntime"
        for name in ("cpu", "cpu_exception", "cpu_interpreter", "cpu_interpreter_table", "cpu_interpreter_float", "cpu_interpreter_integer"):
            q = runtime / f"src/core/{name}.c"
            q.parent.mkdir(parents=True, exist_ok=True)
            q.write_text("/* authored configuration input */\n")
        abi = self.root / "abi"
        abi.mkdir()
        (abi / "StaticRecompABI.h").write_text("/* authored ABI */\n")
        for name in ("generated_composite.h", "module_tables.inc", "rel_modules.inc", "rel_data.inc"):
            (self.base / name).write_text("/* authored configuration input */\n")
        (self.base / "generated.h").write_text("#define BLUEWAKE_NATIVE_GAME_MATH_PREPARED 1\n")
        manifest = self.base / "native_game_math.json"
        cases = [(selected, True, ENTRIES, True) for selected in itertools.product((False, True), repeat=3)]
        cases += [(tuple(j == i for j in range(3)), True, (), False) for i in range(3)]
        cases += [((True, False, False), False, ENTRIES, False)]
        for index, (selected, parent, certified, expect_success) in enumerate(cases):
            manifest.write_text(json.dumps({"abi": 1, "entries": sorted(certified), "files": {}}, indent=2) + "\n")
            build = self.root / f"cmake-case-{index}"
            argv = [cmake, "-S", str(source), "-B", str(build), "-G", "Ninja",
                    f"-DCMAKE_MAKE_PROGRAM={ninja}", f"-DCMAKE_C_COMPILER={compiler}",
                    "-DCMAKE_C_COMPILER_FORCED=TRUE", "-DCMAKE_C_COMPILER_ID=Clang",
                    "-DCMAKE_C_COMPILER_WORKS=TRUE", "-DCMAKE_C_COMPILER_ABI_COMPILED=TRUE",
                    "-DCOMPOSITE_O1_FALLBACK_SOURCES=",
                    f"-DCOMPOSITE_DIR={self.base}", f"-DGXRUNTIME_DIR={runtime}", f"-DABI_DIR={abi}",
                    f"-DBLUEWAKE_NATIVE_GAME_MATH={'ON' if parent else 'OFF'}"]
            argv += [f"-D{macro}={'ON' if enabled else 'OFF'}" for (_, macro, _), enabled in zip(bw.NATIVE_GAME_EXPERIMENTS, selected)]
            r = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            self.assertEqual(r.returncode == 0, expect_success, r.stdout.decode(errors="replace"))
            if expect_success:
                generated = (build / "build.ninja").read_text()
                for (_, macro, _), enabled in zip(bw.NATIVE_GAME_EXPERIMENTS, selected):
                    self.assertEqual(f"-D{macro}=1" in generated, enabled)
            else:
                self.assertIn(b"require" if not parent else b"certified prepared entry", r.stdout)


if __name__ == "__main__":
    unittest.main()
