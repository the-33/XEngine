# Maintaining XEngine documentation {#xengine_documentation_workflow}

This is the contributor guide for documenting the **current XEngine API**.
Documentation is a product for developers: it should teach a supported task,
state real preconditions and failure modes, and link to the relevant code.
Do not use public manuals as release notes or descriptions of what a patch did.

The canonical public site is the [XEngine documentation portal](https://the-33.github.io/XEngine/),
with [Game Developer](https://the-33.github.io/XEngine/game/html/index.html)
and [Engine Developer](https://the-33.github.io/XEngine/engine/html/index.html)
references. See [GitHub Pages publishing](../../GITHUB_PAGES.md) for the
automatic deployment workflow and optional local pre-commit checks.
GitHub Pages rebuilds from `main` on the server without depending on a local
hook. The versioned `.githooks/pre-commit` is opt-in: developers activate it
once in each clone with `git config --local core.hooksPath .githooks`; the
engine build does not install Git hooks. Engine rebuilds may refresh Doxygen
locally, but failed regeneration is a warning rather than a failed build.
Developers who do not want locally generated documentation can pass
`--no-doc` to `tools/build.py engine` or `tools/build.py game`: this suppresses
Doxygen during the build and selects online-only lookups in the optional VS
Code Doxygen Hover extension. The preference lasts until the next successful
build without `--no-doc` and does not disable the separate pre-commit hook
or the GitHub Pages publication workflow. The full onboarding instructions
are in the [Developer Guide](../../DEVELOPER_GUIDE.md#first-checkout-optional-contributor-tools).

## Choose the correct audience

| Audience | Location | What it may describe |
| --- | --- | --- |
| Game developer | `docs/reference/game/`, curated game-facing `docs/*.md`, and public `engine/include/` Doxygen comments | Supported game APIs, examples, lifetime rules, supported parameters and practical workflows. |
| Engine contributor | `docs/reference/engine/`, curated `docs/*.md`, plus internal implementation comments | Private implementations, SDL ownership, build and test workflows, design decisions. |
| Archived research | Historical reports under `docs/` | Past measurements and experimental conclusions; **not** presented as current contracts. |

`Doxyfile_Games` controls the game audience; `Doxyfile_Engine` adds the engine
audience. Shared current manuals such as `docs/DEVELOPER_GUIDE.md`, the
getting-started guide and cross-compilation instructions may appear in
**both** profiles. Add them to the game's curated `INPUT` list and to
`tools/build_doxygen.py::GAME_SHARED` to group their navigation entries.
The Engine profile has its own curated `INPUT`. Historical documents should
remain outside both profiles unless rewritten and verified against current
source. If an included Markdown manual links to another Markdown file, ensure
that target is included in the same Doxygen audience or adjust the document's
navigation, so warnings do not hide unresolved links.

## Documentation must evolve with the implementation

Whenever an API, build command, gameplay pattern, configuration field or
observable behavior changes:

1. Read its implementation and the existing related manuals. Update the
   *existing* guide if possible. Create a new focused guide only when a
   distinct feature cannot be explained clearly in the existing structure.
2. Update public C++ declarations with `///`, `/** ... */` and individual
   enumerator `///<` Doxygen comments. Explain purpose, units, ownership,
   lifetime, valid ranges, errors, side effects and threading constraints
   where relevant. Use `@param`, `@return`, `@warning` and `@see` when helpful.
   Include properties exposed via `Property` / `PropertyRO`, not duplicate
   internal adapter state in the game developer audience.
3. Maintain the README, the relevant manuals, guide indexes and the root
   `AI_CONTEXT.md` together. Review every existing cross-reference and remove
   obsolete claims; do not publish conflicting guides about the same feature.
4. Use relative Markdown links to repository files for browsing in GitHub:
   `[Build guide](../../DEVELOPER_GUIDE.md)`. From C++ Doxygen comments or
   generated-only manuals, use `@ref Scene`, `@ref xengine_documentation_workflow`
   and named `{#anchor}` sections when linking to generated symbols/pages.
   Check that links work in the **rendered** documentation and are not merely
   valid filenames on disk. Keep both Doxyfile inputs and navigation indexes in sync.
5. Regenerate both Doxygen audiences, inspect the relevant pages in light and
   dark modes, verify public/internal separation, run the documentation tests
   and perform the code's normal regression tests.

## Platform and build-mode APIs

Whenever the build-driver definitions, compiler-target detection or supported
targets change, update the comments in `engine/include/Platform.h` and
`engine/include/BuildConfig.h` **and** the game-facing
[Platforms and conditional compilation](../game/PLATFORMS_AND_BUILD_MODES.md)
guide. The game guide documents the exact availability of target macros,
`XENGINE_DEBUG` versus `XENGINE_RELEASE`, `NDEBUG`, and the `0`/`1` feature
macros; do not confuse `#ifdef FEATURE` with `#if FEATURE`.

## Search results and generated Markdown pages

Doxygen can index a Markdown file twice: once as the actual guide, and again
as an empty `NAME.md File Reference`. To keep the search usable,
`tools/build_doxygen.py` detects unambiguous real pages using their source
heading and generated HTML title, adds a fallback redirect to each redundant
file reference and removes the redundant search-index entries. It never
removes the real guide or any C++ source-file reference. The generator first
clears only its `build/docs/game` and `build/docs/engine` **generated output
folders** so pages left by earlier generations cannot pollute the search or
create ambiguous title matches. When adding a manual, prefer a distinctive
first-level heading (`# Title`) and a stable Doxygen `{#anchor}` for linking;
test searching for its title and filename in both generated references.

### File Members versus Class Members

The Files → **File Members** section only enumerates symbols at file scope:
free functions, global variables, typedefs, macros and similar declarations.
It is **not** the list of all public or private class fields. C++ class
properties and engine-managed state appear in **Classes** → class member
documentation. Therefore a sparse File Members → Variables list is normal:
Game Developer may expose no file-scope variables, while Engine Developer
includes the few internal file-scope globals in `UIManager.cpp`. To check
coverage, inspect the class/member XML and source files, not just that list.

## Canonical engine version and release policy

XEngine has exactly one official engine version: the root `VERSION` file.
The last legacy development iteration was **v16.37**; the official initial
development release was **0.1.0** (current release: **0.1.5**). `tools/engine_version.py` validates the
SemVer string and derives the engine distribution archive filenames;
`tools/build_doxygen.py` injects it into both Doxygen profiles. The build
must not maintain a second numeric version in either Doxyfile.

## One theme for the whole documentation site

The selector on `build/docs/index.html` and the Doxygen toggles in Game and
Engine share the single browser setting `localStorage['xengine-doc-theme']`.
The portal stores the chosen light/dark mode; the generated
`xengine_theme_sync.js` adapter applies it using Doxygen's own
`DarkModeToggle.userPreference` setter so Doxygen's per-profile cookies do
not override it. Changes within either Doxygen profile update the same
setting, including when navigating back to the portal or opening another
profile. When browsing the locally generated HTML with `file://`, browsers
may isolate `localStorage` per file; internal page links also carry the
selected `xengine-doc-theme=light|dark` query parameter to preserve the
choice across file origins. The script is included via
`Doxyfile_Games::HTML_EXTRA_FILES` and injected into every generated HTML
page by `tools/build_doxygen.py`.

**Never rewrite a same-page `#fragment` link** (including links in the
right-hand page outline) to an absolute URL with the theme query. Doxygen 1.18
intercepts those links and uses the literal fragment as a CSS selector to
scroll to that section; changing it breaks clicking the page outline. Preserve
native Doxygen event handlers and link targets rather than replacing them.
The organizer fingerprints the Doxygen navigation scripts, the lazily loaded
`navtreeindex*.js` files and the custom theme together; generated HTML and
`navtree.js` reference a matching `xengine-doc-build` query. This prevents
stale browser-cached scripts from mixing different navigation tree generations.
If native navigation fails entirely, the adapter can display grouped clickable
guide links in the left sidebar as a last-resort fallback.

Do not create a separate game/engine theme preference. The independent
**Benchmark-versus-XEngine accent** is based on page content and is still
selected by the profiling marker; it is unrelated to the light/dark choice.

The two Doxygen navigation panes must use identical per-theme visual states.
`docs/reference/theme/xengine.css` declares `--x-nav-normal-text`,
`--x-nav-hover-bg`, `--x-nav-hover-text`, `--x-nav-selected-bg`,
`--x-nav-selected-text`, and `--x-nav-selected-marker` for each light/dark
purple/green combination. Apply them to both `#nav-tree` and `#page-nav`:
normal links, hovered rows, and selected/visible rows should have perceptibly
different text colors and backgrounds in every mode. In the left tree,
Doxygen's `.label` resets font weight, so active label typography needs an
explicit rule; in the right outline, `.vis` belongs to an `li` containing its
entire subtree, so **highlight only the direct `> .item` row** rather than
the whole `li`. Keep the native link and scroll handlers intact.
The bottom breadcrumb chevrons also use Doxygen's native `#nav-path
li.navelem:after` geometry: override their separator color in green profiles,
not their position, width, transform, or padding (which would open visible
gaps between connected breadcrumb arrows).

Use [the release and compatibility policy](../../VERSIONING.md) to decide
between MAJOR, MINOR and PATCH increments. Never bump an engine version
per commit, per patch or per build. Preserve the exact bytes of published
release archives; make a new release number before publishing revised ones.
Games own `project.version` in each `games/<Game>/config.json` and engine
tools must **never** change that value as a side-effect of an engine upgrade.

## Brand a profiling or benchmarking guide

The following marker goes immediately after the Markdown's first heading:

```html
<span data-xengine-doc-theme="profiling"></span>
```

It is invisible on GitHub but survives Doxygen's Markdown processing. The
HTML organizer detects it and applies the green/teal Benchmark palette and
the corresponding light/dark header logo **for the entire page**. It also
adapts highlighted text, the sidebar, links, breadcrumbs, divider borders,
navigation icons and search accents. These items must not retain the purple
standard-engine palette on a green Benchmark page; check CSS-drawn borders
and Doxygen's generated image-based ornaments on Windows Doxygen 1.18 too.
For Doxygen 1.18, specifically verify `--nav-breadcrumb-active-bg` (the
right-hand page outline's active row), `--nav-splitbar-bg-color` and
`--nav-splitbar-handle-color` (the **left and right pane resize indicators**),
and the `--sync-icon-*` palette variables (the native clickable `<>`
tree-sync button). Do not simulate the sync button with a `::after` overlay:
it masks the click target. Earlier Doxygen versions use a native PNG that may
be recolored *in place*. The bottom-right Doxygen attribution still uses the
original `doxygen.svg`, its original shape, and native link. Doxygen 1.18 sizes
it at 75px in light mode but 60px in dark, and the SVG's near-white letter
fills lose contrast on pale backgrounds. XEngine therefore sets a constant
60px width and applies a monochrome CSS `filter` to the **image itself** for
each of the four light/dark and purple/green theme combinations. Do not cover
or replace the attribution link. Confirm that the light-mode letter interiors
remain legible on the pale footer and that the search category selector and
results popup use their current accent rather than purple hardcoded borders.
Use it for profiling, benchmark and performance-diagnostics documentation;
**do not** put it on an unrelated gameplay guide. The generator also has a
fallback list, `PROFILING_PAGE_MARKERS`, for C++ diagnostic symbol pages and
legacy pages that cannot carry a Markdown marker. Only edit that list when
styling a generated *symbol page* or old unmarked document.

GitHub's `#gh-light-mode-only` / `#gh-dark-mode-only` fragments are unrelated
to Doxygen. Keep the maintainer's themed SVGs and GitHub link structure intact.
`tools/build_doxygen.py` converts affected rendered logo links to packaged
PNG copies and chooses the visible variant using Doxygen's `.dark-mode` class.
The source Markdown is not rewritten by that transformation.

## Build and verify

The optional editor support is described in
[VS Code integration](../../VSCODE_INTEGRATION.md). When engine build flags,
SDL includes, generated game headers or project-creation behavior change,
keep `tools/vscode.py` synchronized with `tools/build.py`; the compile database
must preserve exact compiler arguments for both cached and rebuilt `.cpp`
files. Only explicit VS Code opt-in generates `.vscode/` metadata; editor
files and `build/ide/` stay outside Git and release archives.

From the repository root (Windows or Linux with Python and Doxygen on PATH):

```sh
python tools/build_doxygen.py
python tools/check_doxygen_docs.py
python -B -m unittest tests.test_dual_doxygen tests.test_build_doxygen -v
```

The public game API checker must report no undocumented declarations. You
may also run `python tools/check_doxygen_docs.py --xml build/docs/engine/xml`
as an **internal coverage inventory**, but that full profile intentionally
includes private helpers and currently reports existing undocumented internals;
it is not the public API release gate.

Open `build/docs/index.html` and test Game and Engine references, the page
navigation, normal purple branding and green profiling pages in both themes.
In particular, verify that the Engine tree is populated, `<>` remains
clickable, the right-hand outline scrolls to sections, resize-indicator
highlights match the page accent on either side, and Doxygen's original
bottom-right logo is never tinted. Check the generated site with Doxygen 1.18
as well as older supported versions when available.
Generated outputs under `build/docs/` are ignored and are not source files.

The build pipeline already invokes `tools/build_doxygen.py --doxygen ...`
after a **successful actual engine-library rebuild**, not on a cache hit or
game-only rebuild. Doxygen is optional: if absent, the build prints a warning
and succeeds without new documentation. A documentation-only change does not
recompile the engine, so run the Python command above manually. Running
`doxygen Doxyfile_Games` or `doxygen Doxyfile_Engine` directly bypasses the
two-profile organizer, page branding, Markdown-logo resolution, duplicate
Markdown search cleanup and sidebar post-processing; **do not use a raw
Doxygen command to preview the final UI**.

For background on lifecycle and source ownership, see
[Architecture, ownership and extension points](ENGINE_INTERNALS.md). For the
developer's starting workflow, see [Your first game](../../GETTING_STARTED.md).
