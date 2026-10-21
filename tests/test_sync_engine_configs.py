"""Regression tests for template-driven game configuration migration."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('sync_engine_configs', ROOT/'tools/sync_engine_configs.py')
sync = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sync)


class SyncEngineConfigsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.template_path = self.root / sync.TEMPLATE_REL
        self.template_path.parent.mkdir(parents=True)
        self.template = {
            'Engine': {'multiThreading': False, 'profileCsv': False, 'logErrors': True},
            'Window': {'title': '{{PROJECT_NAME}}', 'width': 1280},
            'Renderer': {'bgColor': [0, 0, 0, 255]},
            'Collision': {'broadphase': 'bruteforce', 'cellSize': 2.0},
        }
        self.save_json(self.template_path, self.template)
        self.state_path = self.root / sync.STATE_REL
        self.state_path.parent.mkdir(parents=True, exist_ok=True)
        self.save_json(self.state_path, sync.encode_state(sync.leaf_paths(self.template)))
        (self.root / 'games').mkdir()

    @staticmethod
    def save_json(path, doc):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

    @staticmethod
    def load_json(path):
        return json.loads(path.read_text(encoding='utf-8'))

    def game(self, name='My Game', doc=None):
        root = self.root / 'games' / name
        root.mkdir(parents=True, exist_ok=True)
        (root/'config.json').write_text('{}\n')
        path = root / 'assets/config/engine.json'
        if doc is not None:
            self.save_json(path, doc)
        return path

    def run_sync(self, **opts):
        return sync.sync(self.root, quiet=True, **opts)

    def test_adds_missing_keys_and_preserves_modified_values(self):
        path = self.game(doc={
            'Engine': {'multiThreading': True},
            'Window': {'title': 'Custom!', 'width': 900},
            'Renderer': {'bgColor': [1, 2, 3, 4]},
            'Collision': {'broadphase': 'grid', 'cellSize': 32.0},
        })
        self.assertEqual(self.run_sync(), 0)
        result = self.load_json(path)
        self.assertTrue(result['Engine']['multiThreading'])
        self.assertFalse(result['Engine']['profileCsv'])
        self.assertTrue(result['Engine']['logErrors'])
        self.assertEqual(result['Window'], {'title': 'Custom!', 'width': 900})
        self.assertEqual(result['Renderer']['bgColor'], [1, 2, 3, 4])
        self.assertEqual(result['Collision'], {'broadphase': 'grid', 'cellSize': 32.0})

    def test_obsolete_managed_setting_is_removed_without_deleting_custom_keys(self):
        path = self.game(doc={
            'Engine': {'multiThreading': True, 'profileCsv': True, 'logErrors': False,
                       'customFeature': 'still-here'},
            'Window': {'title': 'Mine', 'width': 999},
            'LegacySection': {'gameSpecific': 33},
        })
        self.template['Engine'].pop('profileCsv')
        self.template.pop('Collision')
        self.save_json(self.template_path, self.template)
        self.assertEqual(self.run_sync(), 0)
        result = self.load_json(path)
        self.assertNotIn('profileCsv', result['Engine'])
        self.assertNotIn('Collision', result)
        self.assertEqual(result['Engine']['customFeature'], 'still-here')
        self.assertEqual(result['LegacySection'], {'gameSpecific': 33})
        state = self.load_json(self.state_path)
        self.assertIn(['Collision', 'cellSize'], state['managedLeaves'])
        self.assertIn(['Engine', 'profileCsv'], state['managedLeaves'])

    def test_new_default_replaces_wrong_type_but_not_valid_custom_value(self):
        path = self.game(doc={'Window': {'width': 'wrong-type'},
                              'Engine': {'multiThreading': True},
                              'Collision': {'cellSize': 8},
                              'Renderer': {'bgColor': [11, 22, 33]}})
        self.run_sync()
        result = self.load_json(path)
        self.assertEqual(result['Window']['width'], 1280)
        self.assertTrue(result['Engine']['multiThreading'])
        self.assertEqual(result['Collision']['cellSize'], 8)
        self.assertEqual(result['Renderer']['bgColor'], [11, 22, 33])
        backups = list((self.root / sync.BACKUP_DIR).glob('*/games/My Game/assets/config/engine.json'))
        self.assertEqual(len(backups), 1)
        self.assertEqual(self.load_json(backups[0])['Window']['width'], 'wrong-type')

    def test_cell_size_auto_template_preserves_numeric_game_tuning(self):
        self.template['Collision']['broadphase']='auto'
        self.template['Collision']['cellSize']='auto'
        self.save_json(self.template_path,self.template)
        custom=self.game(doc={'Collision': {'broadphase':'grid','cellSize':1.25}})
        self.assertEqual(self.run_sync(),0)
        self.assertEqual(self.load_json(custom)['Collision'],
                         {'broadphase':'grid','cellSize':1.25})
        self.assertEqual(self.run_sync(check=True),0)
        fresh=self.game('New Game')
        self.assertEqual(self.run_sync(),0)
        self.assertEqual(self.load_json(fresh)['Collision'],
                         {'broadphase':'auto','cellSize':'auto'})

    def test_render_threshold_is_synced_without_overwriting_user_tuning(self):
        # A new template field must be applied to every startup profile while
        # preserving the different thresholds selected by individual games.
        self.template['Renderer']['parallelMinCommands'] = 384
        self.save_json(self.template_path, self.template)
        path = self.game(name='XEngine Benchmark', doc={
            'Renderer': {'parallelPreparation': True, 'bgColor': [1, 2, 3, 4]},
            'Engine': {'multiThreading': True},
        })
        alternate = path.parent / 'parallel.json'
        self.save_json(alternate, {
            'Renderer': {'parallelPreparation': True, 'parallelMinCommands': 640},
            'Engine': {'multiThreading': True},
        })
        original = path.read_bytes()
        self.assertEqual(self.run_sync(check=True), 1)
        self.assertEqual(path.read_bytes(), original)
        self.assertEqual(self.run_sync(), 0)
        self.assertEqual(self.load_json(path)['Renderer']['parallelMinCommands'], 384)
        self.assertEqual(self.load_json(path)['Renderer']['bgColor'], [1, 2, 3, 4])
        self.assertEqual(self.load_json(alternate)['Renderer']['parallelMinCommands'], 640)
        self.assertEqual(self.run_sync(check=True), 0)

    def test_render_threshold_invalid_value_uses_template_and_creates_backup(self):
        self.template['Renderer']['parallelMinCommands'] = 384
        self.save_json(self.template_path, self.template)
        path = self.game(doc={'Renderer': {'parallelMinCommands': 'incorrect'}})
        self.assertEqual(self.run_sync(), 0)
        self.assertEqual(self.load_json(path)['Renderer']['parallelMinCommands'], 384)
        backups = list((self.root / sync.BACKUP_DIR).glob(
            '*/games/My Game/assets/config/engine.json'))
        self.assertEqual(len(backups), 1)
        self.assertEqual(self.load_json(backups[0])['Renderer']['parallelMinCommands'],
                         'incorrect')

    def test_check_does_not_write_anything_and_reports_drift(self):
        path = self.game(doc={'Engine': {'multiThreading': True}})
        before = path.read_bytes()
        manifest = self.state_path.read_bytes()
        self.assertEqual(self.run_sync(check=True), 1)
        self.assertEqual(before, path.read_bytes())
        self.assertEqual(manifest, self.state_path.read_bytes())
        self.assertFalse((self.root / sync.BACKUP_DIR).exists())
        self.run_sync()
        self.assertEqual(self.run_sync(check=True), 0)

    def test_missing_engine_json_is_created_with_project_name(self):
        path = self.game(name='A Princess Run')
        self.assertEqual(self.run_sync(), 0)
        self.assertEqual(self.load_json(path)['Window']['title'], 'A Princess Run')

    def test_all_mode_configs_migrate_but_unrelated_json_is_not_touched(self):
        engine = self.game(name='XEngine Benchmark', doc={'Engine': {'multiThreading': True}})
        base = engine.parent
        for name, multi, render in (('single', False, False),
                                    ('collision', True, False),
                                    ('parallel', True, True)):
            self.save_json(base / f'{name}.json', {
                'Engine': {'multiThreading': multi},
                'Renderer': {'parallelPreparation': render},
            })
        other = base / 'level.json'
        self.save_json(other, {'map': 7, 'difficulty': 1})
        before = other.read_bytes()
        self.run_sync()
        for name, multi, render in (('single', False, False),
                                    ('collision', True, False),
                                    ('parallel', True, True)):
            cfg = self.load_json(base/f'{name}.json')
            self.assertEqual(cfg['Engine']['multiThreading'], multi)
            self.assertEqual(cfg['Renderer']['parallelPreparation'], render)
            self.assertEqual(cfg['Collision']['cellSize'], 2.0)
        self.assertEqual(other.read_bytes(), before)

    def test_first_bad_file_aborts_before_writing_anything(self):
        first = self.game(name='Game A', doc={'Engine': {'multiThreading': True}})
        second = self.game(name='Game B', doc={'Engine': {'multiThreading': False}})
        before = first.read_bytes()
        second.write_text('{broken', encoding='utf8')
        with self.assertRaises(sync.ConfigError):
            self.run_sync()
        self.assertEqual(first.read_bytes(), before)
        self.assertFalse((self.root / sync.BACKUP_DIR).exists())

    def test_game_selection_retains_obsolete_history_for_later_games(self):
        first = self.game(name='A', doc={'Engine': {'multiThreading': True,
                                                  'profileCsv': True}})
        second = self.game(name='B', doc={'Engine': {'multiThreading': True,
                                                   'profileCsv': False}})
        self.template['Engine'].pop('profileCsv')
        self.save_json(self.template_path, self.template)
        self.run_sync(names=['A'])
        self.assertNotIn('profileCsv', self.load_json(first)['Engine'])
        self.assertIn('profileCsv', self.load_json(second)['Engine'])
        self.run_sync(names=['B'])
        self.assertNotIn('profileCsv', self.load_json(second)['Engine'])

    def test_prune_unknown_is_explicit_and_backed_up(self):
        path = self.game(doc={'Engine': {'multiThreading': True, 'gameOnly': 123}})
        self.run_sync()
        self.assertEqual(self.load_json(path)['Engine']['gameOnly'], 123)
        self.run_sync(prune_unknown=True)
        self.assertNotIn('gameOnly', self.load_json(path)['Engine'])

    def test_migration_is_idempotent_and_second_run_has_no_backup(self):
        path = self.game(doc={'Engine': {'multiThreading': True}})
        self.run_sync()
        contents = path.read_bytes()
        backups = list((self.root / sync.BACKUP_DIR).iterdir())
        self.run_sync()
        self.assertEqual(path.read_bytes(), contents)
        self.assertEqual(list((self.root / sync.BACKUP_DIR).iterdir()), backups)

    def test_duplicate_keys_are_rejected(self):
        path = self.game(doc={'Engine': {'multiThreading': True}})
        path.write_text('{"Engine":{},"Engine":{}}', encoding='utf-8')
        with self.assertRaisesRegex(sync.ConfigError, 'Duplicate JSON key'):
            self.run_sync()

    def test_real_repo_template_is_valid_and_manifest_tracks_it(self):
        template = sync.read_json(ROOT / sync.TEMPLATE_REL)
        known, exists = sync.read_state(ROOT / sync.STATE_REL,
                                        sync.leaf_paths(template))
        self.assertTrue(exists)
        self.assertTrue(sync.leaf_paths(template).issubset(known))
        self.assertEqual(template['Renderer']['parallelMinCommands'], 1536)
        self.assertIn(('Renderer', 'parallelMinCommands'), known)


if __name__ == '__main__':
    unittest.main()
