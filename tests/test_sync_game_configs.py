"""Safe game config.json synchronization and pre-build integration tests."""
from __future__ import annotations

import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import build
import sync_engine_configs as common
import sync_game_configs as gamesync


def fixture(root: Path) -> tuple[dict, dict]:
    template = common.read_json(ROOT / gamesync.TEMPLATE_REL)
    state = common.read_json(ROOT / gamesync.STATE_REL)
    target = root / gamesync.TEMPLATE_REL
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(template), encoding='utf-8')
    manifest = root / gamesync.STATE_REL
    manifest.write_text(json.dumps(state), encoding='utf-8')
    (root / 'games').mkdir()
    return template, state


def game(root: Path, name: str, data: dict | None = None) -> Path:
    folder = root / 'games' / name
    folder.mkdir()
    path = folder / 'config.json'
    if data is not None:
        path.write_text(json.dumps(data), encoding='utf-8')
    return path


class GameConfigSyncTests(unittest.TestCase):
    def test_repository_template_and_all_games_are_in_sync(self):
        self.assertEqual(gamesync.sync(ROOT, check=True, quiet=True), 0)
        for item in gamesync.discover_games(ROOT, None):
            data = common.read_json(item / 'config.json')
            self.assertEqual(data['project']['name'], item.name)

    def test_user_values_and_unknown_extensions_survive_merging(self):
        template = common.read_json(ROOT / gamesync.TEMPLATE_REL)
        original = copy.deepcopy(template)
        original['project']['name'] = 'Original Name'
        original['project']['version'] = '4.12.0'
        original['project']['icon']['windows'] = 'assets/icons/custom.ico'
        original['build']['platform'] = 'windows'
        original['build']['configuration'] = 'Release'
        original['build']['architecture'] = 'x86_64'
        original['defines'] = ['MY_FEATURE=1', 'DISABLE_DEMO']
        original['shipping']['copy_runtime_assets'] = False
        original['customBuild'] = {'linker': {'lto': True}}
        updated = copy.deepcopy(template)
        updated['build']['newFlag'] = True
        before = copy.deepcopy(original)
        result, added, removed, reset, extra = gamesync.merge_game_config(
            original, updated, 'Different Directory', common.leaf_paths(template))
        self.assertEqual(original, before)
        self.assertEqual(result['project'], before['project'])
        self.assertEqual(result['build']['platform'], 'windows')
        self.assertEqual(result['build']['configuration'], 'Release')
        self.assertEqual(result['defines'], ['MY_FEATURE=1', 'DISABLE_DEMO'])
        self.assertFalse(result['shipping']['copy_runtime_assets'])
        self.assertEqual(result['customBuild'], before['customBuild'])
        self.assertTrue(result['build']['newFlag'])
        self.assertIn(('build', 'newFlag'), added)
        self.assertIn(('customBuild', 'linker', 'lto'), extra)
        self.assertEqual(removed, [])
        self.assertEqual(reset, [])

    def test_legacy_icon_string_and_null_remain_supported(self):
        template = common.read_json(ROOT / gamesync.TEMPLATE_REL)
        for icon in ('assets/icons/old-game.ico', 'default', None):
            with self.subTest(icon=icon):
                source = copy.deepcopy(template)
                source['project']['icon'] = icon
                merged, _, _, reset, _ = gamesync.merge_game_config(
                    source, template, 'OldGame', common.leaf_paths(template))
                self.assertEqual(merged['project']['icon'], icon)
                self.assertNotIn(('project', 'icon'), reset)

    def test_disabled_platform_icon_null_is_preserved(self):
        template = common.read_json(ROOT / gamesync.TEMPLATE_REL)
        original = copy.deepcopy(template)
        original['project']['icon']['windows'] = None
        merged, _, _, reset, _ = gamesync.merge_game_config(
            original, template, 'Example', common.leaf_paths(template))
        self.assertIsNone(merged['project']['icon']['windows'])
        self.assertEqual(merged['project']['icon']['linux'], 'default')
        self.assertNotIn(('project', 'icon', 'windows'), reset)

    def test_historical_template_key_removed_but_custom_field_preserved(self):
        template = common.read_json(ROOT / gamesync.TEMPLATE_REL)
        original = copy.deepcopy(template)
        original['build']['removedFlag'] = 10
        original['additional'] = 'keep'
        known = common.leaf_paths(template) | {('build', 'removedFlag')}
        merged, _, removed, _, _ = gamesync.merge_game_config(
            original, template, 'Example', known)
        self.assertNotIn('removedFlag', merged['build'])
        self.assertEqual(merged['additional'], 'keep')
        self.assertIn(('build', 'removedFlag'), removed)

    def test_check_is_read_only_and_migration_backups_are_byte_exact(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            template, _ = fixture(root)
            config = copy.deepcopy(template)
            config['project']['name'] = 'Sample'
            config['project']['icon']['linux'] = 'my/icon.png'
            del config['shipping']
            path = game(root, 'Sample', config)
            before = path.read_bytes()
            backup = root / 'saved-backups'
            self.assertEqual(gamesync.sync(root, check=True, backup_dir=backup, quiet=True), 1)
            self.assertEqual(path.read_bytes(), before)
            self.assertFalse(backup.exists())
            self.assertEqual(gamesync.sync(root, backup_dir=backup, quiet=True), 0)
            self.assertEqual(gamesync.sync(root, check=True, quiet=True), 0)
            merged = common.read_json(path)
            self.assertEqual(merged['project']['icon']['linux'], 'my/icon.png')
            self.assertTrue(merged['shipping']['copy_runtime_assets'])
            copies = list(backup.rglob('config.json'))
            self.assertEqual(len(copies), 1)
            self.assertEqual(copies[0].read_bytes(), before)

    def test_missing_named_config_is_created_with_directory_name(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            template, _ = fixture(root)
            path = game(root, 'Brand New')
            self.assertEqual(gamesync.sync(root, ['Brand New'], check=True, quiet=True), 1)
            self.assertFalse(path.exists())
            self.assertEqual(gamesync.sync(
                root, ['Brand New'], backup_dir=root / 'backups', quiet=True), 0)
            expected = copy.deepcopy(template)
            expected['project']['name'] = 'Brand New'
            self.assertEqual(common.read_json(path), expected)

    def test_invalid_input_prevents_partial_migration(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            template, _ = fixture(root)
            first = copy.deepcopy(template)
            first['project']['name'] = 'One'
            del first['shipping']
            good = game(root, 'One', first)
            broken = game(root, 'Two', template)
            broken.write_text('{invalid json', encoding='utf-8')
            before = good.read_bytes()
            with self.assertRaises(common.ConfigError):
                gamesync.sync(root, backup_dir=root / 'backups', quiet=True)
            self.assertEqual(before, good.read_bytes())
            self.assertFalse((root / 'backups').exists())

    def test_write_failure_restores_first_game_config(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            template, _ = fixture(root)
            for name in ('One', 'Two'):
                source = copy.deepcopy(template)
                source['project']['name'] = name
                source.pop('shipping')
                game(root, name, source)
            first = root / 'games' / 'One' / 'config.json'
            second = root / 'games' / 'Two' / 'config.json'
            original = first.read_bytes()
            original_writer = common._write_atomic

            def fail_second(path, data):
                if path == second:
                    raise OSError('test fault')
                return original_writer(path, data)

            with patch.object(common, '_write_atomic', side_effect=fail_second):
                with self.assertRaisesRegex(OSError, 'test fault'):
                    gamesync.sync(root, backup_dir=root / 'backups', quiet=True)
            self.assertEqual(first.read_bytes(), original)
            self.assertEqual(gamesync.sync(root, check=True, quiet=True), 1)

    def test_build_runs_all_three_sync_steps_before_compiling(self):
        with patch.object(build, 'command') as runner:
            build.sync_before_game_build('Some Game')
        requested = [Path(call.args[0][1]).name for call in runner.call_args_list]
        self.assertEqual(requested, [
            'sync_game_configs.py', 'sync_engine_configs.py', 'sync_game_icons.py'])
        for call in runner.call_args_list:
            self.assertEqual(call.args[0][-2:], ['--game', 'Some Game'])
            self.assertEqual(call.kwargs['cwd'], ROOT)
            self.assertTrue(build._command_stage(call.args[0])[0].startswith('[SYNC]'))

    def test_unknown_keys_are_preserved_by_default(self):
        template = common.read_json(ROOT / gamesync.TEMPLATE_REL)
        before = copy.deepcopy(template)
        before['custom'] = {'any': [1, 2, 3]}
        updated, *_ = gamesync.merge_game_config(
            before, template, 'Example', common.leaf_paths(template))
        self.assertEqual(updated['custom'], {'any': [1, 2, 3]})


if __name__ == '__main__':
    unittest.main()
