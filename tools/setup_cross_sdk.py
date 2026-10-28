#!/usr/bin/env python3
"""Install target SDL2 SDKs without Docker, WSL, CMake or Python dependencies.

Windows -> Linux: assemble a Debian 12 amd64 sysroot from official Debian .deb files.
Linux -> Windows: unpack official MinGW SDL2 developer archives.
Compiler installation is intentionally a separate prerequisite (no silent sudo/pacman).
"""
from __future__ import annotations

from collections import defaultdict, deque

import argparse
import hashlib
import io
import datetime
import json
import lzma
import os
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import traceback
import urllib.request
from pathlib import Path, PurePosixPath
from typing import Callable, TypeVar

ROOT = Path(__file__).resolve().parent.parent
SDL = {
    'SDL2': ('2.32.10', 'https://www.libsdl.org/release/SDL2-devel-2.32.10-mingw.tar.gz'),
    'SDL2_image': ('2.8.12', 'https://www.libsdl.org/projects/SDL_image/release/SDL2_image-devel-2.8.12-mingw.tar.gz'),
    'SDL2_mixer': ('2.8.2', 'https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-devel-2.8.2-mingw.tar.gz'),
    'SDL2_ttf': ('2.24.0', 'https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-devel-2.24.0-mingw.tar.gz'),
}
DEBIAN_SUITE = 'bookworm'  # Keep the target glibc baseline explicit.
DEBIAN_ROOT = 'https://deb.debian.org/debian'
DEBIAN_SOURCES = (
    ('base', f'{DEBIAN_ROOT}/dists/{DEBIAN_SUITE}/main/binary-amd64/Packages.xz', DEBIAN_ROOT),
    ('updates', f'{DEBIAN_ROOT}/dists/{DEBIAN_SUITE}-updates/main/binary-amd64/Packages.xz', DEBIAN_ROOT),
    ('security', f'https://security.debian.org/debian-security/dists/{DEBIAN_SUITE}-security/main/binary-amd64/Packages.xz', 'https://security.debian.org/debian-security'),
)
DEBIAN_ROOT_PACKAGES = (
    'libc6', 'libc6-dev', 'linux-libc-dev', 'libgcc-12-dev', 'libstdc++-12-dev',
    'libsdl2-dev', 'libsdl2-image-dev', 'libsdl2-mixer-dev', 'libsdl2-ttf-dev',
    'nlohmann-json3-dev',
)

# Non-build documentation is neither needed by Clang nor by the linker.
# Linux manpages may have two differently-cased names for the same document;
# on case-insensitive Windows filesystems they collide when symlinks are copied.
SDK_IGNORED_CONTENT_PREFIXES = (
    ('usr', 'share', 'man'),
    ('usr', 'share', 'doc'),
    ('usr', 'share', 'info'),
)


def is_ignored_sysroot_content(path: Path) -> bool:
    parts = path.parts
    return any(parts[:len(prefix)] == prefix for prefix in SDK_IGNORED_CONTENT_PREFIXES)


class SetupError(Exception):
    pass


T = TypeVar('T')


def retry_windows_file_lock(action: Callable[[], T], description: str, *, attempts: int = 8) -> T:
    """Retry transient Windows sharing violations (WinError 32/33) only.

    Antivirus, indexing and cloud-sync software can temporarily hold open a
    freshly downloaded or extracted file. Other IO failures are never hidden.
    """
    for index in range(attempts):
        try:
            return action()
        except OSError as exc:
            if getattr(exc, 'winerror', None) not in (32, 33):
                raise
            if index + 1 == attempts:
                raise SetupError(
                    f'File remained locked while {description} after {attempts} attempts: {exc}. '
                    'Close applications using the SDK and check antivirus/cloud-sync software.'
                ) from exc
            delay = min(0.2 * 1.7 ** index, 1.5)
            info(f'[RETRY] Windows file temporarily locked while {description}; '
                 f'attempt {index + 2}/{attempts} in {delay:.1f}s')
            time.sleep(delay)
    raise AssertionError('unreachable')


def info(message: str) -> None:
    print(message, flush=True)


def host_os() -> str:
    if sys.platform == 'win32':
        return 'windows'
    if sys.platform.startswith('linux'):
        return 'linux'
    raise SetupError(f'Unsupported host: {sys.platform}')


def sdk_locations(host: str, target: str, *,
                  environ: dict[str, str] | None = None,
                  home: Path | None = None) -> tuple[Path, Path]:
    """Return (platform-appropriate location, legacy ~/.xengine location).

    SDKs are large machine-local development data, not roaming preferences.
    An existing legacy installation remains usable until explicitly migrated.
    """
    env = os.environ if environ is None else environ
    user_home = Path.home() if home is None else home
    if host == 'windows':
        base = Path(env.get('LOCALAPPDATA') or user_home / 'AppData' / 'Local') / 'XEngine'
    elif host == 'linux':
        xdg = env.get('XDG_DATA_HOME', '').strip()
        base = ((Path(xdg) if xdg and Path(xdg).is_absolute()
                 else user_home / '.local' / 'share') / 'xengine')
    else:
        raise SetupError(f'Unsupported host: {host}')
    name = f'{target}-x86_64'
    return base / 'sdk' / name, user_home / '.xengine' / 'sdk' / name


def select_sdk_location(host: str, target: str, *,
                        custom: Path | None = None) -> tuple[Path, str]:
    if custom is not None:
        return custom.expanduser().resolve(), 'custom'
    preferred, legacy = sdk_locations(host, target)
    if preferred.is_dir():
        return preferred.resolve(), 'platform default'
    if legacy.is_dir():
        return legacy.resolve(), 'existing legacy SDK (use --migrate-sdk to copy)'
    return preferred.resolve(), 'platform default (new)'


def _replace_sdk_paths(value: object, old: Path, new: Path) -> object:
    """Replace only this target SDK's paths, preserving custom config keys."""
    if isinstance(value, str):
        # pkg-config typically uses POSIX separators, while Windows JSON can
        # contain either representation. Do not rewrite similarly named paths.
        for before, after in ((str(old), str(new)), (old.as_posix(), new.as_posix())):
            if before != after:
                value = re.sub(re.escape(before) + r'(?=$|[\r\n\\/])',
                               lambda _match: after, value)
        return value
    if isinstance(value, list):
        return [_replace_sdk_paths(part, old, new) for part in value]
    if isinstance(value, dict):
        return {key: _replace_sdk_paths(part, old, new) for key, part in value.items()}
    return value


def migrate_legacy_sdk(host: str, target: str) -> Path:
    """Opt-in offline COPY; leaves the original SDK intact for recovery."""
    preferred, legacy = sdk_locations(host, target)
    preferred, legacy = preferred.resolve(), legacy.resolve()
    if not legacy.is_dir():
        raise SetupError(f'No legacy SDK to migrate: {legacy}')
    if preferred.exists():
        raise SetupError(f'Refusing to overwrite an existing SDK: {preferred}')
    if preferred == ROOT or ROOT in preferred.parents or legacy == preferred:
        raise SetupError('Unsafe SDK migration destination')
    required = ((Path('sysroot/usr/include/SDL2/SDL.h'),
                 Path('sysroot/usr/lib/x86_64-linux-gnu/pkgconfig/sdl2.pc'))
                if (host, target) == ('windows', 'linux') else
                (Path('include/SDL2/SDL.h'), Path('lib/pkgconfig/sdl2.pc')))
    missing = [str(legacy / part) for part in required if not (legacy / part).is_file()]
    if missing:
        raise SetupError('Cannot migrate an incomplete SDK; missing: ' + ', '.join(missing))

    config = ROOT / 'tools' / 'toolchains.local.json'
    try:
        doc = json.loads(config.read_text(encoding='utf-8'))
        current = doc[host][target]
    except (OSError, ValueError, KeyError, TypeError) as exc:
        raise SetupError('A valid installed toolchains.local.json entry is required '
                         'to migrate and update SDK references') from exc
    if not isinstance(current, dict) or not current:
        raise SetupError('Missing or invalid toolchain entry for SDK migration')
    changed = _replace_sdk_paths(current, legacy, preferred)
    if changed == current:
        raise SetupError('The selected toolchain does not reference the legacy SDK; '
                         'inspect tools/toolchains.local.json before migrating')

    preferred.parent.mkdir(parents=True, exist_ok=True)
    info(f'[MIGRATE] Copying {legacy} to {preferred}; the original will be preserved.')
    with tempfile.TemporaryDirectory(prefix='.xengine-sdk-migration-',
                                     dir=preferred.parent) as staging_dir:
        staging = Path(staging_dir) / 'sdk'
        shutil.copytree(legacy, staging)
        # Relocate absolute SDK paths embedded in pkg-config metadata. The
        # source is untouched, including its cached Debian/SDL downloads.
        for pc in staging.rglob('*.pc'):
            data = pc.read_text(encoding='utf-8')
            rewritten = _replace_sdk_paths(data, legacy, preferred)
            if rewritten != data:
                pc.write_text(str(rewritten), encoding='utf-8')
        if preferred.exists():
            raise SetupError(f'SDK destination appeared during migration: {preferred}')
        staging.rename(preferred)
        try:
            update_toolchains(host, target, changed)
        except Exception:
            # The original source and its config remain usable. This target
            # was created by this operation and never existed before.
            shutil.rmtree(preferred)
            raise
    info(f'[MIGRATE] Copied SDK and updated toolchains.local.json: {preferred}')
    info(f'[MIGRATE] Original SDK kept at {legacy}; remove it only after a successful cross-build.')
    return preferred


def require_programs(names: tuple[str, ...], instruction: str) -> None:
    missing = [name for name in names if shutil.which(name) is None]
    if missing:
        raise SetupError(f'Missing tools: {", ".join(missing)}.\nInstall them first:\n{instruction}')


def download(url: str, destination: Path, *, sha256: str | None = None, force: bool = False) -> Path:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not url.startswith('https://'):
        raise SetupError(f'Only HTTPS package downloads are supported: {url}')
    if destination.is_file() and not force:
        if not sha256 or retry_windows_file_lock(lambda: checksum(destination), f'validating cached archive {destination}') == sha256:
            info(f'[CACHE] {destination.name}')
            return destination
    temp = destination.with_suffix(destination.suffix + '.partial')
    info(f'[DOWNLOAD] {url}')
    try:
        def fetch() -> None:
            with urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent': 'XEngine-SDK-Setup/1.0'}), timeout=90) as response, temp.open('wb') as output:
                shutil.copyfileobj(response, output)
        retry_windows_file_lock(fetch, f'downloading {url} to {temp}')
        if sha256 and retry_windows_file_lock(lambda: checksum(temp), f'verifying downloaded archive {temp}') != sha256:
            raise SetupError(f'SHA256 mismatch for {url}')
        retry_windows_file_lock(lambda: temp.replace(destination), f'renaming {temp} to {destination}')
    finally:
        try:
            retry_windows_file_lock(lambda: temp.unlink(missing_ok=True), f'removing temporary download {temp}', attempts=4)
        except SetupError as exc:
            info(f'[WARN] {exc}')
    return destination


def checksum(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def safe_subpath(raw: str, *, merged_usr: bool = True) -> Path | None:
    value = PurePosixPath(raw)
    if raw.startswith('/') or '..' in value.parts or not raw or '\\' in raw or ':' in raw:
        raise SetupError(f'Unsafe archive path: {raw}')
    pieces = [v for v in value.parts if v not in ('.', '')]
    if not pieces:
        return None
    # Debian merged-/usr: /lib and /lib64 may be aliases of /usr/lib and /usr/lib64.
    if merged_usr and pieces[0] in ('lib', 'lib64', 'bin', 'sbin'):
        pieces.insert(0, 'usr')
    return Path(*pieces)


def normalized_links(root: Path, links: list[tuple[Path, str]], *, max_passes: int = 5, merged_usr: bool = True) -> None:
    """Materialize symlinks as file copies on Windows; never create links outside SDK."""
    remaining = links[:]
    for _ in range(max_passes):
        next_round = []
        progress = 0
        for dest, raw_target in remaining:
            if raw_target.startswith('/'):
                target = safe_subpath(raw_target.lstrip('/'), merged_usr=merged_usr)
                target_path = root / target if target else root
            else:
                rel_parent = dest.relative_to(root).parent
                relative = PurePosixPath(raw_target)
                parts = list(rel_parent.parts)
                for part in relative.parts:
                    if part == '..':
                        if not parts:
                            raise SetupError(f'Unsafe symbolic link: {dest}: {raw_target}')
                        parts.pop()
                    elif part not in ('', '.'):
                        parts.append(part)
                target = safe_subpath('/'.join(parts), merged_usr=merged_usr)
                target_path = root / target if target else root
            if not target_path.is_file():
                next_round.append((dest, raw_target))
                continue
            # On Windows, differently cased POSIX names can identify the exact
            # same on-disk file. Copying a file onto itself raises WinError 32
            # (or SameFileError), and retrying can never make that succeed.
            # On case-sensitive hosts this also handles genuine self-links.
            if dest.is_file() and dest.samefile(target_path):
                progress += 1
                continue
            dest.parent.mkdir(parents=True, exist_ok=True)
            retry_windows_file_lock(lambda: shutil.copy2(target_path, dest),
                                    f'materializing symbolic link {dest} from {target_path}')
            progress += 1
        remaining = next_round
        if not remaining or not progress:
            break
    if remaining:
        info(f'[WARN] {len(remaining)} unresolved SDK symbolic links (some are optional).')
    # The four SDL2 linker inputs and runtime SONAME aliases must be real files.


def update_toolchains(host: str, target: str, entry: dict) -> None:
    config = ROOT / 'tools' / 'toolchains.local.json'
    try:
        doc = json.loads(config.read_text(encoding='utf-8')) if config.exists() else {}
    except (ValueError, UnicodeError) as exc:
        raise SetupError(f'Invalid existing toolchains.local.json (not overwritten): {exc}') from exc
    if not isinstance(doc, dict) or not isinstance(doc.get(host, {}), dict):
        raise SetupError('Existing toolchain configuration has an invalid structure')
    host_entries = doc.setdefault(host, {})
    current = host_entries.get(target, {})
    if current and not isinstance(current, dict):
        raise SetupError('Existing toolchain target configuration must be an object')
    if current:
        info(f'[INFO] Updating SDK-specific fields in existing {host}.{target}; unrelated values are preserved.')
        if any(current.get(key) != value for key, value in entry.items()):
            suffix = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
            backup = config.with_name(f'toolchains.local.json.{suffix}.bak')
            if backup.exists():
                backup = config.with_name(f'toolchains.local.json.{suffix}.{os.getpid()}.bak')
            shutil.copy2(config, backup)
            info(f'[BACKUP] Existing toolchain configuration: {backup}')
    host_entries[target] = {**current, **entry}
    temp = config.with_suffix('.json.tmp')
    temp.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    temp.replace(config)
    info(f'[CONFIG] {config} ({host}.{target})')


def install_windows_sdk(sdk: Path, *, force: bool) -> None:
    require_programs(('x86_64-w64-mingw32-g++', 'x86_64-w64-mingw32-ar', 'x86_64-w64-mingw32-windres', 'x86_64-w64-mingw32-objdump', 'pkg-config'),
                     'sudo apt update && sudo apt install -y g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64 pkg-config')
    cache = sdk / '.downloads'
    sdk.mkdir(parents=True, exist_ok=True)
    for package, (version, url) in SDL.items():
        archive = download(url, cache / url.rsplit('/', 1)[-1], force=force)
        inside = f'{package}-{version}/x86_64-w64-mingw32/'
        found = 0
        with tarfile.open(archive, 'r:gz') as tar:
            links = []
            for member in tar:
                if not member.name.startswith(inside):
                    continue
                rel = safe_subpath(member.name[len(inside):], merged_usr=False)
                if rel is None:
                    continue
                dst = sdk / rel
                if member.isdir():
                    dst.mkdir(parents=True, exist_ok=True)
                elif member.isfile():
                    dst.parent.mkdir(parents=True, exist_ok=True)
                    extracted = tar.extractfile(member)
                    if extracted is None:
                        raise SetupError(f'Cannot read {member.name}')
                    with extracted, dst.open('wb') as stream:
                        shutil.copyfileobj(extracted, stream)
                    found += 1
                elif member.issym() or member.islnk():
                    links.append((dst, member.linkname))
            normalized_links(sdk, links, merged_usr=False)
        if not found:
            raise SetupError(f'{archive.name}: missing expected directory {inside}')
        info(f'[SDK] {package} {version}: {found} files')
    pc = sdk / 'lib' / 'pkgconfig'
    expected = ('sdl2.pc', 'SDL2_image.pc', 'SDL2_mixer.pc', 'SDL2_ttf.pc')
    for name in expected:
        f = pc / name
        if not f.is_file():
            raise SetupError(f'Missing pkg-config file: {f}')
        data = f.read_text(encoding='utf-8')
        data = re.sub(r'^prefix=.*$', 'prefix=' + sdk.as_posix(), data, flags=re.M)
        f.write_text(data, encoding='utf-8')
    # nlohmann/json is header-only. Prefer locally installed system headers.
    json_dir = Path('/usr/include/nlohmann')
    if json_dir.is_dir():
        shutil.copytree(json_dir, sdk / 'include' / 'nlohmann', dirs_exist_ok=True)
    else:
        info('[INFO] Downloading nlohmann/json single-header distribution.')
        url = 'https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp'
        download(url, sdk / 'include' / 'nlohmann' / 'json.hpp', force=force)
    env = os.environ.copy()
    env['PKG_CONFIG_LIBDIR'] = str(pc)
    env.pop('PKG_CONFIG_PATH', None)
    proc = subprocess.run(['pkg-config', '--modversion', *('sdl2', 'SDL2_image', 'SDL2_mixer', 'SDL2_ttf')], env=env, capture_output=True, text=True)
    if proc.returncode:
        raise SetupError(f'Windows SDL2 pkg-config validation failed: {proc.stderr.strip()}')
    info('[OK] Target SDL2 packages: ' + ', '.join(proc.stdout.split()))
    update_toolchains('linux', 'windows', {
        'cxx': 'x86_64-w64-mingw32-g++',
        'ar': 'x86_64-w64-mingw32-ar',
        'windres': 'x86_64-w64-mingw32-windres',
        'objdump': 'x86_64-w64-mingw32-objdump',
        'pkg_config': 'pkg-config',
        'pkg_config_libdir': str(pc),
        'dll_dirs': [str(sdk / 'bin')],
        'lib_dirs': [str(sdk / 'lib')],
        'cxxflags': ['-I' + str(sdk / 'include')],
    })


def debian_package_records(data: str) -> dict[str, dict]:
    """Parse Debian Packages paragraphs, including multiline fields."""
    records = {}
    for chunk in data.split('\n\n'):
        item = {}
        key = None
        for line in chunk.splitlines():
            if line.startswith(' ') and key:
                item[key] += ' ' + line.strip()
            elif ': ' in line:
                key, value = line.split(': ', 1)
                item[key] = value
        if item.get('Package') and item.get('Filename') and item.get('Architecture') in ('amd64', 'all'):
            records[item['Package']] = item
    return records


def debian_dependencies(value: str) -> list[tuple[str, ...]]:
    result = []
    for group in value.split(','):
        choices = []
        for alternative in group.split('|'):
            name = re.match(r'\s*([a-z0-9][a-z0-9+.-]*)', alternative)
            if name:
                choices.append(name.group(1))
        if choices:
            result.append(tuple(choices))
    return result


def resolve_debian_packages(db: dict[str, dict], roots: tuple[str, ...]) -> list[dict]:
    """Resolve real and virtual Debian dependencies without omitting requirements.

    Debian permits dependencies on virtual names advertised via ``Provides``;
    for example ``libfluidsynth-dev`` depends on ``libsndfile-dev``, which is
    provided by the real package ``libsndfile1-dev`` in Debian 12. Every chosen
    provider must itself be processed, including its transitive dependencies.
    """
    providers: dict[str, list[str]] = defaultdict(list)
    for package, record in db.items():
        for provided in debian_dependencies(record.get('Provides', '')):
            for virtual_name in provided:
                providers[virtual_name].append(package)

    selected: dict[str, dict] = {}
    pending = deque((root, None) for root in roots)
    while pending:
        requested, parent = pending.popleft()
        if requested in db:
            package = requested
        else:
            candidates = providers.get(requested, [])
            if not candidates:
                context = f' (required by {parent})' if parent else ''
                raise SetupError(
                    f'Cannot resolve Debian package or virtual dependency {requested!r}{context}; '
                    'do not create an incomplete sysroot.'
                )
            # Keep a provider already chosen for another dependency. Otherwise,
            # use a deterministic order so identical indexes yield identical SDKs.
            package = next((name for name in candidates if name in selected), None)
            if package is None:
                package = sorted(candidates)[0]
            info(f'[DEPENDENCY] Debian virtual package {requested} -> {package}')

        if package in selected:
            continue
        record = db[package]
        selected[package] = record

        for choices in debian_dependencies(','.join((record.get('Pre-Depends', ''), record.get('Depends', '')))):
            # Honor an already selected real package/provider when possible.
            option = next((name for name in choices if name in selected), None)
            if option is None:
                option = next((name for name in choices if any(p in selected for p in providers.get(name, ()))), None)
            if option is None:
                option = next((name for name in choices if name in db), None)
            if option is None:
                option = next((name for name in choices if providers.get(name)), None)
            if option is None:
                raise SetupError(
                    f'Cannot resolve Debian dependency {choices!r} of {package}; '
                    'no real package or provider exists; do not create an incomplete sysroot.'
                )
            pending.append((option, package))
    return list(selected.values())


def read_ar_archive(path: Path) -> bytes:
    """Return the data.tar.* payload of a Debian ar-format .deb (stdlib only)."""
    with path.open('rb') as stream:
        if stream.read(8) != b'!<arch>\n':
            raise SetupError(f'Not a Debian .deb ar archive: {path}')
        while True:
            h = stream.read(60)
            if not h:
                break
            if len(h) != 60 or h[58:60] != b'`\n':
                raise SetupError(f'Invalid ar member in {path}')
            name = h[:16].decode('ascii').strip().rstrip('/')
            size = int(h[48:58].strip())
            member = stream.read(size)
            if size % 2:
                stream.read(1)
            if name.startswith('data.tar.'):
                if name.endswith(('.xz', '.gz', '.bz2')) or name == 'data.tar':
                    return member
                raise SetupError(f'Unsupported .deb compression {name}; use Debian bookworm .deb packages')
    raise SetupError(f'Missing data.tar in {path}')


def extract_deb(path: Path, root: Path) -> list[tuple[Path, str]]:
    links = []
    archive_data = retry_windows_file_lock(lambda: read_ar_archive(path), f'reading Debian package {path}')
    with tarfile.open(fileobj=io.BytesIO(archive_data), mode='r:*') as tar:
        for member in tar:
            rel = safe_subpath(member.name)
            if rel is None or is_ignored_sysroot_content(rel):
                continue
            dest = root / rel
            if member.isdir():
                dest.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                dest.parent.mkdir(parents=True, exist_ok=True)
                f = tar.extractfile(member)
                if f is None:
                    raise SetupError(f'Could not read .deb content: {member.name}')
                with f:
                    def copy_member() -> None:
                        f.seek(0)  # Start over if Windows temporarily locks the destination.
                        with dest.open('wb') as out:
                            shutil.copyfileobj(f, out)
                    retry_windows_file_lock(copy_member, f'extracting {member.name} to {dest}')
            elif member.issym() or member.islnk():
                # Only materialize files; directory symlinks are normalized via safe_subpath.
                links.append((dest, member.linkname))
    return links


def linux_cross_compiler_flags(sysroot: Path, cpp_include: Path, gcc_dir: Path) -> list[str]:
    """Explicit Linux C/C++ and SDL2 headers for Windows-native Clang.

    Debian's SDL2 pkg-config metadata may be discoverable while its Cflags
    include path is suppressed/rewritten by the Windows pkgconf executable.
    XEngine uses <SDL.h>, which lives inside usr/include/SDL2; keep that
    target-specific path independent of host pkgconf behavior.
    """
    paths = (
        cpp_include,
        sysroot / 'usr/include/x86_64-linux-gnu/c++' / gcc_dir.name,
        sysroot / 'usr/include/x86_64-linux-gnu',
        sysroot / 'usr/include/SDL2',
        sysroot / 'usr/include',
    )
    return ['--target=x86_64-linux-gnu', '-fuse-ld=lld'] + [
        '-isystem' + p.as_posix() for p in paths
    ]


def fix_merged_usr_linker_scripts(sysroot: Path) -> int:
    """Relocate absolute /lib references in GNU ld scripts for merged-/usr.

    Debian 12 uses /lib -> /usr/lib. During SDK extraction on Windows we
    normalize payloads to usr/lib, because Windows cannot reliably represent
    directory symlinks. However, GNU ld scripts (notably libm.so and libc.so)
    can contain absolute GROUP(/lib/...) references. LLD correctly interprets
    those against --sysroot/lib, which does not exist after normalization.

    Rewrite only small textual linker scripts, and only if the canonical file
    exists under usr/lib. ELF binaries and downloaded .deb archives are never
    changed. This operation is idempotent for --verify-only reruns.
    """
    roots = (sysroot / 'usr/lib', sysroot / 'usr/lib/x86_64-linux-gnu')
    changed = 0
    for base in roots:
        if not base.is_dir():
            continue
        # GNU ld scripts are commonly named libfoo.so (no SONAME suffix).
        for item in base.glob('*.so'):
            if not item.is_file() or item.stat().st_size > 64 * 1024:
                continue
            data = item.read_bytes()
            if data.startswith(b'\x7fELF') or (b'GROUP' not in data and b'INPUT' not in data):
                continue
            try:
                content = data.decode('utf-8')
            except UnicodeDecodeError:
                continue

            def merged_target(match: re.Match[str]) -> str:
                absolute = match.group(0)
                normalized = safe_subpath(absolute.lstrip('/'))
                if normalized is None or not (sysroot / normalized).is_file():
                    return absolute
                return '/' + normalized.as_posix()

            rewritten = re.sub(r'(?<![\w/])/(?:lib|lib64)/[\w.+-]+(?:/[\w.+-]+)*',
                               merged_target, content)
            if rewritten != content:
                retry_windows_file_lock(lambda: item.write_text(rewritten, encoding='utf-8'),
                                        f'fixing merged-/usr linker script {item}')
                changed += 1
    if changed:
        info(f'[SYSROOT] Normalized {changed} GNU ld scripts for merged-/usr layout.')
    return changed


def install_linux_sdk(sdk: Path, *, force: bool, verify_only: bool = False) -> None:
    require_programs(('clang++', 'llvm-ar', 'llvm-readelf', 'pkg-config'),
                     'Open MSYS2 UCRT64 and run: pacman -S --needed mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-lld mingw-w64-ucrt-x86_64-llvm mingw-w64-ucrt-x86_64-pkgconf')
    sysroot = sdk / 'sysroot'
    cache = sdk / '.downloads'
    sdk.mkdir(parents=True, exist_ok=True)
    if not verify_only:
        db: dict[str, dict] = {}
        for label, url, repo_url in DEBIAN_SOURCES:
            index = download(url, cache / f'Packages-{label}.xz', force=force)
            info(f'[INDEX] Reading Debian {DEBIAN_SUITE} {label} packages')
            package_data = debian_package_records(lzma.decompress(index.read_bytes()).decode('utf-8'))
            for name, record in package_data.items():
                record['_repo_url'] = repo_url
            db.update(package_data)
        packages = resolve_debian_packages(db, DEBIAN_ROOT_PACKAGES)
        info(f'[SDK] Resolving {len(packages)} Debian development/runtime packages')
        links = []
        for i, record in enumerate(packages, 1):
            location = record['Filename']
            if not location.startswith('pool/') or '..' in PurePosixPath(location).parts:
                raise SetupError(f'Unsafe Debian package URL: {location}')
            file = download(record['_repo_url'] + '/' + location,
                            cache / (record['Package'] + '_' + record['Version'].replace(':', '%3a') + '.deb'),
                            sha256=record.get('SHA256'), force=force)
            try:
                links.extend(extract_deb(file, sysroot))
            except OSError as exc:
                raise SetupError(f'Failed to extract package {record["Package"]} from {file}: {exc}') from exc
            except SetupError as exc:
                raise SetupError(f'Failed to extract package {record["Package"]} from {file}: {exc}') from exc
            if i % 20 == 0:
                info(f'[EXTRACT] {i}/{len(packages)} Debian packages')
        normalized_links(sysroot, links, max_passes=12)
    else:
        if force:
            raise SetupError('--verify-only cannot be combined with --force.')
        if not sysroot.is_dir():
            raise SetupError(f'Linux sysroot has not been extracted: {sysroot}. Run without --verify-only first.')
        info('[VERIFY] Using existing Linux sysroot; skipping package indexes, downloads and extraction.')
    # Fix GNU ld scripts on normal and --verify-only runs; no downloads needed.
    fix_merged_usr_linker_scripts(sysroot)
    # Windows must not receive Debian absolute /usr paths from pkg-config.
    # Patch the sysroot copies, never the downloaded package archives.
    pc_dirs = [sysroot / 'usr/lib/x86_64-linux-gnu/pkgconfig', sysroot / 'usr/lib/pkgconfig', sysroot / 'usr/share/pkgconfig']
    for directory in pc_dirs:
        if not directory.is_dir():
            continue
        for pc in directory.glob('*.pc'):
            text = pc.read_text(encoding='utf-8', errors='replace')
            # Most .pc files are variable-based, but direct /usr values
            # must also be rewritten when pkg-config runs on Windows.
            text = re.sub(r'^(prefix|libdir|includedir|datadir|exec_prefix)=(/usr(?:/[^\n]*)?|/lib(?:/[^\n]*)?)$',
                          lambda m: m.group(1) + '=' + (sysroot / safe_subpath(m.group(2).lstrip('/'))).as_posix(),
                          text, flags=re.M)
            text = re.sub(r'(?<!\S)-([IL])(/usr(?:/[^\s]*)?|/lib(?:/[^\s]*)?)',
                          lambda m: '-' + m.group(1) + (sysroot / safe_subpath(m.group(2).lstrip('/'))).as_posix(),
                          text)
            pc.write_text(text, encoding='utf-8')
    required = (
        'usr/include/SDL2/SDL.h', 'usr/include/nlohmann/json.hpp',
        'usr/include/stdio.h', 'usr/lib/x86_64-linux-gnu/pkgconfig/sdl2.pc',
        'usr/lib/x86_64-linux-gnu/libSDL2.so',
    )
    for item in required:
        if not (sysroot / item).is_file():
            raise SetupError(f'Linux sysroot incomplete: {sysroot / item}')
    # Additional compiler paths needed by Clang/LLD on Windows.
    gcc_dirs = list((sysroot / 'usr/lib/gcc/x86_64-linux-gnu').glob('*'))
    gcc_dirs = [p for p in gcc_dirs if p.is_dir()]
    if not gcc_dirs:
        raise SetupError('Linux sysroot lacks GCC support libraries (libgcc-12-dev)')
    gcc_dir = sorted(gcc_dirs, key=lambda p: p.name)[-1]
    cpp_include = sysroot / 'usr/include/c++' / gcc_dir.name
    if not cpp_include.is_dir() or not (gcc_dir / 'crtbeginS.o').is_file():
        raise SetupError('Linux sysroot lacks C++ headers or ELF CRT startup objects')
    pcd = os.pathsep.join(str(x) for x in pc_dirs if x.is_dir())
    env = os.environ.copy()
    env['PKG_CONFIG_LIBDIR'] = pcd
    env['PKG_CONFIG_SYSROOT_DIR'] = ''
    # Windows pkgconf can otherwise guess /usr/lib from multiarch .pc paths.
    env['PKG_CONFIG_DONT_DEFINE_PREFIX'] = '1'
    env['PKG_CONFIG_DONT_RELOCATE_PATHS'] = '1'
    env.pop('PKG_CONFIG_PATH', None)
    probe = subprocess.run(['pkg-config', '--modversion', 'sdl2', 'SDL2_image', 'SDL2_ttf', 'SDL2_mixer'], env=env, capture_output=True, text=True)
    if probe.returncode:
        raise SetupError(f'Linux SDK pkg-config validation failed: {probe.stderr.strip()}')
    info('[OK] Debian Linux SDL2 pkg-config: ' + ', '.join(probe.stdout.split()))
    # Clang Windows -> Linux needs the target libstdc++ search paths explicitly.
    compiler_flags = linux_cross_compiler_flags(sysroot, cpp_include, gcc_dir)
    link_flags = ['-fuse-ld=lld', '-B' + gcc_dir.as_posix(),
                  '-L' + gcc_dir.as_posix(),
                  '-L' + (sysroot / 'usr/lib/x86_64-linux-gnu').as_posix(),
                  '-L' + (sysroot / 'usr/lib').as_posix()]
    update_toolchains('windows', 'linux', {
        'cxx': 'clang++', 'ar': 'llvm-ar', 'readelf': 'llvm-readelf',
        'cxxflags': compiler_flags, 'ldflags': link_flags,
        'pkg_config': 'pkg-config', 'pkg_config_sysroot_dir': '',
        'pkg_config_libdir': [str(p) for p in pc_dirs if p.is_dir()],
        'sysroot': str(sysroot),
        'lib_dirs': [str(sysroot / 'usr/lib/x86_64-linux-gnu'), str(sysroot / 'usr/lib'), str(gcc_dir)],
    })
    # Verify the actual XEngine toolchain configuration can link a Linux ELF
    # without invoking a Linux process or executing a foreign binary.
    import build as xengine_build
    try:
        tc = xengine_build.prepare_tools('linux')
    except xengine_build.BuildError as exc:
        raise SetupError(f'Cross-toolchain validation failed: {exc}') from exc
    with tempfile.TemporaryDirectory(prefix='xengine-elf-test-') as check_dir:
        check = Path(check_dir)
        src = check / 'main.cpp'
        elf = check / 'probe.elf'
        src.write_text('#include <SDL.h>\n#include <nlohmann/json.hpp>\n'
                       'int main(){ SDL_version v{}; SDL_VERSION(&v); '
                       'nlohmann::json j={{"ok", true}}; return j["ok"].get<bool>() ? 0 : 1; }\n',
                       encoding='utf-8')
        command = [tc.compiler, '-std=c++20', *tc.cxxflags, *tc.pkg_cflags, str(src),
                   '-o', str(elf), *tc.ldflags, *tc.pkg_libs]
        info('[VERIFY] Linking an ELF64 C++/SDL2 program on Windows (without running it)')
        info('[VERIFY] SDL2 target headers: ' + (sysroot / 'usr/include/SDL2').as_posix())
        info('[VERIFY] Target pkg-config Cflags: ' + ' '.join(tc.pkg_cflags))
        linked = subprocess.run(command, capture_output=True, text=True)
        if linked.returncode:
            raise SetupError('Linux ELF link test failed. Check the target sysroot and compiler flags:\n' +
                             linked.stderr.strip()[-5000:])
        with elf.open('rb') as stream:
            header = stream.read(20)
        if len(header) < 20 or not header.startswith(b'\x7fELF') or header[4] != 2 or int.from_bytes(header[18:20], 'little') != 62:
            raise SetupError('Link produced a non-ELF64/non-x86_64 output; target toolchain is misconfigured.')
    info('[OK] Linked a Linux x86_64 ELF executable from Windows; it was NOT executed.')
    if not verify_only:
        manifest = {'distribution': 'Debian', 'suite': DEBIAN_SUITE, 'architecture': 'amd64',
                    'packages': [{'name': p['Package'], 'version': p['Version'],
                                  'sha256': p.get('SHA256'), 'filename': p['Filename']} for p in packages]}
        (sdk / 'packages-manifest.json').write_text(
            json.dumps(manifest, indent=2) + '\n', encoding='utf-8')


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description='Install target SDKs for XEngine cross-compilation (no WSL/Docker required).')
    parser.add_argument('--target', choices=('auto', 'linux', 'windows', 'all'), default='auto')
    parser.add_argument('--sdk-dir', type=Path, help='Custom target SDK directory (not valid for --target all).')
    parser.add_argument('--migrate-sdk', action='store_true',
                        help='Copy an existing ~/.xengine SDK to the OS data folder and update toolchain paths without downloading')
    parser.add_argument('--force', action='store_true', help='Redownload and replace SDK content.')
    parser.add_argument('--debug', action='store_true', help='Include a Python traceback when setup fails.')
    parser.add_argument('--verify-only', action='store_true', help='Reuse an existing Windows-to-Linux sysroot and repeat toolchain/link validation without downloading or extracting packages.')
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        host = host_os()
        if args.migrate_sdk:
            if (args.target in ('auto', 'all') or args.sdk_dir or args.force or args.verify_only):
                raise SetupError('--migrate-sdk requires explicit --target linux/windows '
                                 'and cannot be combined with --sdk-dir, --force or --verify-only.')
            if args.target == host:
                raise SetupError('Native targets do not require a cross SDK to migrate.')
            migrate_legacy_sdk(host, args.target)
            return 0
        if args.verify_only and (host != 'windows' or args.target != 'linux'):
            raise SetupError('--verify-only is supported only for Windows -> Linux (--target linux).')
        if args.verify_only and args.force:
            raise SetupError('--verify-only cannot be combined with --force.')
        if args.sdk_dir and args.target == 'all':
            raise SetupError('--sdk-dir cannot be combined with --target all; install targets separately.')
        targets = ('linux', 'windows') if args.target == 'all' else (host if args.target == 'auto' else args.target,)
        for target in targets:
            info(f'[SETUP] Host: {host}; target: {target}')
            if target == host:
                info('[INFO] Native build: no cross SDK required. Checking native dependencies:')
                check = subprocess.run([sys.executable, str(ROOT / 'tools/check_dependencies.py')], check=False)
                if check.returncode:
                    raise SetupError('Native dependencies are incomplete. See the checker output above.')
                continue
            sdk, location_source = select_sdk_location(host, target, custom=args.sdk_dir)
            if sdk == ROOT or ROOT in sdk.parents:
                raise SetupError('Store cross SDKs outside the XEngine repository.')
            info(f'[SDK] Location: {sdk} ({location_source}; override with --sdk-dir)')
            if host == 'linux' and target == 'windows':
                install_windows_sdk(sdk, force=args.force)
            elif host == 'windows' and target == 'linux':
                install_linux_sdk(sdk, force=args.force, verify_only=args.verify_only)
            else:
                raise SetupError(f'Unsupported target combination: {host} -> {target}')
        info('[OK] SDK setup completed.')
        return 0
    except (SetupError, OSError, ValueError, tarfile.TarError, lzma.LZMAError) as exc:
        info(f'[ERROR] {exc}')
        if args.debug:
            traceback.print_exc()
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
