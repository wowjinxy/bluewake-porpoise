#!/usr/bin/env python3
"""Assemble a local Mac app without touching installed apps or player storage."""
import argparse
import hashlib
import json
import plistlib
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
from module_optimizations import MODES

MACOS_MINIMUM = "14.0"
ARCHITECTURES = {0x01000007: "x86_64", 0x0100000C: "arm64"}


def macho_minimums(path):
    """Read every Mach-O slice; missing/malformed deployment records fail closed."""
    data = path.read_bytes()

    def thin(start, length, expected_cpu=None):
        end = start + length
        if length < 32 or end > len(data):
            raise ValueError(f"{path.name}: truncated Mach-O slice")
        magic = data[start:start + 4]
        endian = {b"\xcf\xfa\xed\xfe": "<", b"\xfe\xed\xfa\xcf": ">"}.get(magic)
        if endian is None:
            raise ValueError(f"{path.name}: expected a 64-bit Mach-O binary")
        _, cpu, _, _, count, command_bytes, _, _ = struct.unpack_from(endian + "8I", data, start)
        if cpu not in ARCHITECTURES or (expected_cpu is not None and cpu != expected_cpu):
            raise ValueError(f"{path.name}: unsupported or inconsistent architecture")
        commands_end = start + 32 + command_bytes
        if commands_end > end or count > command_bytes // 8:
            raise ValueError(f"{path.name}: truncated Mach-O load commands")
        offset, minimum = start + 32, None
        for _ in range(count):
            if offset + 8 > commands_end:
                raise ValueError(f"{path.name}: truncated Mach-O load command")
            command, size = struct.unpack_from(endian + "2I", data, offset)
            if size < 8 or size % 8 or offset + size > commands_end:
                raise ValueError(f"{path.name}: malformed Mach-O load command")
            if command == 0x32:  # LC_BUILD_VERSION
                if size < 24:
                    raise ValueError(f"{path.name}: malformed LC_BUILD_VERSION")
                platform, version, _, tools = struct.unpack_from(endian + "4I", data, offset + 8)
                if platform != 1 or minimum is not None or size != 24 + 8 * tools:
                    raise ValueError(f"{path.name}: invalid macOS LC_BUILD_VERSION")
                minimum = (version >> 16, (version >> 8) & 255, version & 255)
                if minimum[0] == 0:
                    raise ValueError(f"{path.name}: invalid macOS minimum version")
            offset += size
        if offset != commands_end or minimum is None:
            raise ValueError(f"{path.name}: missing or malformed LC_BUILD_VERSION")
        return ARCHITECTURES[cpu], minimum

    magic = data[:4]
    fat = {b"\xca\xfe\xba\xbe": (">", False), b"\xbe\xba\xfe\xca": ("<", False),
           b"\xca\xfe\xba\xbf": (">", True), b"\xbf\xba\xfe\xca": ("<", True)}.get(magic)
    if fat is None:
        arch, version = thin(0, len(data))
        return {arch: version}
    if len(data) < 8:
        raise ValueError(f"{path.name}: truncated universal Mach-O header")
    endian, wide = fat
    count = struct.unpack_from(endian + "I", data, 4)[0]
    size = 32 if wide else 20
    table_end = 8 + count * size
    if count == 0 or table_end > len(data):
        raise ValueError(f"{path.name}: malformed universal Mach-O table")
    spans, minimums = [], {}
    for index in range(count):
        entry = struct.unpack_from(endian + ("IIQQII" if wide else "5I"), data, 8 + index * size)
        cpu, _, start, length, alignment = entry[:5]
        if (start < table_end or start + length > len(data) or alignment > 31 or
                start % (1 << alignment) or any(start < end and begin < start + length for begin, end in spans)):
            raise ValueError(f"{path.name}: invalid or overlapping universal Mach-O slices")
        arch, version = thin(start, length, cpu)
        if arch in minimums:
            raise ValueError(f"{path.name}: duplicate universal Mach-O architecture")
        spans.append((start, start + length))
        minimums[arch] = version
    return minimums


def validate_deployment(executable, module=None):
    binaries = {"host": macho_minimums(executable)}
    if module is not None:
        binaries["module"] = macho_minimums(module)
        if not binaries["host"].keys() <= binaries["module"].keys():
            raise ValueError("the game module does not support every host architecture")
    maximum = (14, 0, 0)
    for name, slices in binaries.items():
        for arch, version in slices.items():
            if version > maximum:
                required = ".".join(map(str, version))
                raise ValueError(f"{name} ({arch}) requires macOS {required}, newer than advertised {MACOS_MINIMUM}")
    return {name: {arch: ".".join(map(str, version)) for arch, version in slices.items()}
            for name, slices in binaries.items()}


def sha(path):
    with path.open("rb") as stream:
        digest = hashlib.sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
        return digest.hexdigest()


def assemble(args):
    personal = args.module is not None
    if personal != (args.game is not None) or personal != (args.disc is not None):
        raise ValueError("--module, --game and --disc must be supplied together")
    if args.module_optimizations != "none" and not personal:
        raise ValueError("combined optimizations require a personal module")
    executable = args.app / "Contents/MacOS/BlueWake"
    if not executable.is_file():
        raise ValueError("the source app has no BlueWake executable")
    if (args.app / "Contents/Resources/Game").exists() or (args.app / "Contents/Frameworks/gGZLE01_recomp.dylib").exists():
        raise ValueError("the source app already contains personal game inputs")
    # A player app must not depend on libraries in a developer checkout/Homebrew.
    linked = subprocess.check_output(["otool", "-L", executable], text=True)
    for line in linked.splitlines()[1:]:
        library = line.strip().split(" (", 1)[0]
        if not library.startswith(("/System/Library/", "/usr/lib/")):
            raise ValueError(f"host dependency is not self-contained: {library}")
    if personal:
        if not args.module.is_file() or not args.disc.is_file() or not (args.game / "main.dol").is_file():
            raise ValueError("missing personal module, disc or extracted executable")
        if len(list((args.game / "rels").glob("*.rel"))) != 415:
            raise ValueError("expected all 415 extracted RELs")
    minimums = validate_deployment(executable, args.module if personal else None)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix="mac-stage-", dir=args.output.parent))
    app = stage / "BlueWake.app"
    shutil.copytree(args.app, app)
    resources = app / "Contents/Resources"
    info_path = app / "Contents/Info.plist"
    info = plistlib.loads(info_path.read_bytes())
    info["LSMinimumSystemVersion"] = MACOS_MINIMUM
    info_path.write_bytes(plistlib.dumps(info))
    provenance = {
        "platform": "macos", "source_commit": args.source_commit,
        "source_modified": args.source_modified,
        "runtime_commit": subprocess.check_output(["git", "-C", args.runtime, "rev-parse", "HEAD"], text=True).strip(),
        "translator_commit": subprocess.check_output(["git", "-C", args.runtime / "DolRecomp", "rev-parse", "HEAD"], text=True).strip(),
        "containsTranslatedGameCode": personal,
        "module_optimizations": args.module_optimizations,
        "macos_minimum": MACOS_MINIMUM,
        "binary_minimums": minimums,
    }
    if personal:
        game = resources / "Game"
        game.mkdir()
        shutil.copy2(args.game / "main.dol", game / "main.dol")
        shutil.copytree(args.game / "rels", game / "rels")
        shutil.copy2(args.disc, game / "GZLE01.iso")
        frameworks = app / "Contents/Frameworks"
        frameworks.mkdir(exist_ok=True)
        module = frameworks / "gGZLE01_recomp.dylib"
        shutil.copy2(args.module, module)
        subprocess.run(["codesign", "--force", "--sign", args.identity, module], check=True)
        provenance.update(module_sha256=sha(module), disc_sha256=sha(game / "GZLE01.iso"))
        if provenance["disc_sha256"] != sha(args.disc):
            raise ValueError("disc copy failed verification")
    # These runtime resources are versioned with RecompCore, not private caches.
    dsp = resources / "DSP"
    dsp.mkdir(exist_ok=True)
    for name in ("dsp_rom.bin", "dsp_coef.bin"):
        shutil.copy2(args.runtime / "Data/Sys/GC" / name, dsp / name)
    (resources / "BuilderProvenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    if args.module_optimizations == "combined-v1":
        (resources / "ModuleOptimizations").write_text("combined-v1\n")
    subprocess.run(["codesign", "--force", "--sign", args.identity, app], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", app], check=True)
    # Only replace the builder's output after the complete candidate verifies.
    # Retain the old app so interruptions or changed inputs never discard it.
    if args.output.exists():
        previous = Path(tempfile.mkdtemp(prefix="mac-previous-", dir=args.output.parent))
        args.output.rename(previous / args.output.name)
        print(f"Previous app preserved: {previous / args.output.name}")
    app.rename(args.output)
    stage.rmdir()
    print(f"{'Personal' if personal else 'App-only'} Mac app: {args.output}")
    return provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("app", "output", "runtime"):
        parser.add_argument("--" + name, required=True, type=Path)
    for name in ("module", "game", "disc"):
        parser.add_argument("--" + name, type=Path)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--source-modified", action="store_true")
    parser.add_argument("--module-optimizations", choices=MODES, default="none")
    parser.add_argument("--identity", default="-")
    args = parser.parse_args()
    if args.output.suffix != ".app" or args.output.resolve() == args.app.resolve():
        parser.error("--output must name a separate .app")
    try:
        assemble(args)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"package-macos: {error}\n")


if __name__ == "__main__":
    main()
