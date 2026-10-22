"""CPU-only proofs for visibility fast path, conservative culling and selector parity."""
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class RenderVisibilityFastPathTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('g++'), 'g++ unavailable')
    def test_cached_bounds_and_saturation_keep_exact_selection(self):
        source = r'''
#include "RenderDiagnostics.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <random>
using namespace RenderDiagnostics;

static bool original(const View& v, float x, float y, float w, float h,
                     float sx, float sy, float ox, float oy, float px, float py) {
    if (!(std::isfinite(x) && std::isfinite(y) && std::isfinite(w) &&
          std::isfinite(h) && std::isfinite(sx) && std::isfinite(sy) &&
          std::isfinite(ox) && std::isfinite(oy) &&
          std::isfinite(px) && std::isfinite(py) &&
          std::isfinite(v.centerX) && std::isfinite(v.centerY) &&
          std::isfinite(v.width) && std::isfinite(v.height) &&
          v.width > 0.f && v.height > 0.f)) return false;
    const float extent = std::fabs(w * sx) *
            std::max(std::fabs(px), std::fabs(1.f - px)) +
        std::fabs(h * sy) * std::max(std::fabs(py), std::fabs(1.f - py)) +
        std::fabs(ox * sx) + std::fabs(oy * sy);
    if (!std::isfinite(extent)) return false;
    return x + extent < v.centerX - v.width * .5f ||
           x - extent > v.centerX + v.width * .5f ||
           y + extent < v.centerY - v.height * .5f ||
           y - extent > v.centerY + v.height * .5f;
}

int main() {
    static_assert(EntryBoundary(1536) == 1536 * 256);
    static_assert(CappedEffectiveSum(393200, 1000, 393216) == 393216);
    static_assert(CappedEffectiveSum(393216, 1000, 393216) == 393216);
    static_assert(CappedEffectiveSum(0, 256, 0) == 0);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const View views[] = {{0,0,22,12.375f}, {5,-8,160,90},
                          {0,0,0,12}, {nan,0,20,12}};
    std::mt19937 rng(1613);
    std::uniform_real_distribution<float> positions(-75.f,75.f);
    std::uniform_real_distribution<float> transforms(-4.f,4.f);
    for (const auto& view : views) {
        const auto bounds = MakeViewBounds(view);
        for (int i=0; i<25000; ++i) {
            float x = positions(rng), y = positions(rng);
            if (i % 7 == 0) { x=0; y=0; } // Common visible-anchor fast path
            const float w = transforms(rng), h = transforms(rng);
            const float sx = transforms(rng), sy = transforms(rng);
            const float ox = transforms(rng), oy = transforms(rng);
            const float px = transforms(rng), py = transforms(rng);
            const bool expected = original(view, x,y,w,h,sx,sy,ox,oy,px,py);
            assert(DefinitelyOffscreenSprite(bounds,x,y,w,h,sx,sy,ox,oy,px,py) == expected);
            assert(DefinitelyOffscreenSprite(view,x,y,w,h,sx,sy,ox,oy,px,py) == expected);
        }
        // Common culling workload: centered sprites with no offset, including
        // negative zero, reflected scales and world positions outside view.
        for (int i=0; i<25000; ++i) {
            const float x = positions(rng), y = positions(rng);
            const float w = transforms(rng), h = transforms(rng);
            const float sx = transforms(rng), sy = transforms(rng);
            const float ox = i%2 ? 0.f : -0.f;
            const float oy = i%3 ? -0.f : 0.f;
            const bool expected = original(view,x,y,w,h,sx,sy,ox,oy,.5f,.5f);
            assert(DefinitelyOffscreenSprite(bounds,x,y,w,h,sx,sy,ox,oy,.5f,.5f) == expected);
        }
    }
    const auto bounds=MakeViewBounds(views[0]);
    assert(!DefinitelyOffscreenSprite(bounds,0,0,nan,1,1,1,0,0,.5,.5));
    assert(!DefinitelyOffscreenSprite(bounds,nan,0,1,1,1,1,0,0,.5,.5));
    assert(!DefinitelyOffscreenSprite(bounds,100,100,nan,1,1,1,0,0,.5,.5));
    assert(!DefinitelyOffscreenSprite(bounds,100,100,1,1,
        std::numeric_limits<float>::infinity(),1,0,0,.5,.5));
    assert(!DefinitelyOffscreenSprite(bounds,100,100,
        std::numeric_limits<float>::max(),1,2,1,0,0,.5,.5));

    for (std::size_t threshold : {0u, 1u, 384u, 1024u, 1536u, 2048u, 4096u}) {
        for (int seed=0; seed<30; ++seed) {
            std::mt19937 gen(seed);
            std::uint64_t exact=0, capped=0;
            const auto boundary=EntryBoundary(threshold);
            unsigned skipped=0;
            for (int i=0;i<3000;++i) {
                const auto units=SpriteUnits((gen()%2)!=0, (gen()%3)!=0, (gen()%5)!=0);
                const bool offscreen=(gen()%4)==0;
                exact+=EffectiveUnits(units,offscreen);
                if (capped>=boundary) ++skipped;
                // Skipped scoring adds any nonnegative units; the cap makes
                // the result route-equivalent even when visibility is unknown.
                const auto effective=(capped>=boundary) ? units :
                    EffectiveUnits(units,offscreen);
                capped=CappedEffectiveSum(capped,effective,boundary);
                assert(capped==std::min(exact,boundary));
                for (bool previousParallel : {false,true}) {
                    assert(SelectAdaptive(true,true,3,exact,threshold,previousParallel)==
                           SelectAdaptive(true,true,3,capped,threshold,previousParallel));
                }
            }
            if (threshold==0) assert(skipped==3000);
        }
    }
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            code = directory / 'visibility.cpp'
            binary = directory / 'visibility'
            code.write_text(source, encoding='utf-8')
            subprocess.run(['g++', '-std=c++20', '-O2', '-I', str(ROOT / 'engine/include'),
                            str(code), '-o', str(binary)], check=True,
                           capture_output=True, timeout=35)
            subprocess.run([str(binary)], check=True, timeout=15)

    def test_engine_wiring_and_probe_isolation(self):
        engine = (ROOT / 'engine/src/Engine.cpp').read_text(encoding='utf-8')
        render = (ROOT / 'engine/src/RenderManager.cpp').read_text(encoding='utf-8')
        header = (ROOT / 'engine/include/RenderManager.h').read_text(encoding='utf-8')
        probe = (ROOT / 'tools/sweep_render_selector_cost.py').read_text(encoding='utf-8')
        assert 'mWorkEstimateProbe == RenderDiagnostics::WorkEstimate::Auto' in engine
        assert 'render_visibility_skipped_sprites' in engine
        assert 'mSkippedVisibilitySprites = 0;' in engine
        assert 'MakeViewBounds(' in render
        assert 'NeedsVisibilityEstimate_()' in render
        assert 'CappedEffectiveSum(' in header
        assert 'forced scoring probe skipped visibility' in probe


if __name__ == '__main__':
    unittest.main()
