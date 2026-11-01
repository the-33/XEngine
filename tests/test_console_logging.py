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
                '  if (!XEngineConsole::BeginStdoutCapture()) return 2;\n'
                '  std::printf("raw-from-game\\n");\n'
                '  std::cout << "cout-from-game" << std::endl;\n'
                '  Console.Log("hello");\n'
                '  Console.Warning("careful", "Inventory");\n'
                '  Console.Error("broken", "FileLoad");\n'
                '  LogError("FileSystem", "No file");\n'
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
                capture_output=True, text=True, check=False,
            )
            self.assertEqual(compilation.returncode, 0, compilation.stderr)
            result = subprocess.run([str(executable)], capture_output=True,
                                    text=True, check=False, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            for marker in ("● [LOG] Game\n    raw-from-game",
                           "● [LOG] Game\n    cout-from-game",
                           "● [LOG] Game\n    hello",
                           "⚠ [WARNING] Inventory\n    careful",
                           "✖ [ERROR] FileLoad\n    broken",
                           "✖ [ERROR] FileSystem\n    No file"):
                self.assertIn(marker, result.stdout)
            self.assertNotIn("\x1b[", result.stdout)


if __name__ == "__main__":
    unittest.main()
