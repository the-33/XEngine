#!/usr/bin/env python3
"""XEngine native g++/ar build driver. No CMake/Make/Ninja; stdlib Python only.

The .bat/.sh scripts are public entry points. This helper parses JSON, supplies
compiler arguments, tracks changes and packages shipping releases.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform as pyplatform
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import zipfile
from typing import NamedTuple
from pathlib import Path
from typing import TextIO

ROOT = Path(__file__).resolve().parent.parent
ENGINE_SRC = ROOT / 'engine' / 'src'
ENGINE_HEADERS = ROOT / 'engine' / 'include'
DOXYFILES = (ROOT / 'Doxyfile_Games', ROOT / 'Doxyfile_Engine')

# Tests import this script as a standalone module; keep sibling helpers visible.
if str(Path(__file__).resolve().parent) not in sys.path:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
from doxygen_cache import docs_current

class BuildError(Exception):
    pass

_raw_log: TextIO | None = None
_raw_log_path: Path | None = None
_show_warnings = False
_show_commands = False
_warning_count = 0
_diagnostic_re = re.compile(r'^(.*?):(\d+)(?::(\d+))?:\s*(fatal error|error|warning|note):\s*(.*)$')


def _raw(message: str) -> None:
    if _raw_log is not None:
        _raw_log.write(message)
        _raw_log.flush()


def log(message: str) -> None:
    print(message, flush=True)
    _raw(message + '\n')


def _short_path(value: str) -> str:
    normalized = value.replace('\\', '/')
    root = str(ROOT).replace('\\', '/')
    if normalized.startswith(root + '/'):
        return normalized[len(root) + 1:]
    for segment in ('/engine/', '/games/', '/tools/'):
        at = normalized.lower().find(segment)
        if at >= 0:
            return normalized[at + 1:]
    return '/'.join(normalized.split('/')[-2:])


def _diagnostic_header(line: str) -> str:
    match = _diagnostic_re.match(line.strip())
    if not match:
        return line.strip()
    path, row, column, severity, message = match.groups()
    location = f'{_short_path(path)}:{row}' + (f':{column}' if column else '')
    return f'{location}: {severity}: {message}'


def _print_warnings(lines: list[str]) -> None:
    """Show GCC warnings as grouped, indented diagnostics, not template backtraces."""
    showing = False
    snippets = 0
    for line in lines:
        clean = line.rstrip('\r\n')
        if ': warning:' in clean or clean.lstrip().startswith('warning:'):
            print('    [WARN] ' + _diagnostic_header(clean), flush=True)
            showing, snippets = True, 0
        elif showing and ': note:' in clean:
            print('      ' + _diagnostic_header(clean), flush=True)
            snippets = 0
        elif showing and re.match(r'^\s*(\d+\s*\||\|)', clean) and snippets < 6:
            print('      ' + clean.strip(), flush=True)
            snippets += 1
        elif not clean.strip():
            showing = False


def _command_stage(args: list[str]) -> tuple[str, bool]:
    """Return a human-readable build action and whether its output is diagnostics."""
    if '-c' in args:
        source = args[args.index('-c') + 1]
        return f'[COMPILE] {_short_path(source)}', True
    first = Path(args[0].replace('\\', '/')).name.lower()
    if 'windres' in first:
        return '[RESOURCE] Windows executable icon', True
    if len(args) > 1 and args[1] == 'rcs':
        return f'[ARCHIVE] {_short_path(args[2])}', True
    if '-o' in args:
        destination = args[args.index('-o') + 1]
        return f'[LINK] {_short_path(destination)}', True
    if len(args) > 1 and args[1].endswith((
            'sync_game_configs.py', 'sync_engine_configs.py', 'sync_game_icons.py')):
        return f'[SYNC] {Path(args[1]).name}', False
    return '[RUN] ' + _short_path(args[0]), False

def command(args: list[str], *, cwd: Path | None = None, capture: bool = False,
            interactive: bool = False) -> str:
    global _warning_count
    full_command = '> ' + ' '.join(shlex.quote(str(s)) for s in args)
    if not capture:
        stage, diagnostic_command = _command_stage(args)
        log(stage)
        _raw(full_command + '\n')
        if _show_commands:
            print('    ' + full_command, flush=True)
    if interactive:
        result = subprocess.run(args, cwd=cwd, check=False)
    else:
        result = subprocess.run(args, cwd=cwd, text=True,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE if capture else subprocess.STDOUT,
                                errors='replace', check=False)
    if not capture and not interactive:
        output = result.stdout or ''
        _raw(output)
        lines = output.splitlines()
        warnings = sum(': warning:' in line or line.lstrip().startswith('warning:')
                       for line in lines)
        _warning_count += warnings
        if diagnostic_command:
            if result.returncode:
                # On a failed toolchain command, do not risk losing the error
                # or its include/linker context through a fragile text parser.
                print(f'    [ERROR] Command failed (exit {result.returncode}):', flush=True)
                for line in lines:
                    print('      ' + _diagnostic_header(line), flush=True)
            elif _show_warnings:
                _print_warnings(lines)
            elif any(': error:' in line or 'fatal error:' in line for line in lines):
                for line in lines:
                    print('    [ERROR] ' + _diagnostic_header(line), flush=True)
        else:
            for line in lines:
                print(line, flush=True)
    if result.returncode != 0:
        msg = result.stderr.strip() if capture else ''
        if os.name == 'nt' and result.returncode in (0xC0000139, -1073741511):
            raise BuildError(
                'Windows STATUS_ENTRYPOINT_NOT_FOUND (0xC0000139): a DLL does not export '
                'a function required by the executable or one of its dependencies. '
                'Do not mix incompatible SDL2/MinGW versions. Ensure the DLLs '
                'next to the .exe come from the same toolchain. '
                f'Command: {args[0]}'
            )
        raise BuildError(f'Command exited with {result.returncode}: {args[0]} {msg}')
    return result.stdout.strip() if capture else ''

def exe(name: str, alternative: str | None = None) -> str:
    found = shutil.which(name) or (shutil.which(alternative) if alternative else None)
    if not found:
        raise BuildError(f'Missing executable: {name}; install the required GCC toolchain / pkg-config')
    return found

def json_file(path: Path) -> dict:
    if not path.is_file():
        raise BuildError(f'Missing configuration: {path}')
    try:
        data = json.loads(path.read_text(encoding='utf-8-sig'))
    except (ValueError, UnicodeError) as e:
        raise BuildError(f'Invalid JSON {path}: {e}') from e
    if not isinstance(data, dict):
        raise BuildError(f'JSON root must be an object: {path}')
    return data

def host_platform() -> str:
    raw = sys.platform
    if raw == 'win32': return 'windows'
    if raw.startswith('linux'): return 'linux'
    if raw == 'darwin': return 'macos'
    raise BuildError(f'Host OS not supported yet: {raw}')

def resolve_platform(value: str | None, fallback: str) -> str:
    name = (value or fallback or 'auto').lower()
    if name == 'auto': name = host_platform()
    if name not in {'windows', 'linux', 'android', 'macos', 'ios', 'web'}:
        raise BuildError(f'Unknown platform: {name}')
    if name not in {'windows', 'linux'}:
        raise BuildError(f'Platform {name} reserved for future backend; currently only Windows/Linux builds are implemented')
    return name

def normalize_profile(v: str) -> str:
    v = str(v).lower()
    if v not in {'debug','release'}: raise BuildError(f'Profile must be Debug or Release, got {v}')
    return v

def normalized_arch(v: str) -> str:
    s=str(v).lower()
    if s in {'auto','x64','amd64','x86_64'}: return 'x86_64'
    raise BuildError(f'Architecture {v} not yet supported (currently x86_64 only)')

def validate_defines(values: object) -> list[str]:
    if not isinstance(values, list): raise BuildError('config.json: defines must be an array')
    out=[]
    for v in values:
        if not isinstance(v,str) or not re.fullmatch(r'[A-Za-z_]\w*(=[A-Za-z0-9_+.-]+)?',v):
            raise BuildError(f'Invalid custom define: {v!r}')
        if v.startswith('XENGINE_') or v == 'NDEBUG' or v.startswith('__'):
            raise BuildError(f'Reserved macro cannot be set by a game: {v}')
        out.append('-D'+v)
    return out

def digest(paths: list[Path], config: list[str]) -> str:
    sha=hashlib.sha256()
    for x in config: sha.update(x.encode('utf-8'));sha.update(b'\0')
    for p in sorted(paths):
        sha.update(str(p).encode('utf-8'));sha.update(b'\0')
        sha.update(p.read_bytes());sha.update(b'\0')
    return sha.hexdigest()

def all_headers(paths: list[Path]) -> list[Path]:
    return [f for p in paths for f in p.rglob('*') if f.is_file() and f.suffix.lower() in {'.h','.hpp','.hh','.hxx'}]

def collect_sources(directory: Path) -> list[Path]:
    return sorted(p for p in directory.rglob('*')
                  if p.is_file() and p.suffix.lower() in {'.cpp', '.cc', '.cxx'}) if directory.exists() else []


def asset_include_dirs(assets: Path) -> list[Path]:
    """Expose assets/ and its C++ header directories to the game compiler.

    GCC does not search -I<assets> recursively. Adding each directory that
    actually contains a header preserves simple includes like "Player.hpp"
    even when game authors organize code into new, arbitrary subfolders.
    Prefer includes relative to assets/ (e.g. "gameobjects/Player.hpp")
    when two folders might contain files with the same basename.
    """
    if not assets.is_dir():
        return [assets]
    header_suffixes = {'.h', '.hh', '.hpp', '.hxx'}
    directories = {p.parent for p in assets.rglob('*')
                   if p.is_file() and p.suffix.lower() in header_suffixes}
    directories.discard(assets)
    return [assets, *sorted(directories)]

def write_if_changed(path: Path, new: str) -> None:
    path.parent.mkdir(parents=True,exist_ok=True)
    if not path.exists() or path.read_text(encoding='utf-8') != new:
        path.write_text(new,encoding='utf-8')

class Toolchain(NamedTuple):
    compiler: str
    archiver: str
    pkgconf: str
    pkg_cflags: list[str]
    pkg_libs: list[str]
    version: str
    windres: str | None = None
    objdump: str | None = None
    readelf: str | None = None
    sysroot: Path | None = None
    lib_dirs: tuple[Path, ...] = ()
    dll_dirs: tuple[Path, ...] = ()
    cross: bool = False
    cxxflags: tuple[str, ...] = ()
    ldflags: tuple[str, ...] = ()



def _expand_home_path(value: str) -> str:
    """Expand ~ to the home directory of the account running this build.

    Resolution happens at build time; no user-specific home path is saved
    in toolchains.local.json. On Linux this usually means /home/<username>/,
    and on Windows the current user's profile directory.
    """
    if not isinstance(value, str):
        raise BuildError(f'Expected a toolchain path string, got {value!r}')
    try:
        return str(Path(value).expanduser())
    except RuntimeError as exc:
        raise BuildError(f'Cannot resolve home directory for {value!r}: {exc}') from exc


def _expand_home_paths(value: str | list[str]) -> str:
    """Expand each item of a pkg-config path list using the active user."""
    if isinstance(value, list):
        if any(not isinstance(v, str) for v in value):
            raise BuildError('pkg_config_libdir must contain only directory strings')
        parts = value
    elif isinstance(value, str):
        parts = value.split(os.pathsep)
    else:
        raise BuildError('pkg_config_libdir must be a string or a list of strings')
    return os.pathsep.join(_expand_home_path(part) for part in parts)


def _expand_home_flags(flags: tuple[str, ...]) -> tuple[str, ...]:
    """Expand home paths in compiler/linker flags without shell expansion."""
    prefixes = ('--sysroot=', '-isystem', '-iquote', '-idirafter', '-I', '-L', '-B')
    result = []
    pending_path = False
    for flag in flags:
        if pending_path:
            result.append(_expand_home_path(flag))
            pending_path = False
            continue
        if flag in ('-I', '-L', '-B', '-isystem', '-iquote', '-idirafter', '--sysroot'):
            result.append(flag)
            pending_path = True
            continue
        expanded = flag
        for prefix in prefixes:
            if flag.startswith(prefix) and len(flag) > len(prefix):
                expanded = prefix + _expand_home_path(flag[len(prefix):])
                break
        # Also allow a standalone ~/... argument in custom flags.
        if expanded == flag and (flag.startswith('~/') or flag.startswith('~\\')):
            expanded = _expand_home_path(flag)
        result.append(expanded)
    return tuple(result)


def _local_toolchain_config(host: str, target: str) -> dict:
    path = ROOT / 'tools' / 'toolchains.local.json'
    if not path.is_file():
        return {}
    data = json_file(path)
    hosts = data.get(host, {})
    if not isinstance(hosts, dict):
        raise BuildError(f'{path}: entry {host!r} must be an object')
    entry = hosts.get(target, {})
    if not isinstance(entry, dict):
        raise BuildError(f'{path}: entry {host}.{target} must be an object')
    return entry


def _tool(name: str, *, required: bool = True) -> str | None:
    if not name:
        return None
    name = _expand_home_path(str(name))
    found = shutil.which(str(name))
    if not found and Path(str(name)).is_file():
        found = str(Path(name).resolve())
    if required and not found:
        raise BuildError(f'Tool {name!r} was not found. Configure tools/toolchains.local.json and PATH.')
    return found


def prepare_tools(target: str | None = None) -> Toolchain:
    target = target or host_platform()
    host = host_platform()
    cross = target != host
    settings = _local_toolchain_config(host, target)
    if cross and not settings:
        raise BuildError(
            f'Missing cross-toolchain for {host} -> {target}. Create tools/toolchains.local.json '
            'from tools/toolchains.example.json. A target compiler, sysroot and target SDL2 SDK are required; '
            'host SDL libraries are not used.'
        )
    prefix = 'x86_64-w64-mingw32-' if target == 'windows' and cross else (
        'x86_64-linux-gnu-' if target == 'linux' and cross else '')
    compiler = _tool(settings.get('cxx', prefix+'g++'))
    archiver = _tool(settings.get('ar', prefix+'ar'))
    pkgconf = _tool(settings.get('pkg_config', 'pkg-config'))
    if target == 'windows':
        windres = _tool(settings.get('windres', prefix+'windres'), required=False)
        objdump = _tool(settings.get('objdump', prefix+'objdump'), required=False)
        readelf = None
    else:
        windres = objdump = None
        readelf = _tool(settings.get('readelf', 'readelf'), required=False)

    env = os.environ.copy()
    pkg_libdir = settings.get('pkg_config_libdir')
    # An explicit empty string disables pkg-config sysroot rewriting. This is
    # needed when the installer relocates Debian .pc prefix paths to Windows.
    pkg_sysroot_explicit = 'pkg_config_sysroot_dir' in settings
    pkg_sysroot = settings.get('pkg_config_sysroot_dir')
    sysroot = settings.get('sysroot')
    if pkg_libdir is not None:
        pkg_libdir = _expand_home_paths(pkg_libdir)
    if pkg_sysroot:
        pkg_sysroot = _expand_home_path(pkg_sysroot)
    if sysroot is not None:
        sysroot = _expand_home_path(sysroot)
    if cross:
        if not pkg_libdir:
            raise BuildError(f'{host} -> {target}: set pkg_config_libdir to the target SDL2 .pc file directories')
        if target == 'linux' and not sysroot:
            raise BuildError('Cross-compiling Linux from Windows requires a Linux x86_64 sysroot')
        # Prevent any host pkg-config .pc files from silently contaminating the target.
        env['PKG_CONFIG_LIBDIR'] = pkg_libdir
        env.pop('PKG_CONFIG_PATH', None)
        env.pop('PKG_CONFIG_SYSTEM_INCLUDE_PATH', None)
        env.pop('PKG_CONFIG_SYSTEM_LIBRARY_PATH', None)
        if host == 'windows' and target == 'linux':
            # Debian .pc files have already been explicitly relocated to SDK
            # paths. Native Windows pkgconf may otherwise infer the wrong
            # prefix from multiarch pkgconfig directories (usr/lib/include).
            env['PKG_CONFIG_DONT_DEFINE_PREFIX'] = '1'
            env['PKG_CONFIG_DONT_RELOCATE_PATHS'] = '1'
        if pkg_sysroot_explicit:
            env['PKG_CONFIG_SYSROOT_DIR'] = pkg_sysroot or ''
        elif sysroot:
            env['PKG_CONFIG_SYSROOT_DIR'] = str(sysroot)
    elif pkg_libdir:
        env['PKG_CONFIG_LIBDIR'] = pkg_libdir
    if sysroot and not Path(sysroot).is_dir():
        raise BuildError(f'Target sysroot directory does not exist: {sysroot}')
    def options_list(key):
        flags = settings.get(key, [])
        if not isinstance(flags, list) or any(not isinstance(v, str) for v in flags):
            raise BuildError(f'{key} must be a list of C++ flags')
        return tuple(flags)
    cxxflags = _expand_home_flags(options_list('cxxflags'))
    ldflags = _expand_home_flags(options_list('ldflags'))
    version = f'{compiler}: ' + command([compiler, '--version'], capture=True).splitlines()[0]
    triple = command([compiler, *cxxflags, '-dumpmachine'], capture=True).strip().lower()
    if cross and not (('linux' in triple) if target == 'linux' else
                      ('mingw' in triple or 'windows-gnu' in triple)):
        raise BuildError(f'Wrong target compiler: {compiler} reports {triple!r} '
                         f'for target={target}. Configure cxxflags/--target correctly.')
    def pc(*flags):
        args = [pkgconf, *flags, 'sdl2', 'SDL2_image', 'SDL2_ttf', 'SDL2_mixer']
        result = subprocess.run(args, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        if result.returncode:
            raise BuildError(f'pkg-config ({target}) failed: {result.stderr.strip()}\n'
                             'Check the target SDL2 .pc files and development libraries.')
        return shlex.split(result.stdout.strip(), posix=True)
    cflags = pc('--cflags')
    libs = pc('--libs')
    if os.name == 'nt':
        cflags = [c.replace('\\', '/') for c in cflags]
        libs = [c.replace('\\', '/') for c in libs]
    if cross and target == 'linux':
        # GCC --sysroot affects headers AND its linker's search paths.
        cflags.insert(0, '--sysroot='+str(Path(sysroot).resolve()))
        libs.insert(0, '--sysroot='+str(Path(sysroot).resolve()))
    def directory_list(key):
        vals = settings.get(key, [])
        if isinstance(vals, str): vals = [vals]
        if not isinstance(vals, list) or any(not isinstance(v, str) for v in vals):
            raise BuildError(f'{key} must be a list of paths')
        return tuple(Path(_expand_home_path(v)).resolve() for v in vals)
    return Toolchain(compiler, archiver, pkgconf, cflags, libs, version,
                     windres, objdump, readelf, Path(sysroot).resolve() if sysroot else None,
                     directory_list('lib_dirs'), directory_list('dll_dirs'), cross, cxxflags, ldflags)

def common_flags(target: str, profile: str) -> list[str]:
    flags=['-std=c++20','-Wall','-Wextra','-Wno-unused-parameter',
           '-DXENGINE_TARGET_'+target.upper()+'=1']
    if profile=='debug':
        flags += ['-O0','-g3','-DXENGINE_DEBUG=1']
    else:
        flags += ['-O2','-DNDEBUG','-DXENGINE_RELEASE=1']
    return flags

def source_compile_command(compiler: str, flags: list[str], cflags: list[str],
                           src: Path, obj: Path) -> list[str]:
    """One authoritative GCC invocation, reused by builds and IDE metadata."""
    return [compiler, *flags, *cflags, '-MMD', '-MP', '-MF',
            str(obj.with_suffix('.d')), '-c', str(src), '-o', str(obj)]

def compile_sources(sources: list[Path], *, objroot: Path, compiler: str,
                    flags: list[str], cflags: list[str], headers: list[Path],
                    compiler_version: str, base: Path) -> tuple[list[Path], bool]:
    objroot.mkdir(parents=True,exist_ok=True)
    header_digest=digest(headers,[])
    objects=[]
    changed=False
    for src in sources:
        try: rel=src.relative_to(base)
        except ValueError: rel=Path(src.name)
        obj=objroot / rel.with_suffix('.o')
        sig=obj.with_suffix('.sha256')
        signature=digest([src],[header_digest,compiler_version,*flags,*cflags])
        if not (obj.is_file() and sig.is_file() and sig.read_text()==signature):
            obj.parent.mkdir(parents=True,exist_ok=True)
            command(source_compile_command(compiler, flags, cflags, src, obj))
            sig.write_text(signature)
            changed=True
        objects.append(obj)
    return objects,changed


def regenerate_engine_api_docs() -> None:
    """Refresh the optional local API reference after a successful engine rebuild.

    Documentation errors should never prevent a developer from compiling or
    running their game. Both Doxyfiles treat documentation warnings as
    errors, so a failed run is still reported prominently here.
    """
    if not all(path.is_file() for path in DOXYFILES):
        log('[WARN] Doxyfile_Games or Doxyfile_Engine is missing; '
            'skipping XEngine API documentation.')
        return
    if docs_current(ROOT):
        log('[DOCS] API references already match the source contents; '
            'skipping Doxygen regeneration')
        return
    doxygen = shutil.which('doxygen')
    if not doxygen:
        log('[WARN] Doxygen is not installed; skipping XEngine API documentation. '
            'If you plan to modify the engine or develop games with XEngine, '
            'install Doxygen so its API reference is regenerated whenever the engine recompiles.')
        return

    log('[DOCS] Regenerating XEngine game and engine references (Doxygen)')
    try:
        result = subprocess.run([sys.executable, str(ROOT / 'tools' / 'build_doxygen.py'),
                                 '--doxygen', doxygen], cwd=ROOT, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                errors='replace', check=False)
        output = result.stdout or ''
        _raw(output)
        if result.returncode:
            log(f'[WARN] Doxygen exited with status {result.returncode}; '
                'the engine was compiled successfully, but its API documentation '
                'may be incomplete. Run `python tools/build_doxygen.py` to inspect the errors.')
            for line in output.splitlines()[-30:]:
                log(f'    {line}')
        else:
            log('[DOCS] API reference: build/docs/index.html (game / engine)')
    except OSError as error:
        log(f'[WARN] Cannot run Doxygen: {error}. The engine build was not affected.')

def create_engine(*,target,profile,arch,tooling,no_doc:bool=False) -> Path:
    compiler,archiver,pkg_cflags,version=tooling.compiler,tooling.archiver,tooling.pkg_cflags,tooling.version
    out=ROOT/'build'/f'{target}-{arch}'/profile/'engine'
    lib=out/'libXEngine.a'
    sources=collect_sources(ENGINE_SRC)
    if not sources: raise BuildError('No engine source files')
    flags=[*common_flags(target,profile),*tooling.cxxflags]
    cflags=['-I'+str(ENGINE_HEADERS),*pkg_cflags]
    objs,modified=compile_sources(sources,objroot=out/'obj',compiler=compiler,flags=flags,
                                   cflags=cflags,headers=all_headers([ENGINE_HEADERS]),
                                   compiler_version=version,base=ENGINE_SRC)
    if modified or not lib.is_file():
        # Remove stale library before archiving, so removed .cpp files are not retained.
        if lib.exists(): lib.unlink()
        command([archiver,'rcs',str(lib),*(str(o) for o in objs)])
        if no_doc:
            log('[DOCS] Skipped local Doxygen generation (--no-doc; online reference preferred)')
        else:
            regenerate_engine_api_docs()
    else: log('[CACHE] XEngine library unchanged')
    return lib

def encode_config(content: bytes) -> str:
    encoded=[c ^ ((i*73+0xA5)&0xFF) for i,c in enumerate(content)]
    # Avoid storing a readable JSON literal inside the binary; this is obfuscation, not encryption.
    chunks=[', '.join(str(k) for k in encoded[i:i+24]) for i in range(0,len(encoded),24)]
    initial=',\n'.join('    '+c for c in chunks)
    return f'''#include "GameBuild.generated.h"
#include <cstddef>
#include <cstdint>

namespace XEngineGameBuild {{
const char* DevelopmentAssetsPath() noexcept {{ return ""; }}
std::string EmbeddedEngineConfig() {{
    static constexpr uint8_t bytes[] = {{
{initial}
    }};
    std::string result;
    result.resize(sizeof(bytes));
    for (std::size_t i = 0; i < sizeof(bytes); ++i)
        result[i] = static_cast<char>(bytes[i] ^ ((i * 73u + 0xA5u) & 0xffu));
    return result;
}}
}}
'''

def generate_game_config(root: Path, out: Path, profile: str, *, cross: bool = False) -> Path:
    gen_dir=out/'generated';gen_dir.mkdir(parents=True,exist_ok=True)
    header=gen_dir/'GameBuild.generated.h'
    write_if_changed(header,'''#pragma once
#include <string>
namespace XEngineGameBuild {
    const char* DevelopmentAssetsPath() noexcept;
    std::string EmbeddedEngineConfig();
}
''')
    cfg=root/'assets'/'config'/'engine.json'
    config=json_file(cfg)
    assets=root/'assets'
    if profile=='debug':
        # Path in a generated source, never passed as -D (safe with spaces on Windows).
        literal=('./' if cross else str(assets.resolve()))
        generated=f'''#include "GameBuild.generated.h"
namespace XEngineGameBuild {{
const char* DevelopmentAssetsPath() noexcept {{ return R"XENGPATH({literal})XENGPATH"; }}
std::string EmbeddedEngineConfig() {{ return {{}}; }}
}}
'''
    else:
        engine=config.setdefault('Engine',{})
        if not isinstance(engine,dict): raise BuildError('Engine config entry must be an object')
        engine['logStats']=False
        asset_config=config.setdefault('Assets',{})
        if not isinstance(asset_config,dict): raise BuildError('Assets config entry must be an object')
        asset_config['assetsFolderPath']='./'
        text=json.dumps(config,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode('utf-8')
        generated=encode_config(text)
    source=gen_dir/'GameBuild.generated.cpp'
    write_if_changed(source,generated)
    return source

def _icon_config(configuration: dict, target: str) -> str | None:
    """Return platform-specific icon path, with backward-compatible Windows .ico strings.

    Accepted forms:
      null
      "assets/icons/game.ico"            (legacy: Windows only)
      "default"                           (Windows and Linux)
      {"windows": "default", "linux": "default"}
      {"windows": "...ico", "linux": "...png"}
    """
    project = configuration.get('project', {})
    if not isinstance(project, dict):
        raise BuildError('config.json: project must be an object')
    icon = project.get('icon')
    if icon is None or icon == '':
        return None
    if isinstance(icon, str):
        # Existing projects specifying a Windows ICO must also compile on Linux.
        return icon if (target == 'windows' or icon == 'default') else None
    if not isinstance(icon, dict):
        raise BuildError('config.json: project.icon must be null, a Windows .ico path, or an object with windows/linux paths')
    unknown = set(icon) - {'windows', 'linux'}
    if unknown:
        raise BuildError(f'config.json: unsupported icon platforms: {sorted(unknown)}')
    selected = icon.get(target)
    if selected is None or selected == '':
        return None
    if not isinstance(selected, str):
        raise BuildError(f'config.json: project.icon.{target} must be a path string or null')
    return selected


def project_icon(game: Path, configuration: dict, target: str = 'windows') -> Path | None:
    """Resolve and validate a platform icon without permitting asset path traversal."""
    raw = _icon_config(configuration, target)
    if raw is None:
        return None
    if target not in ('windows', 'linux'):
        raise BuildError(f'Unsupported icon target: {target}')
    default_path = game / 'assets' / 'icons' / ('xengine_icon.ico' if target == 'windows' else 'xengine_icon.png')
    if raw == 'default':
        raw = 'assets/icons/' + default_path.name
    raw = raw.replace('\\', '/')
    relative = Path(raw)
    if (not raw.strip() or relative.is_absolute() or ':' in raw
            or any(part == '..' for part in relative.parts)
            or any(ord(char) < 32 for char in raw)):
        raise BuildError(f'project.icon.{target} must be relative to the game directory: {raw!r}')
    suffix = {'windows': '.ico', 'linux': '.png'}.get(target)
    if suffix is None:
        raise BuildError(f'Unsupported icon target: {target}')
    if relative.suffix.lower() != suffix:
        raise BuildError(f'project.icon.{target} must refer to a {suffix} file: {raw}')
    full = (game / relative).resolve()
    if not full.is_relative_to(game.resolve()):
        raise BuildError('project.icon cannot point outside the game')
    if not full.is_file():
        # A missing *safe* custom path falls back to the game's own synchronized
        # default. Do not hide invalid paths, malformed files, or symlink escapes.
        if full == default_path.resolve():
            raise BuildError(f'Default game icon not found: {full}; run tools/sync_game_icons.py')
        log(f'[ICON] WARNING | project.icon.{target} refers to a missing file: {relative}. '
            f'Using assets/icons/{default_path.name} instead.')
        full = default_path.resolve()
        if not full.is_relative_to(game.resolve()) or not full.is_file():
            raise BuildError(f'Fallback default icon not found: {full}; run tools/sync_game_icons.py')
    data = full.read_bytes()
    if target == 'windows':
        # ICO directory header: reserved=0, type=1, image_count>=1
        if (len(data) < 22 or data[:4] != b'\x00\x00\x01\x00'
                or int.from_bytes(data[4:6], 'little') < 1):
            raise BuildError(f'Invalid Windows ICO file: {full}')
    else:
        # PNG signature, first chunk IHDR and positive dimensions.
        if (len(data) < 33 or data[:8] != b'\x89PNG\r\n\x1a\n'
                or data[8:12] != b'\x00\x00\x00\x0d'
                or data[12:16] != b'IHDR'
                or int.from_bytes(data[16:20], 'big') == 0
                or int.from_bytes(data[20:24], 'big') == 0):
            raise BuildError(f'Invalid Linux PNG icon: {full}')
    return full


def linux_desktop_id(project: str) -> str:
    """Stable, safe basename for the application launcher; keep display name intact."""
    slug = re.sub(r'[^a-z0-9]+', '-', project.casefold()).strip('-') or 'game'
    suffix = hashlib.sha256(project.encode('utf-8')).hexdigest()[:8]
    return f'xengine-{slug[:48]}-{suffix}'


def linux_desktop_installer(project: str, icon_name: str | None) -> str:
    """Render a Bash helper that refreshes absolute desktop-entry paths after a move."""
    template = (ROOT / 'tools' / 'linux_desktop_setup.sh.in').read_text(encoding='utf-8')
    replacements = {
        '__NAME__': shlex.quote(project),
        '__APP_ID__': shlex.quote(linux_desktop_id(project)),
        '__ICON_REL__': shlex.quote(f'icons/{icon_name}' if icon_name else ''),
    }
    for key, value in replacements.items():
        template = template.replace(key, value)
    return template


def compile_windows_icon(game: Path, configuration: dict, out: Path, target: str, tooling: Toolchain | None = None) -> tuple[Path | None, bool]:
    """Generate a COFF icon resource using MSYS2's windres (no CMake)."""
    icon = project_icon(game, configuration, 'windows') if target == 'windows' else None
    if icon is None:
        return None, False
    try:
        windres = tooling.windres if tooling else exe('windres', 'x86_64-w64-mingw32-windres')
        if not windres:
            raise BuildError('windres is unavailable for the Windows target')
    except BuildError as exc:
        raise BuildError('windres was not found: install mingw-w64-ucrt-x86_64-binutils in MSYS2 UCRT64, or configure target windres, to embed project.icon') from exc
    generated = out / 'generated'
    generated.mkdir(parents=True, exist_ok=True)
    rc = generated / 'GameIcon.generated.rc'
    obj = generated / 'GameIcon.generated.o'
    signature_path = generated / 'GameIcon.generated.sha256'
    # RC strings are double-quoted; forward slashes avoid backslash escaping.
    rc_text = '1 ICON "' + icon.as_posix() + '"\n'
    write_if_changed(rc, rc_text)
    signature = digest([icon, rc], [windres])
    if obj.is_file() and signature_path.is_file() and signature_path.read_text() == signature:
        return obj, False
    command([windres, '--input-format=rc', '--output-format=coff',
             '--target=pe-x86-64', '-i', str(rc), '-o', str(obj)])
    signature_path.write_text(signature, encoding='ascii')
    log(f'[ICON] Embedded {icon.relative_to(game)} in the executable')
    return obj, True


def link_signature(objects: list[Path], library: Path, icon_object: Path | None,
                   compiler: str, linker_options: list[str]) -> str:
    """Track link inputs so icon updates/removals force a new executable."""
    files = [*objects, library]
    if icon_object is not None:
        files.append(icon_object)
    entries = [compiler, *linker_options]
    for item in files:
        stat = item.stat()
        entries.append(f'{item}:{stat.st_size}:{stat.st_mtime_ns}')
    return hashlib.sha256('\0'.join(entries).encode('utf-8')).hexdigest()


def game_sources(game: Path, target: str, generated: Path) -> list[Path]:
    # All C++ translation units inside assets/ are part of the game, regardless
    # of folder name. Header-only .hpp files compile when included by a .cpp.
    sources=[*collect_sources(game/'src'), *collect_sources(game/'assets'),
             *collect_sources(game/'platform'/'desktop'),
             *collect_sources(game/'platform'/target), generated]
    if not (game/'src'/'main.cpp').exists():
        raise BuildError(f'Missing game entry point: {game}/src/main.cpp')
    return sorted(set(sources))

def sync_before_game_build(project: str) -> None:
    """Sync game manifest, runtime settings and default icons before compilation.

    Each command streams output directly to the terminal (and benchmark tee
    logs). A nonzero exit aborts immediately, never using an old executable.
    """
    for tool in ('sync_game_configs.py', 'sync_engine_configs.py', 'sync_game_icons.py'):
        command([sys.executable, str(ROOT / 'tools' / tool), '--game', project], cwd=ROOT)


def game_build(project: str, *,target,profile,arch,tooling,run:bool=False,
               no_doc:bool=False) -> Path:
    game=ROOT/'games'/project
    if not game.is_dir(): raise BuildError(f'Game not found: games/{project}')
    sync_before_game_build(project)
    config=json_file(game/'config.json')
    name=config.get('project',{}).get('name')
    if name!=project: raise BuildError(f'config project.name={name!r} must match directory games/{project}')
    version=config.get('project',{}).get('version','0.1.0')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,63}',str(version)):
        raise BuildError('Invalid game version')
    if profile=='release' and config.get('shipping',{}).get('copy_runtime_assets',True) is False:
        raise BuildError('Release must include runtime assets for a shippable game')
    compiler,pkg_cflags,pkg_libs,compiler_ver=tooling.compiler,tooling.pkg_cflags,tooling.pkg_libs,tooling.version
    library=create_engine(target=target,profile=profile,arch=arch,tooling=tooling,
                          no_doc=no_doc)
    out=ROOT/'build'/f'{target}-{arch}'/profile/'games'/project
    generated=generate_game_config(game,out,profile,cross=tooling.cross)
    sources=game_sources(game,target,generated)
    flags=[*common_flags(target,profile),*tooling.cxxflags,*validate_defines(config.get('defines',[]))]
    # assets/ itself supports #include "gameobjects/Player.hpp". Its header
    # subdirectories also support #include "Player.hpp" and arbitrary folders.
    cflags=['-I'+str(ENGINE_HEADERS),
            *('-I'+str(folder) for folder in asset_include_dirs(game/'assets')),
            '-I'+str(out/'generated'), *pkg_cflags]
    objects,changed=compile_sources(sources,objroot=out/'obj',compiler=compiler,
                                    flags=flags,cflags=cflags,
                                    headers=all_headers([ENGINE_HEADERS,game/'assets',
                                                         game/'src',out/'generated']),
                                    compiler_version=compiler_ver,base=game)
    icon_object, icon_changed = compile_windows_icon(game, config, out, target, tooling=tooling)
    filename=project+('.exe' if target=='windows' else '')
    output=out/filename
    link_options=[*tooling.cxxflags,*tooling.ldflags,*pkg_libs]
    if target=='windows': link_options+=['-ladvapi32']  # PlayerPrefs: HKCU Registry API
    if target=='linux': link_options+=['-Wl,-rpath,$ORIGIN/lib']
    if profile=='release': link_options+=['-s']
    link_sig_path=out/'GameLink.sha256'
    current_link_sig=link_signature(objects, library, icon_object, compiler, link_options)
    if (changed or icon_changed or not output.exists() or not link_sig_path.is_file()
            or link_sig_path.read_text() != current_link_sig):
        linkargs=[compiler,*(str(o) for o in objects),str(library)]
        if icon_object is not None: linkargs.append(str(icon_object))
        linkargs+=['-o',str(output),*link_options]
        command(linkargs)
        link_sig_path.write_text(current_link_sig, encoding='ascii')
    else: log(f'[CACHE] {project} binary unchanged')
    log(f'[OK] {output}')
    if profile == 'release':
        packaged = package_game(game, project, str(version), target, arch, output, tooling=tooling)
        log(f'[SHIP] {packaged}')
        if run:
            command([str(packaged / filename)], cwd=packaged, interactive=True)
    else:
        # A successful link does NOT make a MinGW Debug binary runnable.  The
        # loader first looks beside the executable and otherwise searches PATH,
        # which may contain incompatible SDL2/GCC runtime DLLs (0xC0000139).
        # Synchronize dependencies even WITHOUT --run: benchmark scripts and
        # IDEs normally build first and launch the .exe in a separate process.
        # Do this on cached builds as well, to repair stale or missing DLLs.
        if target == 'windows':
            dlls = package_windows_libs(output.parent, output, tooling=tooling)
            log(f'[RUNTIME] Synchronized {len(dlls)} MinGW/SDL2 DLLs '
                'next to the Debug executable (including --build without --run)')
        if run:
            command([str(output)], cwd=game / 'assets', interactive=True)
    return output

def package_game(game:Path, project:str,version:str,target:str,arch:str,output:Path, *, tooling: Toolchain | None = None) -> Path:
    destination=game/'dist'/f'{target}-{arch}'/version
    stage=destination.with_name(destination.name+'.staging')
    if stage.exists(): shutil.rmtree(stage)
    stage.mkdir(parents=True)
    shutil.copy2(output,stage/output.name)
    if target == 'linux' and os.name != 'nt':
        # Native Linux builds can set POSIX permissions immediately. A Windows
        # host cannot reliably store these bits in an ordinary Windows folder.
        (stage/output.name).chmod(0o755)
    assets=game/'assets'
    # The configured icon is already inside the Windows .exe resource table.
    cfg=json_file(game/'config.json')
    configured_icon = project_icon(game, cfg, target)
    for p in assets.rglob('*'):
        if not p.is_file(): continue
        if configured_icon is not None and p.resolve() == configured_icon: continue
        rel=p.relative_to(assets)
        if p.suffix.lower() in {'.cpp','.h','.hpp','.hh','.cxx','.c'}: continue
        if rel.as_posix().lower()=='config/engine.json': continue
        if p.name=='.gitkeep': continue
        targetfile=stage/rel
        targetfile.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(p,targetfile)
    if target == 'linux':
        # Standardized location; files such as assets/art/game.png also work.
        packaged_icon_name = None
        if configured_icon is not None:
            packaged_icon_name = linux_desktop_id(project) + '.png'
            icon_dest = stage / 'icons' / packaged_icon_name
            icon_dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(configured_icon, icon_dest)
        desktop_installer = stage / 'setup_desktop.sh'
        desktop_installer.write_text(
            linux_desktop_installer(project, packaged_icon_name), encoding='utf-8', newline='\n'
        )
        desktop_installer.chmod(0o755)
    (stage/'version.json').write_text(json.dumps({
        'game':project,'version':version,'platform':target,'architecture':arch,
        'configuration':'Release','engine_library':'statically linked'
    },indent=2),encoding='utf-8')
    # Ship the transitive SDL dependencies with the package when possible.
    if target=='windows':
        if tooling: package_windows_libs(stage, output, tooling=tooling)
        else: package_windows_libs(stage, output)
    elif target=='linux':
        if tooling and tooling.cross: package_linux_libs_cross(stage, output, tooling)
        elif tooling: package_linux_libs(stage, output)
        else: package_linux_libs(stage,output)
    if destination.exists(): shutil.rmtree(destination)
    stage.rename(destination)
    if target == 'linux':
        # A .tar.gz stores Linux executable bits even when built on Windows.
        # Create it before native setup_desktop.sh adds host-specific paths.
        write_linux_release_archive(destination, output.name, project)
    elif target == 'windows':
        # Ship a complete Windows Release (binary, DLLs, assets) as one ZIP.
        write_windows_release_archive(destination, project)
    if target == 'linux':
        # Absolute Linux paths are unknown on a Windows cross-build host.
        # setup_desktop.sh generates the desktop entry when run on Linux.
        if tooling and tooling.cross:
            log('[DESKTOP] Run ./setup_desktop.sh on Linux after copying the Release package')
        elif os.name != 'nt':
            command(['bash', str(destination / 'setup_desktop.sh')])
        else:
            log('[DESKTOP] Run ./setup_desktop.sh on Linux after transferring the Release package')
    return destination

def _gcc_target_dll(compiler: str, dll: str) -> Path | None:
    """Locate a MinGW runtime DLL using the *configured target* GCC.

    Linux distributions usually keep GCC runtime DLLs in a versioned directory
    such as /usr/lib/gcc/x86_64-w64-mingw32/<version>-posix, not next to g++
    in /usr/bin. We must not search arbitrary PATH directories: they may
    contain binaries from an incompatible MinGW runtime or host architecture.
    """
    def gcc_query(argument: str) -> str | None:
        try:
            result = subprocess.run([compiler, argument], capture_output=True,
                                    text=True, check=False)
        except OSError:
            return None
        if result.returncode:
            return None
        return result.stdout.strip() or None

    # GCC knows its own target library search paths and the runtime variant
    # selected by its alternatives (e.g. Debian MinGW POSIX vs Win32 threads).
    location = gcc_query(f'-print-file-name={dll}')
    if location and Path(location).parent != Path('.'):
        path = Path(location).expanduser().resolve()
        if path.is_file() and path.name.lower() == dll.lower():
            return path

    # On some distributions -print-file-name= returns only the bare DLL name,
    # whereas -print-libgcc-file-name still identifies GCC's versioned folder.
    libgcc = gcc_query('-print-libgcc-file-name')
    if libgcc and Path(libgcc).parent != Path('.'):
        base = Path(libgcc).expanduser().resolve()
        candidate = base.parent / dll
        if base.is_file() and candidate.is_file():
            return candidate
    return None


def package_windows_libs(stage: Path, output: Path, *, tooling: Toolchain | None = None) -> list[str]:
    """Bundle transitive MinGW DLLs from the *same* GCC toolchain in Debug/Release.

    On Windows the loader resolves DLLs beside the executable before PATH. This
    prevents an unrelated SDL/Anaconda/old MinGW DLL from shadowing UCRT64 SDL.
    System DLLs are never copied; only toolchain-provided DLLs are staged.
    """
    objdump = (tooling.objdump if tooling else None) or exe('objdump')
    compiler_dir = Path(tooling.compiler if tooling else exe('g++')).parent
    search_dirs = [compiler_dir, *(tooling.dll_dirs if tooling else ())]
    windows = Path(os.environ.get('SystemRoot', r'C:\Windows')) / 'System32'
    found: set[str] = set()
    pending = [output]
    copied: list[str] = []
    system_names = {
        'kernel32.dll', 'kernelbase.dll', 'user32.dll', 'gdi32.dll',
        'ole32.dll', 'advapi32.dll', 'shell32.dll', 'winmm.dll', 'imm32.dll',
        'version.dll', 'setupapi.dll', 'ws2_32.dll', 'msvcrt.dll',
        'ucrtbase.dll', 'comdlg32.dll', 'comctl32.dll', 'shlwapi.dll',
        'ntdll.dll', 'oleaut32.dll', 'crypt32.dll', 'bcrypt.dll', 'hid.dll',
        'dwmapi.dll', 'secur32.dll', 'opengl32.dll', 'winspool.drv',
        'rpcrt4.dll', 'cfgmgr32.dll', 'd3d11.dll', 'dxgi.dll',
        'd3dcompiler_47.dll', 'dwrite.dll', 'dxguid.dll', 'avrt.dll',
        'uxtheme.dll', 'psapi.dll', 'userenv.dll', 'iphlpapi.dll',
        'dbghelp.dll', 'msimg32.dll', 'winhttp.dll', 'wininet.dll',
        'urlmon.dll', 'netapi32.dll', 'mswsock.dll', 'dnsapi.dll',
        'mpr.dll', 'powrprof.dll', 'wintrust.dll', 'cryptbase.dll',
        # Windows-provided Unicode text shaping and common OS components.
        # These can be imported indirectly by SDL2_ttf/SDL2_image/SDL2_mixer.
        'usp10.dll', 'normaliz.dll', 'msctf.dll', 'msacm32.dll',
        'windowscodecs.dll', 'wtsapi32.dll', 'shcore.dll', 'propsys.dll',
        'mmdevapi.dll', 'bcryptprimitives.dll', 'ncrypt.dll',
        'oleacc.dll', 'dinput8.dll', 'dsound.dll', 'd3d9.dll',
        'd3d12.dll', 'd2d1.dll', 'dcomp.dll', 'dxva2.dll',
    }
    while pending:
        src = pending.pop()
        printed = command([objdump, '-p', str(src)], capture=True)
        for dll in re.findall(r'DLL Name:\s*(\S+)', printed, re.I):
            key = dll.lower()
            if key in found or key.startswith(('api-ms-win', 'ext-ms-win')):
                continue
            found.add(key)
            # Imports provided by Windows itself must never be packaged or
            # queried through MinGW; only third-party/runtime DLLs belong here.
            if key in system_names:
                continue
            # A toolchain DLL takes precedence even when the Windows system
            # directory contains an unrelated DLL with the same basename.
            candidate = next((p / dll for p in search_dirs if (p / dll).is_file()), None)
            if candidate is None and tooling and tooling.cross:
                # The MinGW GCC runtime often lives outside its bin directory.
                # Ask the target compiler rather than copying an unrelated DLL.
                candidate = _gcc_target_dll(tooling.compiler, dll)
            if candidate is None:
                if key in system_names or (not tooling or not tooling.cross) and (windows / dll).is_file():
                    continue
                raise BuildError(
                    f'Required DLL {dll} for {src.name} was not found in {search_dirs}. '
                    'Ensure SDL2 and GCC use a compatible MinGW-w64 toolchain. '
                    'DLLs from unrelated installations in PATH will not be copied.'
                )
            # System DLLs should always use the OS implementation.
            if key in system_names:
                continue
            shutil.copy2(candidate, stage / dll)
            copied.append(dll)
            pending.append(candidate)
    if 'sdl2.dll' not in found:
        raise BuildError('SDL2.dll was not found among the executable dependencies')
    return copied

def package_linux_libs(stage:Path,output:Path) -> None:
    if not shutil.which('ldd'): raise BuildError('Missing ldd: cannot verify Linux release dependencies')
    printed=command(['ldd',str(output)],capture=True)
    if 'not found' in printed: raise BuildError(f'Unresolved Linux dependencies:\n{printed}')
    system_names={'linux-vdso.so.1','libc.so.6','libm.so.6','libpthread.so.0',
                  'libdl.so.2','librt.so.1','libresolv.so.2','libutil.so.1',
                  'libnsl.so.1','libanl.so.1','libthread_db.so.1'}
    for line in printed.splitlines():
        m=re.match(r'\s*(\S+)\s+=>\s+(/\S+)',line)
        if not m: continue
        name,source=m.groups()
        if name in system_names or name.startswith(('ld-linux','ld-musl')): continue
        if not Path(source).is_file(): raise BuildError(f'Cannot package dependency {name}: {source}')
        (stage/'lib').mkdir(exist_ok=True)
        shutil.copy2(source,stage/'lib'/name)
    write_linux_run(stage, output.name)


def locate_linux_target_library(name: str, roots: list[Path]) -> Path | None:
    """Find an ELF dependency within target library directories, not the host.

    Debian stores some DT_NEEDED libraries in private subdirectories, for
    example usr/lib/x86_64-linux-gnu/pulseaudio/libpulsecommon-16.1.so.
    Search normal library directories first, then descend into target-only
    subdirectories. Refuse ambiguous matches instead of silently selecting
    an unrelated provider of the same SONAME.
    """
    # A DT_NEEDED entry must be a filename, not an arbitrary relative path.
    if not name or name in ('.', '..') or '/' in name or '\\' in name:
        raise BuildError(f'Invalid Linux DT_NEEDED library name: {name!r}')

    search_roots: list[Path] = []
    seen_roots: set[Path] = set()
    for folder in roots:
        resolved = folder.resolve()
        if resolved.is_dir() and resolved not in seen_roots:
            search_roots.append(resolved)
            seen_roots.add(resolved)

    # Preserve direct-path precedence (e.g. the target's multiarch ABI).
    for folder in search_roots:
        candidate = folder / name
        if candidate.is_file():
            return candidate

    # Private library dirs only: no arbitrary PATH lookup or host ldd.
    # A broader root may overlap a more specific one; deduplicate matches.
    matches: dict[Path, Path] = {}
    for folder in search_roots:
        for candidate in folder.rglob(name):
            if candidate.is_file():
                matches.setdefault(candidate.resolve(), candidate)
    if len(matches) > 1:
        locations = ', '.join(str(p) for p in sorted(matches.values()))
        raise BuildError(f'Ambiguous target Linux dependency {name}: {locations}. '
                         'Configure a more specific target lib_dirs entry.')
    return next(iter(matches.values()), None)


def package_linux_libs_cross(stage: Path, output: Path, tooling: Toolchain) -> None:
    """Resolve ELF DT_NEEDED transitively using the target sysroot, never host ldd.

    The cross toolchain supplies Linux ELF files; host ldd cannot inspect them.
    Missing libraries fail closed rather than copying the host's .so files.
    """
    if not tooling.readelf:
        raise BuildError('Linux cross-compiled Release requires readelf in tools/toolchains.local.json')
    if not tooling.sysroot:
        raise BuildError('Linux cross-compiled Release requires a Linux sysroot')
    roots = [*tooling.lib_dirs]
    for sub in ('lib/x86_64-linux-gnu', 'usr/lib/x86_64-linux-gnu',
                'lib64', 'usr/lib64', 'lib', 'usr/lib', 'usr/local/lib'):
        candidate = tooling.sysroot / sub
        if candidate.is_dir(): roots.append(candidate)
    # Search locations returned by target pkg-config, plus the sysroot.
    for arg in tooling.pkg_libs:
        if arg.startswith('-L') and len(arg) > 2:
            folder = Path(arg[2:])
            if folder.is_dir(): roots.insert(0, folder)
    seen = set()
    ignored = {'linux-vdso.so.1', 'libc.so.6', 'libm.so.6', 'libpthread.so.0',
               'libdl.so.2', 'librt.so.1', 'libresolv.so.2', 'libutil.so.1',
               'libnsl.so.1', 'libanl.so.1', 'libthread_db.so.1'}
    pending = [output]
    target_lib = stage / 'lib'
    while pending:
        binary = pending.pop()
        printed = command([tooling.readelf, '-d', str(binary)], capture=True)
        names = re.findall(r'\(NEEDED\)\s+Shared library: \[([^\]]+)\]', printed)
        for name in names:
            if name in seen or name in ignored or name.startswith(('ld-linux', 'ld-musl')):
                continue
            seen.add(name)
            source = locate_linux_target_library(name, roots)
            if source is None:
                raise BuildError(f'Missing {name} in the Linux target sysroot/lib_dirs '
                                 '(including nested library directories). '
                                 'Verify the SDK contains the corresponding target ELF package; '
                                 'do not copy host libraries.')
            target_lib.mkdir(exist_ok=True)
            packaged = target_lib / name
            shutil.copy2(source, packaged, follow_symlinks=True)
            pending.append(packaged)
    write_linux_run(stage, output.name)


def write_linux_release_archive(destination: Path, binary_name: str, project: str) -> Path:
    """Archive the Release contents at the root, preserving Linux POSIX modes.

    The unpacked Release remains in dist/<target>/<version>/, but the tarball
    contains the executable, scripts, libraries and assets directly. This also
    preserves execute permissions when a cross-build originates on Windows.
    """
    archive = destination.parent / f'{project} - {destination.name}.tar.gz'
    temp_archive = archive.with_name(archive.name + '.tmp')
    executable_names = {binary_name, 'run.sh', 'setup_desktop.sh'}

    def linux_modes(member: tarfile.TarInfo) -> tarfile.TarInfo:
        if member.isdir():
            member.mode = 0o755
        elif member.isfile():
            # Only root-level executable files need execute permission.
            # Everything under assets or lib stays readable (0644).
            member.mode = (0o755 if '/' not in member.name and
                           member.name in executable_names else 0o644)
        return member

    try:
        with tarfile.open(temp_archive, 'w:gz', format=tarfile.PAX_FORMAT) as package:
            for entry in sorted(destination.rglob('*')):
                member = entry.relative_to(destination).as_posix()
                # Add entries individually; adding destination itself would
                # create an unwanted <version>/ directory in the archive.
                package.add(entry, arcname=member, recursive=False, filter=linux_modes)
        os.replace(temp_archive, archive)
    finally:
        if temp_archive.exists():
            temp_archive.unlink()
    log(f'[ARCHIVE] {archive} (flat Release; Linux execute permissions preserved)')
    return archive


def write_windows_release_archive(destination: Path, project: str) -> Path:
    """Archive the Windows executable, DLLs and assets at the ZIP root."""
    archive = destination.parent / f'{project} - {destination.name}.zip'
    temp_archive = archive.with_name(archive.name + '.tmp')
    try:
        with zipfile.ZipFile(temp_archive, 'w', compression=zipfile.ZIP_DEFLATED,
                             compresslevel=6, allowZip64=True) as package:
            for entry in sorted(destination.rglob('*')):
                # Each member is relative to the Release directory itself,
                # not to its parent: no <version>/ wrapper on extraction.
                member = entry.relative_to(destination).as_posix()
                package.write(entry, arcname=member)
        os.replace(temp_archive, archive)
    finally:
        if temp_archive.exists():
            temp_archive.unlink()
    log(f'[ARCHIVE] {archive} (flat Release; Windows executable, DLLs and assets)')
    return archive


def write_linux_run(stage: Path, filename: str) -> None:
    """Create a relocatable Linux launcher, including after Windows file copies.

    The wrapper can be run using `bash run.sh` when executable mode bits were
    lost while copying a loose Release directory from Windows to Linux.
    """
    wrapper = (
        '#!/usr/bin/env bash\n'
        'set -euo pipefail\n'
        'HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"\n'
        'cd "$HERE"\n'
        'export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n'
        'APP="$HERE"/' + shlex.quote(filename) + '\n'
        '# Copies through Windows filesystems may lose POSIX executable bits.\n'
        'if [[ ! -x "$APP" ]]; then\n'
        '    if ! chmod u+x -- "$APP" || [[ ! -x "$APP" ]]; then\n'
        '        printf "[ERROR] Cannot make the Linux game executable: %s\\n" "$APP" >&2\n'
        '        printf "[ERROR] Extract the Linux .tar.gz on a writable executable filesystem.\\n" >&2\n'
        '        exit 1\n'
        '    fi\n'
        'fi\n'
        'exec "$APP" "$@"\n'
    )
    script = stage / 'run.sh'
    script.write_text(wrapper, encoding='utf-8', newline='\n')
    script.chmod(0o755)

def main():
    global _raw_log, _raw_log_path, _show_warnings, _show_commands, _warning_count
    status = 1
    parser=argparse.ArgumentParser(description='Direct GCC/MinGW XEngine build (no CMake)')
    commands=parser.add_subparsers(dest='action',required=True)
    for action in ('engine','game'):
        p=commands.add_parser(action)
        p.add_argument('--platform',default=None)
        p.add_argument('--config','--configuration',dest='configuration',default=None)
        p.add_argument('--arch',default=None)
        p.add_argument('--show', action='append', choices=('warnings', 'commands'),
                       default=[], metavar='{warnings,commands}',
                       help='Show compiler warnings and/or full commands in the console')
        p.add_argument('--show-warnings', action='store_true',
                       help='Alias for --show warnings')
        p.add_argument('--raw-log', type=Path,
                       help='Save all commands and unfiltered compiler output to this file')
        p.add_argument('--no-doc', action='store_true',
                       help='Skip Doxygen and prefer the online reference in XEngine Doxygen Hover')
        if action=='game':
            x=p.add_mutually_exclusive_group(required=True)
            x.add_argument('--project')
            x.add_argument('--project-dir')
            p.add_argument('--run',action='store_true')
    args=parser.parse_args()
    try:
        game=None
        if args.action=='game':
            game=(Path(args.project_dir).resolve() if args.project_dir else ROOT/'games'/args.project)
            if game.parent != (ROOT/'games').resolve():
                raise BuildError(f'Game must be directly inside games/: {game}')
            args.project=game.name
            project_json=json_file(game/'config.json')
            bc=project_json.get('build',{})
        else: bc={}
        target=resolve_platform(args.platform,bc.get('platform','auto'))
        profile=normalize_profile(args.configuration or bc.get('configuration','Debug'))
        arch=normalized_arch(args.arch or bc.get('architecture','auto'))
        _show_warnings = args.show_warnings or 'warnings' in args.show
        _show_commands = 'commands' in args.show
        _warning_count = 0
        _raw_log_path = (args.raw_log or ROOT/'build'/f'{target}-{arch}'/profile/'logs'/
                         f'{args.project if game else "XEngine"}.log').resolve()
        _raw_log_path.parent.mkdir(parents=True, exist_ok=True)
        _raw_log = _raw_log_path.open('w', encoding='utf-8')
        log(f'[BUILD] {args.action} {args.project if game else "XEngine"} | {target}-{arch} | {profile}')
        if args.action == 'game' and not isinstance(bc,dict):
            raise BuildError('config.json: build must be an object')
        if args.action == 'game' and args.run and target != host_platform():
            raise BuildError(f'--run is not supported when building for {target} from {host_platform()}')
        tooling=prepare_tools(target)
        if game:
            game_build(args.project,target=target,profile=profile,arch=arch,tooling=tooling,
                       run=args.run,no_doc=args.no_doc)
        else:
            lib=create_engine(target=target,profile=profile,arch=arch,tooling=tooling,
                              no_doc=args.no_doc)
            log(f'[OK] {lib}')
        # This is local, ignored build state, not a VS Code setting or tracked
        # editor configuration. It selects online docs even if older HTML exists.
        docs_marker = ROOT / 'build' / 'ide' / 'doxygen_online_only'
        if args.no_doc:
            docs_marker.parent.mkdir(parents=True, exist_ok=True)
            docs_marker.write_text('online\n', encoding='ascii')
            log('[DOCS] Online Doxygen reference selected for this checkout (--no-doc)')
        elif docs_marker.is_file():
            docs_marker.unlink()
            log('[DOCS] Local Doxygen reference preference restored')
        # Local opt-in: never create VS Code files for developers who have not
        # enabled the integration. IDE refresh must not break a valid build.
        if (ROOT / 'build' / 'ide' / 'vscode.json').is_file():
            ide = subprocess.run([sys.executable, str(ROOT / 'tools' / 'vscode.py'),
                                  'refresh', '--quiet'], cwd=ROOT, text=True,
                                 capture_output=True, check=False)
            if ide.returncode:
                log('[WARN] VS Code metadata refresh failed: ' +
                    (ide.stderr.strip() or ide.stdout.strip() or 'unknown error'))
        status = 0
        return 0
    except (BuildError,OSError,subprocess.SubprocessError) as e:
        message = f'[ERROR] {e}'
        print(message,file=sys.stderr,flush=True)
        _raw(message+'\n')
        return 1
    finally:
        if _raw_log is not None:
            if _warning_count:
                message = (f'[WARN] {_warning_count} compiler warning(s) '
                           f'{"shown" if _show_warnings else "hidden"}' +
                           ('' if _show_warnings else ' (use --show warnings)'))
                log(message)
            log(f'[SUMMARY] status={"ok" if status == 0 else "failed"} '
                f'errors={status} warnings={_warning_count}')
            log(f'[LOG] Full compiler output: {_raw_log_path}')
            _raw_log.close()
            _raw_log = None

if __name__=='__main__':
    raise SystemExit(main())
