#!/usr/bin/env python3
"""Fetch the pinned DSP HLE sources without requiring a POSIX shell.

Python counterpart of fetch_recompcore.sh. Existing checkouts are inspected but
never moved to another revision. The native build only reads this source tree.
"""

import argparse
from pathlib import Path
import subprocess
import sys


REPOSITORY = "https://github.com/elliotttate/RecompCore.git"
COMMIT = "8ab24daee9c641634fda5cac30389ad4b2cfda5e"
REPO_ROOT = Path(__file__).resolve().parents[2]


def git(directory, *args, capture=False):
    return subprocess.run(
        ["git", "-C", str(directory), *args], check=True, text=True,
        stdout=subprocess.PIPE if capture else None, timeout=300,
    ).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", nargs="?", type=Path,
                        default=REPO_ROOT / "ref" / "recompcore")
    args = parser.parse_args()
    directory = args.directory.resolve()
    if not (directory / ".git").exists():
        if directory.exists() and any(directory.iterdir()):
            parser.error(f"{directory} exists and is not a Git checkout; left unchanged")
        directory.mkdir(parents=True, exist_ok=True)
        git(directory, "init", "-q")
        git(directory, "remote", "add", "origin", REPOSITORY)
        git(directory, "fetch", "--depth", "1", "origin", COMMIT)
        git(directory, "-c", "advice.detachedHead=false", "checkout", "--detach", "FETCH_HEAD")
    head = git(directory, "rev-parse", "HEAD", capture=True).strip()
    if head != COMMIT:
        parser.error(f"{directory} is at {head}, not {COMMIT}; left unchanged")
    if not (directory / "Source/Core/Core/HW/DSPHLE/UCodes/UCodes.cpp").is_file():
        parser.error(f"{directory} lacks the pinned DSP HLE sources")
    print(f"fetch_recompcore: {directory} at {COMMIT}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.SubprocessError) as error:
        sys.exit(f"fetch_recompcore: {error}")
