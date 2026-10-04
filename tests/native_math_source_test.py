#!/usr/bin/env python3
"""Certification must reject modified SDK bodies, including mod variants."""
import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/mods/prepare_native_math.py'
spec = importlib.util.spec_from_file_location('native_math_prepare', SCRIPT)
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


class CertificationTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.body = '\nlabel_00000100:\n    ctx->gpr[3] = 1;\n'
        self.source = self.body + '\nlabel_00000104:\n'
        self.expected = hashlib.sha256(' '.join(self.body.split()).encode()).hexdigest()
        self.old_leaves = prepare.LEAVES
        self.old_porpoise_leaves = prepare.PORPOISE_LEAVES
        prepare.LEAVES = ((0x100, 0x104, '00000100', self.expected),)
        self.porpoise_body = '\nlabel_00000200:\n    ctx->gpr[3] = 7;\n'
        self.porpoise_hash = hashlib.sha256(' '.join(self.porpoise_body.split()).encode()).hexdigest()
        prepare.PORPOISE_LEAVES = ((0x200, 0x204, '00000200', self.porpoise_hash),)
        self.file = self.root / 'chunk_00000100.c'
        self.file.write_text(self.source)
        (self.root / "generated_composite.h").write_text(prepare.TYPE + prepare.CACHE)

    def tearDown(self):
        prepare.LEAVES = self.old_leaves
        prepare.PORPOISE_LEAVES = self.old_porpoise_leaves
        self.temp.cleanup()

    def test_repeat_and_variant(self):
        variant = self.root / 'chunk_00000100_mod.c'
        variant.write_text(self.source)
        prepare.prepare(self.root)
        manifest = (self.root / 'native_math.json').read_bytes()
        stamps = {p.name: p.stat().st_mtime_ns for p in self.root.iterdir()}
        prepare.prepare(self.root)
        self.assertEqual(stamps, {p.name: p.stat().st_mtime_ns for p in self.root.iterdir()})
        self.assertEqual(manifest, (self.root / 'native_math.json').read_bytes())
        # An executable-body change without changing labels must be rejected.
        variant.write_text(self.source.replace('= 1;', '= 2;'))
        with self.assertRaises(ValueError):
            prepare.prepare(self.root)
        self.assertEqual(manifest, (self.root / 'native_math.json').read_bytes())

    def test_modified_dispatcher(self):
        prepare.prepare(self.root)
        header = self.root / "generated_composite.h"
        header.write_text(header.read_text().replace('return native;', 'return NULL;'))
        with self.assertRaises(ValueError):
            prepare.prepare(self.root)

    def test_missing_function(self):
        self.file.write_text('/* unavailable translation */\n')
        with self.assertRaises(ValueError):
            prepare.prepare(self.root)
        self.assertFalse((self.root / 'native_math.json').exists())

    def test_porpoise_requires_exact_body_and_all_variants(self):
        # An incomplete SDK selection cannot change the dispatch certificate.
        prepare.prepare(self.root)
        manifest = (self.root / 'native_math.json').read_bytes()
        header = (self.root / 'generated_composite.h').read_bytes()
        with self.assertRaises(ValueError):
            prepare.prepare(self.root, libporpoise=True)
        self.assertEqual(header, (self.root / 'generated_composite.h').read_bytes())
        self.assertEqual(manifest, (self.root / 'native_math.json').read_bytes())
        path = self.root / 'chunk_00000200.c'
        path.write_text(self.porpoise_body + '\nlabel_00000204:\n')
        variant = self.root / 'chunk_00000200_mod.c'
        variant.write_text(path.read_text().replace('= 7;', '= 9;'))
        with self.assertRaises(ValueError):
            prepare.prepare(self.root, libporpoise=True)
        variant.write_text(path.read_text())
        prepare.prepare(self.root, libporpoise=True)
        self.assertIn(prepare.PORPOISE_MARKER, (self.root / 'generated_composite.h').read_text())
        stamps = {p.name: p.stat().st_mtime_ns for p in self.root.iterdir()}
        prepare.prepare(self.root, libporpoise=True)
        self.assertEqual(stamps, {p.name: p.stat().st_mtime_ns for p in self.root.iterdir()})
        prepare.prepare(self.root, libporpoise=False)
        self.assertNotIn(prepare.PORPOISE_MARKER, (self.root / 'generated_composite.h').read_text())


if __name__ == '__main__':
    unittest.main()
