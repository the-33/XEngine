"""Regression tests for v15 spatial context stability and one-index merge."""
from __future__ import annotations
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class SpatialContextAndMergeTests(unittest.TestCase):
    def test_spatial_classifier_hysteresis_and_prompt_phase_recovery(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if compiler is None:
            self.skipTest('C++20 compiler required')
        src = r'''
#include "SpatialContextClassifier.h"
#include <array>
#include <cassert>
int main() {
    SpatialContextClassifier context;
    // At occupancy 1 and effective-bin count 3 the original class can
    // alternate between 6 and 7 due to tiny object movement.
    assert(context.Observe(1.5,2.99,740,true)==6);
    for (int i=0;i<2000;++i)
        assert(context.Observe(1.5,(i%2) ? 3.04 : 2.96,740,true)==6);
    // A single strong excursion cannot start a new context calibration.
    assert(context.Observe(1.5,12.0,740,true)==6);
    assert(context.Observe(1.5,3.0,740,true)==6);
    // A real layout change is visible after the second stable observation.
    assert(context.Observe(1.5,12.0,740,true)==6);
    assert(context.Observe(1.5,12.0,740,true)==8);
    for(int i=0;i<200;++i)
        assert(context.Observe(1.5,(i%2) ? 8.1 : 7.9,740,true)==8);
    // Reverse transition should not retain the old layout indefinitely.
    assert(context.Observe(1.5,1.0,740,true)==8);
    assert(context.Observe(1.5,1.0,740,true)==6);
    // Another worker mode must not reuse the previous time-series label.
    assert(context.Observe(0.02,16.0,740,false)==2);
    // Restore the other worker mode: fresh classification after mode change.
    assert(context.Observe(1.5,1.0,740,true)==6);
    context.Reset();
    assert(context.Observe(0.2,4.0,520,true)==4);
    std::array<unsigned,16> uniform{}; uniform.fill(10);
    assert(SpatialContextClassifier::EffectiveBins(uniform.data(),16)==16.0);
    std::array<unsigned,16> concentrated{}; concentrated[5]=160;
    assert(SpatialContextClassifier::EffectiveBins(concentrated.data(),16)==1.0);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)
            (path/'test.cpp').write_text(src,encoding='utf-8')
            proc=subprocess.run([compiler,'-std=c++20','-O2','-Wall','-Wextra','-Werror',
                                 '-I'+str(ROOT/'engine/include'),str(path/'test.cpp'),'-o',str(path/'test')],
                                 capture_output=True,text=True,timeout=30)
            self.assertEqual(proc.returncode,0,proc.stderr)
            proc=subprocess.run([str(path/'test')],capture_output=True,text=True,timeout=15)
            self.assertEqual(proc.returncode,0,proc.stderr)

    def test_contact_map_is_single_membership_index(self):
        header=(ROOT/'engine/include/CollisionManager.h').read_text(encoding='utf-8')
        src=(ROOT/'engine/src/CollisionManager.cpp').read_text(encoding='utf-8')
        engine=(ROOT/'engine/src/Engine.cpp').read_text(encoding='utf-8')
        self.assertNotIn('mCurrPairs', header + src + engine)
        self.assertNotIn('mPrevPairs', header + src + engine)
        self.assertIn('mCurrInfo.insert_or_assign(',src)
        self.assertIn('mCurrInfo.reserve(contactCount);',src)
        self.assertIn('out.reserve(out.size() + contactCount);',src)
        self.assertIn('mPrevInfo.find(key)',src)
        self.assertIn('mCurrInfo.find(key)',src)
        self.assertIn('mSuppressPreviousPairMembership',src)
        self.assertIn('mSpatialContextClassifier.Observe(',src)
        self.assertGreaterEqual(src.count('mSpatialContextClassifier.Reset();'), 3)
        self.assertIn('collision->mCurrInfo.size()',engine)

if __name__ == '__main__': unittest.main()
