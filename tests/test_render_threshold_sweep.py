"""Regression checks for render worker threshold overrides and sweep analysis."""
from __future__ import annotations

import csv
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / 'tools' / 'sweep_render_threshold.py'
spec = importlib.util.spec_from_file_location('render_threshold_sweep', TOOL)
sweep = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sweep)


class RenderThresholdSweepTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('g++'), 'g++ unavailable')
    def test_default_threshold_and_override_share_two_gates(self):
        source = r'''
#include "RenderDiagnostics.h"
#include <cassert>
using namespace RenderDiagnostics;
int main() {
    assert(kParallelThreshold == 1536);
    assert(Select(true,true,3,383) == Path::BelowThreshold);
    assert(Select(true,true,3,1536) == Path::Parallel);
    assert(Select(true,true,3,64,1) == Path::Parallel);
    assert(Select(true,true,3,64,128) == Path::BelowThreshold);
    assert(Select(false,true,3,4096,1) == Path::Disabled);
    assert(Select(true,false,3,4096,1) == Path::Disabled);
    assert(Select(true,true,0,4096,1) == Path::NoWorkers);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            src = Path(temp) / 'threshold.cpp'; output = Path(temp) / 'threshold'
            src.write_text(source, encoding='utf-8')
            subprocess.run(['g++', '-std=c++20', '-I', str(ROOT / 'engine/include'),
                            str(src), '-o', str(output)], check=True, capture_output=True, timeout=30)
            subprocess.run([str(output)], check=True, timeout=10)

    def test_actual_render_preparation_uses_same_configured_threshold(self):
        engine = (ROOT / 'engine/src/Engine.cpp').read_text()
        render = (ROOT / 'engine/src/RenderManager.cpp').read_text()
        self.assertIn('mCfg.renderer.parallelMinCommands);', engine)
        self.assertIn('snapshot.cmds.size() >= minCommands', render)
        self.assertNotIn('snapshot.cmds.size() >= 384', render)
        self.assertIn('std::size_t parallelMinCommands = RenderDiagnostics::kParallelThreshold;',
                      (ROOT / 'engine/include/Engine.h').read_text())

    def test_equivalence_compares_each_frame_and_is_fail_closed(self):
        row = dict.fromkeys(sweep.VERIFY, '0')
        row['render_world_draws'] = '123'
        code, _ = sweep.compare_profiles([row.copy(), row.copy()], [row.copy(), row.copy()])
        self.assertEqual(code, 0)
        altered = row.copy(); altered['render_world_draws'] = '122'
        code, detail = sweep.compare_profiles([row], [altered])
        self.assertEqual(code, 1)
        self.assertIn('render_world_draws', detail)
        self.assertEqual(sweep.compare_profiles([row], [row,row])[0], 1)

    def test_aggregation_uses_paired_medians_and_reports_real_wins(self):
        rows = []
        for n, diffs in ((256,[-.1,-.05,-.03]), (512,[.03,.06,.09])):
            for i, delta in enumerate(diffs,1):
                rows.append(dict(sprites=n, repetition=i, immediate_p50_ms=2.0,
                                 parallel_p50_ms=2.0-delta, gain_pct=delta/2*100,
                                 render_eligible_median=n, render_worker_frames=360,
                                 immediate_p95_ms=3.0, parallel_p95_ms=3.0-delta))
        summary = sweep.aggregate(rows)
        self.assertEqual([int(r['wins']) for r in summary], [0,3])
        crosses = sweep.crossing(summary)
        self.assertEqual(len(crosses),1)
        self.assertTrue(256 < crosses[0]['interpolated_sprites'] < 512)
        self.assertIn('Exploratory', crosses[0]['note'])

    def test_invalid_arguments_do_not_build_or_run(self):
        with tempfile.TemporaryDirectory() as temp:
            self.assertEqual(sweep.main(['--counts','0','--output',temp]),2)
            self.assertEqual(sweep.main(['--counts','384','384','--output',temp]),2)
            self.assertEqual(sweep.main(['--counts','128','--render-threshold','0','--output',temp]),2)

    def test_candidate_requires_replicated_consecutive_wins(self):
        rows = []
        for sprites in (512,640):
            for i in range(3):
                rows.append(dict(sprites=sprites, immediate_p50_ms=2.0, parallel_p50_ms=1.9,
                                 immediate_p95_ms=3.0, parallel_p95_ms=2.9,
                                 gain_pct=5,render_eligible_median=sprites,render_worker_frames=360))
        candidate = sweep.recommendation(sweep.aggregate(rows))[0]
        self.assertEqual(candidate['status'],'candidate_only')
        self.assertEqual(candidate['suggested_eligible_commands'],512)
        self.assertEqual(sweep.recommendation(sweep.aggregate(rows[:3]))[0]['status'],'not_confirmed')

    def test_csv_roundtrip(self):
        with tempfile.TemporaryDirectory() as temp:
            f = Path(temp) / 'test.csv'
            sweep.write_csv(f, [{'sprites':384,'result':'ok'}])
            with f.open(newline='',encoding='utf-8') as handle:
                self.assertEqual(list(csv.DictReader(handle))[0]['result'], 'ok')

if __name__ == '__main__':
    unittest.main()
