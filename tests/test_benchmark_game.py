"""Tests for benchmark fixtures, profile aggregation and reproducibility."""
import csv
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / 'games/XEngine Benchmark'
RUNNER = ROOT / 'tools/run_benchmark.py'
spec = importlib.util.spec_from_file_location('xengine_benchmark_runner', RUNNER)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class BenchmarkTests(unittest.TestCase):
    def test_default_engine_config_is_valid_for_debug_and_release_builds(self):
        # tools/build.py always reads this file, even though the Debug game
        # selects config/{mode}.json at runtime. Without it, --build fails.
        spec = importlib.util.spec_from_file_location('xengine_benchmark_build', ROOT / 'tools/build.py')
        build = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(build)
        default = json.loads((GAME/'assets/config/engine.json').read_text(encoding='utf-8'))
        self.assertEqual(default['Window']['title'], 'XEngine Benchmark')
        self.assertFalse(default['Renderer']['vsync'])
        self.assertTrue(default['Engine']['profileCsv'])
        with tempfile.TemporaryDirectory() as temp:
            for profile in ('debug', 'release'):
                with self.subTest(profile=profile):
                    result = build.generate_game_config(GAME, Path(temp)/profile, profile)
                    self.assertTrue(result.is_file())
                    self.assertTrue((result.parent/'GameBuild.generated.h').is_file())

    def test_all_modes_have_independent_startup_config(self):
        for mode in module.MODES:
            cfg = json.loads((GAME/'assets/config'/f'{mode}.json').read_text())
            self.assertTrue(cfg['Engine']['profileCsv'])
            self.assertFalse(cfg['Renderer']['vsync'])
            self.assertEqual(cfg['Engine']['multiThreading'], mode != 'single')
            self.assertEqual(cfg['Renderer']['parallelPreparation'], mode == 'parallel')

    def test_broadphase_settings_in_benchmark_and_blank_game_template(self):
        # Benchmark profiles remain explicit baselines; new games use adaptive default.
        paths = [GAME / 'assets/config' / f'{name}.json'
                 for name in ('engine', *module.MODES)]
        paths.append(ROOT / 'tools/templates/BlankGame/assets/config/engine.json')
        for path in paths:
            with self.subTest(path=path.relative_to(ROOT)):
                config = json.loads(path.read_text(encoding='utf-8'))
                self.assertEqual(config['Collision'], {
                    'broadphase': 'auto' if 'templates' in str(path) else 'bruteforce',
                    'cellSize': 'auto' if 'templates' in str(path) else 2.0})

    def test_mode_configs_reject_different_broadphases(self):
        # Multi-thread comparisons must not silently compare different
        # collision algorithms across single/collision/parallel modes.
        with tempfile.TemporaryDirectory() as tmp:
            dest = Path(tmp)
            for mode in module.MODES:
                original = json.loads((GAME / 'assets/config' /
                                       f'{mode}.json').read_text(encoding='utf-8'))
                if mode == 'collision':
                    original['Collision']['broadphase'] = 'grid'
                (dest / f'{mode}.json').write_text(json.dumps(original), encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'Non-threading settings differ'):
                module.validate_mode_configs(dest, module.MODES)

    def test_mode_config_preflight_and_integrity(self):
        config_dir = GAME / 'assets/config'
        result = module.validate_mode_configs(config_dir, module.MODES)
        self.assertEqual(set(result), set(module.MODES))
        self.assertEqual(len(result['single']['sha256']), 64)
        self.assertTrue(result['collision']['multiThreading'])
        self.assertFalse(result['collision']['parallelPreparation'])

    def test_mode_config_preflight_rejects_unsafe_json(self):
        configs = [json.loads((GAME/'assets/config'/f'{mode}.json').read_text())
                   for mode in module.MODES]
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            for mode, config in zip(module.MODES, configs):
                (directory / f'{mode}.json').write_text(json.dumps(config))
            configs[1]['Engine']['multiThreading'] = False
            (directory / 'collision.json').write_text(json.dumps(configs[1]))
            with self.assertRaisesRegex(ValueError, 'Engine.multiThreading'):
                module.validate_mode_configs(directory, module.MODES)
            configs[1]['Engine']['multiThreading'] = True
            configs[1]['Physics']['maxSubsteps'] = 8
            (directory / 'collision.json').write_text(json.dumps(configs[1]))
            with self.assertRaisesRegex(ValueError, 'Non-threading settings differ'):
                module.validate_mode_configs(directory, module.MODES)
            (directory / 'parallel.json').unlink()
            with self.assertRaises(ValueError):
                module.validate_mode_configs(directory, module.MODES)

    def test_collision_parallel_profile_telemetry_is_emitted(self):
        engine = (ROOT/'engine/src/Engine.cpp').read_text()
        collision = (ROOT/'engine/src/CollisionManager.cpp').read_text()
        self.assertIn('collision_parallel_steps', engine)
        self.assertIn('mCollisionParallelStepsThisFrame = 0', engine)
        self.assertIn('mLastUsedParallelCandidates = true;', collision)
        self.assertIn('collision_parallel_steps', module.METRICS)

    def test_direct_executable_uses_mode_json_without_cli_override(self):
        source = (GAME / 'src/main.cpp').read_text(encoding='utf-8')
        self.assertIn('config.at("Collision")', source)
        self.assertIn('if (!broadphaseOverride) broadphase = configuredBroadphase;', source)
        self.assertIn('if (!cellSizeOverride) {', source)
        self.assertIn('config["Collision"]["broadphase"] = broadphase;', source)
        self.assertIn('config["Collision"]["cellSize"] = autoCellSize ?', source)

    def test_game_startup_uses_verified_inline_config(self):
        source = (GAME/'src/main.cpp').read_text()
        self.assertIn('Engine::StartFromJson(runtimeConfig)', source)
        self.assertIn('config["Collision"]["broadphase"] = broadphase;', source)
        self.assertIn('[BENCH-CONFIG] mode=', source)
        self.assertEqual(module.expected_config_marker('collision'),
                         '[BENCH-CONFIG] mode=collision multithreading=1 render_parallel=0 vsync=0 profile_csv=1')

    def test_all_scenarios_have_deterministic_configurations(self):
        source = (GAME/'assets/scenes/BenchmarkScene.hpp').read_text()
        for scenario in module.SCENARIOS:
            self.assertIn(f'"{scenario}"', source)
        self.assertIn('std::mt19937 rng(opts.seed)', source)
        self.assertIn('AddComponent<Collider2D>()', source)
        self.assertIn('AddComponent<RigidBody2D>()', source)
        self.assertIn('AddComponent<SpriteRenderer>()', source)

    def test_mode_rotation_does_not_bias_first_run(self):
        orders = [module.mode_order(i, module.MODES) for i in range(6)]
        self.assertEqual(orders[0], ['single', 'collision', 'parallel'])
        self.assertTrue(all(set(order) == set(module.MODES) for order in orders))
        self.assertGreater(len(set(tuple(order) for order in orders)), 2)

    def test_measure_skips_warmup_and_reports_parallel_activation(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'profile.csv'
            with path.open('w', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=['frame_ms','collision_ms','render_prepare_ms',
                         'render_path_reason','broadphase_tests','fixed_steps'])
                writer.writeheader()
                writer.writerows([
                    {'frame_ms':100, 'collision_ms':20, 'render_prepare_ms':0, 'render_path_reason':0, 'broadphase_tests':0, 'fixed_steps':1},
                    {'frame_ms':8, 'collision_ms':3, 'render_prepare_ms':1, 'render_path_reason':3, 'broadphase_tests':10000, 'fixed_steps':1},
                    {'frame_ms':12, 'collision_ms':5, 'render_prepare_ms':2, 'render_path_reason':3, 'broadphase_tests':10000, 'fixed_steps':1},
                ])
            stats = module.measure(path, 1)
            self.assertEqual(stats['frame_ms_p50'], 10)
            self.assertEqual(stats['render_parallel_frames'], 2)
            self.assertEqual(stats['sampled_frames'], 2)

    def test_camera_viewbase_property_uses_view_not_center(self):
        source = (ROOT/'engine/include/Camera2D.h').read_text(encoding='utf-8')
        self.assertIn('ViewProperty viewBase{ this };', source)
        self.assertNotIn('CenterProperty viewBase{ this };', source)
        self.assertIn('&Camera2D::SetViewBase', source)

    def test_runner_rejects_benchmarks_without_visible_sprites(self):
        with self.assertRaisesRegex(ValueError, 'no world sprites submitted'):
            module.assert_rendered_sprites({'render_world_draws_mean': 0.0}, 'render')
        with self.assertRaisesRegex(ValueError, 'no world sprites submitted'):
            module.assert_rendered_sprites({}, 'stress')
        module.assert_rendered_sprites({'render_world_draws_mean': 123.0}, 'render')
        self.assertIn('render_world_draws', module.METRICS)

    def test_assets_and_game_entrypoint_exist(self):
        for item in ('build.sh', 'build.bat', 'config.json', 'src/main.cpp',
                     'src/InitCallback.hpp', 'assets/fonts/ui-default.ttf'):
            self.assertTrue((GAME/item).is_file(), item)
        for shape in ('circle','square','triangle','diamond'):
            self.assertTrue((GAME/f'assets/textures/{shape}.png').is_file())

if __name__ == '__main__':
    unittest.main()
