import importlib.util
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('xengine_build',ROOT/'tools'/'build.py')
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

class BuildToolTests(unittest.TestCase):
    def test_windows_dll_sync_resolves_transitive_imports_from_toolchain_only(self):
        with tempfile.TemporaryDirectory() as td:
            tmp = pathlib.Path(td)
            bin_dir = tmp / 'ucrt64' / 'bin'
            bin_dir.mkdir(parents=True)
            stage = tmp / 'debug' / 'Demo'
            stage.mkdir(parents=True)
            output = stage / 'Demo.exe'
            output.write_bytes(b'game')
            for dll in ('SDL2.dll', 'SDL2_image.dll', 'libpng16.dll'):
                (bin_dir / dll).write_bytes(('UCRT:' + dll).encode())
            other = tmp / 'oldmingw'
            other.mkdir()
            (other / 'SDL2.dll').write_bytes(b'wrong SDL')
            mappings = {
                'Demo.exe': 'DLL Name: SDL2.dll\nDLL Name: SDL2_image.dll\nDLL Name: KERNEL32.dll',
                'SDL2_image.dll': 'DLL Name: SDL2.dll\nDLL Name: libpng16.dll',
                'SDL2.dll': 'DLL Name: USER32.dll',
                'libpng16.dll': 'DLL Name: KERNEL32.dll',
            }
            def tool_path(name, alternative=None):
                return str(bin_dir / ('g++.exe' if name == 'g++' else 'objdump.exe'))
            def dump(argv, **_):
                return mappings[pathlib.Path(argv[-1]).name]
            with mock.patch.object(mod, 'exe', side_effect=tool_path), \
                 mock.patch.object(mod, 'command', side_effect=dump), \
                 mock.patch.dict('os.environ', {'PATH': str(other)}):
                installed = mod.package_windows_libs(stage, output)
            self.assertEqual(set(installed), {'SDL2.dll', 'SDL2_image.dll', 'libpng16.dll'})
            self.assertEqual((stage / 'SDL2.dll').read_bytes(), b'UCRT:SDL2.dll')
            self.assertFalse((stage / 'KERNEL32.dll').exists())

    def test_windows_dll_sync_rejects_mixed_toolchains(self):
        with tempfile.TemporaryDirectory() as td:
            tmp = pathlib.Path(td)
            compiler_bin = tmp / 'ucrt64' / 'bin'
            compiler_bin.mkdir(parents=True)
            exe = tmp / 'Game.exe'
            exe.write_bytes(b'game')
            with mock.patch.object(mod, 'exe', side_effect=lambda name, *_: str(compiler_bin / name)), \
                 mock.patch.object(mod, 'command', return_value='DLL Name: SDL2.dll'):
                with self.assertRaisesRegex(mod.BuildError, 'SDL2.dll'):
                    mod.package_windows_libs(tmp, exe)

    def test_inputmanager_cstring_explicit_include(self):
        # std::memcpy and std::memset must be declared by <cstring>.
        source = (ROOT / 'engine' / 'src' / 'InputManager.cpp').read_text(encoding='utf-8')
        self.assertIn('#include <cstring>', source)

    def test_batch_game_launchers_are_safe_with_spaces_in_paths(self):
        # %~dp0 ends with a backslash; passing it in quotes to Windows
        # Python can escape the closing quote and swallow --run.
        for path in (ROOT / 'games' / 'My Princess Run' / 'build.bat',
                     ROOT / 'tools' / 'templates' / 'BlankGame' / 'build.bat'):
            content = path.read_text(encoding='utf-8')
            self.assertNotIn('"%~dp0"', content)
            self.assertEqual(content.count('"%~dp0."'), 2)
            self.assertIn('game --project-dir "%~dp0." %*', content)

    def test_profiles(self):
        self.assertIn('-DXENGINE_RELEASE=1',mod.common_flags('windows','release'))
        self.assertIn('-DNDEBUG',mod.common_flags('linux','release'))
        self.assertIn('-DXENGINE_DEBUG=1',mod.common_flags('linux','debug'))
        self.assertNotIn('-DNDEBUG',mod.common_flags('linux','debug'))

    def test_reserved_defines(self):
        with self.assertRaises(mod.BuildError):
            mod.validate_defines(['XENGINE_DEBUG=1'])
        self.assertEqual(mod.validate_defines(['GAME_PARTICLES','MY_VALUE=2']),['-DGAME_PARTICLES','-DMY_VALUE=2'])

    def test_generate_release_without_sdl(self):
        with tempfile.TemporaryDirectory() as temp:
            root=pathlib.Path(temp)/'Demo'
            (root/'assets'/'config').mkdir(parents=True)
            config={'Engine':{'logStats':True},'Assets':{'assetsFolderPath':'bad'}}
            (root/'assets'/'config'/'engine.json').write_text(json.dumps(config))
            out=pathlib.Path(temp)/'output'
            generated=mod.generate_game_config(root,out,'release')
            src=generated.read_text()
            self.assertNotIn('"logStats"',src)
            self.assertIn('static constexpr uint8_t',src)
            if shutil.which('g++'):
                harness=out/'test_decode.cpp'
                harness.write_text('''#include "GameBuild.generated.h"
#include <iostream>
int main() { std::cout << XEngineGameBuild::EmbeddedEngineConfig(); }''')
                binary=out/'test_decode'
                subprocess.run(['g++','-std=c++20',str(generated),str(harness),'-I'+str(out/'generated'),'-o',str(binary)],check=True)
                decoded=json.loads(subprocess.check_output([str(binary)],text=True))
                self.assertFalse(decoded['Engine']['logStats'])
                self.assertEqual(decoded['Assets']['assetsFolderPath'],'./')

    def test_generate_debug_path(self):
        with tempfile.TemporaryDirectory() as temp:
            root=pathlib.Path(temp)/'Demo'
            (root/'assets'/'config').mkdir(parents=True)
            (root/'assets'/'config'/'engine.json').write_text('{}')
            generated=mod.generate_game_config(root,pathlib.Path(temp)/'out','debug')
            self.assertIn(str((root/'assets').resolve()),generated.read_text())

    def test_config_platform_allows_cross_compilation(self):
        different='windows' if sys.platform.startswith('linux') else 'linux'
        self.assertEqual(mod.resolve_platform(different,'auto'),different)

if __name__=='__main__':
    unittest.main()
