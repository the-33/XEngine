"""Compile/run the SDL-independent candidate generator against brute-force AABB overlap."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SpatialBroadphaseTests(unittest.TestCase):
    def test_candidate_superset_and_pair_order(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('No C++20 compiler')
        source = r'''
#include "SpatialHashBroadphase.h"
#include <cassert>
#include <random>
#include <vector>
#include <limits>
using namespace SpatialHashBroadphase;
static bool overlap(const Bounds& a,const Bounds& b) {
    return a.minX<=b.maxX && b.minX<=a.maxX && a.minY<=b.maxY && b.minY<=a.maxY;
}
int main(){
 std::mt19937 rng(12345); std::uniform_real_distribution<double> p(-50,50),w(0.01,7);
 for (double cell: {0.05,0.5,2.0,10.0,128.0}) {
  std::vector<Bounds> v;
  for (int i=0;i<700;++i) {
   double x=p(rng),y=p(rng),dx=w(rng),dy=w(rng);
   v.push_back({x,y,x+dx,y+dy});
  }
  v.push_back({-1000,-1000,1000,1000});  // oversized/global
  v.push_back({-2,-2,0,0});
  v.push_back({0,0,2,2}); // touching cell/AABB boundary
  v.push_back({-0.0,-0.0,0.0,0.0});
  v.push_back({std::numeric_limits<double>::quiet_NaN(),0,5,5});
  Grid grid(cell);grid.Build(v);
  std::vector<std::size_t> ids;
  for (std::size_t i=0;i<v.size();++i) {
   grid.Candidates(i,v.size(),ids);
   for(std::size_t k=1;k<ids.size();++k)assert(ids[k-1]<ids[k]);
   for(std::size_t j=i+1;j<v.size();++j) {
    if (overlap(v[i],v[j])) {
      assert(std::binary_search(ids.begin(),ids.end(),j));
    }
   }
  }
 }
}
'''
        with tempfile.TemporaryDirectory() as d:
            src = Path(d) / 'grid.cpp'
            exe = Path(d) / 'grid'
            src.write_text(source)
            subprocess.run([compiler, '-std=c++20', '-O2', '-Wall', '-Wextra',
                            '-Werror', '-I', str(ROOT / 'engine/include'), str(src), '-o', str(exe)],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(exe)], check=True, capture_output=True, text=True)

    def test_benchmark_runner_has_equivalence_checks(self):
        import sys
        sys.path.insert(0, str(ROOT / 'tools'))
        import run_benchmark
        with tempfile.TemporaryDirectory() as d:
            from csv import DictWriter
            a, b = Path(d)/'a.csv', Path(d)/'b.csv'
            row = dict(collision_contact_signature='12345678901234', contacts_built='5',
                       narrowphase_tests='6', fixed_steps='1', render_world_draws='100')
            for path in (a,b):
                with path.open('w', newline='') as f:
                    writer=DictWriter(f, fieldnames=list(row)); writer.writeheader(); writer.writerow(row)
            self.assertEqual(run_benchmark.compare_broadphase_profiles(a,b)[0],0)
            with b.open('w', newline='') as f:
                writer=DictWriter(f, fieldnames=list(row)); writer.writeheader();writer.writerow({**row,'contacts_built':'4'})
            self.assertEqual(run_benchmark.compare_broadphase_profiles(a,b)[0],1)


if __name__ == '__main__': unittest.main()
