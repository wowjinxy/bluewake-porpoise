#!/usr/bin/env python3
"""Apply and verify the exact local patch set on the pinned RecompCore source."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "patches/recompcore/active.json"


class PatchError(RuntimeError):
    pass


def git(checkout, *args, env=None, check=True):
    environment = dict(os.environ if env is None else env, GIT_OPTIONAL_LOCKS="0")
    result = subprocess.run(["git", "-C", str(checkout), *map(str, args)],
                            env=environment, capture_output=True, text=True)
    if check and result.returncode:
        raise PatchError(result.stderr.strip() or f"git {' '.join(map(str, args))} failed")
    return result


def apply_patches(checkout, manifest=MANIFEST, root=ROOT, *, verify_only=False):
    """Accept only clean base sources or the exact complete patched tree.

    A temporary index constructs the expected Git tree without changing the
    user's index. Partial patches and unrelated tracked edits are rejected.
    """
    checkout, root, manifest = Path(checkout).resolve(), Path(root).resolve(), Path(manifest).resolve()
    try:
        recipe = json.loads(manifest.read_text(encoding="utf-8"))
        base = recipe["base_sha"]
        patches = recipe["patches"]
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise PatchError(f"invalid runtime patch manifest {manifest}: {error}") from error
    if git(checkout, "rev-parse", "HEAD").stdout.strip() != base:
        raise PatchError(f"runtime patches require RecompCore at {base}")
    if git(checkout, "diff", "--cached", "--quiet", "HEAD", check=False).returncode:
        raise PatchError("RecompCore has staged changes; preserve them before building")
    paths, fingerprints = [], {}
    for patch in patches:
        path = (root / patch["path"]).resolve()
        if not path.is_relative_to(root / "patches/recompcore"):
            raise PatchError(f"runtime patch is outside patches/recompcore: {path}")
        try:
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
        except OSError as error:
            raise PatchError(f"cannot read runtime patch {path}: {error}") from error
        if digest != patch["sha256"]:
            raise PatchError(f"runtime patch checksum mismatch: {path}")
        paths.append(path)
        fingerprints[patch["path"]] = digest
    if len(set(paths)) != len(paths):
        raise PatchError("runtime patch manifest contains duplicate patches")
    with tempfile.TemporaryDirectory(prefix="bluewake-runtime-index-") as work:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(work) / "index"))
        git(checkout, "read-tree", base, env=env)
        for path in paths:
            git(checkout, "apply", "--cached", "--check", path, env=env)
            git(checkout, "apply", "--cached", path, env=env)
        tree = git(checkout, "write-tree", env=env).stdout.strip()
        diff_args = ("diff", "--quiet", "--no-ext-diff", "--no-textconv", "--ignore-submodules=none")
        current = git(checkout, *diff_args, env=env, check=False)
        if current.returncode not in (0, 1):
            raise PatchError(current.stderr.strip() or "cannot verify patched RecompCore")
        if current.returncode:
            if verify_only:
                raise PatchError("RecompCore does not match the verified patch set; rerun the builder's dependencies step")
            base_env = dict(os.environ, GIT_INDEX_FILE=str(Path(work) / "base-index"))
            git(checkout, "read-tree", base, env=base_env)
            clean = git(checkout, *diff_args, env=base_env, check=False)
            if clean.returncode:
                changed = git(checkout, "diff", "--name-only", "--ignore-submodules=none", env=env).stdout.strip()
                raise PatchError("RecompCore differs from the exact pinned patch set; "
                                 f"preserve the local changes before building:\n{changed}")
            for path in paths:
                git(checkout, "apply", "--check", path, env=env)
                git(checkout, "apply", path, env=env)
            if git(checkout, *diff_args, env=env, check=False).returncode:
                raise PatchError("runtime patch application did not produce the expected tree")
    return {"base_sha": base, "patched_tree": tree, "patches": fingerprints}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkout", type=Path)
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    parser.add_argument("--verify-only", action="store_true", help="verify without applying patches")
    args = parser.parse_args()
    try:
        receipt = apply_patches(args.checkout, args.manifest, verify_only=args.verify_only)
    except PatchError as error:
        parser.exit(1, f"runtime patches: {error}\n")
    print(json.dumps(receipt, sort_keys=True))


if __name__ == "__main__":
    main()
