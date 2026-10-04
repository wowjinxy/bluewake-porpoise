#!/usr/bin/env python3
"""Prepare a pinned, source-only libPorpoise matrix constructor library.

The generated translation unit contains the unchanged C_MTXIdentity,
C_MTXTrans and C_MTXScale definitions from upstream src/mtx/mtx.c. Selecting
these independent functions avoids linking libPorpoise's SDL2 platform loop
or mixing its native SDK pointers with the recompiler's guest CPU ABI.
Only line endings are normalized. The upstream header remains the authority
for their declarations; CMake privately renames the symbols at compilation.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


PINNED_REVISION = "9ea0e6ebef7e3be432b92487639991ca0251b0f4"
# SHA256 of the upstream Git blobs, with LF line endings. A clean Windows
# checkout may use CRLF according to Git's configured checkout conversion.
SOURCE_HASHES = {
    "src/mtx/mtx.c": "8f99bc1db0609b809ba468d4b1999e06865f1f80a1968704ff58df96f5efc8dc",
    "include/dolphin/mtx.h": "aec8822c3ae6d1585c55e00a94b090463d580136dcf7fe7e0c3e68c3d827a45b",
    "include/dolphin/types.h": "6dcbb3e41ee10112db2b0f151601d74a3e975f5a259ad0990915168a92dea4c9",
    "include/dolphin/vec.h": "87ddf866cb2cbacc68d11bc6351aba6f8193849030d5a1588763192e72c2c1c4",
    "include/dolphin/os/OSVersion.h": "015d0d86a19ac6c9c6fedec14e8b1fffed455cbb5906df9f2b5021a15628efaa",
}
FUNCTIONS = ("C_MTXIdentity", "C_MTXTrans", "C_MTXScale")


class PreparationError(RuntimeError):
    """The upstream checkout cannot be qualified for this source slice."""


def _git(source: Path, *arguments: str) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(source), *arguments],
            check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            encoding="utf-8", errors="replace",
        )
    except (OSError, subprocess.CalledProcessError) as error:
        detail = getattr(error, "stderr", None) or str(error)
        raise PreparationError(f"Cannot inspect libPorpoise checkout: {detail.strip()}") from error
    return result.stdout.strip()


def verify_source_tree(source: Path) -> dict[str, bytes]:
    source = source.resolve()
    if not source.is_dir():
        raise PreparationError(f"libPorpoise checkout does not exist: {source}")
    git_root = Path(_git(source, "rev-parse", "--show-toplevel")).resolve()
    if git_root != source:
        raise PreparationError(f"LIBPORPOISE_DIR must be the checkout root: {git_root}")
    revision = _git(source, "rev-parse", "HEAD")
    if revision != PINNED_REVISION:
        raise PreparationError(
            f"libPorpoise HEAD must be {PINNED_REVISION}; found {revision}"
        )
    status = _git(source, "status", "--porcelain=v1", "--untracked-files=all")
    if status:
        raise PreparationError(f"libPorpoise checkout must be clean:\n{status}")

    files = {}
    for relative, expected in SOURCE_HASHES.items():
        path = source / relative
        if path.is_symlink() or not path.is_file() or not path.resolve().is_relative_to(source):
            raise PreparationError(f"Missing or redirected libPorpoise source: {relative}")
        content = path.read_bytes().replace(b"\r\n", b"\n")
        actual = hashlib.sha256(content).hexdigest()
        if actual != expected:
            raise PreparationError(
                f"libPorpoise source hash mismatch: {relative}: {actual} (expected {expected})"
            )
        files[relative] = content
    return files


def extract_function(source: str, name: str) -> str:
    """Keep a complete upstream definition, including comments in its body."""
    pattern = re.compile(r"(?m)^void " + re.escape(name) + r"\([^\n]*\)\s*\{")
    matches = list(pattern.finditer(source))
    if len(matches) != 1:
        raise PreparationError(f"Expected exactly one upstream definition of {name}")
    match = matches[0]
    depth, position = 1, match.end()
    state = "code"
    while position < len(source):
        character = source[position]
        following = source[position:position + 2]
        if state == "line_comment":
            if character == "\n":
                state = "code"
        elif state == "block_comment":
            if following == "*/":
                state = "code"
                position += 1
        elif state in ("string", "character"):
            if character == "\\":
                position += 1
            elif character == ('"' if state == "string" else "'"):
                state = "code"
        elif following in ("//", "/*"):
            state = "line_comment" if following == "//" else "block_comment"
            position += 1
        elif character in ('"', "'"):
            state = "string" if character == '"' else "character"
        elif character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():position + 1]
        position += 1
    raise PreparationError(f"Unterminated upstream definition of {name}")


def _write_if_changed(output: Path, content: bytes) -> bool:
    if output.is_file() and output.read_bytes() == content:
        return False
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, prefix=output.name + ".",
                                         suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(content)
        os.replace(temporary, output)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()
    return True


def prepare(source: Path | str, output: Path | str) -> dict:
    source, output = Path(source).resolve(), Path(output).resolve()
    if output.is_relative_to(source):
        raise PreparationError("Generated math source must be outside the libPorpoise checkout")
    if output.suffix != ".c":
        raise PreparationError("Generated math source must have a .c extension")
    files = verify_source_tree(source)
    upstream = files["src/mtx/mtx.c"].decode("utf-8")
    bodies = [extract_function(upstream, name) for name in FUNCTIONS]
    content = (
        "/* Generated from cybervisi0n/libPorpoise, revision " + PINNED_REVISION + ".\n"
        " * Source slice: unchanged C matrix constructors; LF line endings.\n"
        " * Copyright (c) 2025 libPorpoise Contributors. MIT license.\n"
        " * The upstream LICENSE accompanies distributed libraries.\n"
        " */\n#include <dolphin/mtx.h>\n\n" + "\n\n".join(bodies) + "\n"
    ).encode("utf-8")
    changed = _write_if_changed(output, content)
    return {
        "revision": PINNED_REVISION,
        "source_sha256": SOURCE_HASHES,
        "functions": list(FUNCTIONS),
        "output_sha256": hashlib.sha256(content).hexdigest(),
        "changed": changed,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="Pinned libPorpoise checkout root")
    parser.add_argument("--output", type=Path, required=True, help="Generated C file under the build directory")
    parser.add_argument("--quiet", action="store_true", help="Suppress the successful preparation receipt")
    args = parser.parse_args()
    try:
        result = prepare(args.source, args.output)
    except (PreparationError, OSError, UnicodeError) as error:
        parser.exit(1, f"libPorpoise math preparation failed: {error}\n")
    if not args.quiet:
        print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
