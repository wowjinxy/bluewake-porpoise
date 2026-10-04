#!/usr/bin/env python3
"""Build real interpolation sources and test default/on/off pacing without game assets.

Run the builder's dependencies stage first to obtain the pinned patched runtime.
CMake/Ninja and a C++20 compiler are required. Only real Abseil is fetched; an
optional --abseil-source uses an existing source checkout for offline testing.
The pinned original policy is also compiled as a negative control.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BASE = "e280c788dadabd18b085af0085f1558fc9ff5ecc"
ABSEIL_URL = "https://github.com/abseil/abseil-cpp/archive/refs/tags/20240722.0.tar.gz"
ABSEIL_SHA = "f50e5ac311a81382da7fa75b97310e4b9006474f9560ac46f54a9967f07d4ae3"
SOURCE = "GXRuntime/graphics/aurora/lib/gfx/frame_interp.cpp"


def execute(command, log, env):
    result = subprocess.run(list(map(str, command)), env=env, text=True,
                            encoding="utf-8", errors="replace", capture_output=True)
    log.write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        print(result.stdout + result.stderr, flush=True)
        result.check_returncode()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, default=ROOT / "ref/recompcore")
    parser.add_argument("--output", type=Path, default=ROOT / "build/frame-interp-pacing")
    parser.add_argument("--abseil-source", type=Path)
    parser.add_argument("--cxx")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--ninja")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    runtime, out = args.runtime.resolve(), args.output.resolve()
    sys.path.insert(0, str(ROOT / "scripts/builder"))
    from runtime_patches import PatchError, apply_patches
    try:
        runtime_receipt = apply_patches(runtime, root=ROOT,
                                       manifest=ROOT / "patches/recompcore/active.json", verify_only=True)
    except PatchError as error:
        parser.error(str(error))
    env = dict(os.environ)
    if os.name == "nt" and not env.get("INCLUDE"):
        spec = importlib.util.spec_from_file_location("windows_builder", ROOT / "scripts/windows/build.py")
        builder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(builder)
        env = builder.Builder.visual_studio_env(builder.Builder.__new__(builder.Builder))
    out.mkdir(parents=True, exist_ok=True)
    baseline = out / "frame_interp_baseline.cpp"
    baseline.write_bytes(subprocess.check_output(["git", "-C", str(runtime), "show", BASE + ":" + SOURCE]))
    aurora = runtime / "GXRuntime/graphics/aurora"
    fixture = ROOT / "tests/frame_interp_pacing_test.cpp"
    cmake = f'''cmake_minimum_required(VERSION 3.24)
project(frame_interp_pacing LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(abseil-cpp URL "{ABSEIL_URL}"
  URL_HASH SHA256={ABSEIL_SHA} DOWNLOAD_EXTRACT_TIMESTAMP FALSE EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(abseil-cpp)
foreach(policy current baseline)
  if(policy STREQUAL "current")
    set(source "{(runtime / SOURCE).as_posix()}")
  else()
    set(source "{baseline.as_posix()}")
  endif()
  add_library(frame_interp_${{policy}} STATIC "${{source}}")
  target_include_directories(frame_interp_${{policy}} PUBLIC
    "{aurora.as_posix()}/lib/gfx" "{aurora.as_posix()}/include"
    "{runtime.as_posix()}/GXRuntime/graphics/gxcore/include")
  target_compile_definitions(frame_interp_${{policy}} PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
  target_link_libraries(frame_interp_${{policy}} PUBLIC absl::flat_hash_map absl::flat_hash_set)
  add_executable(pacing_${{policy}} "{fixture.as_posix()}")
  target_link_libraries(pacing_${{policy}} PRIVATE frame_interp_${{policy}})
endforeach()
add_executable(frame_interp_full "{aurora.as_posix()}/tests/frame_interp_test.cpp")
target_link_libraries(frame_interp_full PRIVATE frame_interp_current)
if(WIN32 AND NOT MINGW)
  foreach(t pacing_current pacing_baseline frame_interp_full)
    target_link_libraries(${{t}} PRIVATE msvcrt msvcprt vcruntime ucrt)
    target_link_options(${{t}} PRIVATE LINKER:/NODEFAULTLIB:libcmt LINKER:/NODEFAULTLIB:libcpmt LINKER:/NODEFAULTLIB:libucrt)
  endforeach()
endif()
'''
    (out / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
    config = [args.cmake, "-S", out, "-B", out / "cmake", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"]
    for name, value in (("CMAKE_CXX_COMPILER", args.cxx or ("clang++" if os.name == "nt" else None)),
                        ("CMAKE_MAKE_PROGRAM", args.ninja)):
        if value:
            config.append(f"-D{name}={value}")
    if args.abseil_source:
        config.append(f"-DFETCHCONTENT_SOURCE_DIR_ABSEIL-CPP={args.abseil_source.resolve().as_posix()}")
    execute(config, out / "configure.log", env)
    print("Configured real runtime interpolation and pinned baseline", flush=True)
    execute([args.cmake, "--build", out / "cmake", "--parallel", args.jobs,
             "--target", "pacing_current", "pacing_baseline", "frame_interp_full"], out / "build.log", env)
    print("Built pacing fixtures and full interpolation checks", flush=True)
    suffix = ".exe" if os.name == "nt" else ""
    test_env = {key: value for key, value in env.items() if not key.startswith("DOL_AURORA_FRAME_INTERP")}
    results = {}
    for target in ("pacing_current", "frame_interp_full"):
        for mode, mode_args in (("default", []), ("enabled", ["1"]), ("disabled", ["0"])):
            name = f"{target}-{mode}"
            result = execute([out / "cmake" / (target + suffix), *mode_args], out / (name + ".log"), test_env)
            results[name] = {"returncode": result.returncode, "stdout": result.stdout.strip()}
            print(f"PASS {name}: {result.stdout.strip()}", flush=True)
    control = subprocess.run([str(out / "cmake" / ("pacing_baseline" + suffix))], env=test_env,
                             text=True, encoding="utf-8", errors="replace", capture_output=True)
    (out / "baseline-default.log").write_text(control.stdout + control.stderr, encoding="utf-8")
    failures = control.stderr.count("FAIL ")
    if control.returncode != 1 or failures == 0:
        raise RuntimeError("Pinned baseline did not fail the high-rate regression checks")
    sources = [fixture, Path(__file__).resolve(), runtime / SOURCE,
               aurora / "lib/gfx/frame_interp.hpp", aurora / "tests/frame_interp_test.cpp"]
    receipt = {"base_commit": BASE, "runtime_patches": runtime_receipt,
               "abseil": {"url": ABSEIL_URL, "archive_sha256": ABSEIL_SHA,
                           "source_override": str(args.abseil_source.resolve()) if args.abseil_source else None},
               "source_sha256": {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
               "results": results, "baseline_default": {"returncode": control.returncode, "failures": failures},
               "claim": "Functional pacing, mode override and interpolation regression checks; no FPS benchmark."}
    (out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(f"PASS baseline negative control: {failures} expected failures; receipt {out / 'receipt.json'}", flush=True)


if __name__ == "__main__":
    main()
