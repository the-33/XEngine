# XEngine Developer Guide

Detailed dependency installation, cross-compilation, build, release and profiling reference.
For your first scene or gameplay script, follow
[Your first game](GETTING_STARTED.md). The
documentation index (repository reference: `docs/DOCUMENTATION_INDEX.md`) organizes the user-facing guides.

For public C++ classes, functions, properties and enumeration values, see the
[Doxygen API reference](API_REFERENCE.md). Build searchable HTML and XML with
`python tools/build_doxygen.py` from the repository root on Windows or Linux;
Doxygen is
optional and is not needed to build games.

XEngine's official engine version comes from the root [`VERSION`](../VERSION)
file. For the independent game version in `config.json`, engine compatibility,
source archive naming and release increments, see
[XEngine versioning](VERSIONING.md). Updating the engine must not overwrite a
game project's own version.

For runtime save storage, atomic crash recovery, custom JSON/binary types,
bit-packed fields and PlayerPrefs, see
[Persistence and serialization](PERSISTENCE_AND_SERIALIZATION.md).
For resolution, camera projection and responsive UI specifically, use
[Window, camera and UI](WINDOW_AND_CAMERA.md).

> **Note:** Example commands run from the repository root unless they first change directory. Create your own game using `new_project` before following commands targeting `games/My Game`.

## First checkout: optional contributor tools

Only the compiler, Python and SDL2 development dependencies below are needed
to build games. **Git hooks, VS Code and Doxygen are independent opt-in tools.**
After installing the required dependencies, you can enable the following in
your own clone (from the XEngine repository root):

| Optional tool | One-time setup | What it does |
|---|---|---|
| Git pre-commit hooks | `git config --local core.hooksPath .githooks` | Synchronizes opted-in game files with Git and checks staged engine/documentation changes using Doxygen when available. |
| VS Code C++ IntelliSense | `python tools/vscode.py setup` | Creates Git-ignored compiler commands and per-folder editor configurations for the engine and games. |
| XEngine Doxygen Hover | `python tools/vscode.py hover-install` | Adds clickable symbol links to local Doxygen HTML (when generated) or the published API pages. |

None of these is installed or activated by the normal build. The hook applies
only to **this Git clone** and may stage changes in games explicitly included
in Git; review staged files before committing. To check the VS Code extension,
run `python tools/vscode.py hover-status`. Read the detailed
[Git tracking and hook guide](GAME_GIT_TRACKING.md),
[VS Code setup](VSCODE_INTEGRATION.md) and
[GitHub Pages/Doxygen guide](GITHUB_PAGES.md) if you use these tools.

**No local Doxygen required:** browse the
[online API reference](https://the-33.github.io/XEngine/) at any time. To
compile **without generating documentation** even when engine code changes,
add `--no-doc` to the build command:

```bash
python tools/build.py engine --no-doc
python tools/build.py game --project "My Game" --run --no-doc
```

Your VS Code Doxygen Hover extension will then use the published reference,
even if old local HTML files remain. This preference is recorded under the
ignored `build/ide/` directory for that checkout. A successful build without
`--no-doc` restores the default local-first lookup. If you actively maintain
XEngine's documentation, run `python tools/build_doxygen.py` whenever you
want fresh local HTML instead. The optional Git hook and the GitHub Pages
workflow run their own checks independently of `--no-doc`.

## Dependencies

No `third_party/` vendor directory is required. Native builds use SDL2 development packages installed through the host package manager. The compiler and linker flags are obtained from `pkg-config`.

| Tool / library | Purpose |
|---|---|
| GCC or Clang with C++20 | Compile the engine and games |
| GNU `ar` | Create static engine archives |
| `pkg-config` / `pkgconf` | Resolve target SDL2 headers and libraries |
| Python **3.10+** | Build orchestration and templates; no Python packages required for building |
| SDL2 **2.26+** | Window, rendering, input and events |
| SDL2_image | Image loading |
| SDL2_mixer | Music and sound |
| SDL2_ttf | Font rendering |
| nlohmann/json | Header-only JSON library used by the C++ engine |
| `objdump` (Windows), `ldd` (native Linux), `readelf` (cross Linux) | Runtime dependency inspection |
| `windres` (Windows targets with custom ICO files) | Embed Windows executable icon resources |

### Windows 10/11 x86_64 — MSYS2 UCRT64

1. Install [MSYS2](https://www.msys2.org/) (normally in `C:\msys64`) and launch **MSYS2 UCRT64** rather than MSYS, MINGW64 or CLANG64. All libraries and compiler tools must use a compatible runtime.
2. Update the package database and installed packages. Restart the MSYS2 terminal if requested, then repeat until the update finishes:

   ```bash
   pacman -Syu
   ```

3. Install compiler tools, SDL2 and JSON development headers:

   ```bash
   pacman -S --needed \
     mingw-w64-ucrt-x86_64-gcc \
     mingw-w64-ucrt-x86_64-binutils \
     mingw-w64-ucrt-x86_64-pkgconf \
     mingw-w64-ucrt-x86_64-SDL2 \
     mingw-w64-ucrt-x86_64-SDL2_image \
     mingw-w64-ucrt-x86_64-SDL2_ttf \
     mingw-w64-ucrt-x86_64-SDL2_mixer \
     mingw-w64-ucrt-x86_64-nlohmann-json
   ```

   **Python:** If Windows already has Python 3.10+ accessible through `py -3` or `python`, no additional Python package is necessary. Otherwise install `mingw-w64-ucrt-x86_64-python` through `pacman`.

4. From a **new Windows CMD** window, add the UCRT64 tools to the current terminal's `PATH` if they are not already available:

   ```bat
   set "PATH=C:\msys64\ucrt64\bin;%PATH%"
   ```

   This change lasts only for the current terminal. Check that `where g++` and `where pkg-config` resolve to `ucrt64\bin`, not an unrelated MinGW or Anaconda environment.

5. From the XEngine root, verify the setup and build a game:

   ```bat
   g++ --version
   pkg-config --modversion sdl2 SDL2_image SDL2_ttf SDL2_mixer
   check_dependencies.bat
   cd "games\My Game"
   build.bat --run
   ```

   `.bat` launchers prefer `py -3`, falling back to `python` if the Python Launcher is unavailable. When using an MSYS2 Bash terminal, invoke `.bat` through `cmd.exe /c`; do not use `bash build.bat`.

### Linux x86_64 — Ubuntu / Debian

Install native development dependencies:

```bash
sudo apt update
sudo apt install -y \
  build-essential binutils pkg-config python3 \
  libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \
  libsdl2-mixer-dev nlohmann-json3-dev
```

Verify the installation and compile a game:

```bash
g++ --version
python3 --version
pkg-config --modversion sdl2 SDL2_image SDL2_ttf SDL2_mixer
chmod +x ./*.sh engine/*.sh
find games tools/templates -name '*.sh' -type f -exec chmod +x {} +
./check_dependencies.sh
cd "games/My Game"
./build.sh --run
```

On other Linux distributions, adapt the package names and installation commands. On WSL2, use a copy of the project under your Linux home directory for better filesystem performance and case-sensitivity testing. When you change source files in a separate Windows copy, synchronize them again before building in Linux.

### Dependency Checker and Optional Cross-Compilation Guidance

```bat
check_dependencies.bat
```

```bash
./check_dependencies.sh
```

The checker recommends **`install_cross_sdk.bat --target linux`** on Windows and **`./install_cross_sdk.sh --target windows`** on Linux, with the manual procedure still available in `docs/CROSS_COMPILATION.md`. It never executes the installer.

The checker validates **native** compiler tools, SDL2 packages and C++ headers. It then displays optional installation commands and target SDK requirements for compiling to the *other* desktop platform. **It does not inspect any game or its `config.json`, install packages, or mark missing optional cross-toolchains as errors.**

Guidance is maintained in `tools/dependency_guides.py` so Android can be added later without coupling this checker to project builds. The example SDK path `~/.local/share/xengine/windows-sdk` is **not a required location**: it is a per-user directory chosen for the worked example. You may use any writable directory and update `tools/toolchains.local.json` accordingly. XEngine expands `~/` to the *current user's* home directory at build time (not a hard-coded account name). For the complete verified Ubuntu → Windows steps—including SDL2 archive download, SDK assembly, `.pc` repair, validation, and the JSON configuration—see [Cross-compilation: Linux → Windows](CROSS_COMPILATION.md#linux--windows-complete-sdl2-sdk-setup).

## Recommended: Install Cross-Compilation SDKs Automatically

**SDK size and download troubleshooting:** Windows → Linux currently resolves the full Debian SDL2 development/runtime dependency graph; a first install may download hundreds of packages. Cached `.deb` archives are reused on retry. Only files required for a development sysroot are extracted; Debian `usr/share/man`, `usr/share/doc`, and `usr/share/info` are excluded because they do not affect C++ builds and may contain case-colliding filenames on Windows. The installer also skips materializing symlink aliases that already refer to the same physical file. If Windows reports a genuine `WinError 32`/`33`, it retries temporary locks; rerun without `--force` and use `install_cross_sdk.bat --target linux --debug` if the lock persists. See [Cross-compilation](CROSS_COMPILATION.md) for manual alternatives and troubleshooting.

XEngine ships with **`install_cross_sdk.bat`** (Windows) and **`install_cross_sdk.sh`** (Linux). These launch `tools/setup_cross_sdk.py` using Python 3.10+ **and the standard library only**. They do **not** use CMake, Docker or WSL, and they do not inspect games.

The setup tool distinguishes the **host** from the **target**, installs/downloads the *target libraries* to a user-writable SDK directory, and updates only that `host.target` entry in `tools/toolchains.local.json`. It does **not** silently install system-wide compilers or request administrator privileges. Install Clang/LLVM (Windows) or MinGW-w64 (Linux) first, as shown below, if the tool reports they are missing.

From the **XEngine repository root**:

```bat
rem Windows -> Linux (native Windows process; no WSL required)
install_cross_sdk.bat --target linux
rem Optional custom location (quotes support spaces)
install_cross_sdk.bat --target linux --sdk-dir "D:\Cross SDKs\linux-x86_64"
```

```bash
# Linux -> Windows
./install_cross_sdk.sh --target windows
# Optional custom SDK location
./install_cross_sdk.sh --target windows --sdk-dir "$HOME/my-cross-sdks/windows-x86_64"
```

| Host | `--target` | Result |
|---|---|---|
| Windows | `windows` or `auto` | Check native dependencies; no cross SDK installation |
| Windows | `linux` | Download Debian 12 amd64 `.deb` development/runtime packages; assemble an ELF/sysroot and update `windows.linux` |
| Linux | `linux` or `auto` | Check native dependencies; no cross SDK installation |
| Linux | `windows` | Download MinGW SDL2 development archives, prepare Windows headers/libraries/DLLs and update `linux.windows` |
| Either | `all` | Check native dependencies and install the other supported target SDK |

**Options:** `--sdk-dir` chooses a custom location (target-specific; not valid
with `all`); `--force` redownloads; `--verify-only` rechecks an existing
Windows → Linux sysroot without downloads. New SDKs default to
`%LOCALAPPDATA%\XEngine\sdk\<target>-x86_64` on Windows and
`${XDG_DATA_HOME:-$HOME/.local/share}/xengine/sdk/<target>-x86_64` on Linux.
Old `~/.xengine/sdk/` installations remain usable automatically until you run
`install_cross_sdk.bat --target linux --migrate-sdk` or
`./install_cross_sdk.sh --target windows --migrate-sdk`, respectively. The
opt-in operation **copies** the complete SDK and download cache, updates
its pkg-config paths and the Git-ignored `tools/toolchains.local.json`, and
preserves the original; it needs space for both copies. See
[SDK storage and migration](CROSS_COMPILATION.md#recommended-install-cross-compilation-sdks-automatically).

**Windows → Linux implementation:** Debian `bookworm` (amd64) package metadata and `.deb` archives come directly from Debian's official HTTPS repositories. Package SHA-256 values from Debian's package metadata are verified, dependencies are followed, `.so` symlinks are materialized for Windows filesystems, and target `.pc` files are relocated to the SDK. This is a *development sysroot assembler*, **not** an apt/dpkg replacement: it does not run Debian maintainer scripts or provide a complete Linux OS. Network access and enough disk space are required; packages/paths may change as repositories evolve. A completed installation still needs an end-to-end link and runtime test on Linux. The target glibc baseline is Debian 12, not the Windows host.

**Linux linker scripts inside a Windows-hosted sysroot:** Debian 12 uses a merged `/usr` layout (`/lib` aliases `/usr/lib`), while some glibc linker scripts, such as `libm.so` and `libc.so`, still reference absolute `/lib/x86_64-linux-gnu/...` paths. The SDK installer normalizes these *textual linker scripts only* to `/usr/lib/...` when their target exists; it never rewrites ELF binaries. It also disables Windows `pkgconf`'s automatic prefix inference for Debian multiarch `.pc` files, preventing invalid `usr/lib/include/...` paths. Both repairs run with `--verify-only`, so if your SDK already reached `[VERIFY]` there is **no reason to redownload Debian packages**.

**Recheck an existing Windows → Linux sysroot without re-extracting Debian packages:** If the first installation reached `[VERIFY]` and failed to compile/link its ELF probe, update XEngine and run `install_cross_sdk.bat --target linux --verify-only` from the repository root. This reuses the extracted sysroot, refreshes its target toolchain entry, and retries the C++/SDL2 ELF link test without downloading package indexes or extracting the 292 cached archives. It is not a repair for missing or corrupted SDK files: rerun without `--verify-only` when extraction or required-files validation fails. The installer explicitly configures the target `usr/include/SDL2` directory because `<SDL.h>` lives there and Windows `pkgconf` may relocate the `.pc` include path. Use `--debug` alongside `--verify-only` for full errors. `--verify-only` is only supported for Windows → Linux and cannot be combined with `--force`.

**Linux → Windows implementation:** The four SDL2 MinGW development tarballs are downloaded from official SDL release endpoints; the Windows x86_64 SDK is assembled under the chosen directory. The installer uses a local `nlohmann/json` header directory when available or fetches the pinned upstream single header. The MinGW-w64 compiler itself is a prerequisite.

If a download, dependency or link validation fails, the installer exits with an error and does not claim the toolchain is ready. After a successful SDK setup, build games normally using `--platform` or `build.platform` in their `config.json`:

```bat
cd "games\My Game"
build.bat --platform linux --config Release
```

```bash
cd "games/My Game"
./build.sh --platform windows --config Release
```

`--run` is not permitted for cross-compilation. **Manual setup remains fully supported**; see the following two sections, which document both directions.

### Manual alternative: Windows → Linux without WSL

The automated command `install_cross_sdk.bat --target linux` is the easiest way to create a Linux sysroot on a Windows-only machine. **Manual preparation does not require WSL either**, but it involves more steps:

1. Install MSYS2 UCRT64 Clang, LLVM, LLD and pkgconf (`pacman -S --needed mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-llvm mingw-w64-ucrt-x86_64-lld mingw-w64-ucrt-x86_64-pkgconf`).
2. Download Debian 12 `amd64` `.deb` development and runtime packages for glibc, libstdc++, SDL2, SDL2_image, SDL2_mixer, SDL2_ttf and nlohmann/json **plus all required dependencies** from the [official Debian repository](https://deb.debian.org/debian/dists/bookworm/main/binary-amd64/). Verify package hashes and versions against Debian metadata.
3. Extract their `data.tar.*` contents into a common directory, preserve ELF `.so` filenames/SONAME links and GCC startup objects, and relocate Linux `pkg-config` metadata to that sysroot.
4. Configure `windows.linux` in `tools/toolchains.local.json` with Clang, LLVM, the sysroot, the target library/include directories and the target-only `.pc` directories. Test linking an ELF64 program before building XEngine.

For the **full manual procedure**, directory examples, optional WSL-export method, executable commands and troubleshooting, see [Manual Windows → Linux SDK Setup](CROSS_COMPILATION.md#manual-windows--linux-sdk-setup-no-wsl-or-docker). The locations shown there are examples and can be changed. Do not install Linux `.so` files into MSYS2's Windows `bin/` directory.

## Complete Example: Cross-Compile Windows Games on Ubuntu

This walkthrough sets up **Linux → Windows x86_64** using MinGW-w64 and the official Windows SDL2 development packages. It is a worked example for new users; it does **not** assume an existing XEngine game. Cross-compiling requires the SDL2 libraries for the **target** operating system. The instructions are also explained in greater detail in [Cross-compilation](CROSS_COMPILATION.md#linux--windows-complete-sdl2-sdk-setup), including troubleshooting and alternative SDK locations.

**Step 1 — Install tools:**

```bash
sudo apt update
sudo apt install -y \
  g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64 \
  pkg-config wget nlohmann-json3-dev
```

**Step 2 — Choose where to store the Windows SDK and the downloads:**

```bash
SDK="$HOME/.local/share/xengine/windows-sdk"  # example; choose any writable directory
DOWNLOAD_DIR="$HOME/xengine-sdk-downloads"      # example download folder
mkdir -p "$SDK" "$DOWNLOAD_DIR"
cd "$DOWNLOAD_DIR"
```

Neither folder is prescribed by XEngine. `$HOME` refers to the **current user**, not a specific username. You may put the SDK elsewhere, provided that the paths in `toolchains.local.json` match. The shell variables above last only for the current terminal session.

**Step 3 — Download the four Windows/MinGW development archives:**

```bash
wget -c https://www.libsdl.org/release/SDL2-devel-2.32.10-mingw.tar.gz
wget -c https://www.libsdl.org/projects/SDL_image/release/SDL2_image-devel-2.8.12-mingw.tar.gz
wget -c https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-devel-2.8.2-mingw.tar.gz
wget -c https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-devel-2.24.0-mingw.tar.gz
```

These release versions are a tested **example**, not fixed XEngine version requirements. When changing versions, adjust the extracted directory names in the next step too.

**Step 4 — Assemble the Windows SDK and fix its pkg-config paths:**

Run this in the same terminal, with `SDK` and `DOWNLOAD_DIR` already set:

```bash
(
set -e
cd "$DOWNLOAD_DIR"
for archive in *.tar.gz; do
    tar -xzf "$archive"
done
for package in SDL2-2.32.10 SDL2_image-2.8.12 SDL2_mixer-2.8.2 SDL2_ttf-2.24.0; do
    source_dir="$package/x86_64-w64-mingw32"
    test -d "$source_dir" || { echo "Missing $source_dir" >&2; exit 1; }
    cp -a "$source_dir/." "$SDK/"
done
find "$SDK/lib/pkgconfig" -maxdepth 1 -type f -name '*.pc' \
    -exec sed -i "s|^prefix=.*|prefix=$SDK|" {} +
cp -a /usr/include/nlohmann "$SDK/include/"
)
```

The SDK now has Windows DLLs in `bin/`, headers in `include/`, MinGW libraries in `lib/`, and the four `.pc` files in **`lib/pkgconfig/`**. In particular, `pkg_config_libdir` must point to `lib/pkgconfig`—**not** to `bin`.

**Step 5 — Verify Windows SDL2 is detected without using Ubuntu's SDL libraries:**

```bash
SDK="$HOME/.local/share/xengine/windows-sdk"  # use your chosen location

env -u PKG_CONFIG_PATH \
  PKG_CONFIG_LIBDIR="$SDK/lib/pkgconfig" \
  pkg-config --modversion sdl2 SDL2_image SDL2_ttf SDL2_mixer
```

For the example versions, this prints `2.32.10`, `2.8.12`, `2.24.0`, and `2.8.2`. If packages are missing, inspect `find "$SDK/lib/pkgconfig" -name '*.pc'` and verify the location before proceeding.

**Step 6 — Configure the cross-toolchain:**

Return to the root of your XEngine checkout and copy the template (the directory below is just an example):

```bash
cd "$HOME/XEngine"  # replace with the actual location of your XEngine checkout
cp tools/toolchains.example.json tools/toolchains.local.json
```

Use the following `linux.windows` entry, with paths adjusted if you installed the SDK elsewhere:

```json
{
  "linux": {
    "windows": {
      "cxx": "x86_64-w64-mingw32-g++",
      "ar": "x86_64-w64-mingw32-ar",
      "windres": "x86_64-w64-mingw32-windres",
      "objdump": "x86_64-w64-mingw32-objdump",
      "pkg_config": "pkg-config",
      "pkg_config_libdir": "~/.local/share/xengine/windows-sdk/lib/pkgconfig",
      "dll_dirs": ["~/.local/share/xengine/windows-sdk/bin"],
      "lib_dirs": ["~/.local/share/xengine/windows-sdk/lib"],
      "cxxflags": ["-I~/.local/share/xengine/windows-sdk/include"]
    }
  }
}
```

`~/` is expanded dynamically to whoever runs the build; it is not replaced with a hard-coded username. The example configuration is only for Ubuntu → Windows. Native builds do not need this file, and another computer may have a different local configuration.

**Step 7 — Compile a Windows Release:**

```bash
./engine/build.sh --platform windows --config Debug
cd "games/My Game"
./build.sh --platform windows --config Release
```

The resulting `.exe` and distributable DLLs are placed under `dist/windows-x86_64/<version>/`, with a ready-to-share `dist/windows-x86_64/My Game - <version>.zip` containing the Release files directly at the archive root. Do **not** use `--run` for a cross-build. Copy the Release to Windows to verify actual game execution. The packaging system collects required MinGW runtime DLLs and leaves Windows system DLLs (such as `USP10.dll`) for Windows to provide.

For more advanced troubleshooting, custom SDK roots, and Windows → Linux (which requires a Linux sysroot), consult [CROSS_COMPILATION.md](CROSS_COMPILATION.md). The checker itself remains host-only; optional cross-toolchains are guidance, **not required dependencies**.

## Creating and Building Games

To understand the files generated by these commands and write your first
`Behaviour`, follow [Your first game](GETTING_STARTED.md). For the available
`#include` paths and C++ code layout, see
[headers and includes](INCLUDES_AND_DEPENDENCIES.md).

Project names may include letters, digits, spaces, underscores, hyphens and apostrophes. Names must start with a letter and have at most 64 characters. Quote any names containing whitespace or apostrophes; path separator and reserved filename characters are not accepted.

**Windows:**

```bat
new_project.bat "My Game"
cd "games\My Game"
build.bat --run
build.bat --config Release
```

**Linux:**

```bash
./new_project.sh "My Game"
cd "games/My Game"
./build.sh --run
./build.sh --config Release
```

Or use the root-level build dispatcher:

```bash
./build.sh engine --config Debug
./build.sh game --project "My Game" --config Debug --run
./build.sh game --project "My Game" --config Release
```

The engine is rebuilt when required by modified source files, headers or compiler flags. Each game links the `libXEngine.a` built for the **same platform, architecture and build profile**. The incremental build cache conservatively invalidates compilation units when headers change and emits compiler dependency `.d` files.

### Doxygen API generation during engine builds

If XEngine's static library is **actually rebuilt**, `tools/build.py` tries to
regenerate both C++ references using the host's Doxygen executable.
Documentation is generated under `build/docs/game/` and `build/docs/engine/`.
The regeneration happens for both the dedicated `engine` action and game builds
that have to rebuild their engine library.

- If the engine library comes from the build cache, Doxygen is not called.
- If you change **only game code**, the unchanged engine library stays cached;
  Doxygen is not called.
- If Doxygen is not installed or unavailable in `PATH`, the build prints a
  warning recommending its installation for XEngine development and continues.
- A documentation-generation error is also reported as a warning: a successful
  C++ build is not turned into a failed game build by this optional tool.
- Add `--no-doc` to an `engine` or `game` build to skip this step entirely;
  successful builds with that flag also select the online-only reference in
  the optional VS Code Doxygen Hover extension. The mode lasts until another
  successful build without the flag, and never modifies VS Code settings.

To generate docs independently of engine compilation, run from the repository
root:

```bash
python tools/build_doxygen.py
python tools/check_doxygen_docs.py
```

Open `build/docs/index.html` and choose **Game Developer** (supported gameplay
API) or **Engine Developer** (full implementation). The Markdown guides are
grouped in the sidebar under **Guides and manuals**, with light/dark themes
and links to the current feature tutorials; dated optimization histories
remain in the repository instead of cluttering the API reference. See the
[API reference](API_REFERENCE.md). Doxygen is a documentation dependency for
people working on the engine, **not** a required game runtime or SDL dependency.

### Build Configuration (`config.json`)

Each game has independent build settings. For example:

```json
{
  "project": {
    "name": "My Game",
    "version": "0.1.0",
    "icon": {
      "windows": "assets/icons/princess.ico",
      "linux": "assets/icons/princess.png"
    }
  },
  "build": {
    "platform": "auto",
    "architecture": "x86_64",
    "configuration": "Debug"
  }
}
```

The example icon files must exist if you use these paths. New games generated from `BlankGame` already have working default XEngine icons.

**Template synchronization:** The canonical `tools/templates/BlankGame/config.json`
is synchronized into existing game manifests by `tools/sync_game_configs.py`.
It adds newly introduced keys without resetting a game's version, name,
icon paths, build choices, `defines` or shipping options. Preview with
`python tools/sync_game_configs.py --check`; the synchronizer runs automatically
before every game build, creates backups when modifying files, and aborts the
build on an error. See [game config synchronization](GAME_CONFIG_SYNC.md).

| Build option | Meaning |
|---|---|
| `build.platform = "auto"` | Build natively for the host operating system |
| `build.platform = "windows"` | Produce a Windows x86_64 executable (cross-toolchain needed on Linux) |
| `build.platform = "linux"` | Produce a Linux x86_64 executable (cross-toolchain needed on Windows) |
| `build.configuration = "Debug"` | Debug compilation and editable runtime JSON |
| `build.configuration = "Release"` | Optimized compilation and packaged distribution |
| `--platform`, `--config` | Override the JSON setting for the current invocation |
| `--run` | Run a **native** build; rejected when host and target differ |

Cross-builds require `tools/toolchains.local.json`, which you create from `tools/toolchains.example.json`. The SDK directory shown in the example is **only an example**; a different folder works when the corresponding paths are updated. Configure the matching `<host>.<target>` entry with a compiler and SDL2 development SDK for the **target**, not for the host. Native Windows→Windows and Linux→Linux do not require that file. You may build Linux from Windows without WSL using a suitable Windows-hosted ELF cross-toolchain and a Linux sysroot; Windows-target builds from Linux use MinGW-w64 and Windows SDL2 libraries. For details, read [CROSS_COMPILATION.md](CROSS_COMPILATION.md).

The reserved `android`, `macos`, `ios` and `web` targets are not supported yet; selecting them produces an explicit error rather than an unusable executable.

### Debug and Release

- **Debug:** `-O0 -g3 -DXENGINE_DEBUG=1`. The runtime configuration is loaded from editable `assets/config/engine.json` on each run.
- **Release:** `-O2 -DNDEBUG -DXENGINE_RELEASE=1`; the game configuration is serialized, reversibly XOR-obfuscated and embedded into the executable. `logStats` and debugging overlays are disabled. The distribution excludes C++ sources and `config/engine.json`.

`BuildConfig.h` checks that exactly one build profile is defined. `Platform.h` uses actual compiler target macros (`_WIN32`, `__linux__`, etc.) and the selected `XENGINE_TARGET_...` build definition. Game-specific `defines` must not override reserved `XENGINE_...` macros.

Example in `assets/scripts/MyScript.hpp`:

```cpp
#if defined(XENGINE_PLATFORM_ANDROID)
    // Reserved for an eventual Android backend.
#elif defined(XENGINE_PLATFORM_DESKTOP)
    // Windows, Linux or macOS.
#endif
#if XENGINE_ENABLE_DEBUG_DRAW
    // Excluded from Release builds.
#endif
```

For a complete list of all public platform/build-mode macros, `if constexpr`
options and practical `Behaviour` examples, read
[Platforms and conditional compilation](reference/game/PLATFORMS_AND_BUILD_MODES.md).

### Working Directory and Runtime Assets

1. Headers in `assets/scripts/`, `assets/gameobjects/` and `assets/scenes/` are **compiled** when included by game translation units; they are not interpreted or shipped as runtime data.
2. In native Debug, the launcher runs with `games/<Game>/assets/` as its working directory. Thus `./textures/player.png` resolves consistently.
3. In Release, `SDL_GetBasePath()` is used to switch to the distribution directory containing the executable, with `textures/`, `audio/` and `fonts/` copied directly into that directory.
4. Save files and user-editable settings belong in per-user storage, not next
   to a read-only executable. Use `FileSystem::GetPersistentDataPath()` for game
   saves, `AtomicFile` for resilient single-file publication or `PlayerPrefs`
   for simple preferences. See [persistence](PERSISTENCE_AND_SERIALIZATION.md).

## Game Icons

The optional `project.icon` setting accepts `null`, an older Windows-only `.ico` path, or `{ "windows": "...ico", "linux": "...png" }`. Paths are relative to the game directory and cannot escape it.

| Target | Format | Packaging behavior |
|---|---|---|
| Windows | `.ico` (preferably 16, 32, 48 and 256 px inside one file) | Embedded in `.exe` with `windres`; no separate ICO distributed |
| Linux | Transparent `.png` (preferably 256×256 or 512×512 px) | Copied to `dist/icons/` and referenced by a `.desktop` application launcher |

Missing **custom** icons fall back to the synchronized project-local default icons, with a warning. Missing template/default icons and invalid icon formats produce explicit build errors. Updating the Windows ICO forces the executable to be relinked. This **does not** automatically configure an SDL window icon (`SDL_SetWindowIcon`), which is a separate runtime concern.

### Convert PNG to ICO

Use `tools/png_to_ico.py` or its `.bat` / `.sh` wrappers. The optional converter requires **Pillow**; other build tools do not require Python packages.

```powershell
py -3 -m pip install Pillow
.\tools\png_to_ico.bat
.\tools\png_to_ico.bat --png "C:\Artwork\my icon.png"
.\tools\png_to_ico.bat "My Game" princess
```

With **no arguments**, the tool converts `tools/templates/BlankGame/assets/icons/xengine_icon.png` to `xengine_icon.ico` in the same directory, replacing the template ICO if already present. With **only `--png`**, it accepts any absolute or current-directory-relative PNG path and creates an ICO beside the PNG using the same basename (requires `--force` to overwrite an existing ICO). With the legacy **GAME ICON** arguments, it reads `games/My Game/assets/icons/princess.png` and writes `.../assets/icons/princess.ico`. All modes produce **16, 24, 32, 48, 64, 128 and 256 px** images with preserved transparency and leave the source PNG untouched.

```powershell
.\tools\png_to_ico.bat "My Game" blob --png textures/blob.png
.\tools\png_to_ico.bat "My Game" princess --force --set-config
```

When GAME ICON are supplied, `--png` instead locates the source relative to that game's `assets/`, and the ICO is always written under its `assets/icons/`. `--force` permits replacing an existing ICO. `--set-config` works only with GAME ICON and updates Windows/Linux `project.icon` paths automatically. Linux supports all three modes through `./tools/png_to_ico.sh`. Use `--help` for all options.

`tools/templates/BlankGame` is the canonical source for `assets/icons/xengine_icon.png` (512×512, Linux) and `assets/icons/xengine_icon.ico` (Windows). The files must be supplied by the project maintainer; patch archives never synthesize/recolor the artwork. New projects copy these files locally. **Before every game build**, `tools/build.py` runs `sync_game_configs.py`, `sync_engine_configs.py` and `sync_game_icons.py --game <name>` to synchronize the game build manifest, engine runtime settings and reserved default icons. Changed default icons are backed up; custom filenames are untouched. A missing configured custom icon falls back to the game's own default with a console warning. The separate README brand logo remains `docs/branding/xengine_logo.svg`. For details, see [Default game icons](DEFAULT_GAME_ICONS.md) and [game config synchronization](GAME_CONFIG_SYNC.md).

## Release Distribution

Windows example:

```text
games/MyGame/dist/windows-x86_64/0.1.0/
├── MyGame.exe
├── SDL2.dll
├── other required DLLs (resolved with objdump)
├── textures/
├── audio/
├── fonts/
└── version.json
```

Linux example:

```text
games/MyGame/dist/linux-x86_64/0.1.0/
├── MyGame                  # ELF binary
├── run.sh                  # relocatable game launcher
├── setup_desktop.sh        # refresh/install application menu entry
├── MyGame.desktop          # generated on Linux after running setup_desktop.sh
├── icons/
├── lib/                    # bundled runtime .so libraries
├── textures/
├── audio/
├── fonts/
└── version.json
```

Both platforms generate a **named Release archive** alongside the existing unpacked `<version>/` directory:

| Target | Archive example | Contents |
|---|---|---|
| Windows | `dist/windows-x86_64/MyGame - 0.1.0.zip` | `.exe`, runtime DLLs, assets and `version.json` directly at archive root |
| Linux | `dist/linux-x86_64/MyGame - 0.1.0.tar.gz` | ELF binary, runtime `.so` files, assets and launchers directly at archive root, with POSIX executable permissions |

Archives are produced for **native and cross-compiled Release builds**; Debug builds do not create distribution archives. The name and version are taken from the project and its `config.json`. Extract into a dedicated folder on the target OS: the archives have **no enclosing version directory**. C++ source and header files are not shipped. Rebuilding the same version replaces its archive rather than appending duplicate entries.

**Linux execution without installation:**

For **Linux Releases**, XEngine also creates `dist/linux-x86_64/MyGame - 0.1.0.tar.gz` (beside the unpacked `0.1.0/` folder). **Transfer and extract this archive on Linux** to preserve POSIX execute permissions for the game binary and launcher scripts:

```bash
cd "games/MyGame/dist/linux-x86_64"
mkdir -p "MyGame - 0.1.0"
tar -xzf "MyGame - 0.1.0.tar.gz" -C "MyGame - 0.1.0"
cd "MyGame - 0.1.0"
./run.sh
```

For a native Linux build, or an already-extracted folder with correct permissions, use `./run.sh` directly from the Release folder. Copying **loose files** through Windows can lose the executable (`+x`) bit. If the folder was already copied, use `bash run.sh`: the launcher restores the game binary's execute bit when the directory is writable. Alternatively, run `chmod +x "MyGame" run.sh setup_desktop.sh` and then `./run.sh`. If the filesystem is mounted `noexec`, move the Release to an executable filesystem first.

**Install an application-menu entry for the current user (no `sudo`):**

```bash
./setup_desktop.sh --install
```

This regenerates the `.desktop` launcher with absolute paths and copies it to `${XDG_DATA_HOME:-$HOME/.local/share}/applications/`. If you move the distribution, run `./setup_desktop.sh` again and repeat `--install` if the launcher was already installed. On a Windows→Linux cross-build, generate the `.desktop` path **after transferring the package to Linux**. If `setup_desktop.sh` itself lost `+x` during copying, start it with `bash setup_desktop.sh --install`; the installer restores the executable bit on `run.sh` before creating the menu entry.

Native Windows Debug builds may copy compatible UCRT64 DLL dependencies next to the executable when running with `--run`, avoiding unrelated DLLs in `PATH`. Linux native Releases use `ldd` for runtime dependency inspection; cross-built Linux Releases use target `readelf` and the configured sysroot. The cross-packager follows ELF `DT_NEEDED` transitively, including vendor-specific subdirectories such as Debian `usr/lib/x86_64-linux-gnu/pulseaudio/` for `libpulsecommon-16.1.so` (already supplied by the `libpulse0` SDK package). Only target Linux libraries are copied; no Windows DLLs are used. System libraries such as glibc are not generally bundled. **Test each Release on a clean, compatible target system and review library licenses before publication.**

## Gameplay layout and header conventions

For comprehensive include-path rules, focused-vs-umbrella includes and
`Property`/`PropertyRO` usage, follow
[Headers, includes and game organization](INCLUDES_AND_DEPENDENCIES.md).

The current template uses `src/main.cpp` to start the engine, `src/InitCallback.hpp` to load assets and register scenes, and `assets/scenes/`, `assets/gameobjects/`, and `assets/scripts/` to hold reusable gameplay code. Start from the current BlankGame template rather than older one-off conversion scripts.

Use `#pragma once` in headers. Class-inline methods are implicitly `inline`, and header-defined free functions or out-of-class methods should also be `inline` to avoid duplicate symbols in multiple translation units.

To evaluate migrating an actual **XEngine-Legacy** (Visual Studio/MSVC) game, see Legacy migration feasibility (repository reference: `docs/LEGACY_MIGRATION_PLAN.md`). No automated Legacy game migrator is currently provided.

## Troubleshooting

| Symptom | Likely cause | Resolution |
|---|---|---|
| `g++ is not recognized` or `command not found` | Compiler not installed or missing from `PATH` | Use `where g++` on Windows or `which g++` on Linux; select the correct toolchain directory. |
| `Missing executable: pkg-config` | `pkgconf` missing | Install `mingw-w64-ucrt-x86_64-pkgconf` or Linux `pkg-config`. |
| `Package sdl2 was not found` | Missing SDL2 development package or wrong `.pc` search paths | Check `pkg-config --modversion sdl2 SDL2_image SDL2_ttf SDL2_mixer`. |
| `SDL_image.h: No such file or directory` | Missing SDL2_image headers | Install the matching SDL2_image development package. |
| `nlohmann/json.hpp: No such file or directory` | Missing JSON headers | Install `mingw-w64-ucrt-x86_64-nlohmann-json` or `nlohmann-json3-dev`. |
| Python version error | Python older than 3.10 or not in `PATH` | Check `py -3 --version`, `python --version`, or `python3 --version`. |
| `undefined reference` | Wrong target libraries or ABI mismatch | Use libraries built for the same target, compiler and runtime; do not mix MSVC `.lib` files into MinGW builds. |
| Windows `0xC0000139` | Incompatible or stale DLL next to the executable | Use UCRT64 runtime DLLs from the same toolchain and retry `build.bat --run`. |
| `Missing configuration ... --run ...` | Outdated game BAT quoting | Use the updated wrapper with `--project-dir "%~dp0."`. |
| Linux `.so` or loader error | Missing shared libraries, incompatible glibc, or incorrect sysroot | Inspect native binaries using `ldd`, and cross-built binaries using the target's `readelf`. |
| Cross-build SDK error | Compiler installed without matching target SDK | Follow `docs/CROSS_COMPILATION.md` and configure `tools/toolchains.local.json`. |
| Missing UI text | No redistributable font was provided | Put a licensed font under `assets/fonts/` and configure it in `assets/config/engine.json`. |
| `Doxygen is not installed` warning | Optional API generator is not on `PATH` | Install Doxygen to maintain a searchable engine reference, or continue building normally. |

## Testing

Run the automated tests from the workspace root:

```bash
python3 -m unittest discover -s tests -v
```

Tests cover project creation, template headers, build profiles, icon conversion, desktop integration, cross-toolchain selection and dependency-checker guidance. They do not replace native or cross-platform end-to-end builds with real SDL2 libraries.

For engine developers working on persistent storage, the C++ integration
tests in `tests/PersistenceTests.cpp` cover JSON/binary object graphs, length
bounds, bit I/O, backup/temp detection and Linux PlayerPrefs. With the native
SDL2 and JSON headers installed, run from the engine root:

```bash
g++ -std=c++20 -Iengine/include $(pkg-config --cflags sdl2) \
  tests/PersistenceTests.cpp engine/src/AtomicFile.cpp \
  engine/src/FileSystem.cpp engine/src/PlayerPrefs.cpp \
  $(pkg-config --libs sdl2) -o /tmp/xengine-persistence-tests

XENGINE_TEST_DATA="$(mktemp -d)"
XDG_CONFIG_HOME="$XENGINE_TEST_DATA/config" \
  /tmp/xengine-persistence-tests "$XENGINE_TEST_DATA/data"
```

Use a fresh test directory. Real power-loss durability, SDL fullscreen
behavior and Windows Registry replacement must additionally be tested on
the actual target systems; unit tests cannot prove them.

## Current Limitations

- Native Windows and Linux x86_64 compilation are supported; cross-compilation additionally requires target toolchains and development SDKs. Android support is planned but not implemented.
- `BlankGame` defaults to single-threaded execution. Parallel CPU workers are optional and keep SDL rendering on the main thread.
- Asset resources for the migrated games are intentionally not bundled.
- Runtime packaging resolves detectable shared libraries but cannot guarantee compatibility with every clean Linux or Windows system.
- The embedded Release JSON is obfuscated, **not encrypted**, and must never contain credentials or secrets.

## Multithreading and performance profiling

XEngine's optional `Engine.multiThreading` mode keeps SDL rendering, events,
scene lifecycle, physics integration, and gameplay callbacks on the main thread.
A small persistent worker pool parallelizes **read-only collision geometry
checks** and, optionally, **render command projection and frustum culling**
in sufficiently large scenes. SDL rendering and scene callbacks still run on
the main thread. `Renderer.parallelPreparation` is enabled by default when
`Engine.multiThreading` is active, but can be disabled for A/B benchmarks.
Text drawing reuses a bounded cache of up to 128 SDL_ttf textures. CPU worker
threading is not expected to
improve FPS in every game. Both modes share high-resolution, monotonic frame
and phase timing. To capture CSV measurements, set `Engine.profileCsv: true`
(default is `false`) and run the game normally until it exits. Compare results:

```bash
python tools/compare_profiles.py --game "My Game"
# Or pass both CSV files explicitly:
python tools/compare_profiles.py xengine_profile_single.csv xengine_profile_parallel.csv
```

With `--game`, the script automatically searches that game's Debug `assets/`
and Release `dist/<platform>/<version>/` folders, preferring matching pairs
from the same directory. The chosen CSV paths are printed before comparison.

See [Multithreading and profiling](MULTITHREADING_AND_PROFILING.md) for the
architecture, profiling fields, measurement protocol, VSync considerations and
testing limitations.


### Debug profiler overlay

Press **F3** to toggle runtime statistics and **F4** to advance to the next page. The panel adapts to the window size and measures font widths to prevent overlapping columns.

### Render adaptive diagnostics and deterministic CPU benchmark

The Debug F3 overlay and `Engine.profileCsv=true` now report the adaptive
render path (`disabled`, `no_workers`, `below_threshold`, `parallel`),
previous/current eligible *scene* commands, recorded command counts, background,
world, debug, and UI draw submissions, texture tint state changes avoided,
and per-category elapsed times. The threshold counts scene textures and rectangles,
**not** background tiles or debug primitives. The last frame's eligible count
controls the next frame's path selection.

To isolate the CPU cost of the selector's weight/viewport estimate without
changing which render path executes, use
`tools/sweep_render_selector_cost.py` with `--build-config Debug|Release`.
It compares scoring off, weighted, and visibility-aware under the same forced
immediate/recorded route, verifies per-frame equivalence and exports p50/p95/p99
and tail-frame diagnostics. See Adaptive render selection (repository reference: `docs/ADAPTIVE_RENDER_SELECTION.md`)
for commands, new CSV fields and limitations. These are benchmark-only switches;
the default remains `fixed` with an experimental threshold of 1536.

For a repeatable, SDL-independent CPU-only microbenchmark:

```sh
python tools/benchmark_render_cpu.py --sprites 2048 --colliders 256 --workers 3 --repeats 50
```

The benchmark uses identical deterministic geometry and collider datasets in
sequential and parallel runs, verifies matching checksums, and exports timings.
It does **not** measure actual SDL/GPU throughput or FPS. See
[`docs/RENDER_DIAGNOSTICS.md`](RENDER_DIAGNOSTICS.md).

## Reproducible multithreading benchmark

`games/XEngine Benchmark/` contains an independent SDL2 game with deterministic collision and render workloads, startup-selected concurrency modes, and a CSV comparison launcher. See [docs/BENCHMARK_GAME.md](BENCHMARK_GAME.md) for build/run commands, automated experiments and metric interpretation.

## Collision broadphase (current configuration)

The shipped `tools/templates/BlankGame/assets/config/engine.json` is the
source of truth for default collision settings. The `Collision.broadphase`
setting accepts `auto`, `bruteforce` and `grid`. In automatic mode, the engine
chooses among available broadphase strategies for the current workload;
`Collision.cellSize` may also be `auto`. The engine's actual implementation
is documented in the **Engine Developer Reference**, under `CollisionManager`
and related broadphase types.

When comparing strategies, use identical seeded workloads, confirm contact
equivalence and compare timing statistics rather than assuming one strategy
is fastest on every machine. The historical v8–v14 algorithm experiments are
kept as archived reports in `docs/` and intentionally excluded from Doxygen.
