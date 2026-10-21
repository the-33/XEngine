"""Regression checks for English documentation filenames and diagnostics."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
DOCS = (
    (Path('docs/COMPILACION_CRUZADA.md'), Path('docs/CROSS_COMPILATION.md')),
    (Path("games/Blob's Quest/MIGRACION.md"), Path("games/Blob's Quest/MIGRATION.md")),
    (Path('games/My Princess Run/MIGRACION.md'), Path('games/My Princess Run/MIGRATION.md')),
)

spec = importlib.util.spec_from_file_location('migrate_documentation', ROOT/'tools/migrate_documentation.py')
migration = importlib.util.module_from_spec(spec)
spec.loader.exec_module(migration)


class EnglishDocumentationTests(unittest.TestCase):
    def test_english_docs_are_present_and_old_paths_are_gone(self):
        for old, new in DOCS:
            with self.subTest(path=new):
                self.assertTrue((ROOT/new).is_file())
                self.assertFalse((ROOT/old).exists())

    def test_markdown_and_source_references_use_new_names(self):
        # The README intentionally mentions old filenames in upgrade instructions.
        for file in [ROOT/'tools/dependency_guides.py', *ROOT.glob('games/**/InitCallback.hpp')]:
            source = file.read_text(encoding='utf-8')
            self.assertNotIn('COMPILACION_CRUZADA.md', source)
            self.assertNotIn('MIGRACION.md', source)
        readme = (ROOT/'README.md').read_text(encoding='utf-8')
        self.assertIn('docs/CROSS_COMPILATION.md', readme)
        self.assertIn('MIGRATION.md', readme)
        self.assertNotIn('](docs/COMPILACION_CRUZADA.md)', readme)

    def test_legacy_cleanup_preserves_older_docs(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for old, new in DOCS:
                (root / old).parent.mkdir(parents=True, exist_ok=True)
                (root / new).parent.mkdir(parents=True, exist_ok=True)
                (root / old).write_text('Previous user-edited document', encoding='utf-8')
                (root / new).write_text('Updated English documentation', encoding='utf-8')
            archived, missing = migration.migrate(root)
            self.assertEqual((archived, missing), (3, 0))
            self.assertEqual(migration.migrate(root), (0, 0))
            for old, new in DOCS:
                self.assertFalse((root / old).exists())
                self.assertEqual((root / new).read_text(encoding='utf-8'), 'Updated English documentation')
                backup = (root / migration.BACKUP_DIR / new).with_suffix('.previous.md')
                self.assertEqual(backup.read_text(encoding='utf-8'), 'Previous user-edited document')

    def test_cleanup_does_not_remove_doc_without_english_replacement(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            old, _ = DOCS[0]
            (root / old).parent.mkdir(parents=True, exist_ok=True)
            (root / old).write_text('Do not lose this file', encoding='utf-8')
            self.assertEqual(migration.migrate(root), (0, 1))
            self.assertTrue((root / old).is_file())


if __name__ == '__main__':
    unittest.main()
