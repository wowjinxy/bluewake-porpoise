"""Qualification and reproducibility checks for the upstream math source slice."""
from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "prepare_libporpoise_math", ROOT / "scripts/dependencies/prepare_libporpoise_math.py"
)
PREPARE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(PREPARE)


@unittest.skipUnless(shutil.which("git"), "Git is needed to verify actual index/worktree state")
class SourceQualificationTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.source = self.directory / "upstream"
        self.source.mkdir()
        self.output = self.directory / "build" / "libporpoise_mtx.c"
        contents = {
            "src/mtx/mtx.c": (
                "void C_MTXIdentity(Mtx m)\n{\n m[0][0] = 1.0f; /* } */\n}\n\n"
                "void C_MTXTrans(Mtx m, f32 x, f32 y, f32 z)\n{\n m[0][3] = x;\n}\n\n"
                "void C_MTXScale(Mtx m, f32 x, f32 y, f32 z)\n{\n m[0][0] = x;\n}\n"
                "void C_MTXUnselected(Mtx m)\n{\n unrelated();\n}\n"
            ),
            "include/dolphin/mtx.h": "typedef float Mtx[3][4];\n",
            "include/dolphin/types.h": "typedef float f32;\n",
            "include/dolphin/vec.h": "/* no vertex operations selected */\n",
            "include/dolphin/os/OSVersion.h": "#define OS_BUILD_VERSION 20011217L\n",
        }
        self.hashes = {}
        for name, content in contents.items():
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content.encode())
            self.hashes[name] = hashlib.sha256(content.encode()).hexdigest()
        self.git("init", "--quiet")
        self.git("config", "core.autocrlf", "false")
        self.git("config", "user.name", "Source qualification test")
        self.git("config", "user.email", "source-test@example.invalid")
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", "Fixture")
        self.revision = self.git("rev-parse", "HEAD").strip()
        self.addCleanup(patch.stopall)
        patch.object(PREPARE, "PINNED_REVISION", self.revision).start()
        patch.object(PREPARE, "SOURCE_HASHES", self.hashes).start()

    def git(self, *arguments):
        return subprocess.run(
            ["git", "-C", str(self.source), *arguments], check=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        ).stdout

    def test_slice_contains_exact_bodies_and_excludes_other_sdk(self):
        receipt = PREPARE.prepare(self.source, self.output)
        upstream = (self.source / "src/mtx/mtx.c").read_text()
        generated = self.output.read_text()
        for name in PREPARE.FUNCTIONS:
            self.assertIn(PREPARE.extract_function(upstream, name), generated)
        self.assertNotIn("C_MTXUnselected", generated)
        self.assertNotIn("unrelated", generated)
        self.assertIn("#include <dolphin/mtx.h>", generated)
        self.assertNotIn("CPUState", generated)
        self.assertNotIn("SDL", generated)
        self.assertTrue(receipt["changed"])

    def test_identical_preparation_preserves_output_timestamp(self):
        PREPARE.prepare(self.source, self.output)
        before = self.output.stat().st_mtime_ns
        receipt = PREPARE.prepare(self.source, self.output)
        self.assertFalse(receipt["changed"])
        self.assertEqual(before, self.output.stat().st_mtime_ns)

    def test_changed_head_is_rejected_before_output(self):
        (self.source / "notice").write_text("Additional commit\n")
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", "Changed HEAD")
        with self.assertRaisesRegex(PREPARE.PreparationError, "HEAD must be"):
            PREPARE.prepare(self.source, self.output)
        self.assertFalse(self.output.exists())

    def test_dirty_tracked_source_and_staged_changes_rejected(self):
        path = self.source / "src/mtx/mtx.c"
        path.write_bytes(path.read_bytes() + b"\n/* change */\n")
        for staged in (False, True):
            if staged:
                self.git("add", ".")
            with self.assertRaisesRegex(PREPARE.PreparationError, "must be clean"):
                PREPARE.prepare(self.source, self.output)
        self.assertFalse(self.output.exists())

    def test_untracked_source_cannot_override_includes(self):
        (self.source / "include/dolphin/new_override.h").write_text("/* added file */\n")
        with self.assertRaisesRegex(PREPARE.PreparationError, "must be clean"):
            PREPARE.prepare(self.source, self.output)

    def test_hash_rejects_source_change_hidden_from_git_status(self):
        for name in ("src/mtx/mtx.c", "include/dolphin/types.h"):
            with self.subTest(name=name):
                self.git("update-index", "--assume-unchanged", name)
                path = self.source / name
                original = path.read_bytes()
                path.write_bytes(original + b"/* concealed corruption */\n")
                self.assertFalse(self.git("status", "--porcelain=v1").strip())
                with self.assertRaisesRegex(PREPARE.PreparationError, "source hash mismatch"):
                    PREPARE.prepare(self.source, self.output)
                path.write_bytes(original)
                self.git("update-index", "--no-assume-unchanged", name)
        self.assertFalse(self.output.exists())

    def test_clean_crlf_checkout_uses_same_canonical_source(self):
        PREPARE.prepare(self.source, self.output)
        expected = self.output.read_bytes()
        self.git("config", "core.autocrlf", "true")
        for name in self.hashes:
            path = self.source / name
            path.write_bytes(path.read_bytes().replace(b"\n", b"\r\n"))
        # Refresh the fixture index's checkout conversion flags, as a clone
        # under core.autocrlf=true would already have done. Blob IDs stay LF.
        self.git("add", "--renormalize", ".")
        self.assertFalse(self.git("status", "--porcelain=v1").strip())
        PREPARE.prepare(self.source, self.output)
        self.assertEqual(expected, self.output.read_bytes())

    def test_invalid_source_cannot_replace_previous_generated_output(self):
        PREPARE.prepare(self.source, self.output)
        original = self.output.read_bytes()
        (self.source / "include/dolphin/types.h").write_text("/* corrupt */\n")
        with self.assertRaises(PREPARE.PreparationError):
            PREPARE.prepare(self.source, self.output)
        self.assertEqual(original, self.output.read_bytes())

    def test_output_inside_checkout_rejected(self):
        with self.assertRaisesRegex(PREPARE.PreparationError, "outside"):
            PREPARE.prepare(self.source, self.source / "generated.c")
        self.assertFalse((self.source / "generated.c").exists())


class FunctionSelectionTest(unittest.TestCase):
    def test_nested_braces_comments_and_strings_are_retained(self):
        body = 'void C_Test(Mtx m)\n{\n /* } */ if (1) { // }\n const char* s = "}";\n }\n}'
        self.assertEqual(body, PREPARE.extract_function(body + "\nvoid other() {}", "C_Test"))

    def test_missing_duplicated_and_unterminated_definitions_rejected(self):
        for source in ("", "void C_Test(Mtx m) {", "void C_Test(Mtx m) {}\nvoid C_Test(Mtx m) {}"):
            with self.subTest(source=source), self.assertRaises(PREPARE.PreparationError):
                PREPARE.extract_function(source, "C_Test")


if __name__ == "__main__":
    unittest.main()
