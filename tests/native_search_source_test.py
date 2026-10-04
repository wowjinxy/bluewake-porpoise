"""Narrow watch-list exceptions for certified search bodies and resume hooks."""
import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('search_native_entries', ROOT / 'scripts/windows/native_entries.py')
prepare = importlib.util.module_from_spec(spec); spec.loader.exec_module(prepare)


class SearchCertificateTest(unittest.TestCase):
    def certify_body(self, name, entry, chunk, calls, watched):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / f'chunk_fixture_{chunk:08X}.c'
            body = f'\nlabel_{entry:08X}:\n    ctx->pc = 0x{entry:08X}u;\n' + ''.join(
                f'    ctx->pc = 0x{pc:08X}u;\n' for pc in calls)
            path.write_text(prepare.INCLUDE + body + f'\nlabel_{entry+4:08X}:\n    return;\n')
            fragment = hashlib.sha256(' '.join(body.split()).encode()).hexdigest()
            with patch.object(prepare, 'FRAGMENTS', {name: (chunk, entry, entry+4, fragment)}), \
                    patch.object(prepare, 'ENTRIES', {entry: (name,)}):
                return prepare.certify([path], watched)

    def test_watched_judge_entry_requires_runtime_gate_but_interior_stays_observed(self):
        entry, chunk = 0x80245640, 0x802416E0
        self.assertEqual(self.certify_body('judge_filter', entry, chunk, [], {entry}), {entry})
        self.assertEqual(self.certify_body('judge_filter', entry, chunk, [entry+8], {entry+8}), set())

    def test_only_probed_stage_helpers_are_exempt(self):
        entry, chunk = 0x80041544, 0x8003D6E0
        self.assertEqual(self.certify_body('stage_name', entry, chunk, [0x80328F40, 0x80328F8C],
                                         {0x80328F40, 0x80328F8C}), {entry})
        self.assertEqual(self.certify_body('stage_name', entry, chunk, [0x80328F44], {0x80328F44}), set())

    def test_resume_hooks_are_in_the_full_original_body_certificate(self):
        dependencies = prepare.ENTRIES[0x80041544]
        for entry in (0x8004156C, 0x80041578, 0x80041588):
            self.assertEqual(prepare.ENTRIES[entry], dependencies)
            self.assertEqual(prepare.FRAGMENTS[dependencies[0]][0], 0x8003D6E0)


if __name__ == '__main__':
    unittest.main()
