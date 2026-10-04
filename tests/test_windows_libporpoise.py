"""Pinned SDK preparation and training contracts with synthetic public sources."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("porpoise_windows_builder", REPO / "scripts/windows/build.py")
bw = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bw)
OPTIONS = ("prepared_blocks", "fixed_cpu", "fixed_mem1", "inline_fp", "gather_pipe", "direct_calls",
           "inline_gpr", "native_j3d", "native_vec", "native_math", "native_skin", "native_game_math",
           "native_entries", "lean_memory", "libporpoise")


class LibPorpoiseBuilderTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="porpoise builder ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.out = self.root / "output"
        self.out.mkdir()
        args = SimpleNamespace(out=self.out, march="x86-64", jobs=1, opt_level="2", retrain=False,
                               accept_new_composite=False, **dict.fromkeys(OPTIONS, False))
        self.builder = bw.Builder(args)
        self.builder.mods = False
        self.builder.clang_version = "synthetic clang"
        self.builder.libporpoise = self.root / "ref/libporpoise"
        self.source = self.root / "upstream"
        self.source.mkdir()
        for relative, text in (("LICENSE", "synthetic MIT notice"), ("include/dolphin/types.h", "/* types */"),
                               ("include/dolphin/mtx.h", "/* matrix API */"),
                               ("include/dolphin/vec.h", "/* vector API */"),
                               ("include/dolphin/os/OSVersion.h", "/* SDK version */"),
                               ("src/mtx/mtx.c", "void PSMTXIdentity(void) {}\n")):
            path = self.source / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
        self.git("init", "-q")
        self.git("config", "core.autocrlf", "false")
        self.git("config", "user.name", "Synthetic fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.git("add", ".")
        self.git("commit", "-qm", "Public synthetic matrix fixture")
        self.sha = self.git("rev-parse", "HEAD").strip()
        self.addCleanup(patch.stopall)
        patch.object(bw, "profile_value", side_effect=self.profile).start()
        self.builder.run = lambda name, command, **kwargs: subprocess.run(command, check=True, capture_output=True)

    def git(self, *arguments):
        return subprocess.check_output(["git", *arguments], cwd=self.source, text=True, stderr=subprocess.DEVNULL)

    def profile(self, name):
        return {"LIBPORPOISE_SHA": self.sha, "LIBPORPOISE_URL": str(self.source),
                "COMPOSITE_DIGEST": "synthetic source"}[name]

    def test_exact_dependency_fetch_reuse_and_refuse_changes(self):
        self.builder.args.libporpoise = True
        self.builder.libporpoise_dependency()
        self.assertEqual(self.builder.git("rev-parse", "HEAD", cwd=self.builder.libporpoise), self.sha)
        self.assertEqual(self.builder.libporpoise_inputs()["sha"], self.sha)
        self.builder.run = lambda *a, **k: self.fail("a current dependency must not fetch again")
        self.builder.libporpoise_dependency()
        (self.builder.libporpoise / "src/mtx/mtx.c").write_text("local edit")
        with self.assertRaisesRegex(bw.BuildError, "local changes"):
            self.builder.libporpoise_dependency()
        with self.assertRaisesRegex(bw.BuildError, "clean libPorpoise"):
            self.builder.libporpoise_inputs()

    def test_non_checkout_is_preserved_and_untracked_sources_are_rejected(self):
        self.builder.args.libporpoise = True
        self.builder.libporpoise.mkdir(parents=True)
        sentinel = self.builder.libporpoise / "personal.txt"
        sentinel.write_text("preserve me")
        with self.assertRaisesRegex(bw.BuildError, "not a git checkout"):
            self.builder.libporpoise_dependency()
        self.assertEqual(sentinel.read_text(), "preserve me")
        self.builder.libporpoise = self.root / "other-checkout"
        self.builder.libporpoise_dependency()
        (self.builder.libporpoise / "src/mtx/addition.c").write_text("unexpected source")
        with self.assertRaisesRegex(bw.BuildError, "local changes"):
            self.builder.libporpoise_dependency()

    def test_preparation_and_both_compile_modes_select_the_same_sdk(self):
        self.builder.args.native_math = self.builder.args.libporpoise = True
        self.builder.libporpoise_dependency()
        src = self.out / "composite-src"
        src.mkdir()
        (self.out / "composite-src.digest").write_text("synthetic source")
        calls = []
        self.builder.source_step = lambda *args: calls.append(args)
        with patch.object(bw, "tree_digest", return_value="synthetic final"):
            self.builder.prepare_blocks()
        self.assertIn(("native-math", "scripts/mods/prepare_native_math.py", src, "--libporpoise"), calls)
        receipt = json.loads((self.out / "prepared-blocks.json").read_text())
        self.assertTrue(receipt["libporpoise"])
        self.assertEqual(receipt["libporpoise_inputs"]["sha"], self.sha)

        commands = []
        def run(name, argv, **kwargs):
            commands.append([str(x) for x in argv])
            if name.endswith("-build"):
                folder = Path(argv[2]); folder.mkdir(parents=True, exist_ok=True)
                (folder / bw.MODULE).write_bytes(b"synthetic module")
        self.builder.run = run
        for enabled, name in ((True, "training-composite"), (False, "composite")):
            self.builder.args.libporpoise = enabled
            self.builder.compile_composite(self.out / name, "0", [], [], name)
            self.assertIn(f"-DBLUEWAKE_LIBPORPOISE={'ON' if enabled else 'OFF'}", commands[-2])
            self.assertIn(f"-DLIBPORPOISE_DIR={self.builder.libporpoise}", commands[-2])

    def test_training_fingerprint_tracks_sdk_selection_revision_and_sources(self):
        self.builder.args.libporpoise = True
        self.builder.libporpoise_dependency()
        original_git = self.builder.git
        self.builder.git = lambda *a, **k: "synthetic runtime" if a[0] == "-C" else original_git(*a, **k)
        with patch.object(bw, "tree_digest", return_value="synthetic translated source"):
            enabled = self.builder.training_fingerprint()
            self.builder.args.libporpoise = False
            self.assertNotEqual(enabled, self.builder.training_fingerprint())
            self.builder.args.libporpoise = True
            self.assertEqual(enabled, self.builder.training_fingerprint())
            (self.source / "src/mtx/mtx.c").write_text("void PSMTXIdentity(void) { /* newer SDK */ }\n")
            self.git("add", "."); self.git("commit", "-qm", "Updated public fixture")
            self.sha = self.git("rev-parse", "HEAD").strip()
            self.builder.libporpoise_dependency()
            self.assertNotEqual(enabled, self.builder.training_fingerprint())

    def test_cli_default_optout_and_conservative(self):
        captured = []
        with patch.object(bw.Builder, "build", lambda builder: captured.append(builder.args)), \
                patch.object(bw, "default_jobs", return_value=1):
            for flags in ([], ["--no-libporpoise"], ["--conservative"],
                          ["--conservative", "--native-math", "--libporpoise"]):
                with patch.object(sys, "argv", ["build.py", "synthetic.iso", "--out", str(self.out), *flags]):
                    bw.main()
        self.assertTrue(captured[0].libporpoise)
        self.assertTrue(captured[0].native_math)
        self.assertFalse(captured[1].libporpoise)
        self.assertFalse(captured[2].libporpoise)
        self.assertFalse(captured[2].native_math)
        self.assertTrue(captured[3].libporpoise)
        self.assertTrue(captured[3].native_math)

    def test_lock_and_profile_pins_agree(self):
        lock = json.loads((REPO / "config/dependencies.lock.json").read_text())
        dependency = next(x for x in lock["dependencies"] if x["id"] == "libporpoise")
        # Read the actual profile rather than this fixture's substituted pins.
        profile = (REPO / "scripts/builder/profiles/bluewake.sh").read_text()
        self.assertIn("LIBPORPOISE_SHA=" + dependency["sha"], profile)
        self.assertIn("LIBPORPOISE_URL=" + dependency["url"], profile)
        self.assertEqual(dependency["license"], "MIT")


if __name__ == "__main__":
    unittest.main()
