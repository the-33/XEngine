import pathlib
import shutil
import subprocess
import unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]

@unittest.skipUnless(shutil.which('g++'), 'g++ not installed')
class CppDefineTests(unittest.TestCase):
    def preprocess(self,*flags):
        return subprocess.run(['g++','-std=c++20','-fsyntax-only','-x','c++','-I'+str(ROOT/'engine'/'include'),*flags,'-'],
                              input='#include "BuildConfig.h"\n#include "Platform.h"\nstatic_assert(XEngineBuild::IsRelease == EXPECT_RELEASE);\n',text=True,capture_output=True)
    def test_debug(self):
        result=self.preprocess('-DXENGINE_DEBUG=1','-DXENGINE_TARGET_LINUX=1','-DEXPECT_RELEASE=0')
        self.assertEqual(result.returncode,0,result.stderr)
    def test_release(self):
        result=self.preprocess('-DXENGINE_RELEASE=1','-DNDEBUG','-DXENGINE_TARGET_LINUX=1','-DEXPECT_RELEASE=1')
        self.assertEqual(result.returncode,0,result.stderr)
    def test_invalid_both(self):
        result=self.preprocess('-DXENGINE_RELEASE=1','-DXENGINE_DEBUG=1','-DNDEBUG','-DEXPECT_RELEASE=1')
        self.assertNotEqual(result.returncode,0)
    def test_invalid_target(self):
        result=self.preprocess('-DXENGINE_DEBUG=1','-DXENGINE_TARGET_WINDOWS=1','-DEXPECT_RELEASE=0')
        self.assertNotEqual(result.returncode,0)
if __name__=='__main__':unittest.main()
