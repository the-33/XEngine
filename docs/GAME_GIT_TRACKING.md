# Tracking benchmark and opt-in games in Git

`games/` remains a normal directory. **`XEngine Benchmark` is always tracked.**
Every other game is **private by default**, including the bundled example games
`Blob's Quest` and `My Princess Run`. This applies regardless of the contents
of those games' `assets/` folders.

## Opting a game in

Add an empty file **at the project root** (not in `assets/`):

```text
games/
  XEngine Benchmark/        # always included
  My Game/
    .gitkeep               # intentional opt-in: include ALL project source/assets
    config.json
    assets/
    src/
  My Private Game/          # ignored, even if it has assets/audio/.gitkeep
```

From the XEngine repository root, run:

```powershell
# Windows PowerShell: opt in a game
New-Item -ItemType File -Force -Path 'games/My Game/.gitkeep'
python tools/sync_game_gitignore.py --stage
```

On Linux/macOS (where supported for Git maintenance):

```bash
touch 'games/My Game/.gitkeep'
python3 tools/sync_game_gitignore.py --stage
```

The tool regenerates `games/.gitignore`, stages the benchmark and opted-in
games, and removes **previously tracked** private games from the Git index with
`git rm --cached`. **It never deletes local project files.** Existing general
ignore rules still exclude build artifacts, game distributions, and benchmark
result CSVs. Check the staged diff with `git diff --cached --stat` before
committing.

To opt out, delete **the root-level** `.gitkeep` and run the same command.
All nested `.gitkeep` files are ordinary placeholders and **never signal
opt-in**. This explicit rule is necessary because some historical private
games already contain `.gitkeep` files under `assets/`.

To inspect whether markers and rules are synchronized without editing files:

```powershell
python tools/sync_game_gitignore.py --check
```

## Optional automatic sync on every commit

Git cannot express "ignore a directory unless it contains a file" using
`.gitignore` patterns alone. To apply the marker-based rule *automatically
when committing* on your own machine, activate the **versioned pre-commit
hook** once in that clone:

```powershell
git config --local core.hooksPath .githooks
```

This is an **explicit, one-time choice in each local Git clone**. Neither
`tools/build.py` (including engine/game builds) nor `tools/new_project.py`
nor the optional VS Code integration runs `git config` or installs this hook.
To see whether it is enabled, use:

```sh
git config --local --get core.hooksPath
```

The expected output is `.githooks`. If it is unset, Git does not invoke this
repository's versioned hook during commits.

The hook runs the equivalent of `python tools/sync_game_gitignore.py --stage`
before the commit. If staged files change engine code or Doxygen inputs, it
also regenerates and checks the local Doxygen documentation **when Doxygen is
installed**. The online GitHub Pages site is rebuilt independently on pushes
to `main`; see [GitHub Pages](GITHUB_PAGES.md). Building the engine can also
refresh local Doxygen without enabling Git hooks, but a documentation error
there is a build warning rather than a commit failure.
Passing `--no-doc` to `tools/build.py` skips generation **during that build**
and selects the online reference in the optional VS Code hover extension; it
does **not** bypass documentation checks in an enabled pre-commit hook.
For the complete beginner setup (Git, VS Code and documentation choices),
see [First checkout](DEVELOPER_GUIDE.md#first-checkout-optional-contributor-tools).

**Note:** The pre-commit hook stages *all* changed files under the benchmark
and each opted-in game and stages removal of previously tracked, unmarked game
files. Always review what is staged. If you already use a custom
`core.hooksPath`, integrate the hook into your existing hook rather than
silently overwriting that configuration. Hooks are never installed by a
Git checkout automatically; this one-time local configuration is required.

Without the hook, just run `python tools/sync_game_gitignore.py --stage` before
`git commit`. It also works for the initial migration of already tracked games.

## New projects

`tools/new_project.py` creates asset directories locally but **does not copy
or generate any `.gitkeep`** files from BlankGame. New games therefore remain
private until you explicitly add `games/<Name>/.gitkeep` at their root.

## Git history and remote copies

- `.gitignore` affects only untracked files. If a personal game is *already*
  in the Git index, `--stage` stages its removal without deleting local files.
- After committing and pushing, current repository versions omit private
  games. **Past commits and existing clones can still contain their files.**
  This mechanism does **not** erase sensitive data from Git history or remote
  forks; history rewriting must be considered separately if necessary.
- The full downloadable project archive may still contain demonstration games.
  This Git policy governs what `git add`, commits, and GitHub receive.
