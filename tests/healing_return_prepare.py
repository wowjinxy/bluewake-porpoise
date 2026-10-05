#!/usr/bin/env python3
"""Generate the authored analog's return guard; never read game/module assets."""
import importlib.util
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("bw_direct", ROOT / "scripts/windows/direct_calls.py")
direct = importlib.util.module_from_spec(spec)
spec.loader.exec_module(direct)

def prepare(out):
    source = (ROOT / "tests/healing_return_chunk.c.in").read_text()
    guarded, count = direct.transform_healing_return(source)
    assert count == 1 and direct.transform_healing_return(guarded) == (guarded, 0)
    assert direct.healing_return_contract(guarded)
    for name, body in (("healing_return_original.inc", source), ("healing_return_guarded.inc", guarded)):
        body = body.replace(direct.INCLUDE, "")
        (out / name).write_text(body)

if __name__ == "__main__":
    prepare(Path(sys.argv[1]))
