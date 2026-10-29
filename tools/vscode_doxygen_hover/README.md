# XEngine Doxygen Hover for VS Code

An **optional**, dependency-free extension that adds clickable links to XEngine
Doxygen pages from C++ symbol hovers. It prefers the **locally generated HTML**
of the current checkout and falls back to the **published reference**. It does not modify
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
`tools/vscode_doxygen_hover/xengine-doxygen-hover-0.2.0.vsix`.
Or run:

```sh
code --install-extension tools/vscode_doxygen_hover/xengine-doxygen-hover-0.2.0.vsix
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
**📖 Open in XEngine documentation**. It opens the corresponding class page
or member anchor, preferentially from the local `build/docs/` directory.

To generate local docs after editing XEngine, run from the engine root:

```sh
python tools/build_doxygen.py
```

No local docs? You can still use the extension normally with GitHub Pages.
If the local XML knows a symbol but its HTML page is missing, the extension
also falls back to the published reference. The hover indicates
*Local Doxygen reference* when it has selected a local page.

If you build with `python tools/build.py engine --no-doc` (or pass `--no-doc`
to `game`), XEngine skips Doxygen generation and automatically selects **online
only** for that checkout even if old local docs remain. The mode is recorded
only under the Git-ignored `build/ide/`. The next successful build **without**
`--no-doc` restores the normal local-first behavior. This does not change VS
Code settings or affect the independent Git pre-commit hook.

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

By default, the extension searches upwards from the open C++ file to locate
its XEngine checkout. It then checks the selected reference's local files:

- `build/docs/game/xml/index.xml` and `build/docs/game/html/`
- `build/docs/engine/xml/index.xml` and `build/docs/engine/html/`

The local XML is read on demand and reloaded when its modification time or
size changes. **No network connection is needed** while a matching local
HTML page is available. When no local index/page/symbol matches, the
extension downloads Doxygen's **existing public XML indexes** from:

- https://the-33.github.io/XEngine/game/xml/index.xml
- https://the-33.github.io/XEngine/engine/xml/index.xml

It uses the reference for game developers when you edit game code, and the
full engine reference when the edited file resides under `engine/include/`
or `engine/src/`. Configure `xengineDoxygen.profile` to `game` or `engine`
to override this choice. Set `xengineDoxygen.preferLocal` to `false` to
always use the online reference, regardless of the last build flags. The
`baseUrl` defaults to
`https://the-33.github.io/XEngine/`.

Online XML is retrieved on demand, cached for 30 minutes and never added to
the Git index. The extension does not generate Doxygen itself. A link to local
HTML is opened with the operating system's default handler; if that fails,
the extension attempts to open the corresponding online URL. In Remote - WSL,
the local browser may not be able to access Linux paths. In that case set
`xengineDoxygen.preferLocal` to `false` or use a Linux browser that can access
the files. If neither reference can be loaded,
the normal IntelliSense hover still works; inspect VS Code **Output →
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

- Links are limited to symbols present in the selected Doxygen index. Local
  references can be out of date until you regenerate them.
- For similarly named methods, a known declaring class or a working C++
  definition provider is required for an unambiguous choice.
- The extension does not recognize arbitrary user-defined symbols as XEngine
  symbols and does not expose or upload any game source files.
- Automated tests cover XML parsing and URL generation; they are not a
  substitute for trying the VSIX in an actual Windows/WSL VS Code instance.
