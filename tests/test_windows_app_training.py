#!/usr/bin/env python3
"""App PGO training matches the module and preserves private prior attempts."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("windows_app_training", REPO / "scripts/windows/train_app_profile.py")
trainer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(trainer)
bw = trainer.build


class AppTrainingTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="windows app training ")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.out = self.root / "existing-build"
        (self.out / "composite").mkdir(parents=True)
        (self.out / "composite" / bw.MODULE).write_bytes(b"synthetic module")
        (self.out / "game").mkdir()
        (self.out / "game/main.dol").write_bytes(b"synthetic input")
        self.selections = dict.fromkeys(bw.WINDOWS_DEFAULT_OPTIMIZATIONS, False)
        self.selections.update(native_entries=True, lean_memory=True, direct_calls=True,
                               prepared_blocks=True, gather_pipe=True)
        self.receipt = self.out / "prepared-blocks.json"
        self.write_receipt()
        self.target = self.root / "windows/pgo/app.profdata"
        self.target.parent.mkdir(parents=True)
        self.target.write_bytes(b"previous valid profile")
        self.addCleanup(patch.stopall)
        patch.object(bw, "ROOT", self.root).start()
        patch.object(bw.sys, "argv", ["train_app_profile.py", "fixture.iso", "--out", str(self.out)]).start()
        self.environments = []
        def tools(builder):
            builder.env = dict(os.environ, PATH="C:/fixture-tools;C:/fixture-windows",
                               BLUEWAKE_CARD_PATH="player.card", BLUEWAKE_MODS="player-mod")
        patch.object(bw.Builder, "check_tools", tools).start()
        def configure(builder, path, instrument):
            self.assertTrue(instrument)
            self.assertTrue(builder.args.no_app_pgo)
            builder.app_build = path
        patch.object(bw.Builder, "configure_app", configure).start()
        patch.object(bw.Builder, "build_app", lambda builder: builder.app_build / "BlueWake.exe").start()

    def write_receipt(self):
        prepared = dict(self.selections)
        prepared["enabled"] = prepared.pop("prepared_blocks")
        self.receipt.write_text(json.dumps(prepared))

    def playback(self, builder, name, argv, env):
        for key, value in self.selections.items():
            self.assertEqual(getattr(builder.args, key), value)
        self.assertEqual(env["BLUEWAKE_NATIVE_ENTRIES"], "1")
        self.assertEqual(env["BLUEWAKE_DIRECT_CALLS"], "1")
        self.assertEqual(env["BLUEWAKE_GATHER_PIPE"], "1")
        self.assertNotIn("BLUEWAKE_RENDERER", env)
        self.assertNotIn("BLUEWAKE_CARD_PATH", env)
        self.assertNotIn("BLUEWAKE_MODS", env)
        self.assertIn("BLUEWAKE_TEST_WARP", env)
        run = Path(env["BLUEWAKE_DATA_DIR"])
        self.assertTrue(run.is_dir())
        self.assertFalse((run / "private-card.raw").exists())
        (run / "private-card.raw").write_bytes(b"training card")
        (run / "fixture.profraw").write_bytes(b"synthetic counts")
        self.environments.append(dict(env))
        log = builder.logs / f"{name}.log"
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text("[player-milestone] control-admitted")
        return log

    def merge(self, command, check):
        self.assertTrue(check)
        self.assertEqual(command[1:3], ["merge", "--sparse"])
        self.assertNotEqual(Path(command[4]), self.target)
        for raw in command[5:]:
            self.assertEqual(Path(raw).read_bytes(), b"synthetic counts")
        Path(command[4]).write_bytes(b"new validated profile")

    def test_repeated_training_matches_module_flags_and_retains_previous_cards(self):
        old = self.out / "app-pgo-train/old-card.raw"
        old.parent.mkdir(); old.write_bytes(b"prior private attempt")
        with patch.object(bw.Builder, "run", lambda builder, *args, **kw: self.playback(builder, *args, **kw)), \
                patch.object(trainer.subprocess, "run", self.merge):
            trainer.main()
            trainer.main()
        self.assertEqual(old.read_bytes(), b"prior private attempt")
        paths = [Path(env["BLUEWAKE_DATA_DIR"]) for env in self.environments]
        self.assertEqual(len(paths), 2)
        self.assertNotEqual(paths[0], paths[1])
        self.assertTrue(all((run / "private-card.raw").is_file() for run in paths))
        self.assertEqual(self.target.read_bytes(), b"new validated profile")
        self.assertFalse(list(self.target.parent.glob("app-profile-*.tmp")))

    def test_failed_merge_preserves_previous_app_profile(self):
        def fail(command, check):
            Path(command[4]).write_bytes(b"incomplete candidate")
            raise subprocess.CalledProcessError(1, command)
        with patch.object(bw.Builder, "run", lambda builder, *args, **kw: self.playback(builder, *args, **kw)), \
                patch.object(trainer.subprocess, "run", fail):
            with self.assertRaises(subprocess.CalledProcessError):
                trainer.main()
        self.assertEqual(self.target.read_bytes(), b"previous valid profile")
        self.assertEqual(len(list((self.out / "app-pgo-train").glob("attempt-*/app.profdata"))), 1)

    def test_module_receipt_missing_invalid_or_non_boolean_is_rejected(self):
        for text in (None, '{broken', '[]', '{"enabled":true}', '{"enabled":"true"}'):
            if text is None:
                self.receipt.unlink()
            else:
                self.receipt.write_text(text)
            with self.assertRaises(bw.BuildError):
                trainer.main()
        self.assertEqual(self.target.read_bytes(), b"previous valid profile")

    def test_old_receipts_default_only_new_options_off(self):
        prepared = dict(self.selections)
        prepared.pop("native_entries"); prepared.pop("lean_memory")
        prepared["enabled"] = prepared.pop("prepared_blocks")
        self.receipt.write_text(json.dumps(prepared))
        options = trainer.module_options(self.out)
        self.assertFalse(options["native_entries"])
        self.assertFalse(options["lean_memory"])
        self.assertTrue(options["direct_calls"])

    def test_failed_atomic_publish_preserves_previous_profile_and_cleans_stage(self):
        candidate = self.root / "candidate.profdata"; candidate.write_bytes(b"candidate counts")
        with patch.object(trainer.os, "replace", side_effect=OSError("replacement failed")):
            with self.assertRaises(OSError):
                trainer.publish_profile(candidate, self.target)
        self.assertEqual(self.target.read_bytes(), b"previous valid profile")
        self.assertEqual(candidate.read_bytes(), b"candidate counts")
        self.assertFalse(list(self.target.parent.glob("app-profile-*.tmp")))


if __name__ == "__main__":
    unittest.main()
