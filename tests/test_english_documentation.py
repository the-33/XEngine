"""Regression checks for English documentation filenames and diagnostics."""
from __future__ import annotations

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
DOCS = (
    (Path('docs/COMPILACION_CRUZADA.md'), Path('docs/CROSS_COMPILATION.md')),
    (Path("games/Blob's Quest/MIGRACION.md"), Path("games/Blob's Quest/MIGRATION.md")),
    (Path('games/My Princess Run/MIGRACION.md'), Path('games/My Princess Run/MIGRATION.md')),
)

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

    def test_completed_migration_keeps_original_case_studies(self):
        # The one-shot migration script was intentionally retired. The durable
        # contract is that public English docs and original case studies stay.
        for game, slug in (("Blob's Quest", 'blobs-quest'),
                           ('My Princess Run', 'my-princess-run')):
            with self.subTest(game=game):
                original = ROOT / 'games' / game / 'MIGRATION.md'
                published = ROOT / 'docs' / 'migrations' / slug / 'MIGRATION.md'
                self.assertTrue(original.is_file())
                self.assertTrue(published.is_file())
                self.assertIn('# ' + game, original.read_text(encoding='utf-8'))
                self.assertIn('# ' + game, published.read_text(encoding='utf-8'))

    def test_retired_one_shot_document_migrator_is_not_distributed(self):
        self.assertFalse((ROOT / 'tools/migrate_documentation.py').exists())


if __name__ == '__main__':
    unittest.main()
