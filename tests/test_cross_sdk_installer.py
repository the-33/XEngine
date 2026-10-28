"""SDK installer: offline validation of tools, archives, security and config merging."""
from __future__ import annotations

import io
import importlib.util
import json
import lzma
import hashlib
from pathlib import Path
import tarfile
import tempfile
import unittest
import sys
from types import SimpleNamespace
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('setup_cross_sdk', ROOT / 'tools/setup_cross_sdk.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


class CrossSdkInstallerTests(unittest.TestCase):
    def test_default_sdk_paths_and_legacy_reuse(self):
        user = Path('/test-account')
        windows, legacy = sdk.sdk_locations(
            'windows', 'linux', environ={'LOCALAPPDATA': 'C:/Users/example/AppData/Local'},
            home=user)
        self.assertEqual(windows, Path('C:/Users/example/AppData/Local/XEngine/sdk/linux-x86_64'))
        self.assertEqual(legacy, user / '.xengine/sdk/linux-x86_64')
        linux, _ = sdk.sdk_locations('linux', 'windows', environ={}, home=user)
        self.assertEqual(linux, user / '.local/share/xengine/sdk/windows-x86_64')
        xdg, _ = sdk.sdk_locations('linux', 'windows',
                                   environ={'XDG_DATA_HOME': '/custom/data'}, home=user)
        self.assertEqual(xdg, Path('/custom/data/xengine/sdk/windows-x86_64'))
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            target = base / 'new'
            previous = base / 'legacy'
            with mock.patch.object(sdk, 'sdk_locations', return_value=(target, previous)):
                self.assertEqual(sdk.select_sdk_location('windows', 'linux')[0], target)
                previous.mkdir()
                chosen, reason = sdk.select_sdk_location('windows', 'linux')
                self.assertEqual(chosen, previous)
                self.assertIn('legacy', reason)
                target.mkdir()
                self.assertEqual(sdk.select_sdk_location('windows', 'linux')[0], target)
                self.assertEqual(sdk.select_sdk_location('windows', 'linux',
                                                        custom=base / 'my-sdk')[0], base / 'my-sdk')

    def test_sdk_migration_copies_and_updates_only_target_paths(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            old, new = base / 'legacy-sdk', base / 'new-sdk'
            (old / 'lib/pkgconfig').mkdir(parents=True)
            (old / 'include/SDL2').mkdir(parents=True)
            (old / 'include/SDL2/SDL.h').write_bytes(b'header')
            (old / 'lib/pkgconfig/sdl2.pc').write_text(
                f'prefix={old.as_posix()}\nlibdir={old.as_posix()}/lib\n', encoding='utf-8')
            (old / '.downloads').mkdir()
            (old / '.downloads/cached.deb').write_bytes(b'cache')
            project_root = base / 'project'
            (project_root / 'tools').mkdir(parents=True)
            conf = project_root / 'tools/toolchains.local.json'
            conf.write_text(json.dumps({
                'linux': {'windows': {
                    'pkg_config_libdir': f'{old}/lib/pkgconfig',
                    'cxxflags': ['-I' + str(old / 'include')],
                    'custom': 'keep-me'}},
                'windows': {'linux': {'sysroot': '/unrelated'}},
            }), encoding='utf-8')
            with mock.patch.object(sdk, 'ROOT', project_root), \
                 mock.patch.object(sdk, 'sdk_locations', return_value=(new, old)):
                destination = sdk.migrate_legacy_sdk('linux', 'windows')
            self.assertEqual(destination, new)
            self.assertTrue(old.is_dir())
            self.assertEqual((old / 'lib/pkgconfig/sdl2.pc').read_text().splitlines()[0],
                             f'prefix={old.as_posix()}')
            self.assertEqual((new / 'lib/pkgconfig/sdl2.pc').read_text().splitlines()[0],
                             f'prefix={new.as_posix()}')
            self.assertEqual((new / '.downloads/cached.deb').read_bytes(), b'cache')
            entry = json.loads(conf.read_text())
            self.assertEqual(entry['linux']['windows']['pkg_config_libdir'], f'{new}/lib/pkgconfig')
            self.assertEqual(entry['linux']['windows']['cxxflags'], ['-I' + str(new / 'include')])
            self.assertEqual(entry['linux']['windows']['custom'], 'keep-me')
            self.assertEqual(entry['windows']['linux']['sysroot'], '/unrelated')
            self.assertEqual(len(list((project_root / 'tools').glob('toolchains.local.json.*.bak'))), 1)

    def test_migration_never_overwrites_or_downloads(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            old, new = base / 'old', base / 'new'
            old.mkdir()
            new.mkdir()
            with mock.patch.object(sdk, 'sdk_locations', return_value=(new, old)), \
                 mock.patch.object(sdk, 'download', side_effect=AssertionError('download')):
                with self.assertRaisesRegex(sdk.SetupError, 'Refusing to overwrite'):
                    sdk.migrate_legacy_sdk('windows', 'linux')
        self.assertTrue(sdk.parse_args(['--migrate-sdk']).migrate_sdk)
        with mock.patch.object(sdk, 'host_os', return_value='windows'), \
             mock.patch.object(sdk, 'migrate_legacy_sdk') as migrate:
            self.assertEqual(sdk.main(['--target', 'linux', '--migrate-sdk']), 0)
            migrate.assert_called_once_with('windows', 'linux')
            self.assertEqual(sdk.main(['--target', 'linux', '--migrate-sdk', '--force']), 1)
            self.assertEqual(migrate.call_count, 1)

    def test_cli_and_home_directory_defaults(self):
        self.assertEqual(sdk.parse_args([]).target, 'auto')
        self.assertEqual(sdk.parse_args(['--target', 'all']).target, 'all')
        self.assertEqual(str(sdk.parse_args(['--target', 'linux', '--sdk-dir', '~/sdk']).sdk_dir), '~/sdk')

    def test_no_game_lookup_in_installer(self):
        code = (ROOT / 'tools/setup_cross_sdk.py').read_text(encoding='utf-8')
        self.assertNotIn('games/', code)
        self.assertNotIn('config.json', code)
        self.assertNotIn('subprocess.run(["wsl"', code)

    def test_dependency_parser_alternatives_and_versioned_entries(self):
        self.assertEqual(sdk.debian_dependencies('libfoo (>= 2), bar:any | baz [amd64], qux'),
                         [('libfoo',), ('bar', 'baz'), ('qux',)])
        records = {
            'main': {'Package': 'main', 'Depends': 'foo (>= 1), bar | baz'},
            'foo': {'Package': 'foo', 'Pre-Depends': 'base'},
            'base': {'Package': 'base'},
            'baz': {'Package': 'baz'},
        }
        names = [r['Package'] for r in sdk.resolve_debian_packages(records, ('main',))]
        self.assertEqual(set(names), {'main', 'foo', 'base', 'baz'})
        with self.assertRaises(sdk.SetupError):
            sdk.resolve_debian_packages(records, ('absent',))

    def test_debian_virtual_provides_fluidsynth_sndfile_dependency(self):
        # Debian 12: libfluidsynth-dev requires a virtual name supplied by libsndfile1-dev.
        records = {
            'libfluidsynth-dev': {'Package': 'libfluidsynth-dev', 'Depends': 'libsndfile-dev'},
            'libsndfile1-dev': {'Package': 'libsndfile1-dev', 'Provides': 'libsndfile-dev',
                                'Depends': 'libsndfile1 (= 1.2.0)'},
            'libsndfile1': {'Package': 'libsndfile1'},
        }
        with mock.patch.object(sdk, 'info') as log:
            names = [r['Package'] for r in sdk.resolve_debian_packages(records, ('libfluidsynth-dev',))]
        self.assertEqual(set(names), {'libfluidsynth-dev', 'libsndfile1-dev', 'libsndfile1'})
        self.assertTrue(any('libsndfile-dev -> libsndfile1-dev' in c.args[0] for c in log.call_args_list))

    def test_virtual_provider_selection_is_reused_and_deterministic(self):
        records = {
            'main': {'Package': 'main', 'Depends': 'virtual-dev, another-dev'},
            'libalternative-dev': {'Package': 'libalternative-dev', 'Provides': 'virtual-dev'},
            'libpreferred-dev': {'Package': 'libpreferred-dev', 'Provides': 'virtual-dev, another-dev (= 1)'},
            'alternate-provider': {'Package': 'alternate-provider', 'Provides': 'another-dev'},
        }
        # Alphabetical first provider for virtual-dev is libalternative-dev;
        # the other virtual package must also have a real provider.
        a = [r['Package'] for r in sdk.resolve_debian_packages(records, ('main',))]
        b = [r['Package'] for r in sdk.resolve_debian_packages(records, ('main',))]
        self.assertEqual(a, b)
        self.assertIn('libalternative-dev', a)
        self.assertIn('alternate-provider', a)
        self.assertNotIn('libpreferred-dev', a)

    def test_absent_virtual_dependency_does_not_silently_skip(self):
        records = {'main': {'Package': 'main', 'Depends': 'does-not-exist'}}
        with self.assertRaisesRegex(sdk.SetupError, 'does-not-exist'):
            sdk.resolve_debian_packages(records, ('main',))

    def test_parsing_package_index(self):
        text = '''Package: one\nArchitecture: amd64\nFilename: pool/one.deb\nDepends: foo,\n bar | baz\n\nPackage: two\nArchitecture: all\nFilename: pool/two.deb\n\n'''
        parsed = sdk.debian_package_records(text)
        self.assertEqual(parsed['one']['Depends'], 'foo, bar | baz')
        self.assertEqual(set(parsed), {'one', 'two'})

    def test_rejects_traversal_and_absolute_paths(self):
        for unsafe in ('../../etc/shadow', '/etc/hosts', 'a/../b', 'C:\\evil'):
            with self.subTest(unsafe=unsafe), self.assertRaises(sdk.SetupError):
                sdk.safe_subpath(unsafe)
        self.assertEqual(sdk.safe_subpath('bin/SDL2.dll', merged_usr=False), Path('bin/SDL2.dll'))
        self.assertEqual(sdk.safe_subpath('./lib/x86_64-linux-gnu/libc.so'),
                         Path('usr/lib/x86_64-linux-gnu/libc.so'))

    def test_deb_ar_extraction_and_symlink_materialization(self):
        payload = io.BytesIO()
        with tarfile.open(fileobj=payload, mode='w:xz') as tar:
            data = b'hello!'
            info = tarfile.TarInfo('./usr/lib/x86_64-linux-gnu/libfoo.so.1')
            info.size = len(data)
            tar.addfile(info, io.BytesIO(data))
            alias = tarfile.TarInfo('./usr/lib/x86_64-linux-gnu/libfoo.so')
            alias.type = tarfile.SYMTYPE
            alias.linkname = 'libfoo.so.1'
            tar.addfile(alias)
        name = b'data.tar.xz/'
        archive = payload.getvalue()
        header = name.ljust(16, b' ') + b'0'.ljust(12, b' ') + b'0'.ljust(6, b' ') + b'0'.ljust(6, b' ') + b'100644'.ljust(8, b' ') + str(len(archive)).encode().ljust(10, b' ') + b'`\n'
        with tempfile.TemporaryDirectory() as temp:
            deb = Path(temp) / 'test.deb'
            deb.write_bytes(b'!<arch>\n' + header + archive + (b'\n' if len(archive) % 2 else b''))
            out = Path(temp) / 'sysroot'
            links = sdk.extract_deb(deb, out)
            sdk.normalized_links(out, links)
            self.assertEqual((out / 'usr/lib/x86_64-linux-gnu/libfoo.so').read_bytes(), b'hello!')

    def test_self_referential_file_alias_does_not_copy_onto_itself(self):
        # A differently cased symlink alias on a Windows filesystem can
        # resolve to the same physical file; an exact self-alias models the
        # same failure without requiring Windows to run this test.
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            folder = root / 'usr/lib/x86_64-linux-gnu'
            folder.mkdir(parents=True)
            library = folder / 'libfoo.so'
            library.write_bytes(b'ELF')
            with mock.patch.object(sdk.shutil, 'copy2', side_effect=AssertionError('self-copy')):
                sdk.normalized_links(root, [(library, 'libfoo.so')])
            self.assertEqual(library.read_bytes(), b'ELF')

    def test_case_alias_referring_to_same_file_is_not_copied(self):
        # Hardlinks model the samefile() result of case-insensitive Windows
        # paths while remaining runnable in the Linux CI environment.
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            folder = root / 'usr/lib/x86_64-linux-gnu'
            folder.mkdir(parents=True)
            original = folder / 'libExample.so'
            alias = folder / 'LIBEXAMPLE.so'
            original.write_bytes(b'ELF library')
            import os
            os.link(original, alias)
            self.assertTrue(alias.samefile(original))
            with mock.patch.object(sdk.shutil, 'copy2', side_effect=AssertionError('alias self-copy')):
                sdk.normalized_links(root, [(alias, 'libExample.so')])
            self.assertEqual(alias.read_bytes(), b'ELF library')

    def test_skips_nonbuild_documentation_but_preserves_linker_assets(self):
        for path in ('usr/share/man/man3/SD_ID128_TO_STRING.3.gz',
                     'usr/share/doc/libc6/copyright', 'usr/share/info/foo.info.gz'):
            self.assertTrue(sdk.is_ignored_sysroot_content(Path(path)))
        for path in ('usr/share/pkgconfig/foo.pc', 'usr/include/SDL2/SDL.h',
                     'usr/lib/x86_64-linux-gnu/libSDL2.so',
                     'usr/lib/gcc/x86_64-linux-gnu/12/crtbeginS.o'):
            self.assertFalse(sdk.is_ignored_sysroot_content(Path(path)))

    def test_deb_skips_manpage_symlinks_during_extraction(self):
        content = io.BytesIO()
        with tarfile.open(fileobj=content, mode='w:xz') as tar:
            for name, payload in (
                ('./usr/share/man/man3/sd_id128_to_string.3.gz', b'manual'),
                ('./usr/lib/x86_64-linux-gnu/libfoo.so.1', b'ELF'),
                ('./usr/share/pkgconfig/foo.pc', b'Name: foo\n'),
            ):
                entry = tarfile.TarInfo(name)
                entry.size = len(payload)
                tar.addfile(entry, io.BytesIO(payload))
            for name, link in (
                ('./usr/share/man/man3/SD_ID128_TO_STRING.3.gz', 'sd_id128_to_string.3.gz'),
                ('./usr/lib/x86_64-linux-gnu/libfoo.so', 'libfoo.so.1'),
            ):
                entry = tarfile.TarInfo(name)
                entry.type = tarfile.SYMTYPE
                entry.linkname = link
                tar.addfile(entry)
        archive = content.getvalue()
        header = (b'data.tar.xz/'.ljust(16, b' ') + b'0'.ljust(12, b' ') +
                  b'0'.ljust(6, b' ') + b'0'.ljust(6, b' ') +
                  b'100644'.ljust(8, b' ') + str(len(archive)).encode().ljust(10, b' ') + b'`\n')
        with tempfile.TemporaryDirectory() as td:
            deb = Path(td) / 'test.deb'
            deb.write_bytes(b'!<arch>\n' + header + archive + (b'\n' if len(archive) % 2 else b''))
            root = Path(td) / 'sysroot'
            links = sdk.extract_deb(deb, root)
            self.assertEqual([dest.name for dest, _ in links], ['libfoo.so'])
            sdk.normalized_links(root, links)
            self.assertEqual((root / 'usr/lib/x86_64-linux-gnu/libfoo.so').read_bytes(), b'ELF')
            self.assertTrue((root / 'usr/share/pkgconfig/foo.pc').exists())
            self.assertFalse((root / 'usr/share/man').exists())

    def test_windows_sdk_archive_keeps_bin_and_lib_directories(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fake_sdl = {'SDL2': ('2.32.10', 'https://example.org/SDL2.tar.gz')}
            archive = root / 'download.tar.gz'
            with tarfile.open(archive, 'w:gz') as tar:
                prefix = 'SDL2-2.32.10/x86_64-w64-mingw32/'
                for filename, data in (
                    ('bin/SDL2.dll', b'DLL'),
                    ('include/SDL2/SDL.h', b'header'),
                    ('lib/libSDL2.dll.a', b'LIB'),
                    ('lib/pkgconfig/sdl2.pc', b'prefix=/old\nName: SDL2\nVersion: 2.32.10\n'),
                    ('lib/pkgconfig/SDL2_image.pc', b'prefix=/old\n'),
                    ('lib/pkgconfig/SDL2_mixer.pc', b'prefix=/old\n'),
                    ('lib/pkgconfig/SDL2_ttf.pc', b'prefix=/old\n'),
                ):
                    info = tarfile.TarInfo(prefix + filename)
                    info.size = len(data)
                    tar.addfile(info, io.BytesIO(data))
            response = mock.Mock(returncode=0, stdout='2.32.10\n2.8\n2.8\n2.24\n')
            with mock.patch.object(sdk, 'SDL', fake_sdl), \
                 mock.patch.object(sdk, 'require_programs'), \
                 mock.patch.object(sdk, 'download', return_value=archive), \
                 mock.patch.object(sdk.subprocess, 'run', return_value=response), \
                 mock.patch.object(sdk, 'update_toolchains') as update:
                sdk.install_windows_sdk(root / 'sdk', force=False)
            self.assertTrue((root / 'sdk/bin/SDL2.dll').exists())
            self.assertTrue((root / 'sdk/lib/libSDL2.dll.a').exists())
            self.assertFalse((root / 'sdk/usr/bin/SDL2.dll').exists())
            self.assertIn(str(root / 'sdk'), (root / 'sdk/lib/pkgconfig/sdl2.pc').read_text())
            self.assertEqual(update.call_args.args[:2], ('linux', 'windows'))

    def test_merges_local_toolchain_instead_of_overwriting_other_targets(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'tools').mkdir()
            path = root / 'tools/toolchains.local.json'
            path.write_text(json.dumps({'windows': {'linux': {'cxx': 'mycompiler', 'custom': True}},
                                        'linux': {'windows': {'cxx': 'mingw'}}}))
            with mock.patch.object(sdk, 'ROOT', root):
                sdk.update_toolchains('windows', 'linux', {'sysroot': '/sdk'})
            saved = json.loads(path.read_text())
            self.assertEqual(saved['windows']['linux']['custom'], True)
            self.assertEqual(saved['windows']['linux']['cxx'], 'mycompiler')
            self.assertEqual(saved['windows']['linux']['sysroot'], '/sdk')
            self.assertEqual(saved['linux']['windows']['cxx'], 'mingw')

    def test_no_admin_installation_or_cross_actions_for_native_target(self):
        with mock.patch.object(sdk, 'host_os', return_value='windows'), \
             mock.patch.object(sdk.subprocess, 'run', return_value=mock.Mock(returncode=0)) as run, \
             mock.patch.object(sdk, 'install_linux_sdk', side_effect=AssertionError('not native')):
            self.assertEqual(sdk.main(['--target', 'windows']), 0)
            self.assertEqual(run.call_count, 1)

    def test_sdk_custom_path_for_all_is_rejected(self):
        self.assertEqual(sdk.main(['--target', 'all', '--sdk-dir', '/tmp/custom']), 1)

    def test_checksums_are_enforced_for_cached_debs(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / 'data.deb'
            p.write_bytes(b'wrong')
            with mock.patch.object(sdk.urllib.request, 'urlopen', side_effect=RuntimeError('network called')):
                with self.assertRaises(RuntimeError):
                    sdk.download('https://example.org/data.deb', p, sha256='0' * 64)

    def test_retries_windows_sharing_violation_and_succeeds(self):
        locked = OSError('temporarily in use')
        locked.winerror = 32
        operation = mock.Mock(side_effect=[locked, locked, 'success'])
        with mock.patch.object(sdk.time, 'sleep') as sleep, mock.patch.object(sdk, 'info'):
            self.assertEqual(sdk.retry_windows_file_lock(operation, 'test destination'), 'success')
        self.assertEqual(operation.call_count, 3)
        self.assertEqual(sleep.call_count, 2)

    def test_permanently_locked_file_reports_the_path(self):
        locked = OSError('in use')
        locked.winerror = 32
        with mock.patch.object(sdk.time, 'sleep'), mock.patch.object(sdk, 'info'):
            with self.assertRaises(sdk.SetupError) as raised:
                sdk.retry_windows_file_lock(mock.Mock(side_effect=locked),
                                            r'C:\sdk\target', attempts=3)
            self.assertIn(r'C:\sdk\target', str(raised.exception))

    def test_unrelated_io_errors_fail_immediately(self):
        failed = FileNotFoundError('missing file')
        operation = mock.Mock(side_effect=failed)
        with self.assertRaises(FileNotFoundError):
            sdk.retry_windows_file_lock(operation, 'test destination')
        self.assertEqual(operation.call_count, 1)

    def test_deb_extraction_retries_locked_output_file(self):
        payload = io.BytesIO()
        with tarfile.open(fileobj=payload, mode='w:xz') as tar:
            entry = tarfile.TarInfo('./usr/include/example.h')
            entry.size = 6
            tar.addfile(entry, io.BytesIO(b'header'))
        archive = payload.getvalue()
        header = (b'data.tar.xz/'.ljust(16, b' ') + b'0'.ljust(12, b' ') +
                  b'0'.ljust(6, b' ') + b'0'.ljust(6, b' ') +
                  b'100644'.ljust(8, b' ') + str(len(archive)).encode().ljust(10, b' ') + b'`\n')
        with tempfile.TemporaryDirectory() as td:
            deb = Path(td) / 'example.deb'
            deb.write_bytes(b'!<arch>\n' + header + archive + (b'\n' if len(archive) % 2 else b''))
            dest = Path(td) / 'sysroot'
            real_open = Path.open
            blocked = []
            def maybe_locked(file, mode='r', *args, **kwargs):
                if file.name == 'example.h' and mode == 'wb' and not blocked:
                    blocked.append(True)
                    locked = OSError('in use')
                    locked.winerror = 32
                    raise locked
                return real_open(file, mode, *args, **kwargs)
            with mock.patch.object(Path, 'open', maybe_locked), \
                 mock.patch.object(sdk.time, 'sleep'), mock.patch.object(sdk, 'info'):
                sdk.extract_deb(deb, dest)
            self.assertEqual((dest / 'usr/include/example.h').read_bytes(), b'header')
            self.assertEqual(len(blocked), 1)

    def test_download_retries_locked_archive_rename_without_re_download(self):
        with tempfile.TemporaryDirectory() as td:
            output = Path(td) / 'example.deb'
            original_replace = Path.replace
            attempts = []
            def replace_once_locked(src, dest):
                attempts.append(str(src))
                if len(attempts) == 1:
                    locked = OSError('in use')
                    locked.winerror = 32
                    raise locked
                return original_replace(src, dest)
            with mock.patch.object(sdk.urllib.request, 'urlopen', return_value=io.BytesIO(b'abc')), \
                 mock.patch.object(sdk.time, 'sleep'), \
                 mock.patch.object(Path, 'replace', replace_once_locked), \
                 mock.patch.object(sdk, 'info'):
                sdk.download('https://example.org/example.deb', output, sha256=hashlib.sha256(b'abc').hexdigest())
            self.assertEqual(output.read_bytes(), b'abc')
            self.assertEqual(len(attempts), 2)
            self.assertFalse(output.with_suffix('.deb.partial').exists())

    def test_debug_flag_exposes_traceback_when_setup_fails(self):
        self.assertTrue(sdk.parse_args(['--target', 'linux', '--debug']).debug)
        with mock.patch.object(sdk, 'host_os', side_effect=sdk.SetupError('broken')), \
             mock.patch.object(sdk.traceback, 'print_exc') as trace, \
             mock.patch.object(sdk, 'info'):
            self.assertEqual(sdk.main(['--target', 'linux', '--debug']), 1)
        trace.assert_called_once()

    def test_windows_linux_toolchain_always_includes_target_sdl2_headers(self):
        # The Windows pkgconf implementation can report SDL2 versions while
        # omitting/reinterpreting -I flags. <SDL.h> must still be found by Clang.
        root = Path('C:/Users/Example/.xengine/sdk/linux-x86_64/sysroot')
        gcc = root / 'usr/lib/gcc/x86_64-linux-gnu/12'
        cpp = root / 'usr/include/c++/12'
        flags = sdk.linux_cross_compiler_flags(root, cpp, gcc)
        self.assertIn('--target=x86_64-linux-gnu', flags)
        self.assertIn('-isystem' + (root / 'usr/include/SDL2').as_posix(), flags)
        self.assertIn('-isystem' + (root / 'usr/include').as_posix(), flags)
        self.assertIn('-isystem' + (root / 'usr/include/x86_64-linux-gnu').as_posix(), flags)
        self.assertFalse(any('C:/msys64/ucrt64/include' in flag for flag in flags))

    def test_merged_usr_gnu_ld_scripts_use_existing_canonical_targets(self):
        # Reproduce Debian bookworm libm.so's GROUP(/lib/...) script. The
        # extracted Windows sysroot stores libc6 files under usr/lib instead.
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            directory = root / 'usr/lib/x86_64-linux-gnu'
            directory.mkdir(parents=True)
            for name in ('libm.so.6', 'libmvec.so.1'):
                (directory / name).write_bytes(b'\x7fELF' + bytes(32))
            script = directory / 'libm.so'
            script.write_text('/* GNU ld script */\nGROUP ( /lib/x86_64-linux-gnu/libm.so.6  '
                              'AS_NEEDED ( /lib/x86_64-linux-gnu/libmvec.so.1 ) )\n')
            self.assertEqual(sdk.fix_merged_usr_linker_scripts(root), 1)
            output = script.read_text()
            self.assertIn('/usr/lib/x86_64-linux-gnu/libm.so.6', output)
            self.assertIn('/usr/lib/x86_64-linux-gnu/libmvec.so.1', output)
            self.assertNotIn(' /lib/x86_64-linux-gnu/', output)
            self.assertEqual(sdk.fix_merged_usr_linker_scripts(root), 0)
            self.assertTrue((directory / 'libm.so.6').read_bytes().startswith(b'\x7fELF'))

    def test_merged_usr_script_can_link_with_real_lld_if_available(self):
        # End-to-end reproduction of the exact Debian libm.so failure using
        # minimal ELF objects. No system Linux libs or WSL are required.
        import shutil
        import subprocess
        if not shutil.which('clang++') or not shutil.which('ld.lld'):
            self.skipTest('Clang and LLD are unavailable')
        with tempfile.TemporaryDirectory() as td:
            work = Path(td)
            root = work / 'sysroot'
            directory = root / 'usr/lib/x86_64-linux-gnu'
            directory.mkdir(parents=True)
            source = work / 'foo.cpp'
            obj = work / 'foo.o'
            source.write_text('extern "C" int foo(){return 7;}\n')
            subprocess.run(['clang++', '--target=x86_64-linux-gnu', '-fPIC', '-c',
                            str(source), '-o', str(obj)], check=True, capture_output=True)
            for name in ('libm.so.6', 'libmvec.so.1'):
                subprocess.run(['ld.lld', '-shared', '-o', str(directory / name), str(obj)],
                               check=True, capture_output=True)
            script = directory / 'libm.so'
            script.write_text('GROUP ( /lib/x86_64-linux-gnu/libm.so.6 '
                              'AS_NEEDED ( /lib/x86_64-linux-gnu/libmvec.so.1 ) )\n')
            command = ['ld.lld', '-shared', '--sysroot=' + str(root), '-L' + str(directory),
                       '-o', str(work / 'probe.so'), str(obj), '-lm']
            failed = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(failed.returncode, 0)
            self.assertIn('libm.so.6', failed.stderr)
            self.assertEqual(sdk.fix_merged_usr_linker_scripts(root), 1)
            linked = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(linked.returncode, 0, linked.stderr)
            self.assertEqual((work / 'probe.so').read_bytes()[:4], b'\x7fELF')

    def test_merged_usr_script_does_not_rewrite_missing_binary(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            directory = root / 'usr/lib/x86_64-linux-gnu'
            directory.mkdir(parents=True)
            script = directory / 'libc.so'
            content = 'GROUP ( /lib/x86_64-linux-gnu/not-found.so )\n'
            script.write_text(content)
            self.assertEqual(sdk.fix_merged_usr_linker_scripts(root), 0)
            self.assertEqual(script.read_text(), content)

    def test_verify_only_skips_debian_downloads_when_sdk_is_missing(self):
        with tempfile.TemporaryDirectory() as temporary:
            missing_sdk = Path(temporary) / 'missing'
            with mock.patch.object(sdk, 'require_programs'), \
                 mock.patch.object(sdk, 'download', side_effect=AssertionError('unexpected network')):
                with self.assertRaisesRegex(sdk.SetupError, 'has not been extracted'):
                    sdk.install_linux_sdk(missing_sdk, force=False, verify_only=True)

    def test_verify_only_links_existing_sysroot_without_index_or_package_downloads(self):
        with tempfile.TemporaryDirectory() as temporary:
            sdk_root = Path(temporary) / 'linux-sdk'
            sysroot = sdk_root / 'sysroot'
            contents = {
                'usr/include/SDL2/SDL.h': b'SDL',
                'usr/include/nlohmann/json.hpp': b'JSON',
                'usr/include/stdio.h': b'stdio',
                'usr/lib/x86_64-linux-gnu/pkgconfig/sdl2.pc': b'prefix=/usr\nVersion: 2.26.5\n',
                'usr/lib/x86_64-linux-gnu/libSDL2.so': b'ELF mock',
                'usr/lib/gcc/x86_64-linux-gnu/12/crtbeginS.o': b'obj',
                'usr/include/c++/12/vector': b'c++',
            }
            for name, data in contents.items():
                path = sysroot / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            entry = {}
            def remember_toolchain(host, target, settings):
                self.assertEqual((host, target), ('windows', 'linux'))
                entry.update(settings)
            def fake_prepare_tools(target):
                self.assertEqual(target, 'linux')
                return SimpleNamespace(compiler='clang++', cxxflags=entry['cxxflags'],
                                       pkg_cflags=[], ldflags=entry['ldflags'], pkg_libs=['-lSDL2'])
            def fake_run(command, **kwargs):
                if '--modversion' in command:
                    return SimpleNamespace(returncode=0, stdout='2.26.5\n2.6\n2.20\n2.6\n', stderr='')
                self.assertIn('-isystem' + (sysroot / 'usr/include/SDL2').as_posix(), command)
                self.assertIn('-L' + (sysroot / 'usr/lib/x86_64-linux-gnu').as_posix(), command)
                elf = Path(command[command.index('-o') + 1])
                elf.write_bytes(b'\x7fELF\x02\x01\x01' + bytes(11) + b'\x3e\x00')
                return SimpleNamespace(returncode=0, stdout='', stderr='')
            fake_build = SimpleNamespace(BuildError=Exception, prepare_tools=fake_prepare_tools)
            with mock.patch.object(sdk, 'require_programs'), \
                 mock.patch.object(sdk, 'download', side_effect=AssertionError('unexpected download')), \
                 mock.patch.object(sdk, 'update_toolchains', side_effect=remember_toolchain), \
                 mock.patch.object(sdk.subprocess, 'run', side_effect=fake_run), \
                 mock.patch.dict(sys.modules, {'build': fake_build}), \
                 mock.patch.object(sdk, 'info'):
                sdk.install_linux_sdk(sdk_root, force=False, verify_only=True)
            self.assertEqual(entry['sysroot'], str(sysroot))
            self.assertFalse((sdk_root / 'packages-manifest.json').exists())

    def test_verify_only_requires_windows_linux_target_and_disallows_force(self):
        self.assertTrue(sdk.parse_args(['--target', 'linux', '--verify-only']).verify_only)
        with mock.patch.object(sdk, 'host_os', return_value='linux'), \
             mock.patch.object(sdk, 'info'):
            self.assertEqual(sdk.main(['--target', 'linux', '--verify-only']), 1)
        with mock.patch.object(sdk, 'host_os', return_value='windows'), \
             mock.patch.object(sdk, 'info'):
            self.assertEqual(sdk.main(['--target', 'linux', '--verify-only', '--force']), 1)

    def test_repository_guides_document_auto_and_manual_methods(self):
        readme = (ROOT / 'README.md').read_text()
        cross = (ROOT / 'docs/CROSS_COMPILATION.md').read_text()
        guide = (ROOT / 'tools/dependency_guides.py').read_text()
        for body in (readme, cross):
            self.assertIn('install_cross_sdk.bat --target linux', body)
            self.assertIn('./install_cross_sdk.sh --target windows', body)
            self.assertIn('Manual Windows → Linux', cross)
            self.assertIn('--sdk-dir', body)
        self.assertIn('RECOMMENDED:', guide)
        self.assertIn('Manual alternative:', guide)


if __name__ == '__main__':
    unittest.main()
