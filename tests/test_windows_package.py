"""Windows release staging and PE dependency checks using synthetic inputs."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import shutil
import tempfile
import unittest
from unittest.mock import patch
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/windows/package_release.py"
SPEC = importlib.util.spec_from_file_location("package_windows_release", SCRIPT)
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)
SOURCE_SHA = "a" * 40
RUNTIME_SHA = "b" * 40
TRANSLATOR_SHA = "c" * 40
LIBPORPOISE_SHA = "d" * 40


def pe(imports=(), delay=()):
    """Small PE32+ fixture with real import and delay-load directories."""
    data = bytearray(0x800)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HH", data, 0x84, 0x8664, 1)
    struct.pack_into("<H", data, 0x94, 240)
    optional = 0x98
    struct.pack_into("<H", data, optional, 0x20B)
    struct.pack_into("<Q", data, optional + 24, 0x140000000)
    struct.pack_into("<I", data, optional + 60, 0x200)
    struct.pack_into("<I", data, optional + 108, 16)
    section = optional + 240
    data[section:section + 8] = b".rdata\0\0"
    struct.pack_into("<4I", data, section + 8, 0x600, 0x1000, 0x600, 0x200)
    next_name = 0x500
    for index, names, start, entry_size in ((1, imports, 0x200, 20), (13, delay, 0x300, 32)):
        if not names:
            continue
        struct.pack_into("<II", data, optional + 112 + index * 8, 0x1000 + start - 0x200, (len(names) + 1) * entry_size)
        for number, name in enumerate(names):
            name_rva = 0x1000 + next_name - 0x200
            if index == 1:
                struct.pack_into("<5I", data, start + number * entry_size, 1, 0, 0, name_rva, 1)
            else:
                struct.pack_into("<8I", data, start + number * entry_size, 1, name_rva, 0, 0, 0, 0, 0, 0)
            encoded = name.encode("ascii") + b"\0"
            data[next_name:next_name + len(encoded)] = encoded
            next_name += len(encoded)
    return bytes(data)


class PEImportsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "fixture.dll"

    def test_imports_include_delay_loads_and_case(self):
        self.path.write_bytes(pe(("KERNEL32.dll", "MSVCP140_ATOMIC_WAIT.dll"), ("dxcompiler.dll",)))
        self.assertEqual(PACKAGE.pe_imports(self.path), ["KERNEL32.dll", "MSVCP140_ATOMIC_WAIT.dll", "dxcompiler.dll"])
        self.path.write_bytes(pe())
        self.assertEqual(PACKAGE.pe_imports(self.path), [])

    def test_malformed_pe_is_rejected_instead_of_assumed_dependency_free(self):
        fixtures = [b"", b"MZ" + b"\0" * 62, pe()[:120], pe()[:400]]
        bad_arch = bytearray(pe())
        struct.pack_into("<H", bad_arch, 0x84, 0x14C)
        fixtures.append(bytes(bad_arch))
        bad_rva = bytearray(pe(("needed.dll",)))
        struct.pack_into("<I", bad_rva, 0x200 + 12, 0x900000)
        fixtures.append(bytes(bad_rva))
        bad_directory = bytearray(pe(("needed.dll",)))
        struct.pack_into("<I", bad_directory, 0x98 + 112 + 12, 20)
        fixtures.append(bytes(bad_directory))
        missing_name_terminator = bytearray(pe(("needed.dll",)))
        missing_name_terminator[0x500:] = b"x" * (0x800 - 0x500)
        fixtures.append(bytes(missing_name_terminator))
        fixtures.append(pe(("../private.dll",)))
        bad_delay = bytearray(pe(delay=("needed.dll",)))
        struct.pack_into("<I", bad_delay, 0x300, 2)
        fixtures.append(bytes(bad_delay))
        for index, fixture in enumerate(fixtures):
            with self.subTest(index=index):
                self.path.write_bytes(fixture)
                with self.assertRaises(ValueError):
                    PACKAGE.pe_imports(self.path)

    def test_missing_transitive_and_delayed_dependencies_fail(self):
        root = self.path.parent
        (root / "app.exe").write_bytes(pe(("first.dll",), ("delayed.dll",)))
        (root / "first.dll").write_bytes(pe(("transitive.dll",)))
        with self.assertRaisesRegex(ValueError, "delayed.dll.*transitive.dll"):
            PACKAGE.verify_dlls(root)
        (root / "delayed.dll").write_bytes(pe())
        (root / "transitive.dll").write_bytes(pe(("api-ms-win-core-file-l1-1-0.dll", "ucrtbase.dll")))
        PACKAGE.verify_dlls(root)


class WindowsPackageTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = self.root / "repo"
        self.args = argparse.Namespace(version="1.2.3", app=self.root / "personal/BlueWake", out=self.root / "releases",
            runtime=self.root / "runtime", deps=self.root / "build/app/_deps",
            dawn_license=self.root / "Dawn-LICENSE.txt", dxc_license=self.root / "DXC-LICENSE.txt",
            vc_runtime_license=self.root / "VC-LICENSE.txt")
        self.args.app.mkdir(parents=True)
        for name in PACKAGE.BINARY_WHITELIST:
            imports = ("kernel32.dll", "msvcp140_atomic_wait.dll") if name == "webgpu_dawn.dll" else ()
            (self.args.app / name).write_bytes(pe(imports))
        # These must all be ignored, even when a personal build contains them.
        for relative in ("game/GZLE01.iso", "game/main.dol", "game/rels/a.rel", "GZLE01.card", "states/one.sav",
                         "settings.ini", "textures/texture.png", "nodtool.exe", "unexpected.dll", "debug.pdb",
                         "host.profdata", "other.exe", "initial_pipeline_cache.db", "dsp/dsp_rom.bin"):
            target = self.args.app / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"synthetic private data")
        self.provenance = {"source_commit": SOURCE_SHA, "source_modified": False, "containsTranslatedGameCode": True,
                           "module_sha256": PACKAGE.sha256(self.args.app / PACKAGE.MODULE), "march": "x86-64-v3",
                           "compiler": "synthetic clang", "built": "2026-10-03T00:00:00Z", "disc": "private path",
                           "player_settings": {"volume": 0.5}}
        self.write_provenance()
        def write(path, value=b"synthetic public notice or resource"):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(value)
        write(self.repo / "LICENSE")
        write(self.repo / "RIGHTS_AND_LICENSES.md")
        write(self.repo / "windows/resources/initial_pipeline_cache.db", b"versioned pipeline seed")
        write(self.repo / "config/dependencies.lock.json", json.dumps({"dependencies": [
            {"id": "recompcore", "sha": RUNTIME_SHA, "url": "https://example.org/runtime"},
            {"id": "dolrecomp", "sha": TRANSLATOR_SHA, "url": "https://example.org/translator"}]}).encode())
        checker = self.repo / "scripts/release/check_public_assets.py"
        checker.parent.mkdir(parents=True)
        shutil.copyfile(SCRIPT.parents[1] / "release/check_public_assets.py", checker)
        (self.args.runtime / "DolRecomp").mkdir(parents=True)
        for relative in ("COPYING", "LICENSES/GPL-2.0-or-later.txt", "LICENSES/CC0-1.0.txt",
                         "GXRuntime/graphics/aurora/LICENSE", "Data/Sys/GC/dsp_rom.bin", "Data/Sys/GC/dsp_coef.bin"):
            write(self.args.runtime / relative)
        for name in PACKAGE.BUILD_DEPENDENCIES:
            write(self.args.deps / (name + "-src") / "source.c", b"synthetic dependency")
            archive = self.args.deps / (name + "-subbuild") / (name + "-populate-prefix/src") / (name + ".tar.gz")
            write(archive, b"synthetic archive")
            receipt = archive.parent / (name + "-populate-stamp") / (name + "-populate-urlinfo.txt")
            write(receipt, (f"method=url\nurl(s)=https://example.org/{name}.tar.gz\nhash=SHA256={PACKAGE.sha256(archive)}\n").encode())
        for path in (self.args.dawn_license, self.args.dxc_license, self.args.vc_runtime_license):
            write(path)
        dep_licenses = ("sdl3_prebuilt-src/licenses/SDL3/LICENSE.txt", "zlib-src/LICENSE.md", "png-src/LICENSE",
                        "imgui-src/LICENSE.txt", "fmt-src/LICENSE", "freetype-src/LICENSE.TXT", "freetype-src/docs/FTL.TXT",
                        "xxhash-src/LICENSE", "zstd-src/LICENSE", "abseil-cpp-src/LICENSE", "tracy-src/LICENSE")
        for name in dep_licenses:
            write(self.args.deps / name)
        self.addCleanup(patch.stopall)
        patch.object(PACKAGE, "ROOT", self.repo).start()
        self.git = patch.object(PACKAGE, "git", side_effect=self.git_output).start()
        self.check = patch.object(PACKAGE, "check_public_assets", side_effect=self.inspect_candidate).start()
        self.inspected = []

    def git_output(self, path, *arguments):
        if arguments == ("rev-parse", "HEAD"):
            return {self.repo: SOURCE_SHA, self.args.runtime: getattr(self, "runtime_sha", RUNTIME_SHA),
                    self.args.runtime / "DolRecomp": TRANSLATOR_SHA}[path]
        return ""

    def write_provenance(self):
        (self.args.app / "BuilderProvenance.json").write_text(json.dumps(self.provenance))

    def enable_runtime_patches(self):
        def runtime_git(*args):
            return subprocess.check_output(["git", "-C", str(self.args.runtime), *args],
                                           stderr=subprocess.DEVNULL)
        runtime_git("init", "-q")
        runtime_git("config", "core.autocrlf", "false")
        runtime_git("config", "user.name", "Fixture")
        runtime_git("config", "user.email", "fixture@example.invalid")
        self.runtime_source = self.args.runtime / "source.cpp"
        self.runtime_source.write_bytes(b"int answer = 1;\n")
        runtime_git("add", ".")
        runtime_git("commit", "-qm", "pinned runtime")
        self.runtime_sha = runtime_git("rev-parse", "HEAD").decode().strip()
        self.runtime_source.write_bytes(b"int answer = 2;\n")
        folder = self.repo / "patches/recompcore"
        folder.mkdir(parents=True)
        patch_file = folder / "change.patch"
        patch_file.write_bytes(runtime_git("diff", "--binary"))
        self.runtime_source.write_bytes(b"int answer = 1;\n")
        self.runtime_manifest = folder / "active.json"
        self.runtime_recipe = {"base_sha": self.runtime_sha, "patches": [
            {"path": "patches/recompcore/change.patch", "sha256": PACKAGE.sha256(patch_file)}]}
        self.runtime_manifest.write_text(json.dumps(self.runtime_recipe))
        lock_path = self.repo / "config/dependencies.lock.json"
        lock = json.loads(lock_path.read_text())
        lock["dependencies"][0].update(sha=self.runtime_sha, active_patches={
            "manifest": "patches/recompcore/active.json", "sha256": PACKAGE.sha256(self.runtime_manifest),
            "patches": self.runtime_recipe["patches"]})
        lock_path.write_text(json.dumps(lock))
        self.runtime_receipt = PACKAGE.apply_patches(self.args.runtime, self.runtime_manifest, self.repo)
        self.provenance["runtime_patches"] = self.runtime_receipt
        self.write_provenance()

    def test_verified_runtime_patches_are_recorded_and_never_applied_by_packaging(self):
        self.enable_runtime_patches()
        index = self.args.runtime / ".git/index"
        before = self.runtime_source.read_bytes(), index.read_bytes()
        result = PACKAGE.assemble(self.args)
        self.assertEqual((self.runtime_source.read_bytes(), index.read_bytes()), before)
        with zipfile.ZipFile(result) as archive:
            build = json.loads(archive.read("BlueWake/BUILD.json"))
            self.assertEqual(build["builder"]["runtime_patches"], self.runtime_receipt)
            self.assertEqual(build["dependencies"]["locked_sources"]["recompcore"]["active_patches"]["patches"],
                             self.runtime_recipe["patches"])

    def test_runtime_patch_provenance_and_lock_must_match_exactly(self):
        self.enable_runtime_patches()
        self.provenance["runtime_patches"] = {**self.runtime_receipt, "patched_tree": "0" * 40}
        self.write_provenance()
        with self.assertRaisesRegex(ValueError, "builder's runtime provenance"):
            PACKAGE.assemble(self.args)
        self.provenance["runtime_patches"] = self.runtime_receipt
        self.write_provenance()
        lock_path = self.repo / "config/dependencies.lock.json"
        lock = json.loads(lock_path.read_text())
        active = lock["dependencies"][0]["active_patches"]
        active["sha256"] = "0" * 64
        lock_path.write_text(json.dumps(lock))
        with self.assertRaisesRegex(ValueError, "manifest differs from the dependency lock"):
            PACKAGE.assemble(self.args)
        active["sha256"] = PACKAGE.sha256(self.runtime_manifest)
        active["patches"] = []
        lock_path.write_text(json.dumps(lock))
        with self.assertRaisesRegex(ValueError, "recipe differs from the dependency lock"):
            PACKAGE.assemble(self.args)
        self.check.assert_not_called()
        self.assertFalse(self.args.out.exists())

    def test_unpatched_and_modified_runtime_sources_are_rejected_without_changes(self):
        self.enable_runtime_patches()
        for content in (b"int answer = 1;\n", b"int answer = 2;\n// private edit\n"):
            self.runtime_source.write_bytes(content)
            with self.assertRaisesRegex(ValueError, "does not match the verified patch set"):
                PACKAGE.assemble(self.args)
            self.assertEqual(self.runtime_source.read_bytes(), content)
        self.check.assert_not_called()
        self.assertFalse(self.args.out.exists())

    def test_unlocked_runtime_patch_receipt_is_rejected(self):
        self.provenance["runtime_patches"] = {"base_sha": RUNTIME_SHA}
        self.write_provenance()
        with self.assertRaisesRegex(ValueError, "patches absent from the dependency lock"):
            PACKAGE.assemble(self.args)
        self.check.assert_not_called()

    def enable_libporpoise(self):
        self.args.libporpoise_dir = self.root / "sdk"
        self.args.libporpoise_dir.mkdir()
        (self.args.libporpoise_dir / "LICENSE").write_text("synthetic MIT SDK notice")
        lock_path = self.repo / "config/dependencies.lock.json"
        lock = json.loads(lock_path.read_text())
        lock["dependencies"].append({"id": "libporpoise", "sha": LIBPORPOISE_SHA,
                                     "url": "https://example.org/libporpoise"})
        lock_path.write_text(json.dumps(lock))
        self.provenance.update(libporpoise=True, libporpoise_sha=LIBPORPOISE_SHA, native_math=True)
        self.write_provenance()
        self.git.side_effect = lambda path, *arguments: (LIBPORPOISE_SHA if arguments == ("rev-parse", "HEAD")
                                                       else "") if path == self.args.libporpoise_dir else self.git_output(path, *arguments)

    def test_selected_libporpoise_ships_the_mit_notice_and_exact_pin(self):
        self.enable_libporpoise()
        result = PACKAGE.assemble(self.args)
        with zipfile.ZipFile(result) as archive:
            self.assertEqual(archive.read("BlueWake/licenses/libPorpoise-MIT.txt"), b"synthetic MIT SDK notice")
            build = json.loads(archive.read("BlueWake/BUILD.json"))
            self.assertEqual(build["dependencies"]["locked_sources"]["libporpoise"]["sha"], LIBPORPOISE_SHA)
            self.assertTrue(build["builder"]["libporpoise"])
            self.assertEqual(build["builder"]["libporpoise_sha"], LIBPORPOISE_SHA)
            self.assertIn(b"libPorpoise matrix SDK: MIT", archive.read("BlueWake/licenses/THIRD_PARTY_NOTICES.txt"))

    def test_libporpoise_mismatched_pin_changes_and_missing_license_stop_packaging(self):
        self.enable_libporpoise()
        for field, value in (("libporpoise_sha", "e" * 40), ("native_math", False)):
            old = self.provenance[field]
            self.provenance[field] = value
            self.write_provenance()
            with self.assertRaisesRegex(ValueError, "libporpoise.*dependency lock"):
                PACKAGE.assemble(self.args)
            self.provenance[field] = old
        self.write_provenance()
        original = self.git.side_effect
        self.git.side_effect = lambda path, *arguments: (" M src/mtx/mtx.c" if path == self.args.libporpoise_dir
                                                       and arguments[0] == "status" else original(path, *arguments))
        with self.assertRaisesRegex(ValueError, "clean pinned checkout"):
            PACKAGE.assemble(self.args)
        self.git.side_effect = original
        (self.args.libporpoise_dir / "LICENSE").unlink()
        with self.assertRaisesRegex(ValueError, "missing license.*libPorpoise"):
            PACKAGE.assemble(self.args)
        self.check.assert_not_called()
        self.assertFalse(self.args.out.exists())

    def test_release_output_cannot_modify_the_selected_sdk_checkout(self):
        self.enable_libporpoise()
        self.args.out = self.args.libporpoise_dir / "release"
        with self.assertRaisesRegex(ValueError, "separate from libPorpoise"):
            PACKAGE.assemble(self.args)
        self.check.assert_not_called()
        self.assertFalse(self.args.out.exists())

    def inspect_candidate(self, paths):
        self.assertEqual(len(paths), 2)
        self.assertFalse(self.args.out.exists())
        with zipfile.ZipFile(paths[0]) as archive:
            self.inspected.append(archive.namelist())
        self.assertEqual(paths[1].read_text().split()[0], PACKAGE.sha256(paths[0]))

    def test_whitelist_excludes_private_files_and_inputs_are_preserved(self):
        inputs = {p: (PACKAGE.sha256(p), p.stat().st_mtime_ns) for p in self.root.rglob("*") if p.is_file()}
        result = PACKAGE.assemble(self.args)
        self.check.assert_called_once()
        with zipfile.ZipFile(result) as archive:
            names = set(archive.namelist())
            for private in ("game", "nodtool", "settings", "texture", "states", "unexpected", "debug", "other.exe", "profdata"):
                self.assertFalse(any(private in name for name in names), private)
            for name in PACKAGE.REQUIRED_BINARIES:
                self.assertIn("BlueWake/" + name, names)
            self.assertEqual(archive.read("BlueWake/initial_pipeline_cache.db"), b"versioned pipeline seed")
            self.assertEqual(archive.read("BlueWake/dsp/dsp_rom.bin"), (self.args.runtime / "Data/Sys/GC/dsp_rom.bin").read_bytes())
            build = json.loads(archive.read("BlueWake/BUILD.json"))
            self.assertEqual(build["builder"]["source_commit"], SOURCE_SHA)
            self.assertNotIn("disc", build["builder"])
            self.assertNotIn("player_settings", build["builder"])
            self.assertTrue(build["builder"]["containsTranslatedGameCode"])
            self.assertEqual(set(build["dependencies"]["build_packages"]), set(PACKAGE.BUILD_DEPENDENCIES))
            for line in archive.read("BlueWake/SHA256SUMS").decode().splitlines():
                digest, name = line.split("  ", 1)
                self.assertEqual(digest, hashlib.sha256(archive.read("BlueWake/" + name)).hexdigest())
            self.assertIn(b".iso or .gcm", archive.read("BlueWake/README.txt"))
        for path, state in inputs.items():
            self.assertEqual((PACKAGE.sha256(path), path.stat().st_mtime_ns), state)

    def test_zip_bytes_do_not_depend_on_source_timestamps_or_output_directory(self):
        first = PACKAGE.assemble(self.args).read_bytes()
        self.args.out = self.root / "second-release"
        for path in self.args.app.iterdir():
            if path.is_file():
                os.utime(path, (1234567890, 1234567890))
        second = PACKAGE.assemble(self.args).read_bytes()
        self.assertEqual(first, second)

    def test_gate_failure_leaves_no_artifacts_and_preserves_existing_files(self):
        self.args.out.mkdir()
        unrelated = self.args.out / "keep.txt"
        unrelated.write_text("preserve")
        self.check.side_effect = subprocess.CalledProcessError(1, "content gate")
        with self.assertRaises(subprocess.CalledProcessError):
            PACKAGE.assemble(self.args)
        self.assertEqual(list(self.args.out.iterdir()), [unrelated])
        self.assertFalse(list(self.root.glob("windows-release-stage-*")))

    def test_existing_release_is_never_overwritten(self):
        result = PACKAGE.assemble(self.args)
        original = result.read_bytes()
        self.check.reset_mock()
        with self.assertRaisesRegex(ValueError, "already exist"):
            PACKAGE.assemble(self.args)
        self.assertEqual(result.read_bytes(), original)
        self.check.assert_not_called()

    def test_missing_atomic_wait_runtime_or_dynamic_dxc_fails_before_gate(self):
        missing = self.args.app / "msvcp140_atomic_wait.dll"
        content = missing.read_bytes()
        missing.unlink()
        with self.assertRaisesRegex(ValueError, "missing runtime DLLs.*msvcp140_atomic_wait"):
            PACKAGE.assemble(self.args)
        missing.write_bytes(content)
        (self.args.app / "dxil.dll").unlink()
        with self.assertRaisesRegex(ValueError, "dxil.dll"):
            PACKAGE.assemble(self.args)
        self.check.assert_not_called()
        self.assertFalse(self.args.out.exists())

    def test_modified_source_changed_module_and_stale_runtime_pin_are_rejected(self):
        self.provenance["source_modified"] = True
        self.write_provenance()
        with self.assertRaisesRegex(ValueError, "unmodified"):
            PACKAGE.assemble(self.args)
        self.provenance["source_modified"] = False
        self.provenance["module_sha256"] = "0" * 64
        self.write_provenance()
        with self.assertRaisesRegex(ValueError, "module differs"):
            PACKAGE.assemble(self.args)
        self.provenance["module_sha256"] = PACKAGE.sha256(self.args.app / PACKAGE.MODULE)
        self.write_provenance()
        self.git.side_effect = lambda path, *a: "d" * 40 if path == self.args.runtime and a == ("rev-parse", "HEAD") else self.git_output(path, *a)
        with self.assertRaisesRegex(ValueError, "recompcore.*dependency lock"):
            PACKAGE.assemble(self.args)
        self.check.assert_not_called()

    def test_license_omission_is_rejected_before_output(self):
        self.args.dxc_license.unlink()
        with self.assertRaisesRegex(ValueError, "missing license.*DirectX"):
            PACKAGE.assemble(self.args)
        self.check.assert_not_called()
        self.assertFalse(self.args.out.exists())

    def test_runtime_submodules_must_be_initialized_at_their_pins(self):
        command = ("submodule", "status", "--recursive", "--", "DolRecomp")
        self.git.side_effect = lambda path, *a: "-" + TRANSLATOR_SHA + " DolRecomp" if a == command else self.git_output(path, *a)
        with self.assertRaisesRegex(ValueError, "initialize product runtime submodules"):
            PACKAGE.assemble(self.args)
        self.git.side_effect = lambda path, *a: " " + TRANSLATOR_SHA + " DolRecomp (heads/main)" if a == command else self.git_output(path, *a)
        result = PACKAGE.assemble(self.args)
        with zipfile.ZipFile(result) as archive:
            self.assertEqual(json.loads(archive.read("BlueWake/BUILD.json"))["dependencies"]["runtime_submodules"], [{"path": "DolRecomp", "sha": TRANSLATOR_SHA}])

    def test_archive_pin_records_unpinned_prebuilt_download_and_catches_corruption(self):
        base = self.args.deps / "dawn_prebuilt-subbuild/dawn_prebuilt-populate-prefix/src"
        receipt = base / "dawn_prebuilt-populate-stamp/dawn_prebuilt-populate-urlinfo.txt"
        receipt.write_text("method=url\nurl(s)=https://example.org/dawn_prebuilt.tar.gz\nhash=\n")
        pins = PACKAGE.dependency_pins(self.args.deps)
        self.assertEqual(pins["dawn_prebuilt"]["archive_sha256"], PACKAGE.sha256(base / "dawn_prebuilt.tar.gz"))
        (base / "dawn_prebuilt.tar.gz").unlink()
        with self.assertRaisesRegex(ValueError, "preserve the downloaded archive"):
            PACKAGE.dependency_pins(self.args.deps)
        base = self.args.deps / "fmt-subbuild/fmt-populate-prefix/src"
        (base / "fmt.tar.gz").write_bytes(b"corrupt archive")
        # Restore the first dependency, so the later corruption is reached.
        (self.args.deps / "dawn_prebuilt-subbuild/dawn_prebuilt-populate-prefix/src/dawn_prebuilt.tar.gz").write_bytes(b"synthetic archive")
        with self.assertRaisesRegex(ValueError, "fmt.*differs from its SHA256 pin"):
            PACKAGE.dependency_pins(self.args.deps)

    def test_unsafe_version_and_output_inside_input_fail_before_tools(self):
        self.args.version = "../unsafe"
        with self.assertRaisesRegex(ValueError, "filename-safe"):
            PACKAGE.assemble(self.args)
        self.args.version = "1.2.3"
        self.args.out = self.args.app / "release"
        with self.assertRaisesRegex(ValueError, "separate from"):
            PACKAGE.assemble(self.args)
        self.git.assert_not_called()
        self.check.assert_not_called()

    def test_real_checker_invocation_is_required_with_no_bypass(self):
        # Invoke the actual function saved from the module before setUp patches.
        with patch.object(PACKAGE.subprocess, "run") as run:
            REAL_CHECKER([Path("candidate.zip"), Path("candidate.zip.sha256")])
        command = run.call_args.args[0]
        self.assertEqual(command[1], str(self.repo / "scripts/release/check_public_assets.py"))
        self.assertEqual(command[2:], ["candidate.zip", "candidate.zip.sha256"])
        self.assertTrue(run.call_args.kwargs["check"])

    def test_missing_or_rejecting_content_gate_fails_closed(self):
        gate = self.root / "synthetic-gate.py"
        self.check.side_effect = REAL_CHECKER
        with patch.dict(os.environ, {"BLUEWAKE_RELEASE_GATE": str(gate)}):
            with self.assertRaises(subprocess.CalledProcessError):
                PACKAGE.assemble(self.args)
            self.assertFalse(self.args.out.exists())
            gate.write_text("import sys\nsys.exit(1)\n")
            with self.assertRaises(subprocess.CalledProcessError):
                PACKAGE.assemble(self.args)
            self.assertFalse(self.args.out.exists())
            gate.write_text("import sys\nsys.exit(0)\n")
            self.assertTrue(PACKAGE.assemble(self.args).is_file())
        self.assertFalse(list(self.root.glob("windows-release-stage-*")))

    def test_linked_shipping_input_is_rejected(self):
        binary = self.args.app / "SDL3.dll"
        outside = self.root / "outside.dll"
        outside.write_bytes(binary.read_bytes())
        binary.unlink()
        try:
            binary.symlink_to(outside)
        except OSError as error:
            self.skipTest(f"symlink creation unavailable: {error}")
        with self.assertRaisesRegex(ValueError, "linked input"):
            PACKAGE.assemble(self.args)
        self.check.assert_not_called()


REAL_CHECKER = PACKAGE.check_public_assets

if __name__ == "__main__":
    unittest.main()
