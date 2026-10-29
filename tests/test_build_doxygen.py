"""Doxygen generation is optional and only follows a real engine rebuild."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('xengine_build_doxygen', ROOT / 'tools/build.py')
build = importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(build)


class DoxygenBuildTests(unittest.TestCase):
    def test_missing_doxygen_warns_but_skips_generation(self):
        with tempfile.TemporaryDirectory() as temporary:
            doxyfiles = tuple(Path(temporary) / name for name in
                              ('Doxyfile_Games', 'Doxyfile_Engine'))
            for file in doxyfiles:
                file.write_text('PROJECT_NAME = XEngine', encoding='utf-8')
            with mock.patch.object(build, 'DOXYFILES', doxyfiles), \
                 mock.patch.object(build.shutil, 'which', return_value=None), \
                 mock.patch.object(build.subprocess, 'run') as run, \
                 mock.patch.object(build, 'log') as log:
                build.regenerate_engine_api_docs()
                run.assert_not_called()
                message = '\n'.join(call.args[0] for call in log.call_args_list)
                self.assertIn('[WARN] Doxygen is not installed', message)
                self.assertIn('whenever the engine recompiles', message)

    def test_installed_doxygen_runs_from_engine_root(self):
        with tempfile.TemporaryDirectory() as temporary:
            doxyfiles = tuple(Path(temporary) / name for name in
                              ('Doxyfile_Games', 'Doxyfile_Engine'))
            for file in doxyfiles:
                file.write_text('PROJECT_NAME = XEngine', encoding='utf-8')
            completed = subprocess.CompletedProcess(['doxygen'], 0, stdout='')
            with mock.patch.object(build, 'DOXYFILES', doxyfiles), \
                 mock.patch.object(build.shutil, 'which', return_value='/bin/doxygen'), \
                 mock.patch.object(build.subprocess, 'run', return_value=completed) as run, \
                 mock.patch.object(build, 'log') as log:
                build.regenerate_engine_api_docs()
                self.assertEqual(run.call_args.args[0],
                                 [build.sys.executable, str(build.ROOT / 'tools' / 'build_doxygen.py'),
                                  '--doxygen', '/bin/doxygen'])
                self.assertEqual(run.call_args.kwargs['cwd'], build.ROOT)
                self.assertTrue(any('[DOCS] API reference:' in c.args[0] for c in log.call_args_list))

    def test_generation_error_never_fails_engine_build(self):
        with tempfile.TemporaryDirectory() as temporary:
            doxyfiles = tuple(Path(temporary) / name for name in
                              ('Doxyfile_Games', 'Doxyfile_Engine'))
            for file in doxyfiles:
                file.touch()
            completed = subprocess.CompletedProcess(['doxygen'], 1, stdout='bad documentation')
            with mock.patch.object(build, 'DOXYFILES', doxyfiles), \
                 mock.patch.object(build.shutil, 'which', return_value='/bin/doxygen'), \
                 mock.patch.object(build.subprocess, 'run', return_value=completed), \
                 mock.patch.object(build, 'log') as log:
                build.regenerate_engine_api_docs()
                self.assertTrue(any('engine was compiled successfully' in c.args[0]
                                    for c in log.call_args_list))

    def test_rebuilt_engine_triggers_doxygen_but_cache_hit_does_not(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            src = root / 'engine' / 'src' / 'Example.cpp'
            src.parent.mkdir(parents=True)
            src.write_text('int example = 1;', encoding='utf-8')
            headers = root / 'engine' / 'include'
            headers.mkdir(parents=True)
            tooling = SimpleNamespace(compiler='g++', archiver='ar', pkg_cflags=[],
                                      version='mock compiler', cxxflags=[])
            output = root / 'build' / 'linux-x86_64' / 'debug' / 'engine'
            output.mkdir(parents=True)
            lib = output / 'libXEngine.a'
            with mock.patch.object(build, 'ROOT', root), \
                 mock.patch.object(build, 'ENGINE_SRC', src.parent), \
                 mock.patch.object(build, 'ENGINE_HEADERS', headers), \
                 mock.patch.object(build, 'collect_sources', return_value=[src]), \
                 mock.patch.object(build, 'all_headers', return_value=[]), \
                 mock.patch.object(build, 'command') as command, \
                 mock.patch.object(build, 'regenerate_engine_api_docs') as docs, \
                 mock.patch.object(build, 'log'):
                with mock.patch.object(build, 'compile_sources', return_value=([], True)):
                    self.assertEqual(build.create_engine(target='linux', profile='debug',
                                                         arch='x86_64', tooling=tooling), lib)
                    docs.assert_called_once_with()
                    self.assertEqual(command.call_count, 1)
                docs.reset_mock()
                command.reset_mock()
                lib.touch()
                with mock.patch.object(build, 'compile_sources', return_value=([], False)):
                    build.create_engine(target='linux', profile='debug',
                                        arch='x86_64', tooling=tooling)
                    docs.assert_not_called()
                    command.assert_not_called()

                # --no-doc suppresses documentation even on a real rebuild.
                with mock.patch.object(build, 'compile_sources', return_value=([], True)):
                    build.create_engine(target='linux', profile='debug',
                                        arch='x86_64', tooling=tooling, no_doc=True)
                    docs.assert_not_called()

    def test_no_doc_cli_controls_the_local_online_only_marker(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with mock.patch.object(build, 'ROOT', root), \
                 mock.patch.object(build, 'prepare_tools', return_value=object()), \
                 mock.patch.object(build, 'create_engine', return_value=root / 'lib.a') as engine:
                with mock.patch.object(build.sys, 'argv', ['build.py', 'engine', '--no-doc']):
                    self.assertEqual(build.main(), 0)
                engine.assert_called_with(target=mock.ANY, profile='debug',
                                          arch=mock.ANY, tooling=mock.ANY, no_doc=True)
                marker = root / 'build' / 'ide' / 'doxygen_online_only'
                self.assertEqual(marker.read_text(encoding='ascii'), 'online\n')

                with mock.patch.object(build.sys, 'argv', ['build.py', 'engine']):
                    self.assertEqual(build.main(), 0)
                self.assertFalse(marker.exists())


if __name__ == '__main__':
    unittest.main()
