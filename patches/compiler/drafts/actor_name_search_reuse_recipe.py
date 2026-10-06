"""Prepare a fresh, portable CPU-only CMake fixture; never edits the checkout.

Adaptation of Elliott Tate's actor-name search (app 944a1f3c).
This recipe performs file operations only. Run the emitted CMake/CTest commands
separately with a Clang toolchain. It does not load a translated game module.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil


def digest(data):
    return hashlib.sha256(data).hexdigest()


def apply_exact(raw, patch):
    lines = raw.decode("utf-8").splitlines(keepends=True)
    changes = patch.decode("utf-8").splitlines(keepends=True)
    result, cursor, i = [], 0, 2
    while i < len(changes):
        match = re.fullmatch(r"@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@.*\n", changes[i])
        if not match:
            raise ValueError("Unexpected patch hunk")
        start = int(match[1]) - 1
        if start < cursor:
            raise ValueError("Overlapping patch hunk")
        result.extend(lines[cursor:start])
        cursor, i = start, i + 1
        while i < len(changes) and not changes[i].startswith("@@ "):
            row = changes[i]
            if row[0] in " -":
                if cursor >= len(lines) or lines[cursor] != row[1:]:
                    raise ValueError("Patch preimage mismatch")
                cursor += 1
            if row[0] in " +":
                result.append(row[1:])
            if row[0] not in " +-":
                raise ValueError("Unexpected patch row")
            i += 1
    return "".join(result + lines[cursor:]).encode("utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--runtime", required=True, type=Path,
                        help="Recompcore checkout containing GXRuntime")
    parser.add_argument("--out", required=True, type=Path,
                        help="Fresh source directory; must not exist")
    args = parser.parse_args()
    repo, runtime, out = args.repo.resolve(), args.runtime.resolve(), args.out.resolve()
    here = Path(__file__).resolve().parent
    base = repo / "cmake/composite/native_search.c"
    raw = base.read_bytes()
    if digest(raw) != "13511d1c38aafffa36fa131312c5fdfc33f8b26c77a328992269fe80ed9d5138":
        raise ValueError("Unqualified uncached native_search.c preimage")
    patch = (here / "actor-name-search-reuse.patch").read_bytes()
    if digest(patch) != "ee403bcd3e2458bae1419fec4896e3443df501e76f9916db76bfe333a0a84a79":
        raise ValueError("Candidate patch mismatch")
    candidate = apply_exact(raw, patch)
    if digest(candidate) != "f19a5e24a6e15ea52a2301bf135d229436a63a64b0da083eeb297a45fc8cfa4c":
        raise ValueError("Candidate source mismatch")
    fixture = (here / "actor_name_search_reuse_test.c").read_bytes()
    if digest(fixture) != "c70c62144d9b064b6ad4d4b2bb2cba5e85df78656cbd3c2659da73a7ee3a610c":
        raise ValueError("Authored fixture mismatch")
    native = (repo / "cmake/composite/native_entries.c").read_text(encoding="utf-8")
    begin = native.index("static BluewakeNativeEntriesReady s_ready;")
    end = native.index("int bluewake_native_entries_try(")
    gate = ('#include "native_entries.h"\n#define BW_ENTRIES_EXPORT\n' + native[begin:end]).encode()
    if digest(gate) != "76856614bd2999f3e0478ab3c423ad0a372feb3a808ea99013bb2bc9c1520bb0":
        # Compare the exact configure/ready functions, rather than substitute a
        # fake readiness implementation when the production interface changes.
        raise ValueError("Versioned configure/ready source changed")
    core = sorted((runtime / "GXRuntime/src/core").glob("cpu*.c"))
    if len(core) != 6:
        raise ValueError("Expected six real CPU translation units")
    out.mkdir(parents=True, exist_ok=False)
    local = out / "cmake/composite"
    local.mkdir(parents=True)
    pins = []
    for relative in ("cmake/composite/native_search.h", "cmake/composite/direct_calls.c",
                     "cmake/composite/direct_calls.h", "cmake/composite/native_entries.h",
                     "runtime/host/src/health_return_observer.h"):
        source, target = repo / relative, out / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        pins.append({"source": str(source), "sha256": digest(source.read_bytes())})
    (local / "native_search.c").write_bytes(candidate)
    (local / "uncached.c").write_bytes(raw)
    (local / "versioned_gate.c").write_bytes(gate)
    (local / "reuse_test.c").write_bytes(fixture)
    # All implementation TUs, including the uncached reference and real CPU,
    # receive the selected sanitizer flags. No fixture grants native admission.
    cmake = '''cmake_minimum_required(VERSION 3.20)
project(actor_name_search_reuse_fixture C)
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "This fixture requires Clang")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
option(REUSE_ASAN "Instrument the CPU-only fixture with AddressSanitizer" OFF)
add_compile_options(-O3 -UNDEBUG -ffp-contract=off)
if(WIN32)
  add_compile_options(-fms-compatibility-version=19.44 -fms-runtime-lib=dll)
  add_link_options(-fms-runtime-lib=dll)
endif()
if(REUSE_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
  if(WIN32)
    add_compile_options(-shared-libasan)
    add_link_options(-shared-libasan)
  endif()
endif()
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/cmake/composite"
  "@RUNTIME@/GXRuntime/include" "@RUNTIME@/Source/Core/Core/PowerPC/StaticRecomp")
add_library(uncached OBJECT cmake/composite/uncached.c)
target_compile_definitions(uncached PRIVATE
  bluewake_native_search=reference_native_search
  bluewake_native_search_report=reference_native_search_report)
add_executable(reuse_test cmake/composite/reuse_test.c
  cmake/composite/direct_calls.c cmake/composite/versioned_gate.c
  $<TARGET_OBJECTS:uncached>
@CORE@)
if(NOT WIN32)
  target_link_libraries(reuse_test PRIVATE m)
endif()
enable_testing()
add_test(NAME actor_name_search_reuse COMMAND reuse_test)
set_tests_properties(actor_name_search_reuse PROPERTIES TIMEOUT 300)
'''
    cmake = cmake.replace("@RUNTIME@", runtime.as_posix()).replace(
        "@CORE@", "\n".join('  "' + p.as_posix() + '"' for p in core))
    (out / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
    pins += [{"source": str(p), "sha256": digest(p.read_bytes())} for p in core]
    (out / "sources.json").write_text(json.dumps(pins, indent=2) + "\n", encoding="utf-8")
    print("Prepared CPU-only sources:", out)
    print("Configure separate O3 and ASan build directories with Clang/Ninja; see the draft memo.")


if __name__ == "__main__":
    main()
