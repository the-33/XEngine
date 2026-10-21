# Cross-Compilation: Windows and Linux

XEngine uses **C++20, GCC/Clang and Python 3.10+** without requiring CMake, Ninja or WSL. The **target** operating system can differ from the **host** on which the build script runs.

## Selecting a Target

Both `.bat` and `.sh` launch the same `tools/build.py`. Each game specifies a default target in `games/<Game>/config.json`:

```json
{
  "build": {
    "platform": "linux",
    "architecture": "x86_64",
    "configuration": "Release"
  }
}
```

- `"auto"` (default): build for the current host.
- `"windows"`: produce an x86_64 Windows PE/COFF executable, including when invoked from Linux.
- `"linux"`: produce an x86_64 Linux ELF executable, including when invoked from Windows.
- `--platform windows|linux|auto` temporarily overrides `build.platform` for one invocation.
- `--config Debug|Release` temporarily overrides `build.configuration`.
- `--run` is available **only** for a native build. Cross-built executables are never run on the host.

Examples from a game directory:

```bat
rem Windows to Linux: --platform may be omitted if config.json specifies linux
build.bat --platform linux --config Release
build.bat
```

```bash
# Linux to Windows: config.json may specify "platform": "windows"
./build.sh --platform windows --config Release
./build.sh
```

Build outputs are isolated by target: `build/linux-x86_64/release/`, `build/windows-x86_64/release/` and `dist/<target>-x86_64/<version>/`.

## Optional Setup Guidance in the Dependency Checker

Run `check_dependencies.bat` from Windows or `./check_dependencies.sh` from Linux. The checker validates **only native host dependencies**: compiler, binary tools, SDL2 development packages and headers. It also displays an **optional cross-compilation guide** with package installation commands and additional SDK/sysroot requirements.

The checker **does not inspect games or their `config.json` files**, install packages, probe optional cross-compilers, or change its exit status based on optional cross-build tools. A missing native dependency still causes failure; a missing optional cross-toolchain does not.

The guidance registry is `tools/dependency_guides.py`. Android or other targets can be added to that registry when corresponding build backends have been implemented. Android SDK/NDK is **not currently required**, and Android is **not currently supported** as a build destination.

**Important:** Installing a compiler through `apt` or `pacman` does **not** automatically provide SDL2 libraries for the target or a complete Linux sysroot. You must set up a compatible target SDK and configure `tools/toolchains.local.json`. Do not mix incompatible MinGW ABI/CRT variants.

## Recommended: Install Cross-Compilation SDKs Automatically

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

**Options:** `--sdk-dir` chooses a location (target-specific; not usable with `all`); `--force` redownloads/reinstalls; `--verify-only` retests an existing Windows → Linux sysroot without re-extracting it. A default SDK path under `~/.xengine/sdk/<target>-x86_64` is a **configurable example**, not a required global directory. `~` resolves to the current account's home; an SDK may be kept on another drive. The generated `toolchains.local.json` is local and ignored by Git.

**Windows → Linux implementation:** Debian `bookworm` (amd64) package metadata and `.deb` archives come directly from Debian's official HTTPS repositories. Package SHA-256 values from Debian's package metadata are verified, dependencies are followed, `.so` symlinks are materialized for Windows filesystems, and target `.pc` files are relocated to the SDK. This is a *development sysroot assembler*, **not** an apt/dpkg replacement: it does not run Debian maintainer scripts or provide a complete Linux OS. Network access and enough disk space are required; packages/paths may change as repositories evolve. A completed installation still needs an end-to-end link and runtime test on Linux. The target glibc baseline is Debian 12, not the Windows host.

**Debian virtual dependencies:** Debian package metadata can depend on a virtual package name, for example `libfluidsynth-dev` requires `libsndfile-dev`, which is provided by `libsndfile1-dev` in Debian 12. The automated installer resolves the `Provides` field and includes the selected provider and all of its transitive dependencies. If package resolution fails because an SDK setup script predates this support, update XEngine and rerun `install_cross_sdk.bat --target linux`; the index and archive cache is reused, so `--force` and deleting the SDK directory are not necessary. Truly unresolved packages still cause an error instead of producing a purportedly complete SDK.

**Large first-time Linux SDK downloads:** The installer currently follows the full Debian `Depends`/`Pre-Depends` dependency graph of the SDL2 development packages. This can mean **hundreds of packages** (including multimedia backends, graphics runtimes, and helper packages). This is expected for the current conservative installer, but not a minimum-size C++ sysroot. Archives are cached under the selected SDK's `.downloads/` directory and reused on the next run. The SDK folder is configurable with `--sdk-dir`, and no Debian packages are installed into Windows itself.

**Windows `WinError 32` or `WinError 33`:** Windows or antivirus/indexing software may temporarily lock an SDK file while it is being downloaded or extracted. The installer retries transient sharing violations and reports the affected path if the lock persists. Close editors, file explorers, sync tools or other processes using the SDK, then rerun **without `--force`** to reuse downloaded archives. For a Python traceback when diagnosing a persistent error, use `install_cross_sdk.bat --target linux --debug`. Do not turn off security software merely to work around an SDK installation error.

**Windows → Linux: LLD fails to resolve `libm.so.6` (`GROUP ( /lib/x86_64-linux-gnu/... )`):** Debian's merged-/usr system treats `/lib` as an alias of `/usr/lib`. On a Windows-hosted sysroot the installer puts files under `usr/lib`, but glibc's *text* GNU ld scripts can retain absolute `/lib` references. The installer now safely rewrites these references to `/usr/lib` **only when the target file exists**, and does not modify ELF binaries. Windows `pkgconf` also tries to infer `prefix` from a multiarch `usr/lib/x86_64-linux-gnu/pkgconfig` location, which incorrectly changes include paths into `usr/lib/include/...`; the generated toolchain disables this host-specific prefix relocation. For an SDK already extracted, apply the updated installer and run `install_cross_sdk.bat --target linux --verify-only` to apply both repairs and retest linking without downloads. If it reports a missing actual library, do **not** create an empty file or mix Windows `.dll` files into the sysroot: restore the required Linux `.so` from its official Debian package.

**Windows → Linux: SDL2 header not found during the ELF link probe:** Debian installs `<SDL.h>` under `usr/include/SDL2/`. `pkg-config --modversion` can succeed even when `pkg-config --cflags` does not provide a usable include directory on Windows, particularly when the `.pc` prefix is relocated by the host pkgconf implementation. XEngine's automated toolchain explicitly adds the *target* SDL2, C, and C++ include directories; never substitute host `C:/msys64/ucrt64/include/SDL2` headers. For an already extracted sysroot, rerun only the configuration and ELF link test with `install_cross_sdk.bat --target linux --verify-only`. This avoids all Debian package index downloads and extraction, and can be combined with `--debug`, but not `--force`. The normal command, without `--verify-only`, is still required after an incomplete download or extraction. If link validation fails, the `[VERIFY]` output includes the target SDL2 path and the `pkg-config` flags to help diagnose the next issue.

**Linux → Windows implementation:** The four SDL2 MinGW development tarballs are downloaded from official SDL release endpoints; the Windows x86_64 SDK is assembled under the chosen directory. The installer uses a local `nlohmann/json` header directory when available or fetches the pinned upstream single header. The MinGW-w64 compiler itself is a prerequisite.

If a download, dependency or link validation fails, the installer exits with an error and does not claim the toolchain is ready. After a successful SDK setup, build games normally using `--platform` or `build.platform` in their `config.json`:

```bat
cd "games\Blob's Quest"
build.bat --platform linux --config Release
```

```bash
cd "games/Blob's Quest"
./build.sh --platform windows --config Release
```

`--run` is not permitted for cross-compilation. **Manual setup remains fully supported**; see the following two sections, which document both directions.

## Target-Specific Toolchains

Choosing `windows` on Linux does not make native Linux `g++` generate Windows `.exe` files: it requires an x86_64 MinGW-w64 PE/COFF cross-compiler and **Windows** SDL2 headers, import libraries and DLLs. Conversely, Windows → Linux requires an ELF-capable compiler, a compatible x86_64 Linux **sysroot** and **Linux** SDL2 development libraries. Never use host SDL2 binaries for a cross-build.

1. Install an appropriate C++20 cross-compiler, archiver and binary inspection tools (`windres`/`objdump` for Windows; `readelf` for Linux). Prepare the **target** SDK/sysroot containing SDL2, SDL2_image, SDL2_ttf, SDL2_mixer, `nlohmann/json` and their dependencies.
2. Copy `tools/toolchains.example.json` to **`tools/toolchains.local.json`** and replace all example paths with actual installed locations. The local file is ignored by Git because toolchain paths differ between machines.
3. The configuration structure is `{ "<host>": { "<target>": { ... } } }`. XEngine selects the appropriate entry based on the host and the requested target.
4. Re-run the build. Missing toolchains and incompatible compiler targets produce explicit errors rather than silently falling back to host SDKs.

### Linux → Windows: Complete SDL2 SDK Setup

The following **worked example targets Debian/Ubuntu x86_64** and uses the GNU MinGW-w64 cross-compiler. It shows the actual sequence for preparing an independent Windows SDL2 SDK, rather than using the Linux system's SDL2 packages. It is suitable for a Linux installation or Ubuntu under WSL2. The setup is specific to this host/target pairing, not to any game.

**Requirements:** Python 3.10+, an internet connection, a working MinGW-w64 C++ compiler, standard command-line tools, and a Windows-compatible SDL2 development SDK. These instructions use specific SDL2 release archives as a **reproducible example**, not a promise that these versions will always be the newest. See the official [SDL](https://www.libsdl.org/), [SDL_image](https://www.libsdl.org/projects/SDL_image/), [SDL_mixer](https://www.libsdl.org/projects/SDL_mixer/) and [SDL_ttf](https://www.libsdl.org/projects/SDL_ttf/) release pages when selecting newer versions.

#### 1. Install the cross-compilation tools

```bash
sudo apt update
sudo apt install -y \
  g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64 \
  pkg-config wget nlohmann-json3-dev

x86_64-w64-mingw32-g++ --version
x86_64-w64-mingw32-ar --version
x86_64-w64-mingw32-windres --version
```

These provide the **Windows-targeting compiler** but not the Windows SDL2 SDK.

#### 2. Choose your SDK location

**There is no mandatory SDK directory.** In this example, the SDK lives at `$HOME/.local/share/xengine/windows-sdk`. If you prefer another location (such as `/opt/sdk/windows-x64` or a folder on another drive), change the value of `SDK` below and use the corresponding location in `tools/toolchains.local.json`.

```bash
SDK="$HOME/.local/share/xengine/windows-sdk"  # EXAMPLE: change if desired
DOWNLOAD_DIR="$HOME/xengine-sdk-downloads"      # EXAMPLE: change if desired
mkdir -p "$SDK" "$DOWNLOAD_DIR"
cd "$DOWNLOAD_DIR"
```

`$HOME` is the current Linux user's home directory, so these commands do not depend on the username. When the JSON uses `~/`, XEngine similarly expands that prefix for whoever runs the build. **The shell variable `SDK` applies to this terminal session only**; run the definition again in a new terminal.

#### 3. Download the Windows x86_64 MinGW development archives

```bash
wget -c https://www.libsdl.org/release/SDL2-devel-2.32.10-mingw.tar.gz
wget -c https://www.libsdl.org/projects/SDL_image/release/SDL2_image-devel-2.8.12-mingw.tar.gz
wget -c https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-devel-2.8.2-mingw.tar.gz
wget -c https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-devel-2.24.0-mingw.tar.gz
```

These archives were selected for this worked example; future releases may require updating the versions and the extraction-directory names in the next step. **Do not download Linux `.so` packages or MSVC `.lib` packages**: the compiler here expects Windows x86_64 MinGW-w64 import libraries.

#### 4. Extract and assemble the Windows SDK

Run these commands in the **same terminal as step 2**, with `SDK` and `DOWNLOAD_DIR` set:

```bash
(
set -e  # Fail this setup block on error, without closing your terminal.
cd "$DOWNLOAD_DIR"
for archive in *.tar.gz; do
    tar -xzf "$archive"
done

for package in \
    SDL2-2.32.10 \
    SDL2_image-2.8.12 \
    SDL2_mixer-2.8.2 \
    SDL2_ttf-2.24.0
do
    source_dir="$package/x86_64-w64-mingw32"
    if [ ! -d "$source_dir" ]; then
        echo "Missing expected SDK directory: $source_dir" >&2
        exit 1
    fi
    cp -a "$source_dir/." "$SDK/"
done

# Adjust pkg-config metadata to this SDK's actual installed location.
find "$SDK/lib/pkgconfig" -maxdepth 1 -type f -name '*.pc' \
    -exec sed -i "s|^prefix=.*|prefix=$SDK|" {} +

# nlohmann/json is header-only; copy its headers into the target SDK.
cp -a /usr/include/nlohmann "$SDK/include/"
)
```

The resulting SDK directory should contain at least:

```text
windows-sdk/
├── bin/                  # Windows .dll files
├── include/              # SDL2 and nlohmann/json headers
└── lib/
    ├── *.a                # Windows import/static libraries
    └── pkgconfig/
        ├── sdl2.pc
        ├── SDL2_image.pc
        ├── SDL2_mixer.pc
        └── SDL2_ttf.pc
```

These files are SDK inputs for compiling and packaging a Windows game; they do **not** replace the native Linux SDL2 packages.

#### 5. Verify that pkg-config reads the **Windows** SDK

```bash
SDK="$HOME/.local/share/xengine/windows-sdk"  # use your chosen SDK directory
find "$SDK/lib/pkgconfig" -maxdepth 1 -name '*.pc' -type f

env -u PKG_CONFIG_PATH \
  PKG_CONFIG_LIBDIR="$SDK/lib/pkgconfig" \
  pkg-config --modversion sdl2 SDL2_image SDL2_ttf SDL2_mixer

env -u PKG_CONFIG_PATH \
  PKG_CONFIG_LIBDIR="$SDK/lib/pkgconfig" \
  pkg-config --cflags --libs sdl2 SDL2_image SDL2_ttf SDL2_mixer
```

With the example archives, the version query should print `2.32.10`, `2.8.12`, `2.24.0`, and `2.8.2` (in the order requested). Check that the printed include and library paths point into the **Windows SDK** rather than `/usr/include/SDL2` or Ubuntu's native libraries. Clearing `PKG_CONFIG_PATH` ensures another installation does not shadow the target SDK.

If `pkg-config` reports *Package sdl2 was not found*, first verify that `pkg_config_libdir` points to **`lib/pkgconfig`**, **not** `bin` (which contains DLLs). The `.pc` files must exist in that exact directory.

#### 6. Configure tools/toolchains.local.json

Return to the root of your Linux copy of XEngine (the path below is **only an example**):

```bash
cd "$HOME/XEngine"  # replace with the actual clone path if different
cp tools/toolchains.example.json tools/toolchains.local.json
```

Edit `tools/toolchains.local.json` and retain the `linux.windows` entry (the other host entries are optional). For the **example** SDK location under the user's home directory:

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

For a **custom location**, replace the paths in `pkg_config_libdir`, `dll_dirs`, `lib_dirs`, and `cxxflags` with the corresponding actual directories. You do not have to move the SDK into XEngine or commit it to Git. `toolchains.local.json` is deliberately ignored by Git because paths are machine-specific. A native Windows or Linux build does **not** require this file.

#### 7. Compile the engine, game, and Windows Release

From the repository root, substitute the game directory as needed:

```bash
./engine/build.sh --platform windows --config Debug
cd "games/Blob's Quest"
./build.sh --platform windows --config Release
```

The second command builds a Windows PE `.exe` and packages the target SDL2 DLLs and required MinGW runtime DLLs. The Release packager locates GCC runtime libraries (such as `libgcc_s_seh-1.dll` and `libstdc++-6.dll`) from the cross-compiler itself. It **does not** redistribute Windows system DLLs such as `USP10.dll`; those are supplied by the Windows operating system. The game is **not run** on Linux; `--run` is intentionally rejected for cross-builds.

The Release is available both as the unpacked `dist/windows-x86_64/<version>/` folder and as `dist/windows-x86_64/Blob's Quest - <version>.zip`. The ZIP contains the whole `<version>/` directory with the `.exe`, required DLLs, runtime assets and `version.json` (not C++ sources). Copy the ZIP to Windows, extract it, and test the packaged `.exe` there. Successful cross-compilation and packaging alone are not proof of runtime compatibility. Use a matching MinGW-w64 ABI/CRT throughout; do not assume the packages from an MSYS2 UCRT64 installation are interchangeable with other MinGW-w64 runtimes.

#### Common setup problems

| Symptom | Likely cause and solution |
|---|---|
| `Package sdl2 was not found` | `pkg_config_libdir` points to the wrong directory; check `lib/pkgconfig` and the four `.pc` files. |
| `.pc` files exist but build cannot find them | Check `tools/toolchains.local.json`, check `~/` expansion, and verify that you are building from the updated Linux clone rather than an older copy. |
| `libgcc_s_seh-1.dll` or `libstdc++-6.dll` missing during Release packaging | Check the cross-compiler installation and use the current `tools/build.py`, which searches the MinGW runtime directories. |
| `USP10.dll` (or another Windows system DLL) reported missing | Update the packager; Windows system DLLs should not be copied from Linux. Unrecognized **third-party** DLLs should still produce an error. |
| Compiles but crashes on Windows | Verify the correct x86_64 SDL DLLs, the MinGW-w64 runtime/CRT combination, and the Windows version/graphics drivers. |

### Windows → Linux (Without WSL)

You need a compiler that **runs on Windows and produces Linux ELF**, such as LLVM/Clang with `--target=x86_64-linux-gnu`, LLD, a compatible C++ runtime, and a Linux x86_64 sysroot. GCC from MSYS2 UCRT64 alone targets Windows PE, not Linux ELF. Specify `cxx`, `ar`, `readelf`, `cxxflags`, `sysroot`, `pkg_config_libdir` and optionally `lib_dirs` in your local toolchain configuration. `pkg-config` must resolve Linux `.pc` files from the sysroot only.

```bat
cd "games\Blob's Quest"
build.bat --platform linux --config Release
```

On Windows, the packaging process uses `readelf` to inspect ELF `DT_NEEDED` entries and copies the `.so` files found in the target sysroot; it does not run host `ldd`. It also produces Linux-compatible `run.sh` and `setup_desktop.sh` scripts. Target libraries may reside in Debian multiarch **subdirectories** (for example `usr/lib/x86_64-linux-gnu/pulseaudio/libpulsecommon-16.1.so`, supplied by `libpulse0`). The Release packager checks direct library directories first and then their nested directories, copying required ELF dependencies into `dist/.../lib/` and following their own `DT_NEEDED` dependencies. It never downloads PulseAudio or other packages as a substitute for a missing SDK file, and it never searches host Windows libraries. If multiple distinct nested libraries have the same SONAME, packaging stops instead of guessing. A Windows → Linux Release is also exported as `dist/linux-x86_64/Blob's Quest - <version>.tar.gz`, alongside its unpacked `<version>/` folder. **Both Release archives contain files directly at their root (no `<version>/` wrapper). The `.tar.gz` archive is the recommended transfer format**: it records Linux executable permissions even when the archive was generated on NTFS. Move that archive to Linux and extract it there (example with version `0.1.0`):

```bash
cd "games/Blob's Quest/dist/linux-x86_64"
mkdir -p "Blob's Quest - 0.1.0"
tar -xzf "Blob's Quest - 0.1.0.tar.gz" -C "Blob's Quest - 0.1.0"
cd "Blob's Quest - 0.1.0"
./run.sh
./setup_desktop.sh --install   # optional user-level application menu entry
```

If you already copied the **loose Release directory** from Windows, Windows may have stripped `+x` from the ELF binary and the shell scripts. The corrective commands (inside the copied Release directory) are:

```bash
chmod +x "Blob's Quest" run.sh setup_desktop.sh
./run.sh
./setup_desktop.sh --install
```

You can also use `bash run.sh` without manually setting the binary bit: the generated wrapper attempts to restore it if the Release directory is writable. Likewise, `bash setup_desktop.sh --install` repairs `run.sh`'s executable bit for desktop entries. On a filesystem mounted `noexec`, changing permissions cannot make an executable runnable; move or extract the archive onto a normal Linux executable filesystem instead. A Release archive does not replace a runtime test on the target Linux distribution.

**Windows filesystem compatibility:** Debian symlink names can differ only by letter case (for example, `sd_id128_to_string.3.gz` and `SD_ID128_TO_STRING.3.gz`). On most Windows filesystems, those names refer to the same file; copying one over the other causes a permanent `WinError 32` or `SameFileError`, not a transient antivirus lock. The automatic installer excludes non-build documentation (`usr/share/man`, `usr/share/doc`, `usr/share/info`) and detects physical-file aliases before copying symlinks. Actual linker `.so` aliases, target headers, `.pc` files, and GCC startup objects remain part of the sysroot. Rerun `install_cross_sdk.bat --target linux` without `--force` to reuse cached downloads.

**glibc compatibility:** Your target distribution must support the version of glibc against which the executable was linked. For wider compatibility, compile against a suitable older baseline and test on separate Linux installations. Packaging does not bundle the kernel, system dynamic loader, or glibc.



### Manual Windows → Linux SDK Setup (No WSL or Docker)

The automatic installer above is recommended, but it is also possible to assemble the sysroot manually on **Windows only**. This alternative uses official Debian `.deb` development packages, the same source of Linux ELF libraries as the installer, and MSYS2 UCRT64 Clang/LLD. It does **not** download Windows SDL `.dll` files for the Linux target.

1. Install the Windows-hosted ELF compiler, archiver and inspection tools in the **MSYS2 UCRT64 terminal**:

   ```bash
   pacman -Syu
   # Restart UCRT64 if requested.
   pacman -S --needed \
     mingw-w64-ucrt-x86_64-clang \
     mingw-w64-ucrt-x86_64-lld \
     mingw-w64-ucrt-x86_64-llvm \
     mingw-w64-ucrt-x86_64-pkgconf
   ```

   In PowerShell check `clang++ --target=x86_64-linux-gnu -dumpmachine`, `llvm-ar --version` and `llvm-readelf --version`. Merely producing an ELF `.o` file does **not** prove that C++ linking works.

2. Choose an **arbitrary writable SDK location** (for example `$HOME/.xengine/sdk/linux-x86_64/sysroot` in PowerShell). Nothing in XEngine requires `C:\xengine-cross` or a particular Windows username.

3. Browse the official [Debian 12 package index](https://deb.debian.org/debian/dists/bookworm/main/binary-amd64/) (and the corresponding `bookworm-updates` and `bookworm-security` indices). Download the **amd64** `.deb` packages for `libc6`, `libc6-dev`, `linux-libc-dev`, `libgcc-12-dev`, `libstdc++-12-dev`, `libsdl2-dev`, `libsdl2-image-dev`, `libsdl2-mixer-dev`, `libsdl2-ttf-dev` and `nlohmann-json3-dev` **plus their complete direct/transitive dependencies**. Also include the corresponding runtime libraries. The package metadata contains `Filename`, `Depends`, `Pre-Depends` and `SHA256`; validate each download. This is why the automated path is recommended: resolving this closure manually is lengthy, and incomplete installations can compile some `.cpp` files but fail at link time.

4. Extract each `.deb` (an `ar` archive) into the **same sysroot**, unpacking its `data.tar.xz` (or equivalent payload), preserving the `usr/include`, `usr/lib/x86_64-linux-gnu`, `usr/lib/gcc/x86_64-linux-gnu` and `lib/x86_64-linux-gnu` contents. With MSYS2's `bsdtar` (install `libarchive` if needed), a typical sequence is:

   ```bash
   # Illustrative: run from a directory containing validated amd64 .deb files.
   mkdir -p "$HOME/.xengine/sdk/linux-x86_64/sysroot"
   for deb in ./*.deb; do
     bsdtar -xOf "$deb" data.tar.xz | bsdtar -xf - -C "$HOME/.xengine/sdk/linux-x86_64/sysroot"
   done
   ```

   **Note:** actual `.deb` payload names may vary (`data.tar.xz`, `data.tar.zst`, etc.); inspect each archive first. On Windows, Debian symlinks may require developer mode or explicit materialization as file copies. Preserve `.so` SONAME aliases and the `crt*.o` startup objects. A blind archive extraction without the correct layout is not a reliable sysroot.

5. Rewrite **only the SDK's copies** of Linux `.pc` files so their `prefix` and library/include paths resolve under the Windows sysroot. Configure `PKG_CONFIG_LIBDIR` to point to the SDK's Linux `usr/lib/x86_64-linux-gnu/pkgconfig`, `usr/lib/pkgconfig` and `usr/share/pkgconfig` directories, never the native UCRT64 `.pc` directory. When using fully relocated Windows paths in `.pc` files, set `pkg_config_sysroot_dir` to `""` to prevent applying the sysroot prefix twice.

6. Create `tools/toolchains.local.json` with **`windows.linux`**. The following is a *structure example*, not a finished toolchain for every layout:

   ```json
   {
     "windows": {
       "linux": {
         "cxx": "clang++",
         "ar": "llvm-ar",
         "readelf": "llvm-readelf",
         "sysroot": "C:/path/to/linux-x86_64/sysroot",
         "pkg_config": "pkg-config",
         "pkg_config_sysroot_dir": "",
         "pkg_config_libdir": [
           "C:/path/to/linux-x86_64/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig",
           "C:/path/to/linux-x86_64/sysroot/usr/share/pkgconfig"
         ],
         "cxxflags": ["--target=x86_64-linux-gnu", "-fuse-ld=lld"],
         "ldflags": ["-fuse-ld=lld"],
         "lib_dirs": ["C:/path/to/linux-x86_64/sysroot/usr/lib/x86_64-linux-gnu"]
       }
     }
   }
   ```

   In addition, supply target GCC/libstdc++ include paths and GCC CRT search directories (`-isystem`, `-B`, `-L`) appropriate to the extracted GCC version; the installer generates these automatically. Confirm `pkg-config --modversion sdl2 SDL2_image SDL2_ttf SDL2_mixer` with the target environment and verify library paths point into the Linux SDK. Link a trivial ELF program with the full sysroot and C++ standard library before attempting the engine. Do not use an unrelated Windows `libstdc++.a`.

7. From the repository root, run `engine\build.bat --platform linux --config Debug`. Then compile a game with `games\<Game>\build.bat --platform linux --config Release`. Windows cannot directly run the ELF file; test the release on Linux afterward.

**Optional manual source via WSL2:** If someone already has Ubuntu they can export its installed development headers/libs to a sysroot with `tar`, but **WSL2 is not required** and is *not* the recommended public Windows setup. Mixing an Ubuntu sysroot with Debian packages or GCC startup objects from different baselines can cause ABI/link failures.

### Home Directory Paths (`~/`)

You can use `~/` in `tools/toolchains.local.json` instead of hard-coding a
username. XEngine resolves it **at build time for the account running the
build**, using Python's `Path.expanduser()`. For example, on Linux,
`~/.local/share/xengine/windows-sdk/bin` resolves to `/home/<current-user>/.local/share/xengine/windows-sdk/bin`.
It is not permanently rewritten in the JSON. On Windows, `~/` resolves to
the running user's Windows profile directory instead.

Home expansion is supported in `pkg_config_libdir`, `pkg_config_sysroot_dir`,
`sysroot`, `lib_dirs`, `dll_dirs`, tool executable paths, and path arguments
in `cxxflags` / `ldflags` (for example, `-I~/sdk/include`).

For a working Ubuntu → Windows example, see the `linux.windows` JSON configuration in [the complete SDK setup above](#linux--windows-complete-sdl2-sdk-setup).

Remember: home expansion only resolves filesystem paths. The SDK and target
libraries must still exist and be compatible with your cross-compiler.

## `toolchains.local.json` Fields

| Field | Purpose |
|---|---|
| `cxx` | C++ compiler targeting the requested operating system. |
| `ar` | Archiver from the same toolchain; creates `libXEngine.a`. |
| `pkg_config` | `pkg-config`/`pkgconf` command. |
| `pkg_config_libdir` | Directory or list of directories containing **target** SDL2 `.pc` files; required for cross-builds. |
| `pkg_config_sysroot_dir` | Optional override; defaults to `sysroot` when set. |
| `sysroot` | Root of the target SDK; required when cross-compiling to Linux. |
| `cxxflags` | Additional compilation and linking flags, such as `--target=...`. |
| `ldflags` | Additional link-only flags. |
| `windres`, `objdump` | Windows icon resource compilation and PE/DLL inspection. |
| `readelf` | ELF shared-library dependency inspection. |
| `dll_dirs` | Directories containing target Windows DLLs. |
| `lib_dirs` | Directories containing target Linux `.so` files. |

## Cross-Compiled Debug Builds

Cross-compiled Debug binaries can be generated but cannot be run on the build host. For cross-build Debug, the assets working directory is set to `./` to avoid embedding absolute host-specific paths. After transferring the files, run the executable with the **working directory set to the game's `assets/` folder**. Prefer packaged Releases for portable testing.

## Verification

```bash
python3 -m unittest discover -s tests -v
```

The test suite covers config/CLI precedence, compiler-target validation, `pkg-config` isolation, Windows resources/icons and cross-packaging without executing foreign binaries. **Unit tests are not a substitute for actual cross-compilation** with both complete toolchains and target SDKs.
