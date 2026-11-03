"""Comprueba el registro real de stdout sin depender de SDL2."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ConsoleLoggingTests(unittest.TestCase):
    def test_debug_levels_and_stdout_capture(self):
        compiler = shutil.which("g++")
        if not compiler:
            self.skipTest("g++ no está instalado")

        with tempfile.TemporaryDirectory(prefix="xengine-console-") as directory:
            folder = Path(directory)
            (folder / "Engine.h").write_text(
                "#pragma once\nstruct Engine { bool logErrors = true; "
                "static Engine* GetInstancePtr_NO_ERROR_MSG() noexcept {return nullptr;} };\n",
                encoding="utf-8",
            )
            (folder / "main.cpp").write_text(
                '#include "ErrorHandler.h"\n'
                '#include <cstdio>\n#include <iostream>\n'
                'int main() {\n'
                '  Console.Log("early-startup", "Engine");\n'
                '  if (!XEngineConsole::BeginStdoutCapture()) return 2;\n'
                '  std::printf("raw-from-game\\n");\n'
                '  std::cout << "cout-from-game" << std::endl;\n'
                '  Console.Log("hello");\n'
                '  Console.Warning("careful", "Inventory");\n'
                '  Console.Error("broken", "FileLoad");\n'
                '  LogError("FileSystem", "No file");\n'
                '  Console.Log("starting", "Engine");\n'
                '  Console.Warning("backend fallback", "Engine - Render");\n'
                '  LogError("Engine - Assets", "Cannot load asset");\n'
                '  XEngineConsole::EndStdoutCapture();\n'
                '}\n',
                encoding="utf-8",
            )
            executable = folder / "logging_test"
            compilation = subprocess.run(
                [compiler, "-std=c++20", "-pthread", "-I", str(folder),
                 "-I", str(ROOT / "engine" / "include"),
                 str(ROOT / "engine" / "src" / "ErrorHandler.cpp"),
                 str(folder / "main.cpp"), "-o", str(executable)],
                capture_output=True, text=True, encoding="utf-8",
                errors="replace", check=False,
            )
            self.assertEqual(compilation.returncode, 0, compilation.stderr)
            # The engine emits UTF-8 glyphs (●, ⚠, ✖) on every platform.
            # MSYS2 Python on Windows may default to cp1252, which cannot
            # decode their bytes and can leave CompletedProcess.stdout=None.
            result = subprocess.run([str(executable)], capture_output=True,
                                    text=True, encoding="utf-8", errors="replace",
                                    check=False, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            for marker in ("● [LOG] Engine\n    early-startup",
                           "● [LOG] Game\n    raw-from-game",
                           "● [LOG] Game\n    cout-from-game",
                           "● [LOG] Game\n    hello",
                           "⚠ [WARNING] Inventory\n    careful",
                           "✖ [ERROR] FileLoad\n    broken",
                           "✖ [ERROR] FileSystem\n    No file",
                           "● [LOG] Engine\n    starting",
                           "⚠ [WARNING] Engine - Render\n    backend fallback",
                           "✖ [ERROR] Engine - Assets\n    Cannot load asset"):
                self.assertIn(marker, result.stdout)
            self.assertNotIn("\x1b[", result.stdout)

    def test_windows_console_utf8_is_initialized_before_first_log(self):
        source = (ROOT / 'engine' / 'src' / 'ErrorHandler.cpp').read_text(encoding='utf-8')
        emit = source.split('void Emit(', 1)[1].split('bool ErrorsEnabled()', 1)[0]
        self.assertIn('SetConsoleOutputCP(CP_UTF8)', emit)
        self.assertLess(emit.index('SetConsoleOutputCP(CP_UTF8)'),
                        emit.index('WriteAll(target, prefix)'))
        capture = source.split('bool XEngineConsole::BeginStdoutCapture()', 1)[1]
        self.assertNotIn('SetConsoleOutputCP(CP_UTF8)', capture)


if __name__ == "__main__":
    unittest.main()
