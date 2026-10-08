#!/usr/bin/env python3
"""ROM-free actual-runtime/generated PSQ differential qualification.

Standalone, never part of a default game/module build. Example from a Windows
developer shell (Clang and ASan must be installed):

    python -B tests/test_quantized_psq_runtime.py --compiler C:/path/clang.exe \
        --output build/psq-runtime-test1

The SDK and optional scaling patch are read-only. All objects, generated helper
text, patched source, logs and receipts go to a NEW output directory. Existing
outputs are never replaced. --environment-json can provide an explicit saved
compiler environment; its contents are used privately and never emitted.
"""
import argparse
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
CANONICAL_SHA256 = "6f6f0251355bcf4e7f4beb45487e504fe6b4148e33719f928756f291068b106b"
PATCH_SHA256 = "8c9ce9fcf57ebce4bc00925c844ccba10de64e53a274aa09c38ad6faf61bfcfc"
CANDIDATE_SHA256 = "d9dd08fbb1aebb8fc312694a26fed3de01506ae16e89aa92f2d35b6f0b413875"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def record(path):
    path = Path(path).resolve()
    require(path.is_file() and not path.is_symlink(), "Regular input file: " + str(path))
    return {"path": str(path), "bytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def dependencies(text):
    # Clang Windows paths contain literal backslashes and escaped spaces.
    text = text.replace("\\\r\n", " ").replace("\\\n", " ")
    offset = text.find(": ")
    require(offset > 0, "Malformed Make dependency target")
    text = text[offset + 2:]
    result, token, index = [], "", 0
    while index < len(text):
        value = text[index]
        if value == "\\" and index + 1 < len(text) and text[index + 1] in " \t#\\":
            token += text[index + 1]
            index += 2
            continue
        require(value not in "#$", "Unsupported Make comment/variable")
        if value.isspace():
            if token:
                result.append(Path(token).resolve())
                token = ""
        else:
            token += value
        index += 1
    if token:
        result.append(Path(token).resolve())
    return result


def apply_guarded_patch(original, patch, newline):
    """Apply only the two exact, pinned single-file hunks in private memory."""
    require(patch.startswith("--- a/GXRuntime/src/core/cpu.c\n+++ b/GXRuntime/src/core/cpu.c\n"),
            "Only the approved single cpu.c patch is accepted")
    source, output, cursor = original.splitlines(keepends=True), [], 0
    lines = patch.splitlines(keepends=True)
    index, hunks = 2, 0
    while index < len(lines):
        match = re.fullmatch(r"@@ -(\d+),(\d+) \+(\d+),(\d+) @@\n", lines[index])
        require(match is not None, "Unexpected patch hunk")
        start, old_count, _, new_count = map(int, match.groups())
        require(start - 1 >= cursor, "Overlapping patch hunk")
        output.extend(source[cursor:start - 1])
        cursor = start - 1
        index += 1
        removed = added = 0
        while index < len(lines) and not lines[index].startswith("@@"):
            line = lines[index]
            require(line[:1] in " +-", "Unsupported patch line")
            if line[0] in " -":
                require(cursor < len(source) and source[cursor] == line[1:],
                        "Patch context differs from canonical SDK")
                cursor += 1
                removed += 1
            if line[0] in " +":
                output.append(line[1:])
                added += 1
            index += 1
        require((removed, added) == (old_count, new_count), "Patch hunk counts")
        hunks += 1
    require(hunks == 2, "Exactly two arithmetic hunks required")
    output.extend(source[cursor:])
    result = "".join(output).encode()
    # The frozen Windows SDK/overlay use CRLF; retain its byte-level contract.
    if newline == b"\r\n":
        result = result.replace(b"\n", b"\r\n")
    require(hashlib.sha256(result).hexdigest() == CANDIDATE_SHA256, "Patched CPU SHA mismatch")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--compiler", required=True, type=Path, help="Explicit Clang executable")
    parser.add_argument("--output", required=True, type=Path, help="New private output directory")
    parser.add_argument("--sdk", type=Path, default=ROOT / "ref/recompcore/GXRuntime")
    parser.add_argument("--patch", type=Path,
                        default=ROOT / "patches/recompcore/drafts/quantized-psq-scaling.patch")
    parser.add_argument("--environment-json", type=Path, help="Private saved environment or {'environment': ...}")
    parser.add_argument("--gather-dir", type=Path,
                        help="Also qualify actual gather_pipe.h macros, batch0/writerNULL; explicit source directory")
    parser.add_argument("--skip-asan", action="store_true", help="Explicitly report limited O3-only qualification")
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.sdk = args.sdk.resolve()
    args.patch = args.patch.resolve()
    if args.environment_json:
        args.environment_json = args.environment_json.resolve()
    if args.gather_dir:
        args.gather_dir = args.gather_dir.resolve()
    args.output = args.output.resolve()
    require(not args.output.exists(), "Output already exists; choose a fresh directory")
    require(args.output != ROOT and ROOT not in args.output.parents or
            (ROOT / "build") in args.output.parents, "Repository output must be under build/")
    args.output.mkdir(parents=True)
    logs = args.output / "logs"
    logs.mkdir()
    rows, pins, profiles = [], {}, []
    failure = None

    def pin(path):
        value = record(path)
        key = os.path.normcase(value["path"])
        require(key not in pins or pins[key] == value, "Input changed: " + value["path"])
        pins[key] = value
        return value

    def check_all():
        for value in pins.values():
            require(record(value["path"]) == value, "Input preservation: " + value["path"])

    def run(role, command):
        command = list(map(str, command))
        log = logs / (str(len(rows)) + ".log")
        row = {"role": role, "argv": command, "log": str(log)}
        rows.append(row)
        with log.open("xb") as stream:
            value = subprocess.run(command, cwd=args.output, env=environment,
                                   stdin=subprocess.DEVNULL, stdout=stream, stderr=subprocess.STDOUT,
                                   timeout=120, creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        row["exit_code"] = value.returncode
        row["log"] = record(log)
        require(value.returncode == 0, "Failed " + role + "; preserved log " + str(log))
        return log.read_text(errors="replace")

    environment = dict(os.environ)
    if args.environment_json:
        loaded = json.loads(args.environment_json.read_text())
        environment = loaded.get("environment", loaded)
        require(isinstance(environment, dict) and all(isinstance(k, str) and isinstance(v, str)
                for k, v in environment.items()), "Compiler environment must contain string keys/values")
    environment.update(TEMP=str(args.output), TMP=str(args.output), PYTHONDONTWRITEBYTECODE="1")

    try:
        pin(__file__)
        compiler = pin(args.compiler)
        if args.environment_json:
            pin(args.environment_json)
        canonical = args.sdk / "src/core/cpu.c"
        require(pin(canonical)["sha256"] == CANONICAL_SHA256, "Canonical cpu.c changed; refresh/review oracle deliberately")
        require(pin(args.patch)["sha256"] == PATCH_SHA256, "Scaling patch changed; review before qualification")
        fixture = ROOT / "tests/quantized_psq_fixture.c"
        oracle = ROOT / "tests/quantized_psq_oracle.c"
        pin(fixture)
        pin(oracle)
        source_text = canonical.read_text()
        oracle_body = oracle.read_text().split("static s32 gqr_scale(", 1)[1]
        canonical_body = source_text.split("static s32 gqr_scale(", 1)[1].split("void ppc_rfi(", 1)[0]
        require(oracle_body == canonical_body, "Tracked oracle differs from actual canonical PSQ region")
        copied_sdk = args.output / "sdk"
        for source in sorted((args.sdk / "include/core").glob("*.h")) + sorted((args.sdk / "src/core").glob("cpu*.c")) + [args.sdk / "src/core/cpu_interpreter_private.h"]:
            pin(source)
            target = copied_sdk / source.relative_to(args.sdk)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            pin(target)
        candidate = args.output / "candidate-cpu.c"
        newline = b"\r\n" if b"\r\n" in canonical.read_bytes() else b"\n"
        candidate.write_bytes(apply_guarded_patch(source_text, args.patch.read_text(), newline))
        pin(candidate)
        generator = ROOT / "scripts/generate_composite.py"
        pin(generator)
        spec = importlib.util.spec_from_file_location("quantized_psq_generator", generator)
        gc = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(gc)
        helper = args.output / "quantized_psq_helpers.h"
        helper.write_text("#include <string.h>\n#include <math.h>\n" + "\n".join(gc.quantized_psq_helpers()) + "\n")
        pin(helper)
        for source in (fixture, oracle):
            target = args.output / source.name
            shutil.copyfile(source, target)
            pin(target)
        if args.gather_dir:
            gather = args.output / "gather"
            gather.mkdir()
            for name in ("gather_pipe.h", "gather_pipe_batch.h"):
                source = args.gather_dir / name
                pin(source)
                shutil.copyfile(source, gather / name)
                pin(gather / name)

        flags = ["-std=c11", "-march=x86-64-v3", "-O3", "-fno-fast-math", "-ffp-contract=off",
                 "-DBW_GUEST_MEM1=bw_guest_mem1", "-DBW_GUEST_MEM1_SIZE=0x02000000u",
                 "-DBW_F32_LOAD_HW_WIDEN=1", "-DBLUEWAKE_FIXED_CPU=1",
                 "-I", copied_sdk / "include", "-I", copied_sdk / "src/core", "-I", args.output]
        if os.name == "nt":
            flags += ["-fms-runtime-lib=dll", "-D_DLL", "-D_MT", "-Xclang", "--dependent-lib=msvcrt",
                      "-D_CRT_SECURE_NO_WARNINGS", "-D_CRT_NONSTDC_NO_DEPRECATE"]
        variants = ["reference-o3", "candidate-o3"] + ([] if args.skip_asan else ["candidate-asan"])
        if args.gather_dir:
            variants += ["candidate-gather-o3"] + ([] if args.skip_asan else ["candidate-gather-asan"])
        for profile in variants:
            directory = args.output / profile
            directory.mkdir()
            asan = profile.endswith("asan")
            gather_profile = "-gather-" in profile
            options = flags + (["-fsanitize=address", "-fno-omit-frame-pointer"] if asan else [])
            main_cpu = copied_sdk / "src/core/cpu.c" if profile.startswith("reference") else candidate
            sources = [main_cpu, args.output / fixture.name, args.output / oracle.name] + [p for p in sorted((copied_sdk / "src/core").glob("*.c")) if p.name != "cpu.c"]
            objects, closure = [], []
            for number, source in enumerate(sources):
                if gather_profile and number != 1:
                    base_name = "candidate-asan" if asan else "candidate-o3"
                    base = next(item for item in profiles if item["profile"] == base_name)
                    prior = base["closure"][number]
                    require(record(source) == prior["source"] and record(prior["object"]["path"]) == prior["object"],
                            "Gather profile reuse must retain exact same CPU/oracle objects")
                    for dependency in prior["dependencies"]:
                        require(record(dependency["path"]) == dependency, "Gather reused dependency drift")
                    closure.append(dict(prior, reused_from=base_name))
                    objects.append(Path(prior["object"]["path"]))
                    continue
                obj = directory / (str(number) + ".obj")
                pre, dep = obj.with_suffix(".pre.d"), obj.with_suffix(".d")
                compile_options = options + (["-DBW_PSQ_GATHER_PROFILE=1", "-I", args.output / "gather"]
                                             if gather_profile else [])
                run(profile + "-M-" + source.name, [args.compiler, *compile_options, "-M", "-MF", pre, "-MT", obj, source])
                expected = {os.path.normcase(str(p)): pin(p) for p in dependencies(pre.read_text())}
                check_all()
                run(profile + "-compile-" + source.name, [args.compiler, *compile_options, "-MD", "-MF", dep, "-MT", obj, "-c", source, "-o", obj])
                actual = {os.path.normcase(str(p)) for p in dependencies(dep.read_text())}
                require(actual == set(expected), "Actual -M/-MD dependency mismatch")
                closure.append({"source": pin(source), "object": record(obj), "dependencies": list(expected.values()),
                                "M": record(pre), "MD": record(dep)})
                objects.append(obj)
            exe = directory / ("psq-runtime.exe" if os.name == "nt" else "psq-runtime")
            link = [args.compiler, *options, *objects, "-o", exe]
            if os.name == "nt":
                link += ["-fuse-ld=lld"]
                if asan:
                    link += ["-shared-libasan"]
                    resource = Path(run(profile + "-resource-dir", [args.compiler, "-print-resource-dir"]).strip())
                    runtime = resource / "lib/windows/clang_rt.asan_dynamic-x86_64.dll"
                    pin(runtime)
                    shutil.copyfile(runtime, directory / runtime.name)
            else:
                link += ["-lm"]
            run(profile + "-link", link)
            output = run(profile + "-execute", [exe])
            summary = json.loads(next(line for line in output.splitlines() if line.startswith("{")))
            require(summary["status"] == "PASS" and summary["cases"] == 165056 and
                    summary["exact_result_comparisons"] == 330112 and summary["negative_mutants_rejected"] == 2 and
                    summary["gqr_index_mask"] == 255 and summary["explicit_gqr_gate_cases"] == 65536,
                    "Full fixture/negative controls failed")
            require(not profiles or profiles[0]["summary"] == summary, "Profile outputs differ")
            profiles.append({"profile": profile, "summary": summary, "executable": record(exe), "closure": closure,
                             "gather_macros": gather_profile, "gather_guard": "batch length0, writerNULL; any flush is fatal" if gather_profile else None})
            print(profile + ": " + json.dumps(summary), flush=True)
        check_all()
    except BaseException as error:
        failure = repr(error)
    status = "FAIL" if failure else "PASS_O3_ONLY" if args.skip_asan else "PASS"
    receipt = {"status": status, "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
               "failure": failure, "profiles": profiles, "commands": rows, "inputs": list(pins.values()),
               "original_inputs_preserved": failure is None,
               "scope": "ROM-free synthetic actual CPU/helper proof; no game, pixel, module-build or speed claim"}
    with (args.output / "result.json").open("x") as stream:
        json.dump(receipt, stream, indent=2)
    print(json.dumps({"status": status, "failure": failure, "receipt": str(args.output / "result.json")}))
    return 1 if failure else 0


if __name__ == "__main__":
    raise SystemExit(main())
