"""Developer documentation should remain navigable and task-oriented."""
from __future__ import annotations

from pathlib import Path
import re
import unittest
from urllib.parse import unquote


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / 'docs'


class DocumentationGuidesTests(unittest.TestCase):
    def test_main_readme_points_to_developer_entry_points(self):
        readme = (ROOT / 'README.md').read_text(encoding='utf-8')
        index = (DOCS / 'DOCUMENTATION_INDEX.md').read_text(encoding='utf-8')
        for name in ('GETTING_STARTED.md', 'INCLUDES_AND_DEPENDENCIES.md',
                     'WINDOW_AND_CAMERA.md', 'PERSISTENCE_AND_SERIALIZATION.md'):
            with self.subTest(guide=name):
                self.assertIn(name, readme)
                self.assertIn(name, index)

    def test_window_topics_are_not_part_of_persistence_guide(self):
        persistence = (DOCS / 'PERSISTENCE_AND_SERIALIZATION.md').read_text(encoding='utf-8')
        window = (DOCS / 'WINDOW_AND_CAMERA.md').read_text(encoding='utf-8')
        self.assertNotIn('## Window resolution', persistence)
        self.assertNotIn('Window->SetSize', persistence)
        self.assertIn('WINDOW_AND_CAMERA.md', persistence)
        self.assertIn('Window->SetSize', window)
        self.assertIn('CurrentCamera', window)

    def test_doxygen_output_paths_and_input_guides(self):
        conf = (ROOT / 'Doxyfile_Games').read_text(encoding='utf-8')
        build = (ROOT / 'tools/build.py').read_text(encoding='utf-8')
        self.assertIn('OUTPUT_DIRECTORY       = build/docs/game\n', conf)
        self.assertIn('OUTPUT_DIRECTORY        = build/docs/engine\n',
                      (ROOT / 'Doxyfile_Engine').read_text(encoding='utf-8'))
        self.assertIn('regenerate_engine_api_docs()', build)
        self.assertIn('build/docs/index.html', build)
        self.assertIn('INPUT                  = engine/include docs/reference/game \\', conf)
        self.assertIn('docs/DEVELOPER_GUIDE.md', conf)
        self.assertIn('RECURSIVE              = YES', conf)
        for name in ('DOCUMENTATION_INDEX.md', 'GETTING_STARTED.md',
                     'INCLUDES_AND_DEPENDENCIES.md', 'WINDOW_AND_CAMERA.md'):
            self.assertTrue((DOCS / name).is_file(), name)

    def test_local_markdown_links_in_developer_guides_resolve(self):
        guides = [ROOT / 'README.md', *DOCS.glob('*.md')]
        # Include guides kept in subdirectories only if they directly link to Markdown.
        broken = []
        for guide in guides:
            text = guide.read_text(encoding='utf-8')
            for destination in re.findall(r'\]\(([^\s)]+)(?:\s+"[^"]*")?\)', text):
                if destination.startswith(('http:', 'https:', 'mailto:', '#', 'data:')):
                    continue
                pathname = unquote(destination.split('#', 1)[0].split('?', 1)[0])
                if not pathname:
                    continue
                if not (guide.parent / pathname).exists():
                    broken.append(f'{guide.relative_to(ROOT)} -> {destination}')
        self.assertEqual(broken, [], '\n'.join(broken))


if __name__ == '__main__':
    unittest.main()
