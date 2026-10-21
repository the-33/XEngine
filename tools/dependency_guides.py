#!/usr/bin/env python3
"""Optional cross-compilation setup guidance, independent of game projects.

This registry does not inspect games, read project configuration, install tools,
or check whether optional toolchains are available. Future targets such as
Android can be added once their build backends are implemented.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Callable


@dataclass(frozen=True)
class TargetGuide:
    """Instructions for building from a given host to a different target."""

    host: str
    target: str
    title: str
    install_commands: tuple[str, ...]
    remaining_steps: tuple[str, ...]


# Reference hosts: Debian/Ubuntu Linux and MSYS2 UCRT64 Windows.
# Commands are displayed for reference only; they are never executed.
OPTIONAL_TARGETS: tuple[TargetGuide, ...] = (
    TargetGuide(
        host='linux', target='windows',
        title='Windows x86_64 from Linux (Debian/Ubuntu)',
        install_commands=(
            'sudo apt update',
            'sudo apt install -y g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64 pkg-config',
        ),
        remaining_steps=(
            'RECOMMENDED: from the repository root run ./install_cross_sdk.sh --target windows after installing MinGW-w64 (downloads SDL2 and configures tools/toolchains.local.json).',
            'Manual alternative: prepare a Windows x86_64 SDK using SDL2/SDL2_image/SDL2_mixer/SDL2_ttf development archives from libsdl.org (MinGW packages, not Linux packages).',
            'Example SDK directory: ~/.local/share/xengine/windows-sdk (NOT required; choose any writable directory).',
            'Place Windows DLLs under <SDK>/bin, import libraries under <SDK>/lib, headers under <SDK>/include and SDL2 .pc files under <SDK>/lib/pkgconfig.',
            'Verify with: env -u PKG_CONFIG_PATH PKG_CONFIG_LIBDIR="$HOME/.local/share/xengine/windows-sdk/lib/pkgconfig" pkg-config --modversion sdl2 SDL2_image SDL2_ttf SDL2_mixer (adjust to your SDK path).',
            'Copy tools/toolchains.example.json to tools/toolchains.local.json; configure linux.windows: pkg_config_libdir=<SDK>/lib/pkgconfig, dll_dirs=<SDK>/bin, lib_dirs=<SDK>/lib.',
            'XEngine expands ~/ to the home directory of the CURRENT user at build time. Replace every example path when using a custom SDK directory.',
            'MinGW-w64 does NOT install the Windows SDL development libraries. Match the ABI/CRT of the target toolchain and SDK.',
            'Complete download and SDK assembly commands: README.md (Complete Example) and docs/CROSS_COMPILATION.md (Linux -> Windows).',
        ),
    ),
    TargetGuide(
        host='windows', target='linux',
        title='Linux x86_64 from Windows (MSYS2 UCRT64)',
        install_commands=(
            '# Run from the MSYS2 UCRT64 terminal:',
            'pacman -S --needed mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-lld mingw-w64-ucrt-x86_64-llvm',
        ),
        remaining_steps=(
            'RECOMMENDED: from the repository root run install_cross_sdk.bat --target linux after installing Clang/LLVM (downloads a Debian Linux sysroot directly on Windows, without WSL/Docker).',
            'Manual alternative: prepare a Linux x86_64 sysroot with glibc, C++ runtime and SDL2, SDL2_image, SDL2_ttf and SDL2_mixer: headers, libraries and .pc files.',
            'The Linux SDK needs ELF libraries and CRT objects; Windows DLLs are not compatible.',
            'Copy tools\\toolchains.example.json to tools\\toolchains.local.json and configure windows.linux: sysroot, pkg_config_libdir, lib_dirs and --target=x86_64-linux-gnu.',
            'The example sysroot C:/xengine-cross/linux-x86_64/sysroot is NOT mandatory; choose any directory and update toolchains.local.json accordingly.',
            'Clang and LLD alone do NOT provide the Linux sysroot or SDL development libraries.',
        ),
    ),
)


def show_optional_targets(host: str, *, emit: Callable[[str], None] = print) -> None:
    """Show optional setup guidance without probing or changing the system."""
    emit('\n[INFO] Optional cross-compilation setup (not checked):')
    guides = [guide for guide in OPTIONAL_TARGETS if guide.host == host]
    if not guides:
        emit('  No cross-compilation setup instructions are available for this host.')
        return

    for guide in guides:
        emit(f'\n  To build for {guide.title}, install:')
        for command in guide.install_commands:
            emit(f'    {command}')
        emit('  Next steps:')
        for instruction in guide.remaining_steps:
            emit(f'    - {instruction}')

    emit('\n  Recommended automated SDK setup: install_cross_sdk.bat (Windows) or ./install_cross_sdk.sh (Linux).')
    emit('  Custom SDK location: --sdk-dir <path> (example paths are NOT mandatory).')
    emit('  Manual installation for both directions: docs/CROSS_COMPILATION.md')
    emit('  These tools are OPTIONAL and do not affect the native dependency check result.')
