#!/usr/bin/env python3
"""Generate native asset headers without requiring Bash or Unix utilities.

Python counterpart of gen_assets.sh. --stubs produces headers containing no game
data for COS_RUNTIME_ASSETS=ON; the game reads the player's disc at startup.
Outputs must be fresh directories. Failed preparation directories are preserved.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import uuid


REPO_ROOT = Path(__file__).resolve().parents[2]
VERSION = "GZLE01"


def run(argv, cwd=None, capture=False):
    return subprocess.run(
        [str(arg) for arg in argv], cwd=cwd, check=True, text=True,
        stdout=subprocess.PIPE if capture else None, timeout=600,
    ).stdout


def ensure_decomp(directory, url, commit):
    if not (directory / ".git").exists():
        if directory.exists() and any(directory.iterdir()):
            raise ValueError(f"{directory} exists and is not a Git checkout; left unchanged")
        directory.mkdir(parents=True, exist_ok=True)
        run(["git", "-C", directory, "init", "-q"])
        run(["git", "-C", directory, "remote", "add", "origin", url])
        run(["git", "-C", directory, "fetch", "--depth", "1", "origin", commit])
        run(["git", "-C", directory, "-c", "advice.detachedHead=false",
             "checkout", "--detach", "FETCH_HEAD"])
    head = run(["git", "-C", directory, "rev-parse", "HEAD"], capture=True).strip()
    if head != commit:
        raise ValueError(f"{directory} is at {head}, not {commit}; left unchanged")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--disc", type=Path, default=os.environ.get("COS_DISC"))
    parser.add_argument("--decomp", type=Path, default=REPO_ROOT / "build/decomp")
    build_name = "native-win" if os.name == "nt" else "native-mac"
    parser.add_argument("--out", type=Path,
                        default=REPO_ROOT / "build" / build_name / "assets" / VERSION)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--res-only", action="store_true")
    modes.add_argument("--stubs", action="store_true")
    args = parser.parse_args()
    decomp, out = args.decomp.resolve(), args.out.resolve()
    resource_only = args.res_only or args.stubs
    if out.exists() and (not out.is_dir() or any(out.iterdir())):
        parser.error(f"output {out} is not an empty directory; choose a fresh --out")
    if not resource_only:
        if args.disc is None or not args.disc.is_file():
            parser.error("pass --disc PATH or set COS_DISC to the GZLE01 revision 0 ISO")
        if shutil.which("ninja") is None:
            parser.error("the full asset extraction needs Ninja on PATH; --stubs does not")
    upstream = {}
    for line in (REPO_ROOT / "game/UPSTREAM").read_text(encoding="utf-8").splitlines():
        if "=" in line and not line.startswith("#"):
            name, value = line.split("=", 1)
            upstream[name] = value
    url, commit = upstream["url"], upstream["commit"]
    ensure_decomp(decomp, url, commit)
    stage = out.with_name(out.name + ".prepare-" + uuid.uuid4().hex[:10])
    stage.mkdir(parents=True)
    print(f"gen_assets: preparing {stage}", flush=True)
    try:
        shutil.copytree(decomp / "assets" / VERSION / "res", stage / "res")
        if args.stubs:
            run([sys.executable, REPO_ROOT / "native/tools/gen_asset_stubs.py",
                 "--decomp", decomp, "--out", stage])
        elif not resource_only:
            disc = args.disc.resolve()
            orig = decomp / "orig" / VERSION
            orig.mkdir(parents=True, exist_ok=True)
            linked_disc = orig / disc.name
            if linked_disc.exists():
                if not linked_disc.samefile(disc):
                    raise ValueError(f"{linked_disc} already exists; left unchanged")
            else:
                try:
                    os.link(disc, linked_disc)
                except OSError:
                    # Windows symlinks need elevated privileges. A full extraction
                    # may instead need a private copy when the disc is on another drive.
                    print(f"gen_assets: copying disc to {linked_disc}", flush=True)
                    shutil.copyfile(disc, linked_disc)
            run([sys.executable, "configure.py", "--version", VERSION], cwd=decomp)
            run(["ninja", f"build/{VERSION}/config.json"], cwd=decomp)
            targets = run(["ninja", "-t", "targets", "all"], cwd=decomp, capture=True)
            headers = re.findall(rf"^(build/{VERSION}/include/assets/[^:]+):", targets, re.M)
            if not headers:
                raise ValueError("no asset header targets were generated in build.ninja")
            run(["ninja", *headers], cwd=decomp)
            shutil.copytree(decomp / "build" / VERSION / "include/assets",
                            stage / "include/assets")
        metadata = {"game_id": VERSION, "decomp_repository": url,
                    "decomp_commit": commit,
                    "mode": "stubs" if args.stubs else "res-only" if args.res_only else "disc"}
        (stage / "generation.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        if out.exists():
            out.rmdir()  # Only the already-validated empty output directory.
        stage.rename(out)
    except BaseException:
        print(f"gen_assets: incomplete preparation preserved at {stage}", file=sys.stderr)
        raise
    count = sum(path.is_file() for path in out.rglob("*"))
    print(f"gen_assets: {count} files in {out} ({metadata['mode']})")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        sys.exit(f"gen_assets: {error}")
