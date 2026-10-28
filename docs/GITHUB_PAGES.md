# Publishing XEngine documentation with GitHub Pages

XEngine has two Doxygen references and a shared documentation portal. The
optional workflow in `.github/workflows/publish-docs.yml` generates and
publishes them to GitHub Pages automatically, without committing generated
HTML to the repository.

## One-time activation

1. Commit and push `.github/workflows/publish-docs.yml` to `main` in
   [the-33/XEngine](https://github.com/the-33/XEngine).
2. In the repository, open **Settings → Pages**.
3. Under **Build and deployment → Source**, select **GitHub Actions**.
4. If a workflow did not start automatically, open **Actions → Publish XEngine
   documentation → Run workflow** and choose `main`.
5. Once the `deploy` job succeeds, visit
   **https://the-33.github.io/XEngine/**. GitHub's **Settings → Pages** also
   displays the final live URL if a custom domain has been configured.

This website is **public** and includes the full Engine Developer Reference,
including internal APIs and source-derived implementation documentation. It
does not include private games or their source files.

## Site layout

| URL | Purpose |
|:--|:--|
| [Home](https://the-33.github.io/XEngine/) | Documentation portal and theme selector |
| [Game reference](https://the-33.github.io/XEngine/game/html/index.html) | Supported public C++ API and game-development guides |
| [Engine reference](https://the-33.github.io/XEngine/engine/html/index.html) | Engine internals, architecture and maintenance |

The links inside the generated site are relative, so they also work locally
from `build/docs/index.html` and under GitHub's `/XEngine/` project prefix.

## Automatic updates

- Every push to `main` generates and deploys the current docs. The workflow
  can also be run manually from the repository's **Actions** tab.
- GitHub installs a pinned Doxygen release, generates both profiles using
  `tools/build_doxygen.py` and checks the public API using
  `tools/check_doxygen_docs.py` before publication. Failed builds do not
  replace the existing live site.
- Generated files stay under the gitignored `build/docs/` directory. The
  workflow uploads that directory as a GitHub Pages artifact; no `gh-pages`
  branch or checked-in HTML is necessary.
- Users building games do **not** need Doxygen or VS Code. The workflow runs
  entirely on GitHub and does not build any games.

## Optional local pre-commit documentation refresh

To regenerate the local documentation **before committing** changes to the
engine or documentation, enable the existing versioned Git hook in your clone:

```bash
git config --local core.hooksPath .githooks
```

The hook first applies XEngine's existing marker-based game tracking and then
inspects **staged** paths. Changes in `engine/`, `docs/`, the Doxyfiles,
`VERSION` or Doxygen tooling trigger regeneration of both references and a
public API check. Other changes skip Doxygen. If generation/validation fails,
the hook stops the commit so the developer can fix the problem.

Doxygen is optional: if it is not installed, the hook warns and skips the
local refresh. **GitHub Pages independently rebuilds the docs from each
commit pushed to `main`**, so the published reference cannot depend on an
individual developer's hook. Generated `build/docs` output is never staged.

Like any pre-commit hook, generation reads the local working tree; partially
staged files may therefore appear newer in the local preview than in the
commit. GitHub Pages always rebuilds the committed version. The game-tracking
stage can add files to the index, so review staged files before committing.

To preview the site locally, install Doxygen and run from the repository root:

```bash
python tools/build_doxygen.py
python tools/check_doxygen_docs.py
```

Then open `build/docs/index.html`.

## VS Code hover integration

The published URL can be used by an **optional VS Code extension** to add
an “Open in XEngine documentation” link to C++ symbol hovers. That extension
must map each symbol to its generated Doxygen HTML page and anchor. VS Code's
normal IntelliSense and `compile_commands.json` do not add such a link by
themselves. Hosting and game builds remain independent of this integration.
