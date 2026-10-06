"""App PGO cache checks with synthetic profiles and captured configure commands."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from threading import Barrier
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

REPO = Path(__file__).resolve().parents[1]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


profiles = load("app_profile", REPO / "scripts/windows/app_profile.py")
bw = load("windows_builder", REPO / "scripts/windows/build.py")


class ProfileCacheTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="app profile ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "app.profdata"
        self.source.write_bytes(b"valid counts A")
        self.folder = self.root / "cached profiles"
        self.cache = profiles.AppProfileCache(self.folder)
        self.probe = Mock(return_value=True)

    def select(self, compiler="clang A"):
        return self.cache.select(self.source, compiler_key=compiler, readable=self.probe)

    def test_same_content_reuses_path_probe_and_timestamp(self):
        first = self.select()
        before = first.stat().st_mtime_ns
        replacement = self.root / "replacement"
        replacement.write_bytes(self.source.read_bytes())
        os.replace(replacement, self.source)
        self.assertEqual(self.select(), first)
        self.assertEqual(first.stat().st_mtime_ns, before)
        self.assertEqual(first.name, "app-" + hashlib.sha256(self.source.read_bytes()).hexdigest() + ".profdata")
        self.probe.assert_called_once_with(first)
        self.assertFalse(list(self.folder.glob("*.tmp")))

    def test_same_size_and_timestamp_changed_content_changes_input(self):
        original = self.source.stat()
        first = self.select()
        self.source.write_bytes(b"valid counts B")
        os.utime(self.source, ns=(original.st_atime_ns, original.st_mtime_ns))
        self.assertEqual(self.source.stat().st_size, original.st_size)
        self.assertEqual(self.source.stat().st_mtime_ns, original.st_mtime_ns)
        second = self.select()
        self.assertNotEqual(first, second)
        self.assertEqual(first.read_bytes(), b"valid counts A")
        self.assertEqual(second.read_bytes(), b"valid counts B")
        self.assertEqual(self.probe.call_count, 2)

    def test_unreadable_changed_content_falls_back_and_caches_by_content(self):
        first = self.select()
        self.source.write_bytes(b"invalid counts")
        self.probe.return_value = False
        self.assertIsNone(self.select())
        self.assertIsNone(self.select())
        self.assertEqual(self.probe.call_count, 2)
        self.source.write_bytes(b"valid counts A")
        self.assertEqual(self.select(), first)
        self.assertEqual(self.probe.call_count, 2)

    def test_changed_compiler_probes_same_content_again(self):
        first = self.select()
        self.probe.return_value = False
        self.assertIsNone(self.select("clang B"))
        self.assertEqual(self.select("clang A"), first)
        self.assertEqual(self.probe.call_count, 2)

    def test_missing_or_permission_denied_optional_source_falls_back(self):
        self.source.unlink()
        self.assertIsNone(self.select())
        self.assertFalse(self.folder.exists())
        self.source.write_bytes(b"valid counts A")
        with patch.object(Path, "open", side_effect=PermissionError("unreadable input")):
            self.assertIsNone(self.select())
        self.probe.assert_not_called()
        self.assertEqual(self.source.read_bytes(), b"valid counts A")

    def test_probe_launch_failure_falls_back_without_changing_source(self):
        self.probe.side_effect = OSError("unavailable profdata tool")
        self.assertIsNone(self.select())
        self.assertIsNone(self.select())
        self.probe.assert_called_once()
        self.assertEqual(self.source.read_bytes(), b"valid counts A")

    def test_corrupt_snapshot_is_rejected_without_repair_or_reprobe(self):
        cached = self.select()
        cached.write_bytes(b"corrupt cached counts")
        with self.assertRaises(profiles.ProfileCacheError):
            self.select()
        self.assertEqual(cached.read_bytes(), b"corrupt cached counts")
        self.probe.assert_called_once()
        self.assertFalse(list(self.folder.glob("*.tmp")))

    def test_failed_cache_publish_propagates_and_removes_temporary(self):
        with patch.object(profiles.os, "link", side_effect=OSError("cache disk failure")):
            with self.assertRaisesRegex(OSError, "cache disk failure"):
                self.select()
        self.probe.assert_not_called()
        self.assertFalse(list(self.folder.iterdir()))
        self.assertEqual(self.source.read_bytes(), b"valid counts A")

    def test_concurrent_writers_publish_one_complete_immutable_input(self):
        barrier = Barrier(8)
        real_link = os.link

        def publish(source, target):
            barrier.wait(timeout=10)
            real_link(source, target)

        def select(_):
            cache = profiles.AppProfileCache(self.folder)
            return cache.select(self.source, compiler_key="clang A", readable=lambda p: p.read_bytes() == b"valid counts A")

        with patch.object(profiles.os, "link", side_effect=publish), ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(select, range(8)))
        self.assertTrue(all(p == results[0] for p in results))
        self.assertEqual(list(self.folder.iterdir()), [results[0]])
        before = results[0].stat().st_mtime_ns
        self.assertEqual(self.select(), results[0])
        self.assertEqual(results[0].stat().st_mtime_ns, before)

    @unittest.skipUnless(shutil.which("ninja"), "Ninja is needed for the command-cache oracle")
    def test_ninja_rebuilds_every_affected_object_only_on_changed_flags(self):
        # A tiny recording compiler exercises Ninja's real command cache, without
        # a game/app compile or requiring LLVM-readable synthetic profile bytes.
        folder = self.root / "ninja"
        folder.mkdir()
        compiler = folder / "record_compile.py"
        compiler.write_text("import pathlib,sys\n"
                            "out=pathlib.Path(sys.argv[1]); log=pathlib.Path(sys.argv[2])\n"
                            "out.write_text('authored object')\n"
                            "with log.open('a') as stream: stream.write(out.name+'\\n')\n")
        log = folder / "compiles.log"

        def quoted(value):
            return '"' + str(value).replace("$", "$$") + '"'

        def build(selected):
            profile = "" if selected is None else quoted("-fprofile-instr-use=" + selected.as_posix())
            command = " ".join(map(quoted, (sys.executable, compiler)))
            (folder / "build.ninja").write_text(
                "rule affected\n  command = " + command + " $out " + quoted(log) + " " + profile + "\n"
                "rule unrelated\n  command = " + command + " $out " + quoted(log) + "\n"
                "build c.obj: affected\nbuild cxx.obj: affected\nbuild unrelated.obj: unrelated\n"
                "default c.obj cxx.obj unrelated.obj\n")
            subprocess.run([shutil.which("ninja"), "-j", "1", "-f", "build.ninja"], cwd=folder, check=True,
                           capture_output=True, timeout=30,
                           creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            return log.read_text().splitlines()

        first = self.select()
        self.assertCountEqual(build(first), ["c.obj", "cxx.obj", "unrelated.obj"])
        self.assertEqual(len(build(self.select())), 3)
        info = self.source.stat()
        self.source.write_bytes(b"valid counts B")
        os.utime(self.source, ns=(info.st_atime_ns, info.st_mtime_ns))
        self.assertCountEqual(build(self.select())[3:], ["c.obj", "cxx.obj"])
        self.probe.return_value = False
        self.source.write_bytes(b"invalid counts")
        self.assertCountEqual(build(self.select())[5:], ["c.obj", "cxx.obj"])
        self.assertEqual(len(build(self.select())), 7)


class BuilderProfileTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="configured app profile ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.builder = bw.Builder(SimpleNamespace(out=self.root, march="x86-64-v3", console=False, no_app_pgo=False))
        self.builder.APP_PROFILE = self.root / "app.profdata"
        self.builder.APP_PROFILE.write_bytes(b"valid counts A")
        self.builder.clang = str(self.root / "compiler/clang.exe")
        self.builder.clang_version = "clang version fixture A"
        self.builder.env = {}
        self.tool = Path(self.builder.clang).with_name("llvm-profdata.exe")
        self.tool.parent.mkdir()
        self.tool.write_bytes(b"authored tool A")
        self.commands = []
        self.builder.run = lambda name, argv, **kw: self.commands.append(list(map(str, argv)))

    def flags(self):
        return [x for x in self.commands[-1] if x.startswith(("-DCMAKE_C_FLAGS=", "-DCMAKE_CXX_FLAGS="))]

    def test_configure_same_changed_unreadable_and_replaced_profiles(self):
        def probe(argv, **kwargs):
            path = Path(argv[2])
            self.assertNotEqual(path, self.builder.APP_PROFILE)
            self.assertEqual(path.name, "app-" + hashlib.sha256(path.read_bytes()).hexdigest() + ".profdata")
            return SimpleNamespace(returncode=0 if path.read_bytes().startswith(b"valid") else 1)

        with patch.object(bw.subprocess, "run", side_effect=probe) as checked:
            self.builder.configure_app()
            original_flags = self.flags()
            self.builder.configure_app()
            self.assertEqual(self.flags(), original_flags)
            self.assertEqual(checked.call_count, 1)
            info = self.builder.APP_PROFILE.stat()
            self.builder.APP_PROFILE.write_bytes(b"valid counts B")
            os.utime(self.builder.APP_PROFILE, ns=(info.st_atime_ns, info.st_mtime_ns))
            self.builder.configure_app()
            self.assertTrue(all(a != b for a, b in zip(original_flags, self.flags())))
            self.assertEqual(checked.call_count, 2)
            self.builder.APP_PROFILE.write_bytes(b"invalid counts")
            self.builder.configure_app()
            self.assertNotIn("-fprofile-instr-use", " ".join(self.commands[-1]))
            self.assertNotIn("-flto=thin", " ".join(self.commands[-1]))
            self.builder.configure_app()
            self.assertEqual(checked.call_count, 3)
            self.builder.APP_PROFILE.write_bytes(b"valid counts A")
            self.builder.configure_app()
            self.assertEqual(self.flags(), original_flags)
            self.assertEqual(checked.call_count, 3)

    def test_changed_selected_tool_reprobes_same_profile(self):
        with patch.object(bw.subprocess, "run", return_value=SimpleNamespace(returncode=0)) as checked:
            self.builder.configure_app()
            self.tool.write_bytes(b"authored tool B")
            self.builder.configure_app()
        self.assertEqual(checked.call_count, 2)

    def test_instrumentation_and_opt_out_never_snapshot_or_probe(self):
        with patch.object(self.builder, "app_profile_path", side_effect=AssertionError("unexpected profile selection")):
            self.builder.configure_app(instrument=True)
            self.builder.args.no_app_pgo = True
            self.builder.configure_app()
        self.assertIn("-fprofile-instr-generate", " ".join(self.commands[0]))
        self.assertNotIn("-fprofile-instr", " ".join(self.commands[1]))
        self.assertFalse((self.root / "pgo-app").exists())


if __name__ == "__main__":
    unittest.main()
