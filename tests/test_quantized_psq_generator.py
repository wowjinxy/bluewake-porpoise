#!/usr/bin/env python3
"""ROM-free CLI and output-contract checks for experimental PSQ generation.

CPU/memory/FP equivalence requires the separate full-runtime differential
fixture; these tests only establish option admission and bounded output edits.
"""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

import test_composite as fixture


# Captured before the option was added, from the existing deterministic two-
# chunk synthetic fixture at root commit 0dae32f847b510fc35f3d13ba44390f6d108ea51.
DEFAULT_HEADER_SHA256 = {
    "generated.h": "67a22356d7ed94533b1fe5e28c08d50fe0d27c96343e2676f4d952e436581316",
    "generated_composite.h": "bbf7bea1ee0cf2518e8342fcc87c69494b3a5821e60eb121ca312a7b9818cdae",
}


def generate(root, output, *options):
    command = [sys.executable, str(fixture.SCRIPTS_DIR / "generate_composite.py"),
               "--dol-dir", str(root / "dol_out"),
               "--rels-dir", str(root / "rels_out"),
               "--rels-bin-dir", str(root / "rels_bin"),
               "--main-dol", str(root / "input" / "main.dol"),
               "--output-dir", str(output), *options]
    result = subprocess.run(command, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    return {path.relative_to(output).as_posix(): path.read_bytes()
            for path in output.rglob("*") if path.is_file()}


def test_default_output_remains_exact():
    with tempfile.TemporaryDirectory() as directory:
        root = fixture.setup_fixture(Path(directory))
        outputs = generate(root, root / "default")
        for name, expected in DEFAULT_HEADER_SHA256.items():
            assert hashlib.sha256(outputs[name]).hexdigest() == expected, name


def test_option_changes_only_helper_region():
    with tempfile.TemporaryDirectory() as directory:
        root = fixture.setup_fixture(Path(directory))
        baseline = generate(root, root / "default")
        candidate = generate(root, root / "candidate", "--quantized-psq")
        assert baseline.keys() == candidate.keys()
        for name, content in baseline.items():
            if name not in DEFAULT_HEADER_SHA256:
                assert candidate[name] == content, name
        for name in DEFAULT_HEADER_SHA256:
            old = baseline[name].decode()
            new = candidate[name].decode()
            assert old.split("/* Unquantised", 1)[0] == new.split("/* Experimental quantized", 1)[0]
            assert old.split("// Function entry points", 1)[1] == new.split("// Function entry points", 1)[1]
            assert "\n".join(fixture.gc.quantized_psq_helpers()) in new
            # Independently emitted default bodies must be retained verbatim
            # except their private names, before either new GQR snapshot.
            legacy = old[old.index("static inline bool ppc_psq_load_inline"):]
            legacy = legacy.split("// Function entry points", 1)[0].rstrip()
            legacy = legacy.replace("ppc_psq_load_inline", "bw_composite_psq_type0_load_inline")
            legacy = legacy.replace("ppc_psq_store_inline", "bw_composite_psq_type0_store_inline")
            assert legacy in new
            for mode in ("load", "store"):
                body = new.split(f"static inline bool ppc_psq_{mode}_inline", 1)[1]
                assert body.index("if ((gqr & 7u) < 4u)") < body.index("const u32 g =")
                # Hardware integer operations retain the existing whole-PSQ
                # barrier after admission/type0, before using the old snapshot.
                assert body.index("if (type == 0u)") < body.index("if (bw_hardware(ea))")
                assert body.index("if (bw_hardware(ea))") < body.index("const f32 factor")
                assert f"return ppc_psq_{mode}(" in body.split("if (bw_hardware(ea))", 1)[1]
            start = new.index('#pragma push_macro("mem_read8")')
            end = new.index('#pragma pop_macro("mem_read8")')
            assert start < new.index("static inline f64 bw_composite_psq_load_value") < end
            assert start < new.index("static inline void bw_composite_psq_store_value") < end
            assert end < new.index("static inline bool ppc_psq_load_inline")
            for name in ("mem_read8", "mem_read16", "mem_write8", "mem_write16"):
                assert new.count(f'#pragma push_macro("{name}")') == 1
                assert new.count(f'#pragma pop_macro("{name}")') == 1
                assert new.count(f"#undef {name}\n") == 1
            for name in ("mem_read32", "mem_write32", "ppc_psq_load", "ppc_psq_store"):
                assert f"#undef {name}\n" not in new


def test_option_output_is_deterministic():
    with tempfile.TemporaryDirectory() as directory:
        root = fixture.setup_fixture(Path(directory))
        first = generate(root, root / "first", "--quantized-psq")
        second = generate(root, root / "second", "--quantized-psq")
        assert first == second


if __name__ == "__main__":
    for test in [test_default_output_remains_exact,
                 test_option_changes_only_helper_region,
                 test_option_output_is_deterministic]:
        test()
        print("PASS:", test.__name__)
