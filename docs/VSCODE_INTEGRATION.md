# Optional VS Code IntelliSense integration {#xengine_vscode_integration}

XEngine builds directly with `tools/build.py`, GCC/MinGW and SDL2; it does
**not** require CMake or an IDE. Developers who use Microsoft's C/C++ extension
in VS Code can optionally generate accurate project configuration for both the
repository root and each game directory. Developers using other editors are
not required to install VS Code or generate any editor-specific files.

## Enable once

Install the VS Code **C/C++** extension and the XEngine build toolchain for your
host platform (including GCC/MinGW, `pkg-config` and the SDL2 development
dependencies). From the XEngine repository root, run:

```sh
python tools/vscode.py setup
```

This creates a single shared `build/ide/compile_commands.json`, containing
the complete compiler arguments for the engine and **every discovered game**.
It also configures each existing game's local `.vscode/c_cpp_properties.json`
and the root `.vscode/c_cpp_properties.json` to point to that database.

- Open the **XEngine repository root** in VS Code to work on the engine and
  all games, or open `games/<Game Name>` to focus on that game. Both use the
  same database, including engine headers, SDL include paths, generated game
  headers, game-specific `defines` and the actual compiler.
- The default IntelliSense configuration is **native host / Debug / x86_64**.
  This matches the local GCC build rather than the unrelated MSVC `cl.exe`.
  `cppStandard` is `c++20`, just like `tools/build.py`.
- The database records **all** source files, including those skipped by
  compilation cache, rather than only files rebuilt in the latest run.
- `build/` and `.vscode/` are ignored by Git; generated paths are local to
  each developer and never belong in source archives or Git commits.

If a `c_cpp_properties.json` already exists, the first setup saves its bytes
under `build/ide/backups/` before installing the generated configuration.
Future refreshes preserve files you have manually edited and warn about them
rather than silently overwriting them.

## Stay up to date automatically

After opting in, every successful engine/game build refreshes the shared
database. New games created with `python tools/new_project.py "Game Name"`
receive their local editor configuration immediately. If you add a `.cpp`
or change include directories **before** the next build, refresh manually:

```sh
python tools/vscode.py refresh
```

You can explicitly choose another supported platform/profile for IntelliSense:

```sh
python tools/vscode.py refresh --platform windows --configuration Release
```

The selected profile is remembered locally. Cross-platform choices require
the corresponding target toolchain and SDL SDK already configured in
`tools/toolchains.local.json`. A build with a different target does not
silently change the chosen IntelliSense profile.

## Disable without losing personal settings

```sh
python tools/vscode.py disable
```

This removes the integration's generated database and unedited managed
configurations, and restores any original `c_cpp_properties.json` from the
backup made at setup. A configuration you edited after setup is **left
untouched**, even when disabling. XEngine always builds normally without
these optional files.

## Clickable Doxygen links in C++ hovers (optional)

The C/C++ extension uses each `.cpp` entry in `compile_commands.json` for
IntelliSense: type information, completion, Go to Definition, error squiggles
and hover details. Hover descriptions come from `/** ... */` and `///`
documentation comments in the engine/game header files. The published
Doxygen HTML reference is available at the
[XEngine documentation portal](https://the-33.github.io/XEngine/), with separate
[game API](https://the-33.github.io/XEngine/game/html/index.html) and
[engine internals](https://the-33.github.io/XEngine/engine/html/index.html).
VS Code does **not** automatically add links to those pages to native C/C++
hovers. XEngine includes the optional **XEngine Doxygen Hover** extension,
with its source code and a ready-to-install VSIX under
`tools/vscode_doxygen_hover/`. To install it, run from the repository root:

```sh
python tools/vscode.py hover-install
python tools/vscode.py hover-status
```

Hover over an XEngine class or method in a C++ file. Alongside the normal
IntelliSense tooltip, the extension contributes a clickable
**📖 Open in XEngine documentation** link. It targets the corresponding
Doxygen class page or method anchor in your browser rather than the portal
homepage. It reads published XML indexes from GitHub Pages and uses the
C/C++ definition provider to disambiguate member names when available;
ambiguous results are omitted instead of linking to an unrelated symbol.

- The `game` reference is used for game code; files under `engine/include/`
  or `engine/src/` use the full `engine` reference. Settings
  `xengineDoxygen.profile` and `xengineDoxygen.baseUrl` can override these
  choices. Source indexes are cached for 30 minutes and can be refreshed with
  **XEngine: Refresh Doxygen Symbol Index** in the Command Palette.
- The extension uses the network to read the already published XML indexes.
  If offline, the existing C/C++ IntelliSense hover is unaffected. Some
  overloaded or ambiguous symbols cannot be mapped reliably; those links
  are deliberately omitted.
- It is **not installed by** `setup`, `refresh`, compilation or game creation.
  It is installed only by the explicit `hover-install` command or the
  standard VS Code **Install from VSIX…** action. The source and prebuilt
  VSIX are tracked in Git; generated `.vscode/` settings and `build/ide/`
  are not.
- For VS Code Insiders or a nonstandard command-line launcher, append
  `--code-cli code-insiders` (or the absolute path to your launcher). When
  using **Remote - WSL**, run the install command in the appropriate WSL
  environment so that the extension is installed in the extension host used
  for your C++ project.

To remove it, run:

```sh
python tools/vscode.py hover-uninstall
```

`python tools/vscode.py disable` affects only the generated IntelliSense
files; it deliberately does **not** uninstall an extension from your VS Code
user profile. See [XEngine Doxygen Hover source and usage](https://github.com/the-33/XEngine/blob/main/tools/vscode_doxygen_hover/README.md)
and [GitHub Pages publishing](GITHUB_PAGES.md).

Running `clang-tidy` for additional static analysis is a separate optional
tool.
