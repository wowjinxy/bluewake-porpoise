#!/usr/bin/env python3
"""Bounded Windows build recovery with synthetic subprocesses and inputs."""
import ctypes
import importlib.util
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("windows_builder", REPO / "scripts/windows/build.py")
bw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bw)
OPTIONS = ("prepared_blocks", "fixed_cpu", "fixed_mem1", "inline_fp", "gather_pipe", "direct_calls",
           "inline_gpr", "native_j3d", "native_vec", "native_math", "native_skin", "native_game_math",
           "native_entries", "lean_memory")


class RecoveryTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="windows recovery ")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.b = bw.Builder(SimpleNamespace(out=self.root, march="x86-64-v3", jobs=8, jobs_auto=False,
                                            **dict.fromkeys(OPTIONS, False)))
        self.b.logs.mkdir()

    def compiler(self, failures):
        jobs = []
        def execute(name, argv, **kwargs):
            if not name.endswith("-build"):
                return
            jobs.append(argv[argv.index("-j") + 1])
            log = self.b.logs / f"{name}.log"
            if len(jobs) <= len(failures):
                log.write_text(failures[len(jobs) - 1])
                raise bw.CommandError(name, 1, log)
            folder = Path(argv[2]); folder.mkdir(parents=True, exist_ok=True)
            (folder / bw.MODULE).write_bytes(b"synthetic module")
        self.b.run = execute
        return jobs

    def compile(self, name="module", level="2"):
        return self.b.compile_composite(self.root / name, level, [], [], name)

    def test_memory_cap_uses_available_physical_and_commit_memory(self):
        def status(pointer):
            memory = pointer._obj
            memory.ullAvailPhys = 20 * 2**30
            memory.ullAvailPageFile = 5 * 2**30
            return 1
        fake = SimpleNamespace(kernel32=SimpleNamespace(GlobalMemoryStatusEx=status))
        with patch.object(ctypes, "windll", fake, create=True), patch.object(os, "cpu_count", return_value=16):
            self.assertEqual(bw.default_jobs(), 2)

    def test_memory_cap_never_requests_zero_jobs_and_falls_back_to_cores(self):
        def status(pointer):
            pointer._obj.ullAvailPhys = pointer._obj.ullAvailPageFile = 1
            return 1
        fake = SimpleNamespace(kernel32=SimpleNamespace(GlobalMemoryStatusEx=status))
        with patch.object(ctypes, "windll", fake, create=True), patch.object(os, "cpu_count", return_value=4):
            self.assertEqual(bw.default_jobs(), 1)
            fake.kernel32.GlobalMemoryStatusEx = lambda pointer: 0
            self.assertEqual(bw.default_jobs(), 4)

    def test_automatic_jobs_refresh_for_training_and_optimized_compile(self):
        jobs = self.compiler([])
        self.b.args.jobs_auto = True
        with patch.object(bw, "default_jobs", side_effect=(4, 1)) as cap:
            self.compile("training", "0")
            self.compile("optimized")
        self.assertEqual(jobs, [4, 1])
        self.assertEqual(cap.call_count, 2)

    def test_explicit_jobs_are_respected(self):
        jobs = self.compiler([])
        with patch.object(bw, "default_jobs") as cap:
            self.compile()
        self.assertEqual(jobs, [8])
        cap.assert_not_called()

    def test_memory_failure_reduces_jobs_and_preserves_failed_log(self):
        jobs = self.compiler(["LLVM ERROR: out of memory", "LLVM ERROR: out of memory"])
        self.compile()
        self.assertEqual(jobs, [8, 4, 2])
        self.assertEqual((self.b.logs / "module-build-failed-oom-8.log").read_text(), "LLVM ERROR: out of memory")

    def test_memory_failure_at_one_job_stops(self):
        jobs = self.compiler(["LLVM ERROR: out of memory"] * 8)
        with self.assertRaises(bw.CommandError):
            self.compile()
        self.assertEqual(jobs, [8, 4, 2, 1])

    def test_compiler_crash_retries_are_bounded_even_at_one_job(self):
        self.b.args.jobs = 1
        jobs = self.compiler(["clang: error: clang frontend command failed due to signal"] * 8)
        with self.assertRaises(bw.CommandError):
            self.compile()
        self.assertEqual(jobs, [1, 1, 1])

    def test_compiler_crash_reduces_jobs_and_success_is_used(self):
        jobs = self.compiler(["clang: error: unable to execute command: exception 0xC0000005"])
        self.assertTrue(self.compile().exists())
        self.assertEqual(jobs, [8, 4])

    def test_compiler_diagnostics_and_link_failures_are_not_retried(self):
        for message in ("clang: error: unknown argument", "undefined symbol", "clang frontend command failed with exit code 1"):
            with self.subTest(message=message):
                jobs = self.compiler([message])
                with self.assertRaises(bw.CommandError):
                    self.compile()
                self.assertEqual(jobs, [8])

    def test_source_crash_retries_once_including_signed_windows_status(self):
        for status in (0xC0000005, -1073741819):
            calls = []
            def execute(name, argv):
                calls.append(list(argv))
                if len(calls) == 1:
                    log = self.b.logs / f"{name}.log"; log.write_text("crashed source step")
                    raise bw.CommandError(name, status, log)
                return "finished"
            self.b.run = execute
            self.assertEqual(self.b.source_step("fixture", "synthetic.py", self.root, "--selected"), "finished")
            self.assertEqual(calls[0], calls[1])
            self.assertEqual((self.b.logs / "fixture-failed-1.log").read_text(), "crashed source step")

    def test_source_validation_error_or_repeated_crash_stops(self):
        for status, expected_calls in ((1, 1), (0xC0000005, 2), (0xC0000135, 1)):
            calls = []
            def execute(name, argv):
                calls.append(argv)
                raise bw.CommandError(name, status, self.b.logs / f"{name}.log")
            self.b.run = execute
            with self.assertRaises(bw.CommandError):
                self.b.source_step("fixture", "synthetic.py", self.root)
            self.assertEqual(len(calls), expected_calls)

    def test_new_optimizations_are_opt_in_and_lean_requires_both_helpers(self):
        captured = []
        def build(builder):
            captured.append(builder.args)
        argv = ["build.py", "fixture.iso", "--out", str(self.root)]
        with patch.object(bw.Builder, "build", build), patch.object(bw, "default_jobs", return_value=3):
            with patch.object(bw.sys, "argv", argv):
                bw.main()
            self.assertFalse(captured[-1].native_entries)
            self.assertFalse(captured[-1].lean_memory)
            self.assertTrue(captured[-1].jobs_auto)
            with patch.object(bw.sys, "argv", argv + ["--native-entries", "--lean-memory", "--jobs", "2"]):
                bw.main()
            self.assertTrue(captured[-1].native_entries)
            self.assertTrue(captured[-1].lean_memory)
            self.assertFalse(captured[-1].jobs_auto)
            with patch.object(bw.sys, "argv", argv + ["--conservative", "--lean-memory", "--prepared-blocks", "--gather-pipe"]):
                bw.main()
            self.assertTrue(captured[-1].lean_memory)
            for missing in ([], ["--prepared-blocks"], ["--gather-pipe"]):
                with patch.object(bw.sys, "argv", argv + ["--conservative", "--lean-memory", *missing]):
                    with self.assertRaises(SystemExit) as error:
                        bw.main()
                    self.assertEqual(error.exception.code, 2)
            with patch.object(bw.sys, "argv", argv + ["--conservative", "--native-entries", "--gather-pipe", "--direct-calls"]):
                bw.main()
            self.assertTrue(captured[-1].native_entries)
            for missing in ([], ["--gather-pipe"], ["--direct-calls"]):
                with patch.object(bw.sys, "argv", argv + ["--conservative", "--native-entries", *missing]):
                    with self.assertRaises(SystemExit) as error:
                        bw.main()
                    self.assertEqual(error.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
