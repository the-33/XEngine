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

## Hover documentation and linting

The C/C++ extension uses each `.cpp` entry in `compile_commands.json` for
IntelliSense: type information, completion, Go to Definition, error squiggles
and hover details. Hover descriptions come from `/** ... */` and `///`
documentation comments in the engine/game header files. The generated
Doxygen HTML reference is a separate browser resource; VS Code does not
automatically read that HTML for hover documentation. Running `clang-tidy`
for additional static analysis is a separate optional tool.
