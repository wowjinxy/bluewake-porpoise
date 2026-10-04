"""Invented translated bodies: dependency, mod and observer certification."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/windows/native_entries.py'
spec = importlib.util.spec_from_file_location('native_entries_prepare', SCRIPT)
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


class CertificationTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / 'generated.h').write_text('/* synthetic */\n')
        self.folder = self.root / 'chunks_dol'; self.folder.mkdir()
        self.entry, self.chunk, self.dep_chunk = 0x80004000, 0x80004000, 0x80008000
        self.body = '\nlabel_80004000:\n    ctx->gpr[3] = 1;\n'
        self.dependency = '\nlabel_80008000:\n    ctx->gpr[4] = 2;\n'
        digest = lambda b: hashlib.sha256(' '.join(b.split()).encode()).hexdigest()
        self.fragments = {'entry': (self.chunk, self.entry, 0x80004004, digest(self.body)),
                          'dep': (self.dep_chunk, self.dep_chunk, None, digest(self.dependency))}
        self.source = (prepare.INCLUDE + self.body + '\nlabel_80004004:\n'
                       '    return;\n\nreturn_dispatch_80004000:\n    return;\n')
        self.dep_source = prepare.INCLUDE + self.dependency + '\nreturn_dispatch_80008000:\n    return;\n'
        self.file = self.folder / 'chunk_0001_80004000.c'; self.file.write_text(self.source)
        self.dep = self.folder / 'chunk_0002_80008000.c'; self.dep.write_text(self.dep_source)
        self.patches = [patch.object(prepare, 'FRAGMENTS', self.fragments),
                        patch.object(prepare, 'ENTRIES', {self.entry: ('entry', 'dep')}),
                        patch.object(prepare, 'watched_addresses', return_value=set())]
        for p in self.patches: p.start()

    def tearDown(self):
        for p in reversed(self.patches): p.stop()
        self.temp.cleanup()

    def test_repeat_and_every_mod_variant(self):
        variant = self.root / 'chunks_mod'; variant.mkdir()
        (variant / self.file.name).write_text(self.source)
        (variant / self.dep.name).write_text(self.dep_source)
        prepare.prepare(self.root)
        before = {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        prepare.prepare(self.root)
        self.assertEqual(before, {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob('*') if p.is_file()})
        manifest = json.loads((self.root / 'native_entries.json').read_text())
        self.assertEqual(manifest['entries'], [self.entry])
        self.assertEqual(len(manifest['files']), 2)
        for name, digest in manifest['files'].items():
            self.assertEqual(hashlib.sha256((self.root / name).read_bytes()).hexdigest(), digest)

    def test_changed_dependency_variant_never_partially_routes(self):
        variant = self.root / 'chunks_mod'; variant.mkdir()
        (variant / self.dep.name).write_text(self.dep_source.replace('= 2;', '= 3;'))
        with self.assertRaisesRegex(ValueError, 'uncertified'): prepare.prepare(self.root)
        self.assertEqual(self.file.read_text(), self.source)
        self.assertNotIn(prepare.MARKER, (self.root / 'generated.h').read_text())
        self.assertFalse((self.root / 'native_entries.json').exists())

    def test_observers_and_mirror_addresses_reject(self):
        for address in (self.entry, self.entry | 0x40000000, self.dep_chunk):
            with patch.object(prepare, 'watched_addresses', return_value={address}):
                with self.assertRaisesRegex(ValueError, 'uncertified'): prepare.prepare(self.root)
            self.assertEqual(self.file.read_text(), self.source)

    def test_missing_fragment_and_modified_existing_hook(self):
        prepare.prepare(self.root)
        before = (self.root / 'native_entries.json').read_bytes()
        hooked = self.file.read_text()
        for mutation in (hooked.replace('0x80004000u)', '0x80004004u)'),
                         hooked.replace('goto return_dispatch_80004000;', 'goto return_dispatch_80008000;'),
                         hooked.replace('bluewake_native_entries_try(ctx,', 'bluewake_native_entries_try(other,')):
            self.file.write_text(mutation)
            with self.assertRaises(ValueError): prepare.prepare(self.root)
            self.assertEqual((self.root / 'native_entries.json').read_bytes(), before)
        self.file.write_text(hooked)
        self.dep.write_text(self.dep_source.replace('label_80008000:', 'label_80008004:'))
        with self.assertRaisesRegex(ValueError, 'uncertified'): prepare.prepare(self.root)


if __name__ == '__main__': unittest.main()
