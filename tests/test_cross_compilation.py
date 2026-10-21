"""Target-host separation, toolchain selection and cross packaging (without SDL installations)."""
from __future__ import annotations
import importlib.util
import pathlib
import os
import tempfile
import unittest
from unittest import mock

ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('cross_build',ROOT/'tools/build.py')
b=importlib.util.module_from_spec(spec); spec.loader.exec_module(b)

class CrossBuildTests(unittest.TestCase):
    def test_config_priority_and_cross_resolution(self):
        different = 'windows' if b.host_platform() == 'linux' else 'linux'
        self.assertEqual(b.resolve_platform(None,different),different)
        self.assertEqual(b.resolve_platform('auto',different),b.host_platform())
        self.assertEqual(b.resolve_platform('linux','windows'),'linux')

    def test_cross_requires_explicit_toolchain(self):
        target = 'windows' if b.host_platform() == 'linux' else 'linux'
        with mock.patch.object(b,'_local_toolchain_config',return_value={}):
            with self.assertRaisesRegex(b.BuildError,'cross-toolchain'):
                b.prepare_tools(target)

    def test_target_pkg_config_does_not_use_host_pc(self):
        target = 'windows' if b.host_platform() == 'linux' else 'linux'
        with tempfile.TemporaryDirectory() as td:
            root=pathlib.Path(td)
            settings={'cxx':'cross-g++','ar':'cross-ar','pkg_config':'cross-pkg-config',
                      'pkg_config_libdir':str(root/'target-pc'),'sysroot':str(root),
                      'lib_dirs':[str(root/'lib')], 'dll_dirs':[str(root/'bin')]}
            seen=[]
            def subprocess_fake(argv,*,env,**kwargs):
                self.assertEqual(env['PKG_CONFIG_LIBDIR'],str(root/'target-pc'))
                self.assertNotIn('PKG_CONFIG_PATH',env)
                seen.append(argv)
                return mock.Mock(returncode=0, stdout='-I/target/include\n' if '--cflags' in argv else '-L/target/lib -lSDL2\n',stderr='')
            def tool_fake(name,required=True): return '/fake/'+name if name else None
            with mock.patch.object(b,'_local_toolchain_config',return_value=settings), \
                 mock.patch.object(b,'_tool',side_effect=tool_fake), \
                 mock.patch.object(b,'command',side_effect=lambda argv, **_: ('x86_64-w64-mingw32' if target=='windows' else 'x86_64-linux-gnu') if '-dumpmachine' in argv else 'cross-g++ 1.0'), \
                 mock.patch.object(b.subprocess,'run',side_effect=subprocess_fake), \
                 mock.patch.dict(os.environ,{'PKG_CONFIG_PATH':'/host/sdl2-pc'}):
                tc=b.prepare_tools(target)
            self.assertTrue(tc.cross)
            self.assertEqual(tc.compiler,'/fake/cross-g++')
            self.assertIn('-I/target/include',tc.pkg_cflags)
            self.assertEqual(len(seen),2)
            if target == 'linux':
                self.assertIn('--sysroot='+str(root.resolve()),tc.pkg_cflags)

    def test_windows_to_linux_pkgconf_keeps_explicit_multiarch_prefix(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            settings = {'cxx': 'clang++', 'ar': 'llvm-ar',
                        'pkg_config': 'pkg-config',
                        'pkg_config_libdir': str(root / 'usr/lib/x86_64-linux-gnu/pkgconfig'),
                        'pkg_config_sysroot_dir': '', 'sysroot': str(root),
                        'cxxflags': ['--target=x86_64-linux-gnu']}
            seen = []
            def fake_pc(argv, *, env, **kwargs):
                seen.append(env)
                self.assertEqual(env.get('PKG_CONFIG_DONT_DEFINE_PREFIX'), '1')
                self.assertEqual(env.get('PKG_CONFIG_DONT_RELOCATE_PATHS'), '1')
                self.assertEqual(env.get('PKG_CONFIG_SYSROOT_DIR'), '')
                return mock.Mock(returncode=0, stdout='-I' + str(root / 'usr/include/SDL2') if '--cflags' in argv else '-lSDL2', stderr='')
            def fake_command(argv, **kwargs):
                return 'x86_64-unknown-linux-gnu' if '-dumpmachine' in argv else 'clang version 23.1.3'
            with mock.patch.object(b, 'host_platform', return_value='windows'), \
                 mock.patch.object(b, '_local_toolchain_config', return_value=settings), \
                 mock.patch.object(b, '_tool', side_effect=lambda n, required=True: n), \
                 mock.patch.object(b, 'command', side_effect=fake_command), \
                 mock.patch.object(b.subprocess, 'run', side_effect=fake_pc):
                tc = b.prepare_tools('linux')
            self.assertEqual(len(seen), 2)
            self.assertIn('-I' + str(root / 'usr/include/SDL2'), tc.pkg_cflags)
            self.assertFalse(any('usr/lib/include/SDL2' in f for f in tc.pkg_cflags))

    def test_linux_desktop_cross_is_not_executed_on_windows(self):
        with tempfile.TemporaryDirectory() as td:
            root=pathlib.Path(td)
            game=root/'Game'
            (game/'assets').mkdir(parents=True)
            (game/'config.json').write_text('{"project":{"name":"Game"}}')
            binary=root/'game.bin'; binary.write_bytes(b'ELF')
            tc=b.Toolchain('/fake/c++','/fake/ar','/fake/pkg-config',[],[], 'v',cross=True,
                           sysroot=root,readelf='/fake/readelf')
            def package_fake(stage,output,tooling):
                b.write_linux_run(stage,output.name)
            with mock.patch.object(b,'package_linux_libs_cross',side_effect=package_fake), \
                 mock.patch.object(b,'command',side_effect=AssertionError('must not execute Linux scripts')):
                result=b.package_game(game,'Game','0.1.0','linux','x86_64',binary,tooling=tc)
            self.assertTrue((result/'setup_desktop.sh').exists())
            self.assertTrue((result/'run.sh').exists())
            self.assertFalse((result/'Game.desktop').exists())

    def test_linux_cross_resolves_elf_from_target_sysroot_only(self):
        with tempfile.TemporaryDirectory() as td:
            root=pathlib.Path(td)
            exe=root/'game'; exe.write_bytes(b'elf')
            (root/'usr/lib').mkdir(parents=True)
            (root/'usr/lib/libSDL2.so').write_bytes(b'so')
            stage=root/'dist'; stage.mkdir()
            tc=b.Toolchain('cross-c++','cross-ar','cross-pkg',[],[], 'v',readelf='readelf',sysroot=root,cross=True)
            inspected=[]
            def fake_command(argv,**kwargs):
                inspected.append(argv)
                if pathlib.Path(argv[-1]).name == 'game':
                    return '0x0000000000000001 (NEEDED) Shared library: [libSDL2.so]\n'
                return ''
            with mock.patch.object(b,'command',side_effect=fake_command):
                b.package_linux_libs_cross(stage,exe,tc)
            self.assertEqual((stage/'lib/libSDL2.so').read_bytes(),b'so')
            self.assertEqual([a[0] for a in inspected],['readelf','readelf'])
            self.assertIn('exec "$APP"', (stage/'run.sh').read_text())

    def test_cross_windows_uses_target_windres_and_objdump(self):
        with tempfile.TemporaryDirectory() as td:
            root=pathlib.Path(td)
            stage=root/'dist'; stage.mkdir()
            exe=stage/'Game.exe'; exe.write_bytes(b'bin')
            dll_dir=root/'targetdll';dll_dir.mkdir()
            (dll_dir/'SDL2.dll').write_bytes(b'sdl')
            tc=b.Toolchain(str(root/'targetgcc/g++.exe'),str(root/'targetgcc/ar.exe'), 'pkg',[],[], 'v',
                           windres='cross-windres',objdump='cross-objdump',dll_dirs=(dll_dir,),cross=True)
            def fake_command(argv,**kwargs):
                return 'DLL Name: SDL2.dll\n' if pathlib.Path(argv[-1]).suffix.lower()=='.exe' else ''
            with mock.patch.object(b,'command',side_effect=fake_command):
                copied=b.package_windows_libs(stage,exe,tooling=tc)
            self.assertEqual(copied,['SDL2.dll'])
            self.assertEqual((stage/'SDL2.dll').read_bytes(),b'sdl')

    def test_main_cli_override_json_and_run_protection(self):
        from contextlib import redirect_stdout, redirect_stderr
        from io import StringIO
        import sys
        different = 'windows' if b.host_platform() == 'linux' else 'linux'
        game = ROOT / 'games' / "Blob's Quest"
        cfg={'project':{'name':game.name}, 'build':{'platform':different,'configuration':'Release'}}
        tc=b.Toolchain('compiler','archiver','pkg',[],[],'version',cross=True)
        fake_build=mock.Mock()
        with mock.patch.object(b,'json_file',return_value=cfg), \
             mock.patch.object(b,'prepare_tools',return_value=tc), \
             mock.patch.object(b,'game_build',fake_build), \
             mock.patch.object(b,'ROOT',ROOT), \
             mock.patch.object(sys,'argv',['build.py','game','--project',game.name]), \
             redirect_stdout(StringIO()), redirect_stderr(StringIO()):
            self.assertEqual(b.main(),0)
        self.assertEqual(fake_build.call_args.kwargs['target'], different)
        self.assertEqual(fake_build.call_args.kwargs['profile'], 'release')
        fake_build.reset_mock()
        with mock.patch.object(b,'json_file',return_value=cfg), \
             mock.patch.object(b,'prepare_tools',return_value=tc), \
             mock.patch.object(b,'game_build',fake_build), \
             mock.patch.object(b,'ROOT',ROOT), \
             mock.patch.object(sys,'argv',['build.py','game','--project',game.name,'--platform','auto']), \
             redirect_stdout(StringIO()), redirect_stderr(StringIO()):
            self.assertEqual(b.main(),0)
        self.assertEqual(fake_build.call_args.kwargs['target'],b.host_platform())
        fake_build.reset_mock()
        with mock.patch.object(b,'json_file',return_value=cfg), \
             mock.patch.object(b,'game_build',fake_build), \
             mock.patch.object(b,'ROOT',ROOT), \
             mock.patch.object(sys,'argv',['build.py','game','--project',game.name,'--run']), \
             redirect_stdout(StringIO()), redirect_stderr(StringIO()):
            self.assertEqual(b.main(),1)
        fake_build.assert_not_called()

    def test_reject_mismatched_compiler_target(self):
        target = 'windows' if b.host_platform() == 'linux' else 'linux'
        with tempfile.TemporaryDirectory() as td:
            settings={'cxx':'g++', 'ar':'ar', 'pkg_config':'pkg-config', 'pkg_config_libdir':td,'sysroot':td}
            with mock.patch.object(b,'_local_toolchain_config',return_value=settings), \
                 mock.patch.object(b,'_tool',side_effect=lambda name, required=True:name), \
                 mock.patch.object(b,'command',side_effect=lambda argv, **_: 'x86_64-linux-gnu' if '-dumpmachine' in argv else 'version'):
                if target == 'windows':
                    with self.assertRaisesRegex(b.BuildError, 'Wrong target compiler'):
                        b.prepare_tools(target)

if __name__=='__main__': unittest.main()

class LinuxPrivateDependencyPackagingTests(unittest.TestCase):
    """Debian multiarch libraries may live below a private vendor directory."""

    def test_private_pulseaudio_lib_is_packaged_transitively(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            multiarch = root / 'usr/lib/x86_64-linux-gnu'
            private = multiarch / 'pulseaudio'
            private.mkdir(parents=True)
            (multiarch / 'libpulse.so.0').write_bytes(b'pulse')
            (private / 'libpulsecommon-16.1.so').write_bytes(b'pulsecommon')
            (multiarch / 'libsystemd.so.0').write_bytes(b'systemd')
            game = root / 'BlobQuest'
            game.write_bytes(b'elf')
            stage = root / 'release'
            stage.mkdir()
            tc = b.Toolchain('clang++', 'llvm-ar', 'pkg-config', [], [], 'v',
                             readelf='llvm-readelf', sysroot=root, cross=True,
                             lib_dirs=(multiarch,))
            inspections = []

            def fake_readelf(argv, **kwargs):
                filename = pathlib.Path(argv[-1]).name
                inspections.append(filename)
                dep = {
                    'BlobQuest': ['libpulse.so.0'],
                    'libpulse.so.0': ['libpulsecommon-16.1.so'],
                    'libpulsecommon-16.1.so': ['libsystemd.so.0'],
                    'libsystemd.so.0': ['libc.so.6'],
                }[filename]
                return ''.join(f' 0x00000001 (NEEDED) Shared library: [{n}]\n' for n in dep)

            with mock.patch.object(b, 'command', side_effect=fake_readelf):
                b.package_linux_libs_cross(stage, game, tc)
            self.assertEqual((stage / 'lib/libpulse.so.0').read_bytes(), b'pulse')
            self.assertEqual((stage / 'lib/libpulsecommon-16.1.so').read_bytes(), b'pulsecommon')
            self.assertEqual((stage / 'lib/libsystemd.so.0').read_bytes(), b'systemd')
            self.assertEqual(set(inspections), {'BlobQuest', 'libpulse.so.0',
                                                 'libpulsecommon-16.1.so', 'libsystemd.so.0'})
            self.assertIn('LD_LIBRARY_PATH', (stage / 'run.sh').read_text())

    def test_direct_library_directory_takes_precedence(self):
        with tempfile.TemporaryDirectory() as td:
            base = pathlib.Path(td)
            root = base / 'usr/lib/x86_64-linux-gnu'
            sub = root / 'plugins'
            sub.mkdir(parents=True)
            (root / 'libexample.so').write_bytes(b'direct')
            (sub / 'libexample.so').write_bytes(b'nested')
            chosen = b.locate_linux_target_library('libexample.so', [root])
            self.assertEqual(chosen.read_bytes(), b'direct')

    def test_ambiguous_nested_target_libraries_fail(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            for name in ('vendorA', 'vendorB'):
                sub = root / name
                sub.mkdir()
                (sub / 'libexample.so').write_bytes(name.encode())
            with self.assertRaisesRegex(b.BuildError, 'Ambiguous target Linux dependency'):
                b.locate_linux_target_library('libexample.so', [root])

    def test_missing_linux_library_fails_without_host_lookup(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            (root / 'usr/lib/x86_64-linux-gnu').mkdir(parents=True)
            self.assertIsNone(b.locate_linux_target_library('libnothere.so',
                                  [root / 'usr/lib/x86_64-linux-gnu']))

    def test_invalid_needed_name_rejected(self):
        with self.assertRaisesRegex(b.BuildError, 'Invalid Linux DT_NEEDED'):
            b.locate_linux_target_library('../../host.dll', [])
