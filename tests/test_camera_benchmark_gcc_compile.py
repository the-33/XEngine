"""Compile-smoke regressions for camera ownership and the benchmark API.

These tests deliberately use *opaque* SDL stand-ins. They validate only C++
syntax, access control and template instantiation; they are not a replacement
for linking/running with a real SDL2 SDK.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
INCLUDE = ROOT / 'engine' / 'include'
GAME = ROOT / 'games' / 'XEngine Benchmark'

STUBS = {
    'SDL.h': '''#pragma once
#include <cstdint>
typedef struct SDL_Texture SDL_Texture;
typedef struct SDL_Surface SDL_Surface;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Window SDL_Window;
typedef struct SDL_FRect SDL_FRect;
typedef union SDL_Event { std::uint32_t type; } SDL_Event;
#define SDL_QUIT 0x100
inline int SDL_PushEvent(SDL_Event*) { return 1; }
inline char* SDL_GetBasePath() { return nullptr; }
inline void SDL_free(void*) {}
''',
    'SDL_stdinc.h': '#pragma once\n#include <cstdint>\n',
    'SDL_mixer.h': '#pragma once\ntypedef struct Mix_Chunk Mix_Chunk;\ntypedef struct _Mix_Music Mix_Music;\n',
    'SDL_ttf.h': '#pragma once\ntypedef struct _TTF_Font TTF_Font;\n',
    'nlohmann/json.hpp': '''#pragma once
#include <string>
namespace nlohmann { class json { public:
  static json parse(const std::string&);
  json() = default;
  json(const char*);
  json(float);
  bool is_string() const;
  const json& at(const char*) const;
  json& operator[](const char*);
  template<class T> json& operator=(T&&);
  std::string dump() const;
  template<class T> T get() const;
}; }
''',
    'GameBuild.generated.h': '''#pragma once
#include <string>
namespace XEngineGameBuild {
    const char* DevelopmentAssetsPath() noexcept;
    std::string EmbeddedEngineConfig();
}
''',
}


class CameraBenchmarkGccCompile(unittest.TestCase):
    def test_modified_cpp_translation_units(self):
        compiler = shutil.which('g++')
        if not compiler:
            self.skipTest('g++ is not available on PATH')

        with tempfile.TemporaryDirectory(prefix='xengine-sdl-stubs-') as directory:
            stub_dir = Path(directory)
            for name, contents in STUBS.items():
                (stub_dir / name).parent.mkdir(parents=True, exist_ok=True)
                (stub_dir / name).write_text(contents, encoding='utf-8')

            include_dirs = (
                stub_dir, INCLUDE, GAME / 'assets',
                GAME / 'assets' / 'scripts', GAME / 'assets' / 'scenes',
            )
            flags = [compiler, '-std=c++20', '-fsyntax-only', '-DXENGINE_DEBUG',
                     '-Werror=ignored-qualifiers',
                     *(f'-I{directory}' for directory in include_dirs)]
            sources = [
                ROOT / 'engine' / 'src' / 'Scene.cpp',
                ROOT / 'engine' / 'src' / 'GameObject.cpp',
                ROOT / 'engine' / 'src' / 'Camera2D.cpp',
                ROOT / 'engine' / 'src' / 'SceneManager.cpp',
                ROOT / 'engine' / 'src' / 'CollisionManager.cpp',
                GAME / 'src' / 'main.cpp',
            ]
            for source in sources:
                with self.subTest(file=source.name):
                    result = subprocess.run([*flags, str(source)], cwd=ROOT,
                                            text=True, capture_output=True, check=False)
                    self.assertEqual(result.returncode, 0,
                                     f'{source.relative_to(ROOT)} failed to compile:\n{result.stdout}\n{result.stderr}')


if __name__ == '__main__':
    unittest.main()
