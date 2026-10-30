"""Engine version is a release identifier, never a game's project version."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('engine_version', ROOT / 'tools/engine_version.py')
assert SPEC and SPEC.loader
versioning = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(versioning)


class EngineVersionTests(unittest.TestCase):
    def test_root_file_is_single_semver_source(self):
        self.assertEqual(versioning.read_engine_version(), (ROOT / 'VERSION').read_text(encoding='utf-8').strip())
        self.assertEqual(versioning.source_archive_name('0.1.1'), 'XEngine-0.1.1-source.zip')
        self.assertEqual(versioning.source_archive_name('0.1.1', full=False),
                         'XEngine-0.1.1-patch.zip')

    def test_version_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for valid in ('0.1.0', '1.0.0', '10.3.14', '0.2.0-beta.1',
                          '0.2.0-1alpha+build.2'):
                (root / 'VERSION').write_text(valid + '\n', encoding='utf-8')
                self.assertEqual(versioning.read_engine_version(root), valid)
            for invalid in ('v16.37', '16.37', '01.2.3', '0.01.1', '0.1.01',
                            '0.2.0-01', '0.2.0+', '0.2.0-'):
                (root / 'VERSION').write_text(invalid, encoding='utf-8')
                with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                    versioning.read_engine_version(root)

    def test_game_project_version_stays_independent(self):
        template = json.loads((ROOT / 'tools/templates/BlankGame/config.json').read_text(
            encoding='utf-8'))
        self.assertIn('version', template['project'])
        self.assertIn('games', (ROOT / 'tools/build.py').read_text(encoding='utf-8'))
        self.assertIn('VERSIONING.md', (ROOT / 'Doxyfile_Games').read_text(encoding='utf-8'))

    def test_iteration_always_reviews_release_decision(self):
        policy = (ROOT / 'docs/VERSIONING.md').read_text(encoding='utf-8')
        self.assertIn('At the end of every development iteration, review the version decision.', policy)
        self.assertIn('Games own', (ROOT / 'docs/reference/engine/DOCUMENTATION_WORKFLOW.md').read_text(encoding='utf-8'))
        # AI_CONTEXT.md is developer-local and deliberately excluded from Git.
        # Release policy is validated only against the committed public guide.

    def test_doxygen_is_given_version_from_canonical_file(self):
        spec = importlib.util.spec_from_file_location('version_doxygen', ROOT / 'tools/build_doxygen.py')
        assert spec and spec.loader
        doc = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(doc)
        config = (ROOT / 'Doxyfile_Games').read_text(encoding='utf-8')
        self.assertIn('PROJECT_NUMBER         = "$(XENGINE_VERSION)"', config)
        self.assertNotIn('v16.37', config)
        with mock.patch.object(doc, 'group_markdown_navigation', return_value=0), \
             mock.patch.object(doc, 'style_document_pages', return_value=(0, 0)), \
             mock.patch.object(doc, 'redirect_empty_markdown_file_references', return_value=0), \
             mock.patch.object(doc, 'prune_duplicate_markdown_search', return_value=0), \
             mock.patch.object(doc.subprocess, 'run') as runner, \
             mock.patch.object(doc, 'version_navigation_assets', return_value=0), \
             mock.patch.object(doc, 'invalidate_docs_cache'), \
             mock.patch.object(doc, 'stamp_docs_current', return_value=True), \
             mock.patch.object(doc.shutil, 'rmtree'), \
             mock.patch.object(doc.shutil, 'copyfile'):
            runner.return_value.returncode = 0
            self.assertEqual(doc.build('/usr/bin/doxygen'), 0)
            self.assertEqual(runner.call_count, 2)
            self.assertEqual(runner.call_args_list[0].kwargs['env']['XENGINE_VERSION'], versioning.read_engine_version())
            self.assertEqual(runner.call_args_list[1].kwargs['env']['XENGINE_VERSION'], versioning.read_engine_version())
            self.assertEqual(runner.call_args_list[0].args[0][1], 'Doxyfile_Games')
            self.assertEqual(runner.call_args_list[1].args[0][1], 'Doxyfile_Engine')


if __name__ == '__main__':
    unittest.main()
