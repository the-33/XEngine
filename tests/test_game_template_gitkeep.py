"""BlankGame runtime settings and Git-friendly empty directories."""
from __future__ import annotations

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / 'tools' / 'templates' / 'BlankGame'


def load_module(name: str, source: Path):
    spec = importlib.util.spec_from_file_location(name, source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class TemplateTests(unittest.TestCase):
    def test_explicit_runtime_settings_match_engine_parser(self):
        cfg = json.loads((TEMPLATE / 'assets/config/engine.json').read_text(encoding='utf-8'))
        self.assertEqual(cfg['Engine']['multiThreading'], False)
        self.assertEqual(cfg['Engine']['profileCsv'], False)
        self.assertEqual(cfg['Engine']['logStats'], True)
        self.assertEqual(cfg['Renderer']['vsync'], True)
        self.assertEqual(cfg['Renderer']['parallelPreparation'], True)
        engine_cpp = (ROOT / 'engine/src/Engine.cpp').read_text(encoding='utf-8')
        for section, fields in [('Engine', ('multiThreading', 'profileCsv', 'logStats', 'logErrors')),
                                ('Renderer', ('parallelPreparation', 'vsync', 'hardwareAcceleration', 'bgColor'))]:
            for field in fields:
                with self.subTest(section=section, field=field):
                    self.assertIn('"' + field + '"', engine_cpp)
        self.assertEqual(cfg['Window']['title'], '{{PROJECT_NAME}}')
        self.assertIn('Assets', cfg)
        self.assertIn('Physics', cfg)

    def test_template_has_resource_dirs_but_no_project_root_opt_in(self):
        assets = TEMPLATE / 'assets'
        for dirname in ('audio', 'fonts', 'textures', 'gameobjects',
                        'scripts', 'scenes', 'icons', 'config'):
            self.assertTrue((assets / dirname).is_dir(), dirname)
        # Keeping placeholders in BlankGame is fine; new_project strips them.
        self.assertFalse((TEMPLATE / '.gitkeep').exists())

    def test_generated_project_does_not_copy_or_create_any_gitkeep(self):
        new_project = load_module('xengine_new_project_template_gitkeep', ROOT / 'tools/new_project.py')
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'games').mkdir()
            with mock.patch.object(new_project, 'ROOT', root), \
                 mock.patch.object(new_project, 'TEMPLATE', TEMPLATE), \
                 mock.patch.object(sys, 'argv', ['new_project.py', 'Sample Game']), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(new_project.main(), 0)
            game = root / 'games/Sample Game'
            target = game / 'assets'
            for dirname in ('audio', 'fonts', 'textures'):
                self.assertTrue((target / dirname).is_dir())
            self.assertFalse(list(game.rglob('.gitkeep')))
            cfg = json.loads((target / 'config/engine.json').read_text(encoding='utf-8'))
            self.assertEqual(cfg['Window']['title'], 'Sample Game')
            self.assertEqual(cfg['Engine']['profileCsv'], False)
            self.assertEqual(cfg['Renderer']['parallelPreparation'], True)

    def test_release_assets_ignore_gitkeep(self):
        source = (ROOT / 'tools/build.py').read_text(encoding='utf-8')
        self.assertIn("if p.name=='.gitkeep': continue", source)


if __name__ == '__main__':
    unittest.main()
