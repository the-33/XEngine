"""Validate current-guide selection, Markdown navigation and audience boundaries."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('xengine_doxygen_builder', ROOT / 'tools/build_doxygen.py')
docs = importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(docs)


class DualDoxygenTests(unittest.TestCase):
    def test_current_only_and_no_windows_root_readme_error(self):
        game = (ROOT / 'Doxyfile_Games').read_text(encoding='utf-8')
        engine = (ROOT / 'Doxyfile_Engine').read_text(encoding='utf-8')
        self.assertIn('docs/reference/game', game)
        self.assertIn('engine/src', engine)
        for old in ('ADAPTIVE_BROADPHASE.md', 'RENDER_THRESHOLD_CALIBRATION.md',
                    'RENDER_CULLING_RELEASE_FORENSICS_20261022.md', 'LEGACY_MIGRATION_PLAN.md'):
            self.assertNotIn('docs/' + old, engine)
        self.assertNotIn('docs README.md', game)
        self.assertNotIn('](../README.md)',
                         (ROOT / 'docs/LEGACY_MIGRATION_PLAN.md').read_text(encoding='utf-8'))

    def test_public_game_manuals_and_platform_build_flags(self):
        config = (ROOT / 'Doxyfile_Games').read_text(encoding='utf-8')
        for source in ('docs/DEVELOPER_GUIDE.md', 'docs/GETTING_STARTED.md',
                       'docs/API_REFERENCE.md', 'docs/INCLUDES_AND_DEPENDENCIES.md',
                       'docs/WINDOW_AND_CAMERA.md', 'docs/PERSISTENCE_AND_SERIALIZATION.md'):
            self.assertIn(source, config)
        self.assertNotIn('docs/ARCHITECTURE.md', config)
        self.assertNotIn('engine/include/BuildConfig.h \\', config)
        guide = (ROOT / 'docs/reference/game/PLATFORMS_AND_BUILD_MODES.md').read_text(encoding='utf-8')
        self.assertIn('{#xengine_game_build_macros}', guide)
        self.assertIn('XENGINE_PLATFORM_WINDOWS', guide)
        self.assertIn('XENGINE_PLATFORM_LINUX', guide)
        self.assertIn('XENGINE_DEBUG', guide)
        self.assertIn('XENGINE_RELEASE', guide)
        self.assertIn('XEngineBuild::IsDebug', guide)
        self.assertIn('#if XENGINE_ENABLE_PROFILER', guide)
        for header in ('engine/include/Platform.h', 'engine/include/BuildConfig.h'):
            comments = (ROOT / header).read_text(encoding='utf-8')
            self.assertIn('@file', comments)
            self.assertIn('xengine_game_build_macros', comments)

    def test_duplicate_markdown_file_search_redirects_to_real_guides(self):
        with tempfile.TemporaryDirectory() as tmp:
            html_dir = Path(tmp)
            search_dir = html_dir / 'search'
            search_dir.mkdir()
            target = 'md_docs_2ARCHITECTURE.html'
            (html_dir / target).write_text(
                '<html><head><title>XEngine: Architecture and Platform Boundaries</title></head>'
                '<body><h1>Architecture and Platform Boundaries</h1></body></html>',
                encoding='utf-8')
            ref = 'ARCHITECTURE_8md.html'
            (html_dir / ref).write_text(
                '<html><head><title>XEngine: ARCHITECTURE.md File Reference</title></head>'
                '<body><div class="contents"></div></body></html>', encoding='utf-8')
            js = ('var searchData=\n[\n'
                  "  ['architecture_2emd_2',['ARCHITECTURE.md',['../ARCHITECTURE_8md.html',1]]],\n"
                  "  ['architecture_20and_20platform_20boundaries_1',['Architecture and Platform Boundaries',['../md_docs_2ARCHITECTURE.html',1]]]\n"
                  '];\n')
            for index in ('all_0.js', 'files_0.js'):
                (search_dir / index).write_text(js, encoding='utf-8')
            self.assertEqual(docs.redirect_empty_markdown_file_references(html_dir), 1)
            result = (html_dir / ref).read_text(encoding='utf-8')
            self.assertIn('content="0;url=md_docs_2ARCHITECTURE.html"', result)
            self.assertIn('data-xengine-guide-redirect="1"', result)
            self.assertIn('the full guide', result)
            self.assertEqual(docs.prune_duplicate_markdown_search(html_dir), 2)
            for index in ('all_0.js', 'files_0.js'):
                content = (search_dir / index).read_text(encoding='utf-8')
                self.assertNotIn(ref, content)
                self.assertIn(target, content)

    def test_markdown_title_with_inline_code_still_redirects(self):
        with tempfile.TemporaryDirectory() as tmp:
            html_dir = Path(tmp)
            (html_dir / 'md_game_sync.html').write_text(
                '<html><head><title>XEngine: Synchronizing game '
                '&lt;tt&gt;config.json&lt;/tt&gt; manifests</title></head></html>',
                encoding='utf-8')
            (html_dir / 'GAME__CONFIG__SYNC_8md.html').write_text(
                '<html><head><title>XEngine: GAME_CONFIG_SYNC.md File Reference</title>'
                '</head><body><div class="contents"></div></body></html>',
                encoding='utf-8')
            self.assertEqual(docs.redirect_empty_markdown_file_references(html_dir), 1)

    def test_unmatched_empty_markdown_file_does_not_pollute_search(self):
        with tempfile.TemporaryDirectory() as tmp:
            html_dir = Path(tmp)
            search_dir = html_dir / 'search'
            search_dir.mkdir()
            (html_dir / 'UNMATCHED_8md.html').write_text(
                '<html><head><title>XEngine: UNMATCHED.md File Reference</title></head>'
                '<body><div class="contents"></div></body></html>', encoding='utf-8')
            (search_dir / 'all_1.js').write_text(
                "var searchData=\n[\n"
                "  ['unmatched_1',['UNMATCHED.md',['../UNMATCHED_8md.html',1]]],\n"
                "  ['real_2',['Full guide',['../md_docs_2REAL.html',1]]]  \n];\n",
                encoding='utf-8')
            self.assertEqual(docs.redirect_empty_markdown_file_references(html_dir), 0)
            self.assertEqual(docs.prune_duplicate_markdown_search(html_dir), 1)
            content = (search_dir / 'all_1.js').read_text(encoding='utf-8')
            self.assertNotIn('UNMATCHED_8md.html', content)
            self.assertIn('md_docs_2REAL.html', content)

    def test_theme_assets_and_two_readable_profiles(self):
        for name in ('xengine_logo_light.png', 'xengine_logo_dark.png',
                     'xengine_benchmark_logo_light.png', 'xengine_benchmark_logo_dark.png',
                     'xengine.css', 'portal.html'):
            self.assertTrue((ROOT / 'docs/reference/theme' / name).is_file())
        conf = (ROOT / 'Doxyfile_Games').read_text(encoding='utf-8')
        self.assertIn('HTML_COLORSTYLE        = TOGGLE', conf)
        self.assertIn('xengine.css', conf)
        self.assertIn('tools/build_doxygen.py', (ROOT / 'tools/build.py').read_text(encoding='utf-8'))

    def test_benchmark_markdown_logos_and_profiling_theme(self):
        self.assertTrue(docs.is_profiling_page('md_docs_2BENCHMARK__GAME.html'))
        self.assertTrue(docs.is_profiling_page('md_docs_2MULTITHREADING__AND__PROFILING.html'))
        self.assertTrue(docs.is_profiling_page('xengine_manual_advanced.html'))
        self.assertTrue(docs.is_profiling_page('structRenderDiagnostics.html'))
        self.assertFalse(docs.is_profiling_page('classScene.html'))
        self.assertFalse(docs.is_profiling_page('md_docs_2WINDOW__AND__CAMERA.html'))

        fragment = '''<html lang="en-US"><head></head><body>
<td id="projectlogo"><img alt="Logo" src="xengine_logo_light.png"/></td>
<img src="branding/xengine_benchmark_logo_light.svg?v=4#gh-light-mode-only" class="inline"/>
<img src="branding/xengine_benchmark_logo_dark.svg?v=4#gh-dark-mode-only" class="inline"/>
</body></html>'''
        with tempfile.TemporaryDirectory() as tmp:
            html_dir = Path(tmp)
            for name in ('xengine_logo_light.png', 'xengine_logo_dark.png',
                         'xengine_benchmark_logo_light.png', 'xengine_benchmark_logo_dark.png'):
                (html_dir / name).write_bytes(b'logo fixture')
            page = html_dir / 'md_docs_2BENCHMARK__GAME.html'
            page.write_text(fragment, encoding='utf-8')
            other = html_dir / 'classScene.html'
            other.write_text(fragment.split('<img src="branding/')[0] + '</body></html>', encoding='utf-8')
            themed, logos = docs.style_document_pages(html_dir)
            self.assertEqual((themed, logos), (1, 2))
            output = page.read_text(encoding='utf-8')
            self.assertIn('class="xengine-profiling"', output)
            self.assertIn('src="xengine_benchmark_logo_light.png"', output)
            self.assertIn('src="xengine_benchmark_logo_dark.png"', output)
            self.assertIn('xengine-inline-logo-light', output)
            self.assertIn('xengine-inline-logo-dark', output)
            self.assertNotIn('gh-light-mode-only', output)
            self.assertNotIn('gh-dark-mode-only', output)
            self.assertNotIn('xengine-profiling', other.read_text(encoding='utf-8'))
            self.assertIn('src="xengine_logo_dark.png"', other.read_text(encoding='utf-8'))

        css = (ROOT / 'docs/reference/theme/xengine.css').read_text(encoding='utf-8')
        self.assertIn('html.xengine-profiling.dark-mode', css)
        self.assertIn('html.dark-mode img.xengine-inline-logo-light', css)
        self.assertIn('html:not(.dark-mode) img.xengine-inline-logo-dark', css)

    def test_profiling_markdown_marker_is_independent_of_doxygen_filename(self):
        for source in ('docs/BENCHMARK_GAME.md', 'docs/MULTITHREADING_AND_PROFILING.md',
                       'docs/RENDER_DIAGNOSTICS.md', 'docs/RENDER_BACKENDS.md',
                       'docs/reference/engine/ENGINE_MANUALS_ADVANCED.md'):
            markdown = (ROOT / source).read_text(encoding='utf-8')
            self.assertIn('<span data-xengine-doc-theme="profiling"></span>', markdown, source)

        # Simulate a version of Doxygen that chooses a different page URL.
        # Source metadata, not the output filename, must decide the brand.
        with tempfile.TemporaryDirectory() as tmp:
            html_dir = Path(tmp)
            for logo in ('xengine_logo_light.png', 'xengine_logo_dark.png',
                         'xengine_benchmark_logo_light.png', 'xengine_benchmark_logo_dark.png'):
                (html_dir / logo).write_bytes(b'PNG')
            renamed = html_dir / 'renamed_page_1759.html'
            renamed.write_text('<html><body><td id="projectlogo"><img src="old.png"/></td>'
                               '<span data-xengine-doc-theme="profiling"></span>'
                               '</body></html>', encoding='utf-8')
            ordinary = html_dir / 'classGameObject.html'
            ordinary.write_text('<html><body><td id="projectlogo"><img src="old.png"/></td>'
                                '</body></html>', encoding='utf-8')
            self.assertEqual(docs.style_document_pages(html_dir), (1, 0))
            self.assertIn('class="xengine-profiling"', renamed.read_text(encoding='utf-8'))
            self.assertIn('xengine_benchmark_logo_dark.png', renamed.read_text(encoding='utf-8'))
            self.assertNotIn('xengine-profiling', ordinary.read_text(encoding='utf-8'))

    def test_all_four_theme_variants_include_correct_glow_and_accent(self):
        css = (ROOT / 'docs/reference/theme/xengine.css').read_text(encoding='utf-8')
        variants = css.split('html.xengine-profiling.dark-mode {', 1)
        self.assertEqual(len(variants), 2)
        standard_light, standard_dark = css.split('html.dark-mode {', 1)
        standard_dark = standard_dark.split('html.xengine-profiling {', 1)[0]
        profiling_light = css.split('html.xengine-profiling {', 1)[1].split(
            'html.xengine-profiling.dark-mode {', 1)[0]
        profiling_dark = variants[1].split('}', 1)[0]
        for segment in (standard_light, standard_dark, profiling_light, profiling_dark):
            self.assertIn('--glow-color:', segment)
            for token in ('--x-nav-selected-bg:', '--x-nav-selected-text:',
                          '--x-nav-selected-marker:', '--x-nav-normal-text:',
                          '--x-nav-hover-bg:', '--x-nav-hover-text:'):
                self.assertIn(token, segment)
        for segment in (profiling_light, profiling_dark):
            for name in ('--footer-foreground-color:', '--search-filter-border-color:',
                         '--search-filter-background-color:',
                         '--search-results-border-color:', '--search-box-border-color:',
                         '--search-close-icon-bg-color:',
                         '--nav-text-active-color:', '--tooltip-link-color:',
                         '--code-external-link-color:', '--nav-border-color:',
                         '--nav-breadcrumb-active-bg:', '--nav-splitbar-bg-color:',
                         '--nav-splitbar-handle-color:', '--sync-icon-color:',
                         '--sync-icon-selected-color:'):
                self.assertIn(name, segment)
        self.assertIn('html #nav-tree .selected', css)
        self.assertIn('html.xengine-profiling #nav-sync img', css)
        self.assertIn('html.xengine-profiling .ui-resizable-e', css)
        self.assertIn('html.xengine-profiling #page-nav-resize-handle', css)
        self.assertIn('html #page-nav .page-outline li.vis > .item', css)
        self.assertIn('html #page-nav .page-outline li.vis > .item a', css)
        self.assertIn('html #nav-tree .selected a', css)
        self.assertIn('html #nav-tree .selected a .label', css)
        self.assertIn('font-weight: 700 !important;', css)
        self.assertIn('html #nav-tree .item:not(.selected):hover a', css)
        self.assertIn('html #page-nav .page-outline li:not(.vis):not(.selected) > .item:hover a', css)
        # The native Doxygen SVG stays in place; all four combinations show
        # the same 60px outline with distinct high-contrast palette filters.
        self.assertIn('html img.footer {\n  width: 60px;', css)
        self.assertIn('html.dark-mode img.footer {', css)
        self.assertIn('html.xengine-profiling img.footer {', css)
        self.assertIn('html.xengine-profiling.dark-mode img.footer {', css)
        self.assertIn('filter: brightness(0) saturate(100%)', css)
        self.assertNotIn('html.xengine-profiling .navpath img.footer', css)
        self.assertNotIn('html.xengine-profiling #nav-sync::after', css)
        self.assertIn('html.xengine-profiling .navpath li.navelem::after', css)
        self.assertNotIn('padding-right: 22px;', css)
        self.assertNotIn('transform: rotate(45deg);\n  border-top: 1px solid var(--x-accent);', css)
        theme = (ROOT / 'docs/reference/theme/xengine_theme_sync.js').read_text(encoding='utf-8')
        portal = (ROOT / 'docs/reference/theme/portal.html').read_text(encoding='utf-8')
        self.assertIn("xengine-doc-theme", theme)
        self.assertIn("xengine-doc-theme", portal)
        self.assertIn('DarkModeToggle.userPreference', theme)
        self.assertIn('MutationObserver', theme)
        self.assertIn('URLSearchParams', theme)
        self.assertIn("target.searchParams.set('xengine-doc-theme'", theme)
        self.assertIn("href.startsWith('#')", theme)
        self.assertIn('target.pathname === currentPage.pathname', theme)
        for profile in ('game', 'engine'):
            page = ROOT / 'build/docs' / profile / 'html' / 'index.html'
            if page.is_file() and 'xengine_theme_sync.js' in page.read_text(encoding='utf-8'):
                self.assertIn('xengine_theme_sync.js', page.read_text(encoding='utf-8'))

    def test_rebuilt_navigation_uses_the_same_cache_fingerprint(self):
        with tempfile.TemporaryDirectory() as temporary:
            html_dir = Path(temporary)
            for name, contents in {
                'navtreedata.js': 'var NAVTREE = [];',
                'navtree.js': "script.src = scriptName+'.js';",
                'navtreeindex0.js': 'var NAVTREEINDEX0 = {};',
                'navtree.css': 'body {}',
                'doxygen.css': 'body {}',
                'xengine.css': 'body {}',
                'xengine_theme_sync.js': '/* user preference */',
                'index.html': ('<script src="navtreedata.js"></script>'
                               '<script src="navtree.js"></script>'
                               '<link href="navtree.css" rel="stylesheet" />'
                               '<link href="xengine.css" rel="stylesheet" />'
                               '<script src="xengine_theme_sync.js"></script>'
                               '<a href="#details">Details</a>'),
            }.items():
                (html_dir / name).write_text(contents, encoding='utf-8')
            self.assertEqual(docs.version_navigation_assets(html_dir, '0.1.4'), 1)
            page = (html_dir / 'index.html').read_text(encoding='utf-8')
            nav = (html_dir / 'navtree.js').read_text(encoding='utf-8')
            self.assertIn('navtreedata.js?xengine-doc-build=0.1.4-', page)
            self.assertIn('navtree.js?xengine-doc-build=0.1.4-', page)
            self.assertIn('xengine_theme_sync.js?xengine-doc-build=0.1.4-', page)
            self.assertIn("script.src = scriptName+'.js?xengine-doc-build=0.1.4-", nav)
            self.assertIn('href="#details"', page)

    def test_documentation_maintenance_guide_and_python_build_pipeline(self):
        guide = (ROOT / 'docs/reference/engine/DOCUMENTATION_WORKFLOW.md').read_text(encoding='utf-8')
        self.assertIn('data-xengine-doc-theme="profiling"', guide)
        self.assertIn('python tools/build_doxygen.py', guide)
        self.assertIn('PropertyRO', guide)
        build = (ROOT / 'tools/build.py').read_text(encoding='utf-8')
        self.assertIn("str(ROOT / 'tools' / 'build_doxygen.py')", build)
        self.assertIn('regenerate_engine_api_docs()', build)
        self.assertIn("[CACHE] XEngine library unchanged", build)

    def test_generated_markdown_search_has_no_blank_file_reference_hits(self):
        for profile in ('game', 'engine'):
            html_dir = ROOT / 'build' / 'docs' / profile / 'html'
            if not html_dir.is_dir():
                self.skipTest('Generate Doxygen first')
            redirected = {page.name for page in html_dir.glob('*_8md.html')
                          if 'data-xengine-guide-redirect="1"' in page.read_text(encoding='utf-8')}
            self.assertTrue(redirected, profile)
            navigation = (html_dir / 'navtreedata.js').read_text(encoding='utf-8')
            self.assertIn('Guides and manuals', navigation)
            if profile == 'game':
                self.assertIn('Start, includes and full developer guide', navigation)
                self.assertIn('DEVELOPER__GUIDE.html', navigation)
            for index in (html_dir / 'search').glob('*.js'):
                if index.name.startswith(('all_', 'files_')):
                    content = index.read_text(encoding='utf-8')
                    self.assertNotIn('_8md.html', content,
                                     f'{profile}: {index.name} still indexes an empty Markdown file')
                    self.assertFalse(any(ref in content for ref in redirected),
                                     f'{profile}: {index.name} contains a duplicate Markdown hit')

    def test_grouping_preserves_navigation_sync_indices(self):
        with tempfile.TemporaryDirectory() as tmp:
            html = Path(tmp)
            original = [["XEngine", "index.html", [
                ["Main", "index.html", "index"],
                ["Scenes", "md_docs_2reference_2game_2SCENES__AND__BEHAVIOURS.html", None],
                ["API", "classes.html", None],
                ["Persistence", "md_docs_2reference_2game_2DATA__AND__PERSISTENCE.html", None]]]]
            (html / 'navtreedata.js').write_text('var NAVTREE =\n' + json.dumps(original) + ';\n\n'
                                                 + 'var NAVTREEINDEX = [];\n', encoding='utf-8')
            (html / 'navtreeindex0.js').write_text('var NAVTREEINDEX0 = {\n'
                                                   '"classes.html":[2],\n'
                                                   '"md_docs_2reference_2game_2SCENES__AND__BEHAVIOURS.html":[1],\n'
                                                   '"md_docs_2reference_2game_2DATA__AND__PERSISTENCE.html":[3]\n};',
                                                   encoding='utf-8')
            self.assertEqual(docs.group_markdown_navigation(html, 'game'), 2)
            source = (html / 'navtreedata.js').read_text(encoding='utf-8')
            tree = json.loads(source.split('var NAVTREE =\n', 1)[1].split(';\n', 1)[0])
            self.assertEqual(tree[0][2][1][0], 'Guides and manuals')
            self.assertEqual(tree[0][2][2][1], 'classes.html')
            sync = (html / 'navtreeindex0.js').read_text(encoding='utf-8')
            self.assertIn('"classes.html":[2]', sync)
            self.assertIn('"md_docs_2reference_2game_2SCENES__AND__BEHAVIOURS.html":[1,0,0]', sync)
            self.assertIn('"md_docs_2reference_2game_2DATA__AND__PERSISTENCE.html":[1,1,0]', sync)

    def test_game_manuals_remain_under_single_group_with_new_doxygen_urls(self):
        # The original 1.18 report had flat individual guides under XEngine;
        # Doxygen's spelling of underscores varies between its versions.
        with tempfile.TemporaryDirectory() as tmp:
            html = Path(tmp)
            original = [["XEngine", "index.html", [
                ["Game reference", "index.html", None],
                ["Developer Guide", "md_docs_2DEVELOPER_GUIDE.html", None],
                ["Getting started", "md_docs_2GETTING_STARTED.html", None],
                ["Game scripts", "md_docs_2reference_2game_2SCENES_AND_BEHAVIOURS.html", None],
                ["Versioning", "md_docs_2VERSIONING.html", None],
                ["Uncategorised guide", "md_docs_2SOMETHING_NEW.html", None],
                ["Topics", "modules.html", None],
            ]]]
            (html / 'navtreedata.js').write_text(
                'var NAVTREE =\n' + json.dumps(original) + ';\n', encoding='utf-8')
            (html / 'navtreeindex0.js').write_text(
                'var NAVTREEINDEX0 = {"modules.html":[6],'
                '"md_docs_2VERSIONING.html":[4]};', encoding='utf-8')
            self.assertEqual(docs.group_markdown_navigation(html, 'game'), 5)
            tree = json.loads((html / 'navtreedata.js').read_text(encoding='utf-8').split(
                'var NAVTREE =\n', 1)[1].split(';\n', 1)[0])[0][2]
            self.assertEqual([entry[0] for entry in tree],
                             ['Game reference', 'Guides and manuals', 'Topics'])
            topics = tree[1][2]
            self.assertIn('Other current guides', [entry[0] for entry in topics])
            grouped_urls = [page[1] for category in topics for page in category[2]]
            self.assertEqual(len(grouped_urls), 5)
            self.assertIn('md_docs_2VERSIONING.html', grouped_urls)
            self.assertIn('"modules.html":[2]', (html / 'navtreeindex0.js').read_text())

    def test_file_members_are_not_class_property_inventories(self):
        # Doxygen's File Members/Variables is intentionally sparse: class
        # fields are documented under Classes and may not be globals at all.
        for profile in ('game', 'engine'):
            html = ROOT / 'build/docs' / profile / 'html'
            if not (html / 'globals.html').is_file():
                self.skipTest('Generate Doxygen to inspect File Members')
            self.assertIn('File Members', (html / 'globals.html').read_text(encoding='utf-8'))
            self.assertTrue((html / 'classGameObject.html').is_file())
            self.assertTrue(any(html.glob('globals_*.html')))

    def test_generated_xml_hides_aliases_but_engine_keeps_them(self):
        game = ROOT / 'build/docs/game/xml/classWindowManager.xml'
        engine = ROOT / 'build/docs/engine/xml/classWindowManager.xml'
        if not game.is_file() or not engine.is_file():
            self.skipTest('Run python tools/build_doxygen.py to validate generated XML')
        game_symbols = {name.text for name in ET.parse(game).iter('name')}
        engine_symbols = {name.text for name in ET.parse(engine).iter('name')}
        self.assertIn('size', game_symbols)
        self.assertNotIn('SizeProp', game_symbols)
        self.assertIn('SizeProp', engine_symbols)


if __name__ == '__main__':
    unittest.main()
