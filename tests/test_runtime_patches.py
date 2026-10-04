#!/usr/bin/env python3
"""Real Git checks for reproducible runtime patches without overwriting edits."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("runtime_patches", ROOT / "scripts/builder/runtime_patches.py")
patches = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patches)


class RuntimePatchTest(unittest.TestCase):
    def setUp(self):
        self.work = tempfile.TemporaryDirectory(prefix="runtime patches ")
        self.addCleanup(self.work.cleanup)
        self.root = Path(self.work.name)
        self.checkout = self.root / "runtime"
        self.checkout.mkdir()
        self.git("init", "-q")
        self.git("config", "core.autocrlf", "false")
        self.git("config", "user.name", "Fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.source = self.checkout / "source.cpp"
        self.source.write_bytes(b"int answer = 1;\n")
        self.git("add", ".")
        self.git("commit", "-qm", "base")
        self.base = self.git("rev-parse", "HEAD").strip()
        self.source.write_bytes(b"int answer = 2;\n")
        folder = self.root / "patches/recompcore"
        folder.mkdir(parents=True)
        self.patch = folder / "change.patch"
        self.patch.write_bytes(subprocess.check_output(["git", "diff", "--binary"], cwd=self.checkout))
        self.source.write_bytes(b"int answer = 1;\n")
        self.manifest = folder / "active.json"
        self.recipe = {"base_sha": self.base, "patches": [{"path": "patches/recompcore/change.patch",
                       "sha256": hashlib.sha256(self.patch.read_bytes()).hexdigest()}]}
        self.save_manifest()

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.checkout, text=True, stderr=subprocess.DEVNULL)

    def save_manifest(self):
        self.manifest.write_text(json.dumps(self.recipe), encoding="utf-8")

    def apply(self):
        return patches.apply_patches(self.checkout, self.manifest, self.root)

    def test_clean_apply_and_repeated_verification_preserve_real_index(self):
        index = self.checkout / ".git/index"
        before = index.read_bytes()
        first = self.apply()
        self.assertEqual(self.source.read_bytes(), b"int answer = 2;\n")
        self.assertEqual(first, self.apply())
        self.assertEqual(index.read_bytes(), before)
        self.assertEqual(self.git("rev-parse", "HEAD").strip(), self.base)

    def test_unrelated_edits_and_modified_patched_source_are_preserved(self):
        for content in (b"int answer = 3;\n", b"int answer = 2;\n// local edit\n"):
            self.source.write_bytes(content)
            with self.assertRaisesRegex(patches.PatchError, "differs from the exact"):
                self.apply()
            self.assertEqual(self.source.read_bytes(), content)

    def test_checksum_mismatch_never_applies(self):
        self.patch.write_bytes(self.patch.read_bytes() + b"\n")
        with self.assertRaisesRegex(patches.PatchError, "checksum mismatch"):
            self.apply()
        self.assertEqual(self.source.read_bytes(), b"int answer = 1;\n")

    def test_verification_never_changes_unpatched_or_modified_sources(self):
        with self.assertRaisesRegex(patches.PatchError, "does not match"):
            patches.apply_patches(self.checkout, self.manifest, self.root, verify_only=True)
        self.assertEqual(self.source.read_bytes(), b"int answer = 1;\n")
        receipt = self.apply()
        self.assertEqual(receipt, patches.apply_patches(self.checkout, self.manifest, self.root, verify_only=True))
        self.source.write_bytes(b"int answer = 9;\n")
        with self.assertRaises(patches.PatchError):
            patches.apply_patches(self.checkout, self.manifest, self.root, verify_only=True)
        self.assertEqual(self.source.read_bytes(), b"int answer = 9;\n")

    def test_wrong_pin_and_staged_changes_are_rejected(self):
        self.recipe["base_sha"] = "0" * 40
        self.save_manifest()
        with self.assertRaisesRegex(patches.PatchError, "require RecompCore"):
            self.apply()
        self.recipe["base_sha"] = self.base
        self.save_manifest()
        self.source.write_bytes(b"int answer = 9;\n")
        self.git("add", "source.cpp")
        with self.assertRaisesRegex(patches.PatchError, "staged changes"):
            self.apply()
        self.assertEqual(self.git("show", ":source.cpp"), "int answer = 9;\n")

    def test_partial_patch_set_is_rejected_without_finishing_it(self):
        self.source.write_bytes(b"int answer = 2;\n")
        self.git("add", "source.cpp")
        self.git("commit", "-qm", "first change")
        self.source.write_bytes(b"int answer = 4;\n")
        second = self.patch.with_name("second.patch")
        second.write_bytes(subprocess.check_output(["git", "diff"], cwd=self.checkout))
        self.git("reset", "--quiet", self.base)
        self.source.write_bytes(b"int answer = 2;\n")
        self.recipe["patches"].append({"path": "patches/recompcore/second.patch",
                                       "sha256": hashlib.sha256(second.read_bytes()).hexdigest()})
        self.save_manifest()
        with self.assertRaisesRegex(patches.PatchError, "differs from the exact"):
            self.apply()
        self.assertEqual(self.source.read_bytes(), b"int answer = 2;\n")


if __name__ == "__main__":
    unittest.main()
