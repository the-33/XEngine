"""Build and organise XEngine's two Doxygen references (stdlib only).

Run from any directory: python tools/build_doxygen.py
This also normalises the Markdown navigation tree: versions of Doxygen do not
reliably reflect @subpage relationships between separate Markdown files.
"""
from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
import html
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

# The script is also loaded directly by tests with importlib; the sibling
# module must resolve both then and when run from an arbitrary working dir.
TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))
from engine_version import read_engine_version


ROOT = Path(__file__).resolve().parents[1]

# Only guides describing the current code are inputs to Doxygen. Documents
# recording historic experiments remain in docs/ but are not API contracts.
GAME = {
    "Getting started": ("PROJECTS__AND__INCLUDES", "SCENES__AND__BEHAVIOURS",
                        "PLATFORMS__AND__BUILD__MODES"),
    "Game systems": ("INPUT__AND__TIME", "RENDERING__AND__UI", "PHYSICS__AND__COLLISIONS",
                     "ASSETS__AND__AUDIO", "MATH__AND__UTILITIES"),
    "Files and persistence": ("DATA__AND__PERSISTENCE",),
}
GAME_SHARED = {
    "Start, includes and full developer guide": (
        "GETTING__STARTED", "INCLUDES__AND__DEPENDENCIES", "DEVELOPER__GUIDE",
        "API__REFERENCE"),
    "Windows, settings and data": (
        "WINDOW__AND__CAMERA", "PERSISTENCE__AND__SERIALIZATION"),
    "Build targets, assets and shipping": (
        "CROSS__COMPILATION", "DEFAULT__GAME__ICONS", "GAME__CONFIG__SYNC",
        "GAME__GIT__TRACKING", "ENGINE__CONFIG__MIGRATION", "RENDER__BACKENDS",
        "VERSIONING", "VSCODE__INTEGRATION"),
    "Optional profiling and diagnostics": (
        "MULTITHREADING__AND__PROFILING", "RENDER__DIAGNOSTICS", "BENCHMARK__GAME"),
}
ENGINE = {
    "Architecture and gameplay": ("ENGINE__INTERNALS", "API__REFERENCE", "ARCHITECTURE",
                                  "DEVELOPER__GUIDE", "GETTING__STARTED", "INCLUDES__AND__DEPENDENCIES",
                                  "WINDOW__AND__CAMERA", "PERSISTENCE__AND__SERIALIZATION"),
    "Build, platforms and configuration": ("CROSS__COMPILATION", "RENDER__BACKENDS", "DEFAULT__GAME__ICONS",
                                           "GAME__CONFIG__SYNC", "GAME__GIT__TRACKING",
                                           "ENGINE__CONFIG__MIGRATION", "VERSIONING", "VSCODE__INTEGRATION"),
    "Profiling and diagnostics": ("BENCHMARK__GAME", "MULTITHREADING__AND__PROFILING", "RENDER__DIAGNOSTICS"),
}

# A profiling page uses the XEngine Benchmark identity rather than the
# purple general-purpose engine theme. This is based on the documentation's
# role, not whether an arbitrary class happens to expose timing counters.
PROFILING_PAGE_MARKERS = (
    "BENCHMARK__GAME", "MULTITHREADING__AND__PROFILING", "RENDER__DIAGNOSTICS",
    "RENDER__BACKENDS", "ENGINE__MANUALS__ADVANCED", "XENGINE_MANUAL_ADVANCED",
    "RENDERDIAGNOSTICS", "FRAMEPROFILE",
)

# A maintainer can explicitly brand any Markdown guide as profiling without
# relying on Doxygen's version-dependent output filename. Doxygen passes this
# harmless HTML span through to the generated page; GitHub renders nothing.
PROFILING_DOCUMENT_MARKER = re.compile(
    r'<span\b[^>]*\bdata-xengine-doc-theme\s*=\s*["\']profiling["\'][^>]*>', re.I
)


def is_profiling_page(filename: str) -> bool:
    """Recognise benchmarking, profiling and performance diagnostics pages."""
    name = filename.upper()
    return any(marker in name for marker in PROFILING_PAGE_MARKERS)


def style_document_pages(html_directory: Path) -> tuple[int, int]:
    """Apply correct brand/theme to generated HTML without editing GitHub MD.

    GitHub's #gh-light-mode-only / #gh-dark-mode-only suffixes are GitHub UI
    conventions; Doxygen cannot resolve those image paths or hide the opposite
    theme variant. Use local copied PNGs and Doxygen's own `.dark-mode` class.
    """
    header = re.compile(r'(<td\s+id="projectlogo">\s*)<img\b[^>]*>\s*(</td>)', re.I)
    image = re.compile(r'<img\b[^>]*>', re.I)
    src = re.compile(r'\bsrc="([^"]+)"', re.I)
    classes = re.compile(r'\bclass="([^"]*)"', re.I)
    themed_pages = 0
    inline_logos = 0

    for page in html_directory.glob('*.html'):
        source = page.read_text(encoding='utf-8')
        original_html = source
        # Source-level marker is authoritative even when Doxygen changes the
        # generated filename. The legacy filename list keeps API/diagnostic
        # symbol pages and older, unmarked guides compatible.
        profiling = (bool(PROFILING_DOCUMENT_MARKER.search(source)) or
                     is_profiling_page(page.name))
        brand = 'xengine_benchmark_logo' if profiling else 'xengine_logo'
        brand_label = 'XEngine Benchmark' if profiling else 'XEngine'
        for variant in ('light', 'dark'):
            if not (html_directory / f'{brand}_{variant}.png').is_file():
                raise FileNotFoundError(f'Missing {brand}_{variant}.png in {html_directory}')

        if profiling:
            def add_theme_class(match: re.Match[str]) -> str:
                tag = match.group(0)
                existing = classes.search(tag)
                if existing:
                    return (tag[:existing.end(1)] + ' xengine-profiling' +
                            tag[existing.end(1):])
                return tag[:-1] + ' class="xengine-profiling">'

            source, count = re.subn(r'<html\b[^>]*>', add_theme_class, source,
                                    count=1, flags=re.I)
            if count != 1:
                raise ValueError(f'Cannot apply profiling theme to {page}')
            themed_pages += 1

        def change_header(match: re.Match[str]) -> str:
            return (match.group(1) +
                    f'<img class="xengine-header-light" alt="{brand_label} logo (light theme)" '
                    f'src="{brand}_light.png"/>' +
                    f'<img class="xengine-header-dark" alt="{brand_label} logo (dark theme)" '
                    f'src="{brand}_dark.png"/>' + match.group(2))

        source = header.sub(change_header, source)

        def change_inline(match: re.Match[str]) -> str:
            nonlocal inline_logos
            tag = match.group(0)
            found = src.search(tag)
            if not found:
                return tag
            original = html.unescape(found.group(1))
            variant = re.search(r'#gh-(light|dark)-mode-only(?:$|&)', original)
            basename = original.split('?', 1)[0].split('#', 1)[0].rsplit('/', 1)[-1]
            if not variant or not re.fullmatch(r'xengine_(?:benchmark_)?logo_(?:light|dark)\.svg', basename):
                return tag
            mode = variant.group(1)
            replacement = basename[:-4] + '.png'
            if not (html_directory / replacement).is_file():
                raise FileNotFoundError(f'Missing Markdown logo {replacement} in {html_directory}')
            tag = tag[:found.start(1)] + replacement + tag[found.end(1):]
            marker = f'xengine-inline-logo-{mode}'
            current_class = classes.search(tag)
            if current_class:
                tag = tag[:current_class.start(1)] + current_class.group(1) + ' ' + marker + tag[current_class.end(1):]
            else:
                tag = tag.replace('<img', f'<img class="{marker}"', 1)
            inline_logos += 1
            return tag

        source = image.sub(change_inline, source)
        # The portal, Game and Engine must never keep independent light/dark
        # choices. Load the adapter after Doxygen's own theme-toggle script.
        if '</body>' in source and 'xengine_theme_sync.js' not in source:
            source = source.replace('</body>',
                                    '<script src="xengine_theme_sync.js"></script>\n</body>', 1)
        if source != original_html:
            page.write_text(source, encoding='utf-8')
    return themed_pages, inline_logos


def redirect_empty_markdown_file_references(html_directory: Path) -> int:
    """Open the actual guide when Doxygen search finds an empty .md File Reference.

    Doxygen lists a Markdown INPUT both as a rendered page and as a source-file
    reference. The latter is normally empty, yet can appear first in search
    results. Match the file basename to the source H1 and the generated page
    title rather than assuming a Doxygen-version-specific HTML filename.
    """
    title_re = re.compile(r'<title>\s*(.*?)\s*</title>', re.I | re.S)
    heading_re = re.compile(r'^#\s+(.+?)\s*$', re.M)
    anchor_re = re.compile(r'\s+\{#[^}]+\}\s*$')
    content_re = re.compile(r'<div\s+class="contents"[^>]*>', re.I)

    def normalized_heading(heading: str) -> str:
        # Doxygen 1.12 emits inline Markdown backticks as escaped HTML <tt>
        # inside <title>, but the source Markdown heading still has backticks.
        # Both names describe the same page and should resolve identically.
        plain = re.sub(r'<[^>]*>', '', html.unescape(heading))
        plain = re.sub(r'[`*_]', '', plain)
        return ' '.join(plain.split())

    targets: dict[str, set[str]] = defaultdict(set)
    for page in html_directory.glob('*.html'):
        match = title_re.search(page.read_text(encoding='utf-8'))
        if not match:
            continue
        title = normalized_heading(match.group(1).removeprefix('XEngine: '))
        if not title.endswith(' File Reference'):
            targets[title].add(page.name)

    markdown_titles: dict[str, set[str]] = defaultdict(set)
    for source in (ROOT / 'docs').rglob('*.md'):
        if any(part in {'build', '__pycache__'} for part in source.parts):
            continue
        match = heading_re.search(source.read_text(encoding='utf-8'))
        if match:
            markdown_titles[source.name].add(normalized_heading(anchor_re.sub('', match.group(1))))

    redirected = 0
    for reference in html_directory.glob('*_8md.html'):
        original = reference.read_text(encoding='utf-8')
        if 'data-xengine-guide-redirect=' in original:
            continue
        match = title_re.search(original)
        if not match:
            continue
        title = html.unescape(match.group(1)).removeprefix('XEngine: ').strip()
        suffix = ' File Reference'
        if not title.endswith('.md' + suffix):
            continue
        name = title[:-len(suffix)]
        matches = {destination for heading in markdown_titles.get(name, ())
                   for destination in targets.get(heading, ())}
        if len(matches) != 1:
            # An ambiguous page stays untouched, avoiding a wrong redirect.
            continue
        target = html.escape(next(iter(matches)), quote=True)
        if '</head>' not in original or not content_re.search(original):
            continue
        changed = original.replace('</head>',
                                   f'<meta http-equiv="refresh" content="0;url={target}" '
                                   'data-xengine-guide-redirect="1"/>\n</head>', 1)
        changed = content_re.sub(lambda m: m.group(0) +
                                 f'<p>This Markdown file is documented in '
                                 f'<a href="{target}">the full guide</a>.</p>', changed, count=1)
        reference.write_text(changed, encoding='utf-8')
        redirected += 1
    return redirected


def prune_duplicate_markdown_search(html_directory: Path) -> int:
    """Suppress empty .md File Reference hits from Doxygen search.

    Doxygen's search JS includes Markdown *files* as well as Markdown *pages*.
    The former are empty and confusing. Doxygen versions can fail to match a
    file reference with its rendered guide by title; still remove the empty
    file hit while keeping the actual guide/page entries searchable.
    """
    file_refs = {page.name for page in html_directory.glob('*_8md.html')}
    if not file_refs:
        return 0
    removed = 0
    for index in (html_directory / 'search').glob('*.js'):
        if not (index.name.startswith('all_') or index.name.startswith('files_')):
            continue
        original = index.read_text(encoding='utf-8')
        kept = []
        for line in original.splitlines(keepends=True):
            stripped = line.strip()
            if stripped.startswith('[') and stripped.endswith(('],', ']]', ']],', ']]],')) and any(
                f"{ref}'" in line or f'{ref}"' in line for ref in file_refs
            ):
                removed += 1
                continue
            kept.append(line)
        changed = ''.join(kept)
        if changed != original:
            index.write_text(changed, encoding='utf-8')
    return removed


def _page_group(entry: list, profile: str) -> str | None:
    """Choose a user-visible category; no generated C++ symbol is moved."""
    url = entry[1] or ""
    if url == "index.html" or not (url.startswith("md_") or url.startswith("xengine_")):
        return None
    # Doxygen versions differ in the number of underscores used for Markdown
    # path components. Normalize both sides before comparing; every Markdown
    # guide must remain under one expandable sidebar section.
    key = re.sub(r'[^a-z0-9]', '', url.lower())

    def contains(name: str) -> bool:
        return re.sub(r'[^a-z0-9]', '', name.lower()) in key

    if profile == "game":
        if url == 'xengine_game_build_macros.html':
            return 'Getting started'
        if contains("reference_2game_2"):
            return next((category for category, names in GAME.items()
                         if any(contains(name) for name in names)), "Other game guides")
        for category, names in GAME_SHARED.items():
            if any(contains(name) for name in names):
                return category
        return "Other current guides"
    if "reference_2game" in url or url == "xengine_game_reference.html":
        return "Game-facing feature guides"
    if "ENGINE__MANUALS" in url or url == "xengine_engine_manuals.html":
        return "Manual directory"
    for category, names in ENGINE.items():
        if any(contains(name) for name in names):
            return category
    return "Engine developer guides"


def group_markdown_navigation(html_directory: Path, profile: str) -> int:
    """Group Markdown in the sidebar and update sync indexes to match paths.

    Doxygen stores NAVTREE and NAVTREEINDEX*.js separately. Rewriting only
    NAVTREE would make page highlighting/synchronisation point at wrong nodes.
    """
    nav = html_directory / "navtreedata.js"
    source = nav.read_text(encoding="utf-8")
    marker = "var NAVTREE =\n"
    start = source.index(marker) + len(marker)
    end = source.index(";\n", start)
    tree = json.loads(source[start:end])
    root = tree[0][2]

    categories: dict[str, list] = {}
    originals: list[tuple[int, list, str | None]] = []
    for old_index, entry in enumerate(root):
        category = _page_group(entry, profile)
        originals.append((old_index, entry, category))
        if category:
            categories.setdefault(category, []).append((old_index, entry))
    if not categories:
        return 0

    arranged: list[list] = []
    remap: dict[int, list[int]] = {}
    inserted = False
    for old_index, entry, category in originals:
        if category:
            if not inserted:
                group_index = len(arranged)
                section_nodes = []
                for category_index, (title, members) in enumerate(categories.items()):
                    section_nodes.append([title, "", [item for _, item in members]])
                    for member_index, (original_index, _) in enumerate(members):
                        remap[original_index] = [group_index, category_index, member_index]
                arranged.append(["Guides and manuals", "", section_nodes])
                inserted = True
            continue
        remap[old_index] = [len(arranged)]
        arranged.append(entry)

    tree[0][2] = arranged
    nav.write_text(source[:start] + json.dumps(tree, ensure_ascii=False, indent=2) +
                   source[end:], encoding="utf-8")

    # Each link-to-node path starts with its original index in root.children.
    # Preserve the remaining indices (headings, members, etc.) unchanged.
    match_path = re.compile(r'(:\s*)\[([0-9,\s]+)\]')
    for index_file in html_directory.glob("navtreeindex*.js"):
        content = index_file.read_text(encoding="utf-8")

        def replace(match: re.Match[str]) -> str:
            path = [int(n) for n in match.group(2).split(",")]
            if not path or path[0] not in remap:
                return match.group(0)
            new_path = [*remap[path[0]], *path[1:]]
            return match.group(1) + json.dumps(new_path, separators=(",", ":"))

        index_file.write_text(match_path.sub(replace, content), encoding="utf-8")
    return sum(map(len, categories.values()))


def version_navigation_assets(html_directory: Path, version: str) -> int:
    """Keep Doxygen's lazily loaded tree JS in sync with its rewritten indices.

    In particular, repeatedly generating `file://` docs on Windows can leave
    a cached navtreedata.js or navtreeindexN.js from the previous build. The
    HTML then has no usable navigation tree, although the title/main page is
    current. Cache-bust both eager and dynamic assets with the *same* content
    fingerprint, without changing their relative paths or exported JS names.
    """
    assets = [html_directory / name for name in (
        'navtreedata.js', 'navtree.js', 'navtree.css', 'doxygen.css',
        'xengine.css', 'xengine_theme_sync.js')]
    assets += sorted(html_directory.glob('navtreeindex*.js'))
    loader = re.compile(r"script\.src = scriptName\+'\.js(?:\?xengine-doc-build=[^']+)?';")
    digest = hashlib.sha256()
    for asset in assets:
        if asset.is_file():
            digest.update(asset.name.encode('utf-8'))
            data = asset.read_bytes()
            # Documentation tests may run the organizer on a previously
            # generated tree; ignore its old cache stamp when hashing.
            if asset.name == 'navtree.js':
                data = loader.sub("script.src = scriptName+'.js';",
                                  data.decode('utf-8')).encode('utf-8')
            digest.update(data)
    stamp = f'{version}-{digest.hexdigest()[:12]}'

    nav = html_directory / 'navtree.js'
    source = nav.read_text(encoding='utf-8')
    if not loader.search(source):
        raise ValueError(f'Unsupported Doxygen navtree.js script loader: {nav}')
    # Every dynamically loaded navtreeindexN.js (and lazy subtree index.js)
    # must use the same fingerprint as the top-level HTML includes.
    nav.write_text(loader.sub(
        f"script.src = scriptName+'.js?xengine-doc-build={stamp}';", source), encoding='utf-8')

    paths = ('navtreedata.js', 'navtree.js', 'navtree.css', 'doxygen.css',
             'xengine.css', 'xengine_theme_sync.js')
    pattern = re.compile(r'(?P<attr>\b(?:src|href))="(?P<path>' +
                         '|'.join(re.escape(p) for p in paths) +
                         r')(?:\?xengine-doc-build=[^\"]+)?"')
    count = 0
    for page in html_directory.glob('*.html'):
        original_html = page.read_text(encoding='utf-8')
        changed = pattern.sub(
            lambda match: f'{match.group("attr")}="{match.group("path")}?xengine-doc-build={stamp}"',
            original_html)
        if changed != original_html:
            page.write_text(changed, encoding='utf-8')
            count += 1
    return count


def build(doxygen: str) -> int:
    version = read_engine_version()
    environment = os.environ.copy()
    environment['XENGINE_VERSION'] = version
    print(f"[DOCS] XEngine {version} (root VERSION)")
    for profile, config in (("game", "Doxyfile_Games"), ("engine", "Doxyfile_Engine")):
        output = ROOT / "build" / "docs" / profile
        # Doxygen does not remove obsolete generated pages. Old outputs can
        # contain an identically titled guide from another source directory;
        # this makes redirects ambiguous and shows old pages in search.
        # Limit cleanup to these two exclusively generated reference folders.
        if output.is_dir():
            shutil.rmtree(output)
        output.mkdir(parents=True, exist_ok=True)
        print(f"[DOCS] Generating {profile} reference...")
        result = subprocess.run([doxygen, config], cwd=ROOT, check=False,
                                env=environment)
        if result.returncode:
            print(f"[WARN] {config} failed (exit {result.returncode}).", file=sys.stderr)
            return result.returncode
        number = group_markdown_navigation(output / "html", profile)
        print(f"[DOCS] Grouped {number} Markdown pages in {profile} navigation")
        themed, logos = style_document_pages(output / 'html')
        print(f"[DOCS] Branded {themed} profiling pages and resolved {logos} Markdown logos")
        references = redirect_empty_markdown_file_references(output / 'html')
        duplicates = prune_duplicate_markdown_search(output / 'html')
        # Do not publish an Engine reference with an unexpectedly flat or
        # empty left tree. Doxygen 1.18 can leave the newly generated
        # navtreedata.js ungrouped on repeated local builds; a final pass
        # ensures its Markdown entries are nested before version stamping.
        tree_source = (output / 'html' / 'navtreedata.js').read_text(encoding='utf-8')
        if number and '"Guides and manuals"' not in tree_source:
            recovered = group_markdown_navigation(output / 'html', profile)
            if not recovered or '"Guides and manuals"' not in (
                    output / 'html' / 'navtreedata.js').read_text(encoding='utf-8'):
                raise RuntimeError(f'{profile}: grouped guide navigation is missing')
            print(f'[DOCS] Recovered {recovered} {profile} navigation entries')
        versioned = version_navigation_assets(output / 'html', version)
        print(f"[DOCS] Linked {references} empty Markdown File Reference pages; "
              f"filtered {duplicates} duplicate search hits")
        print(f"[DOCS] Versioned assets on {versioned} pages to prevent stale navigation trees")

    root = ROOT / "build" / "docs"
    shutil.copyfile(ROOT / "docs" / "reference" / "theme" / "portal.html", root / "index.html")
    print("[DOCS] Open build/docs/index.html (Game Developer / Engine Developer)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--doxygen", help="path to an existing Doxygen executable")
    args = parser.parse_args()
    executable = args.doxygen or shutil.which("doxygen")
    if not executable:
        print("[WARN] Doxygen is not installed; documentation generation skipped. "
              "If you plan to develop games with XEngine or modify the engine, "
              "install Doxygen to keep the reference in sync with rebuilds.")
        return 0
    return build(executable)


if __name__ == "__main__":
    raise SystemExit(main())
