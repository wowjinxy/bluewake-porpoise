"""Local training accepts only the verified runtime recipe and keys its profiles."""
import argparse
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts/builder"))
spec = importlib.util.spec_from_file_location("train_local_pgo", ROOT / "scripts/builder/train_local_pgo.py")
TRAIN = importlib.util.module_from_spec(spec)
spec.loader.exec_module(TRAIN)


class LocalTrainingRuntimeTest(unittest.TestCase):
    def setUp(self):
        self.work = tempfile.TemporaryDirectory()
        self.addCleanup(self.work.cleanup)
        self.root = Path(self.work.name)
        self.args = argparse.Namespace(out=self.root / "build", disc=self.root / "disc.iso",
                                       save=None, module_optimizations="none")
        for relative in ("scripts/builder/module_optimizations.py", "scripts/builder/runtime_patches.py",
                         "patches/recompcore/active.json", "apple/ios/src/dsp_common_shim.cpp",
                         "ref/recompcore/GXRuntime/CMakeLists.txt", "ref/recompcore/Data/Sys/GC/dsp_rom.bin",
                         "ref/recompcore/Data/Sys/GC/dsp_coef.bin", "build/game/main.dol",
                         "build/composite-src/generated.h", "disc.iso"):
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"synthetic fixture\n")
        self.receipt = {"base_sha": "a" * 40, "patched_tree": "b" * 40,
                        "patches": {"patches/recompcore/change.patch": "c" * 64}}
        self.addCleanup(patch.stopall)
        patch.object(TRAIN, "ROOT", self.root).start()
        self.verify = patch.object(TRAIN, "apply_patches", return_value=self.receipt).start()
        self.output = patch.object(TRAIN.subprocess, "check_output", side_effect=self.command_output).start()

    def command_output(self, command, **kwargs):
        if command[0] == "xcrun":
            return "synthetic clang"
        self.assertNotIn("status", command)
        return b"pinned-runtime\n"

    def call_main(self):
        with patch.object(TRAIN.platform, "system", return_value="Darwin"), \
             patch.object(TRAIN.platform, "machine", return_value="arm64"), \
             patch.object(sys, "argv", ["train_local_pgo", "--disc", str(self.args.disc),
                                        "--out", str(self.args.out)]):
            TRAIN.main()

    def test_fingerprint_includes_verified_tree_manifest_and_verifier(self):
        original = TRAIN.fingerprint(self.args, "synthetic clang")
        self.verify.assert_called_once_with(self.root / "ref/recompcore", root=self.root,
                                           manifest=self.root / "patches/recompcore/active.json", verify_only=True)
        self.verify.return_value = {**self.receipt, "patched_tree": "d" * 40}
        self.assertNotEqual(TRAIN.fingerprint(self.args, "synthetic clang"), original)
        self.verify.return_value = self.receipt
        manifest = self.root / "patches/recompcore/active.json"
        manifest.write_bytes(b"new recipe\n")
        changed = TRAIN.fingerprint(self.args, "synthetic clang")
        self.assertNotEqual(changed, original)
        (self.root / "scripts/builder/runtime_patches.py").write_bytes(b"new verifier\n")
        self.assertNotEqual(TRAIN.fingerprint(self.args, "synthetic clang"), changed)

    def test_unverified_source_stops_before_compiler_or_training(self):
        self.verify.side_effect = TRAIN.PatchError("runtime does not match the verified patch set")
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
            self.call_main()
        self.assertEqual(error.exception.code, 2)
        self.output.assert_not_called()
        self.assertFalse((self.args.out / "pgo-local").exists())

    def test_exact_patched_runtime_can_reuse_matching_profiles(self):
        key = TRAIN.fingerprint(self.args, "synthetic clang", self.receipt)
        work = self.args.out / "pgo-local"
        work.mkdir()
        profiles = {}
        for name in ("composite.profdata", "host.profdata"):
            path = work / name
            path.write_bytes(b"synthetic profile")
            profiles[name] = hashlib.sha256(path.read_bytes()).hexdigest()
        (work / "training.json").write_text(json.dumps({"fingerprint": key, "profiles": profiles}))
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.call_main()
        self.assertIn("reusing verified local profiles", output.getvalue())
        self.verify.assert_called_once()


if __name__ == "__main__":
    unittest.main()
