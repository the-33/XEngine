#!/usr/bin/env python3
"""Read-only diagnostics for XEngine's GCC/SDL2 dependencies."""
from __future__ import annotations

import os
import shlex
import shutil
import subprocess
import sys

from dependency_guides import show_optional_targets

PACKAGES = ('sdl2', 'SDL2_image', 'SDL2_ttf', 'SDL2_mixer')
TEST_SOURCE = r'''
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <nlohmann/json.hpp>
#if !SDL_VERSION_ATLEAST(2,26,0)
# error "XEngine requires SDL 2.26.0 or newer"
#endif
int main() {
    SDL_version v{};
    SDL_VERSION(&v);
    nlohmann::json value = {{"ok", true}};
    return value["ok"].get<bool>() ? 0 : 1;
}
'''


def which(name: str, alternative: str | None = None) -> str | None:
    return shutil.which(name) or (shutil.which(alternative) if alternative else None)


def run(args: list[str], *, stdin: str | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, input=stdin, capture_output=True, text=True, check=False)


def main() -> int:
    print('[XEngine] Dependency check for the current host')
    problems: list[str] = []
    if sys.version_info < (3, 10):
        problems.append('Python 3.10 or newer is required')
    else:
        print(f'[OK] Python {sys.version.split()[0]}')

    dependency_inspector = 'objdump' if os.name == 'nt' else 'ldd'
    binaries = {'g++': which('g++'), 'ar': which('ar'),
                dependency_inspector: which(dependency_inspector),
                'pkg-config': which('pkg-config', 'pkgconf')}
    for name, binary in binaries.items():
        if binary:
            print(f'[OK] {name}: {binary}')
        else:
            problems.append(f'{name} was not found in PATH')

    if os.name == 'nt':
        resource_compiler = which('windres', 'x86_64-w64-mingw32-windres')
        if resource_compiler:
            print(f'[OK] windres (embedding .ico resources in .exe files): {resource_compiler}')
        else:
            print('[WARN] windres not found: required only when project.icon specifies a .ico file')

    pkg = binaries['pkg-config']
    pkg_flags: list[str] = []
    if pkg:
        for package in PACKAGES:
            result = run([pkg, '--modversion', package])
            if result.returncode:
                problems.append(f'pkg-config cannot find {package}: {result.stderr.strip()}')
            else:
                print(f'[OK] {package}: {result.stdout.strip()}')
        flags = run([pkg, '--cflags', *PACKAGES])
        libs = run([pkg, '--libs', *PACKAGES])
        if flags.returncode or libs.returncode:
            problems.append('pkg-config cannot resolve SDL2 compiler/linker flags')
        else:
            pkg_flags = shlex.split(flags.stdout, posix=True)
            if os.name == 'nt':
                pkg_flags = [x.replace('\\', '/') for x in pkg_flags]

    if binaries['g++'] and pkg and not any('SDL2' in x or 'sdl2' in x for x in problems):
        try:
            checked = run([binaries['g++'], '-std=c++20', *pkg_flags,
                           '-x', 'c++', '-fsyntax-only', '-'], stdin=TEST_SOURCE)
            if checked.returncode:
                problems.append('Cannot compile SDL2 and nlohmann/json headers:\n' + checked.stderr.strip())
            else:
                print('[OK] SDL2 and nlohmann/json header compilation test passed')
        except OSError as exc:
            problems.append('Failed to run g++: ' + str(exc))

    if problems:
        print('\n[ERROR] Missing dependencies or invalid environment configuration:')
        for item in problems:
            print(' - ' + item)
        print('See the Dependency installation section in README.md.')
        show_optional_targets('windows' if os.name == 'nt' else 'linux')
        return 1
    print('\n[OK] Tools and headers are ready to build XEngine.')
    print('Note: this check does not link or run a game.')
    show_optional_targets('windows' if os.name == 'nt' else 'linux')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
