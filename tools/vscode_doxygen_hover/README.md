# XEngine Doxygen Hover for VS Code

An **optional**, dependency-free extension that adds clickable links to the
published XEngine Doxygen pages from C++ symbol hovers. It does not modify
or replace the C/C++ IntelliSense hover, and does not create `.vscode/` files
in XEngine or in a game project.

## Install from XEngine

The **source code and prebuilt VSIX are versioned in the XEngine repository**
under `tools/vscode_doxygen_hover/`. No download from the Marketplace and no
Node.js installation are needed to use the extension.

From the XEngine repository root, run:

```sh
python tools/vscode.py hover-install
python tools/vscode.py hover-status
```

To remove it, run `python tools/vscode.py hover-uninstall`. These commands
only manage the extension in the VS Code installation targeted by the `code`
command. If your launcher is named `code-insiders`, add
`--code-cli code-insiders`; for another installed VS Code use a full path.
**The normal `tools/vscode.py setup` never installs this extension.**

Alternatively, install it manually using
**Extensions → ⋯ → Install from VSIX…** and choose
`tools/vscode_doxygen_hover/xengine-doxygen-hover-0.1.0.vsix`.
Or run:

```sh
code --install-extension tools/vscode_doxygen_hover/xengine-doxygen-hover-0.1.0.vsix
```

If you use VS Code Remote - WSL, install the extension into the **WSL**
extension host as well when prompted. No Marketplace account or publisher
is required for manual VSIX installation.

It is entirely opt-in: no extension installation or VS Code configuration is
needed by people who build XEngine with another editor.

## Use

Open either the **XEngine repository root** or an individual
`games/<Game>/` folder. Hover over an XEngine class or method, for example
`GameObject` or `AssetManager::LoadTexture`, and click
**📖 Open in XEngine documentation**. The browser opens its corresponding
published class page or a member anchor.

For symbol resolution, continue using the official VS Code **C/C++**
extension and XEngine's existing optional `python tools/vscode.py setup`
configuration. The extension asks VS Code for Go to Definition information
when a member name is ambiguous. Method calls on an object are therefore
most reliable after IntelliSense has indexed the project's headers.

To avoid misleading links, ambiguous methods without a known class are
omitted. Overloads with the same name may show several available member
anchors; each link points to a real Doxygen member. Hovering over an XEngine
class name always links directly to its class page.

## How the links are found

The extension downloads and caches Doxygen's **existing public XML indexes**
from:

- https://the-33.github.io/XEngine/game/xml/index.xml
- https://the-33.github.io/XEngine/engine/xml/index.xml

It uses the reference for game developers when you edit game code, and the
full engine reference when the edited file resides under `engine/include/`
or `engine/src/`. Configure `xengineDoxygen.profile` to `game` or `engine`
to override this choice. The `baseUrl` defaults to
`https://the-33.github.io/XEngine/`.

The XML is retrieved on demand, cached for 30 minutes and never added to
the Git index. The engine and game builds do not run this extension. Network
access is needed when refreshing the cache. If the index cannot be loaded,
the normal IntelliSense hover still works; inspect the VS Code **Output →
XEngine Doxygen Hover** channel for details.

Commands in the Command Palette:

- **XEngine: Open Online Documentation**
- **XEngine: Refresh Doxygen Symbol Index**

## Build your own VSIX

If you want to change the extension itself, use Node.js and the official
VS Code packaging utility; there are **no runtime npm dependencies**:

```sh
npm install -g @vscode/vsce
npm test
vsce package --no-dependencies --allow-missing-repository --skip-license
```

Source files: `index.js` (Doxygen lookup), `extension.js` (VS Code hover
provider), `test/` (isolated tests). Place the freshly packaged VSIX at this
folder's top level if updating the version shipped by XEngine. Do not move it
under `dist/`, which is ignored by XEngine's `.gitignore`. Update
`package.json`, the VSIX filename and the `HOVER_VSIX` value in
`tools/vscode.py` together.

## Limitations

- Links are limited to symbols present in the selected Doxygen index.
- For similarly named methods, a known declaring class or a working C++
  definition provider is required for an unambiguous choice.
- The extension does not recognize arbitrary user-defined symbols as XEngine
  symbols and does not expose or upload any game source files.
- Automated tests cover XML parsing and URL generation; they are not a
  substitute for trying the VSIX in an actual Windows/WSL VS Code instance.
