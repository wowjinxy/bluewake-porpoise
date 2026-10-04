#!/usr/bin/env python3
r"""Stage a reproducible Windows ZIP from a personal builder output.

    python scripts/windows/package_release.py VERSION --dawn-license FILE \
        --dxc-license FILE --vc-runtime-license FILE

The build's initialized runtime and app/_deps supply dependency pins and
licenses. Only known app binaries and versioned runtime resources are copied;
the personal build is never changed. nodtool, player data and texture packs
are excluded. Every candidate must pass scripts/release/check_public_assets.py
and its configured content gate before ZIP/checksum outputs are installed.
This command creates local artifacts only; it does not publish them.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/builder"))
from runtime_patches import PatchError, apply_patches

NAME = "BlueWake"
MODULE = "gGZLE01_recomp.dll"
REQUIRED_BINARIES = ("BlueWake.exe", MODULE, "SDL3.dll", "webgpu_dawn.dll", "dxcompiler.dll", "dxil.dll")
VC_BINARIES = ("msvcp140.dll", "msvcp140_atomic_wait.dll", "vcruntime140.dll", "vcruntime140_1.dll")
OPTIONAL_BINARIES = ("libpng16.dll", "zlib1.dll", "freetype.dll")
BINARY_WHITELIST = REQUIRED_BINARIES + VC_BINARIES + OPTIONAL_BINARIES
BUILD_DEPENDENCIES = ("dawn_prebuilt", "sdl3_prebuilt", "zlib", "png", "imgui", "fmt", "freetype",
                      "xxhash", "zstd", "abseil-cpp", "tracy", "sqlite3")
SYSTEM_DLLS = {
    "kernel32.dll", "kernelbase.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll",
    "ole32.dll", "oleaut32.dll", "imm32.dll", "version.dll", "winmm.dll", "setupapi.dll", "ntdll.dll",
    "bcrypt.dll", "bcryptprimitives.dll", "comctl32.dll", "comdlg32.dll", "dxgi.dll", "d3d12.dll",
    "d3d11.dll", "dwmapi.dll", "uxtheme.dll", "hid.dll", "cfgmgr32.dll", "ws2_32.dll", "crypt32.dll",
    "psapi.dll", "dbghelp.dll", "shlwapi.dll", "userenv.dll", "secur32.dll", "ncrypt.dll", "powrprof.dll",
    "winhttp.dll", "iphlpapi.dll", "mfplat.dll", "avrt.dll", "ucrtbase.dll", "wintrust.dll", "normaliz.dll",
    "dwrite.dll", "d2d1.dll", "rpcrt4.dll", "msimg32.dll", "winspool.drv", "dcomp.dll",
}
PROVENANCE_FIELDS = ("profile", "source_commit", "source_modified", "composite_digest", "mods", "march",
                     "prepared_blocks", "fixed_cpu", "fixed_mem1", "inline_fp", "gather_pipe", "direct_calls",
                     "inline_gpr", "native_j3d", "native_vec", "native_math", "native_skin", "native_game_math",
                     "native_entries", "lean_memory", "libporpoise", "libporpoise_sha",
                     "local_training", "composite_profile_sha256", "compiler", "module_sha256", "built", "runtime_patches")
INSTALL = """BlueWake {version} for Windows x64

Extract this entire folder and run BlueWake.exe. Windows 10/11 x64 and a
Direct3D 12 GPU are required. This build targets CPU level {march}.
Supply your own GameCube USA GZLE01 revision 0 disc image (.iso or .gcm).
The first launch checks your disc and prepares its files in %APPDATA%\\BlueWake.
Compressed disc images require conversion to ISO separately: nodtool is not
included. Saves, settings and session logs also live in %APPDATA%\\BlueWake.
F1 opens settings; BlueWake.exe --help lists command-line options.

The archive contains translated game code, runtime libraries and pipeline
metadata. It contains no disc image, extracted assets, saves or texture packs.
BUILD.json and SHA256SUMS record source revisions, dependencies and file hashes.
Licenses and third-party notices are in licenses/. Source and build instructions:
https://github.com/wowjinxy/bluewake-porpoise (docs/WINDOWS.md).
BlueWake is unofficial and is not affiliated with Nintendo.
"""


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def regular_file(path):
    if not path.is_file() or any(parent.is_symlink() for parent in (path, *path.parents)):
        raise ValueError(f"missing or linked input: {path}")
    if any(getattr(parent, "is_junction", lambda: False)() for parent in (path, *path.parents)):
        raise ValueError(f"junction input is unsupported: {path}")
    return path


def pe_imports(path):
    """Read regular and delay-load imports from a bounded, validated AMD64 PE."""
    data = regular_file(path).read_bytes()

    def unpack(fmt, offset):
        if offset < 0 or offset + struct.calcsize(fmt) > len(data):
            raise ValueError(f"{path.name}: truncated PE structure")
        return struct.unpack_from(fmt, data, offset)

    if len(data) < 64 or data[:2] != b"MZ":
        raise ValueError(f"{path.name}: missing DOS/PE header")
    pe, = unpack("<I", 0x3C)
    if pe < 64 or data[pe:pe + 4] != b"PE\0\0":
        raise ValueError(f"{path.name}: invalid PE signature")
    machine, count = unpack("<HH", pe + 4)
    optional_size, = unpack("<H", pe + 20)
    optional = pe + 24
    magic, = unpack("<H", optional)
    if machine != 0x8664 or magic != 0x20B or optional_size < 112:
        raise ValueError(f"{path.name}: expected an x64 PE32+ binary")
    directories, = unpack("<I", optional + 108)
    headers_size, = unpack("<I", optional + 60)
    image_base, = unpack("<Q", optional + 24)
    if (directories > (optional_size - 112) // 8 or optional + optional_size + 40 * count > len(data)
            or headers_size > len(data)):
        raise ValueError(f"{path.name}: malformed PE headers")
    spans = []
    for index in range(count):
        section = optional + optional_size + 40 * index
        _, va, raw_size, raw = unpack("<4I", section + 8)
        if raw_size and (raw < headers_size or raw + raw_size > len(data)):
            raise ValueError(f"{path.name}: invalid PE section")
        if any(va < v + n and v < va + raw_size for v, n, _ in spans):
            raise ValueError(f"{path.name}: overlapping PE sections")
        spans.append((va, raw_size, raw))

    def offset(rva, size=1):
        if 0 <= rva and rva + size <= headers_size:
            return rva, headers_size
        for va, length, raw in spans:
            if va <= rva and rva + size <= va + length:
                return raw + rva - va, raw + length
        raise ValueError(f"{path.name}: PE RVA {rva:#x} is not backed by file data")

    def dll_name(rva):
        start, end = offset(rva)
        nul = data.find(b"\0", start, min(end, start + 261))
        if nul < 0:
            raise ValueError(f"{path.name}: unterminated PE import name")
        try:
            name = data[start:nul].decode("ascii")
        except UnicodeDecodeError as error:
            raise ValueError(f"{path.name}: non-ASCII PE import name") from error
        if not re.fullmatch(r"[A-Za-z0-9_.-]+\.(?:dll|drv)", name, re.I):
            raise ValueError(f"{path.name}: unsafe PE import name {name!r}")
        return name

    names = []
    for index, entry_size in ((1, 20), (13, 32)):
        if directories <= index:
            continue
        rva, length = unpack("<II", optional + 112 + index * 8)
        if not rva and not length:
            continue
        if not rva or length < entry_size:
            raise ValueError(f"{path.name}: malformed PE import directory")
        start, _ = offset(rva, length)
        for entry in range(start, start + length - entry_size + 1, entry_size):
            values = unpack("<" + "I" * (entry_size // 4), entry)
            if not any(values):
                break
            name_rva = values[3] if index == 1 else values[1]
            if index == 13:
                if values[0] not in (0, 1):
                    raise ValueError(f"{path.name}: malformed PE delay-load attributes")
                if values[0] == 0:
                    name_rva -= image_base
            names.append(dll_name(name_rva))
        else:
            raise ValueError(f"{path.name}: unterminated PE import directory")
    return names


def verify_dlls(stage):
    binaries = [p for p in stage.iterdir() if p.suffix.lower() in (".exe", ".dll")]
    shipped = {p.name.lower() for p in binaries}
    missing = []
    for binary in binaries:
        for name in pe_imports(binary):
            lower = name.lower()
            if (lower not in shipped and lower not in SYSTEM_DLLS and
                    not lower.startswith(("api-ms-win-", "ext-ms-win-"))):
                missing.append(f"{name} (for {binary.name})")
    if missing:
        raise ValueError("missing runtime DLLs: " + ", ".join(sorted(set(missing))))


def git(path, *arguments):
    # Preserve submodule status's leading marker (space, +, -, U).
    return subprocess.check_output(["git", "-C", str(path), *arguments], text=True).rstrip()


def tree_sha256(path):
    digest = hashlib.sha256()
    if not path.is_dir() or any(parent.is_symlink() for parent in (path, *path.parents)):
        raise ValueError(f"missing or linked dependency source: {path}")
    for source in sorted(path.rglob("*"), key=lambda p: p.relative_to(path).as_posix()):
        relative = source.relative_to(path)
        if ".git" in relative.parts:
            continue
        if source.is_symlink():
            raise ValueError(f"linked dependency input: {source}")
        if source.is_file():
            digest.update(relative.as_posix().encode("utf-8") + b"\0")
            digest.update(sha256(regular_file(source)).encode("ascii") + b"\n")
    return digest.hexdigest()


def dependency_pins(deps):
    pins = {}
    for name in BUILD_DEPENDENCIES:
        source = deps / (name + "-src")
        if not source.is_dir():
            raise ValueError(f"missing build dependency source: {source}")
        receipts = sorted((deps / (name + "-subbuild")).rglob("*-urlinfo.txt"))
        if len(receipts) != 1:
            raise ValueError(f"expected one download receipt for {name}")
        receipt = receipts[0]
        fields = dict(line.split("=", 1) for line in regular_file(receipt).read_text().splitlines() if "=" in line)
        url = fields.get("url(s)", "")
        parsed = urlsplit(url)
        if parsed.scheme != "https" or not parsed.netloc or parsed.username or parsed.password or ";" in url:
            raise ValueError(f"{name}: download receipt must record one public HTTPS URL")
        declared = fields.get("hash", "")
        archive = receipt.parent.parent / Path(parsed.path).name
        archive_hash = sha256(regular_file(archive)) if archive.is_file() else None
        if declared:
            if not re.fullmatch("SHA256=[0-9a-fA-F]{64}", declared):
                raise ValueError(f"{name}: download does not have an exact SHA256 pin")
            pin = declared.split("=", 1)[1].lower()
            if archive_hash is not None and archive_hash != pin:
                raise ValueError(f"{name}: downloaded archive differs from its SHA256 pin")
        elif archive_hash:
            # Aurora's prebuilt packages have versioned URLs but no URL_HASH.
            # Record the bytes CMake actually fetched, rather than claim a pin.
            pin = archive_hash
        else:
            raise ValueError(f"{name}: preserve the downloaded archive to record its exact SHA256")
        pins[name] = {"url": url, "archive_sha256": pin, "source_tree_sha256": tree_sha256(source)}
    return pins


def license_sources(args, *, libporpoise=False):
    runtime, deps = args.runtime, args.deps
    sources = {
        "BlueWake-GPL-3.0.txt": (ROOT / "LICENSE",),
        "RIGHTS_AND_LICENSES.md": (ROOT / "RIGHTS_AND_LICENSES.md",),
        "RecompCore-COPYING.txt": (runtime / "COPYING",),
        "Dolphin-GPL-2.0-or-later.txt": (runtime / "LICENSES/GPL-2.0-or-later.txt",),
        "Aurora-MIT.txt": (runtime / "GXRuntime/graphics/aurora/LICENSE",),
        "Native-J3D-CC0-1.0.txt": (runtime / "LICENSES/CC0-1.0.txt",),
        "Dawn.txt": (args.dawn_license,), "DirectXShaderCompiler.txt": (args.dxc_license,),
        "SDL3-zlib.txt": (deps / "sdl3_prebuilt-src/licenses/SDL3/LICENSE.txt",),
        "zlib.txt": (deps / "zlib-src/LICENSE.md", deps / "zlib-src/LICENSE"),
        "libpng.txt": (deps / "png-src/LICENSE",), "Dear-ImGui-MIT.txt": (deps / "imgui-src/LICENSE.txt",),
        "fmt.txt": (deps / "fmt-src/LICENSE",), "FreeType.txt": (deps / "freetype-src/LICENSE.TXT",),
        "FreeType-FTL.txt": (deps / "freetype-src/docs/FTL.TXT",), "xxHash.txt": (deps / "xxhash-src/LICENSE",),
        "zstd.txt": (deps / "zstd-src/LICENSE",), "Abseil-Apache-2.0.txt": (deps / "abseil-cpp-src/LICENSE",),
        "Tracy.txt": (deps / "tracy-src/LICENSE",),
    }
    if any((args.app / name).is_file() for name in VC_BINARIES):
        sources["Visual-Cpp-Runtime.txt"] = (args.vc_runtime_license,)
    if libporpoise:
        sources["libPorpoise-MIT.txt"] = (getattr(args, "libporpoise_dir", ROOT / "ref/libporpoise") / "LICENSE",)
    result = {}
    for name, candidates in sources.items():
        source = next((p for p in candidates if p is not None and p.is_file()), None)
        if source is None:
            raise ValueError(f"missing license text for {name}")
        regular_file(source)
        if not source.stat().st_size:
            raise ValueError(f"empty license text for {name}")
        result[name] = source
    return result


def validate_provenance(args):
    original = json.loads(regular_file(args.app / "BuilderProvenance.json").read_text())
    if (not isinstance(original, dict) or original.get("source_modified") is not False or
            original.get("containsTranslatedGameCode") is not True or
            not re.fullmatch("[0-9a-f]{40}", str(original.get("source_commit", "")))):
        raise ValueError("expected an unmodified, committed Windows builder provenance record")
    if original.get("source_commit") != git(ROOT, "rev-parse", "HEAD"):
        raise ValueError("package from the source checkout revision recorded by the builder")
    if git(ROOT, "status", "--porcelain"):
        raise ValueError("source changes must be committed before packaging")
    if original.get("module_sha256") != sha256(regular_file(args.app / MODULE)):
        raise ValueError("module differs from the builder's recorded SHA256")
    lock_path = regular_file(ROOT / "config/dependencies.lock.json")
    lock = json.loads(lock_path.read_text())
    dependencies = {item["id"]: item for item in lock["dependencies"]}
    actual = {}
    for name, path in (("recompcore", args.runtime), ("dolrecomp", args.runtime / "DolRecomp")):
        expected = dependencies[name]["sha"]
        if not re.fullmatch("[0-9a-f]{40}", expected) or git(path, "rev-parse", "HEAD") != expected:
            raise ValueError(f"{name}: checkout does not match the dependency lock")
        actual[name] = {"url": dependencies[name]["url"], "sha": expected}
        active = dependencies[name].get("active_patches") if name == "recompcore" else None
        if active is not None:
            if not isinstance(active, dict) or active.get("manifest") != "patches/recompcore/active.json":
                raise ValueError("recompcore: invalid active patch manifest in the dependency lock")
            manifest = regular_file(ROOT / active["manifest"])
            if sha256(manifest) != active.get("sha256"):
                raise ValueError("recompcore: active patch manifest differs from the dependency lock")
            recipe = json.loads(manifest.read_text())
            if (not isinstance(recipe, dict) or recipe.get("base_sha") != expected or
                    recipe.get("patches") != active.get("patches")):
                raise ValueError("recompcore: active patch recipe differs from the dependency lock")
            try:
                receipt = apply_patches(path, manifest=manifest, root=ROOT, verify_only=True)
            except PatchError as error:
                raise ValueError(f"recompcore: {error}") from error
            if original.get("runtime_patches") != receipt:
                raise ValueError("recompcore: verified patches differ from the builder's runtime provenance")
            actual[name]["active_patches"] = {key: active[key] for key in ("manifest", "sha256", "patches")}
        else:
            if name == "recompcore" and original.get("runtime_patches") is not None:
                raise ValueError("recompcore: builder records patches absent from the dependency lock")
            if git(path, "status", "--porcelain", "--untracked-files=no"):
                raise ValueError(f"{name}: tracked dependency changes must be committed")
    if not isinstance(original.get("libporpoise", False), bool):
        raise ValueError("libPorpoise selection must be a boolean in builder provenance")
    if original.get("libporpoise", False):
        path = getattr(args, "libporpoise_dir", ROOT / "ref/libporpoise")
        dependency = dependencies["libporpoise"]
        expected = dependency["sha"]
        if (not original.get("native_math") or original.get("libporpoise_sha") != expected or
                not re.fullmatch("[0-9a-f]{40}", expected) or git(path, "rev-parse", "HEAD") != expected):
            raise ValueError("libporpoise: builder and checkout must match the dependency lock")
        if git(path, "status", "--porcelain"):
            raise ValueError("libporpoise: dependency must be a clean pinned checkout")
        actual["libporpoise"] = {"url": dependency["url"], "sha": expected}
    submodules = []
    for line in git(args.runtime, "submodule", "status", "--recursive", "--", "DolRecomp").splitlines():
        if not line.startswith(" "):
            raise ValueError("initialize product runtime submodules at their recorded revisions before packaging")
        sha, path, *_ = line.strip().split()
        submodules.append({"path": path, "sha": sha})
    # Unknown fields can contain personal paths or settings. Ship only the
    # builder's public fields, with the complete dependency record separately.
    provenance = {key: original[key] for key in PROVENANCE_FIELDS if key in original}
    provenance["containsTranslatedGameCode"] = True
    return provenance, {"locked_sources": actual, "runtime_submodules": submodules,
                        "dependency_lock_sha256": sha256(lock_path), "build_packages": dependency_pins(args.deps)}


def reproducible_zip(stage, target):
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for source in sorted((p for p in stage.rglob("*") if p.is_file()), key=lambda p: p.relative_to(stage).as_posix()):
            entry = zipfile.ZipInfo(f"{NAME}/{source.relative_to(stage).as_posix()}", (1980, 1, 1, 0, 0, 0))
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            entry.compress_type = zipfile.ZIP_DEFLATED
            with source.open("rb") as stream, archive.open(entry, "w", force_zip64=True) as output:
                shutil.copyfileobj(stream, output, length=1024 * 1024)


def check_public_assets(paths):
    subprocess.run([sys.executable, str(ROOT / "scripts/release/check_public_assets.py"),
                    *(str(path) for path in paths)], check=True)


def assemble(args):
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,63}", args.version):
        raise ValueError("version must be a short filename-safe release identifier")
    out = args.out.resolve()
    for source in (args.app, args.runtime, args.deps):
        if out == source.resolve() or source.resolve() in out.parents:
            raise ValueError("release output must be separate from app, runtime and dependency inputs")
    zip_name = f"{NAME}-{args.version}-windows-x64.zip"
    outputs = [out / zip_name, out / (zip_name + ".sha256")]
    if any(path.exists() for path in outputs):
        raise ValueError("release outputs already exist; use a new output directory or version")
    for name in REQUIRED_BINARIES:
        regular_file(args.app / name)
    provenance, dependencies = validate_provenance(args)
    if provenance.get("libporpoise", False):
        sdk = getattr(args, "libporpoise_dir", ROOT / "ref/libporpoise").resolve()
        if out == sdk or sdk in out.parents:
            raise ValueError("release output must be separate from libPorpoise dependency inputs")
    licenses = license_sources(args, libporpoise=provenance.get("libporpoise", False))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="windows-release-stage-", dir=args.out.parent) as work:
        work = Path(work)
        stage = work / NAME
        stage.mkdir()
        for name in BINARY_WHITELIST:
            source = args.app / name
            if source.exists():
                shutil.copyfile(regular_file(source), stage / name)
        if sha256(stage / MODULE) != provenance["module_sha256"]:
            raise ValueError("module changed while release inputs were being staged")
        verify_dlls(stage)
        (stage / "dsp").mkdir()
        for name in ("dsp_rom.bin", "dsp_coef.bin"):
            shutil.copyfile(regular_file(args.runtime / "Data/Sys/GC" / name), stage / "dsp" / name)
        shutil.copyfile(regular_file(ROOT / "windows/resources/initial_pipeline_cache.db"), stage / "initial_pipeline_cache.db")
        shutil.copyfile(regular_file(ROOT / "config/dependencies.lock.json"), stage / "dependencies.lock.json")
        (stage / "licenses").mkdir()
        for name, source in licenses.items():
            shutil.copyfile(source, stage / "licenses" / name)
        notices = ("BlueWake: GPL-3.0-or-later; RecompCore/Dolphin: GPL-2.0-or-later compatible with GPLv3.\n"
                   "Aurora and Dear ImGui: MIT. Dawn/DXC: see their complete bundled notices.\n"
                   "SDL3 and zlib: zlib. libpng: libpng-2.0. fmt: MIT. FreeType: FreeType License.\n"
                   "xxHash: BSD-2-Clause. Zstandard and Tracy: BSD-3-Clause. Abseil: Apache-2.0.\n"
                   "SQLite: public domain; its sqlite3.c header documents its dedication.\n"
                   "Native J3D formulas: zeldaret/tww (CC0-1.0). See RIGHTS_AND_LICENSES.md for attribution.\n"
                   "Microsoft Visual C++ runtime: see Visual-Cpp-Runtime.txt when bundled.\n"
                   "Game code and assets retain their owners' rights; no license grant is implied.\n")
        if provenance.get("libporpoise", False):
            notices += "libPorpoise matrix SDK: MIT; see libPorpoise-MIT.txt and BUILD.json for its pinned revision.\n"
        (stage / "licenses/THIRD_PARTY_NOTICES.txt").write_text(notices, encoding="utf-8", newline="\n")
        (stage / "BuilderProvenance.json").write_text(json.dumps(provenance, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
        (stage / "README.txt").write_text(INSTALL.format(version=args.version, march=provenance.get("march", "see BUILD.json")), encoding="utf-8", newline="\n")
        build = {"version": args.version, "platform": "windows-x64", "builder": provenance, "dependencies": dependencies,
                 "packager_sha256": sha256(Path(__file__)),
                 "files": {p.relative_to(stage).as_posix(): sha256(p) for p in sorted(stage.rglob("*")) if p.is_file()}}
        (stage / "BUILD.json").write_text(json.dumps(build, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
        files = sorted((p for p in stage.rglob("*") if p.is_file()), key=lambda p: p.relative_to(stage).as_posix())
        (stage / "SHA256SUMS").write_text("".join(f"{sha256(p)}  {p.relative_to(stage).as_posix()}\n" for p in files), encoding="utf-8", newline="\n")
        candidate = work / zip_name
        reproducible_zip(stage, candidate)
        checksum = work / (zip_name + ".sha256")
        checksum.write_text(f"{sha256(candidate)}  {zip_name}\n", encoding="ascii", newline="\n")
        check_public_assets([candidate, checksum])
        out.mkdir(parents=True, exist_ok=True)
        # A same-filesystem exclusive link cannot overwrite an existing release.
        # If installing either output fails, remove only this run's own output.
        installed = []
        try:
            for source, target in zip((candidate, checksum), outputs):
                os.link(source, target)
                installed.append(target)
        except OSError:
            for target in installed:
                target.unlink()
            raise
    print(f"Packaged {outputs[0]} (sha256 {sha256(outputs[0])})")
    return outputs[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("version")
    parser.add_argument("--app", type=Path, default=ROOT / "build/windows/BlueWake")
    parser.add_argument("--out", type=Path, default=ROOT / "build/windows/release")
    parser.add_argument("--runtime", type=Path, default=ROOT / "ref/recompcore")
    parser.add_argument("--libporpoise-dir", type=Path, default=ROOT / "ref/libporpoise",
                        help="clean pinned matrix SDK checkout when the builder enabled libPorpoise")
    parser.add_argument("--deps", type=Path, default=ROOT / "build/windows/app/_deps")
    parser.add_argument("--dawn-license", required=True, type=Path, help="complete Dawn/Tint and third-party license text")
    parser.add_argument("--dxc-license", required=True, type=Path, help="complete DirectXShaderCompiler license text")
    parser.add_argument("--vc-runtime-license", type=Path, help="Microsoft redistribution license for any bundled VC runtime")
    args = parser.parse_args()
    try:
        assemble(args)
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"package-release: {error}\n")


if __name__ == "__main__":
    main()
