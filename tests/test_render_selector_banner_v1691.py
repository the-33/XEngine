"""Regression: fixed/adaptive render selector benchmark handshake."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('render_benchmark_runner', ROOT / 'tools/run_benchmark.py')
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class RenderSelectorBannerTests(unittest.TestCase):
    def check(self, output: str, selection: str = 'fixed', cycle: bool = True) -> None:
        runner.assert_render_configuration_marker(
            output, threshold=1536, sprites=2048, workload='baseline',
            selection=selection, cycle=cycle)

    def test_real_windows_log_fixed_cycle_is_accepted(self):
        self.check("""[BENCH-SPRITE-PROJECTION] mode=workers
[BENCH-RENDER] threshold=1536 sprites=2048 workload=baseline selection=fixed cycle=1
[BENCH-BROADPHASE] mode=bruteforce cell_size=2.000""")

    def test_adaptive_cycle_is_accepted(self):
        self.check('[BENCH-RENDER] threshold=1536 sprites=2048 workload=baseline selection=adaptive cycle=1',
                   selection='adaptive')

    def test_older_banner_without_selector_and_cycle_fails_closed(self):
        with self.assertRaisesRegex(RuntimeError, 'selection: expected=fixed, actual=<missing>'):
            self.check('[BENCH-RENDER] threshold=1536 sprites=2048 workload=baseline')

    def test_wrong_mode_cycle_and_workload_are_rejected(self):
        cases = [
            ('[BENCH-RENDER] threshold=1536 sprites=2048 workload=baseline selection=adaptive cycle=1', 'selection'),
            ('[BENCH-RENDER] threshold=1536 sprites=2048 workload=baseline selection=fixed cycle=0', 'cycle'),
            ('[BENCH-RENDER] threshold=1536 sprites=2048 workload=mixed selection=fixed cycle=1', 'workload'),
            ('[BENCH-RENDER] threshold=1 sprites=2048 workload=baseline selection=fixed cycle=1', 'threshold'),
            ('[BENCH-RENDER] threshold=1536 sprites=256 workload=baseline selection=fixed cycle=1', 'sprites'),
        ]
        for banner, field in cases:
            with self.subTest(field=field), self.assertRaisesRegex(RuntimeError, field):
                self.check(banner)

    def test_future_fields_and_reordered_fields_remain_compatible(self):
        self.check('[BENCH-RENDER] cycle=1 selection=fixed sprites=2048 custom=42 workload=baseline threshold=1536')

    def test_missing_and_duplicate_banners_rejected(self):
        banner = '[BENCH-RENDER] threshold=1536 sprites=2048 workload=baseline selection=fixed cycle=1'
        for stdout in ('Other output', banner + '\n' + banner,
                       banner + ' selection=adaptive'):
            with self.subTest(stdout=stdout), self.assertRaises(RuntimeError):
                self.check(stdout)


if __name__ == '__main__':
    unittest.main()
