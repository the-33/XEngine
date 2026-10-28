"""Guard GitHub-specific light/dark logo markup against documentation regressions."""

from html.parser import HTMLParser
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
THEMES = ('light', 'dark')


class LogoParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.anchors = []
        self.images = []
        self.current_anchor = None

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'a':
            self.current_anchor = {'href': attrs.get('href'), 'images': []}
            self.anchors.append(self.current_anchor)
        elif tag == 'img':
            record = {'src': attrs.get('src'), 'width': attrs.get('width')}
            self.images.append(record)
            if self.current_anchor is not None:
                self.current_anchor['images'].append(record)

    def handle_endtag(self, tag):
        if tag == 'a':
            self.current_anchor = None


def parse(relative_path):
    data = (ROOT / relative_path).read_text(encoding='utf-8')
    assert '<picture' not in data.lower(), 'GitHub theme logos must not use <picture>'
    parser = LogoParser()
    parser.feed(data)
    return parser


class GithubThemeLogoTests(unittest.TestCase):
    def test_main_readme_logos_and_separate_benchmark_links(self):
        parser = parse('README.md')
        for theme in THEMES:
            fragment = f'#gh-{theme}-mode-only'
            main_src = f'docs/branding/xengine_logo_{theme}.svg?v=4{fragment}'
            self.assertEqual(
                [{'src': main_src, 'width': '176'}],
                [img for img in parser.images if img['src'] == main_src],
            )
            benchmark_src = f'docs/branding/xengine_benchmark_logo_{theme}.svg?v=4{fragment}'
            links = [a for a in parser.anchors
                     if a['href'] == f'docs/BENCHMARK_GAME.md{fragment}']
            self.assertEqual(1, len(links), f'Expected separate {theme} benchmark anchor')
            self.assertEqual(
                [{'src': benchmark_src, 'width': '136'}], links[0]['images'],
            )

        self.assertEqual(2, len([a for a in parser.anchors
                                 if a['href'] and a['href'].startswith('docs/BENCHMARK_GAME.md')]),
                         'Benchmark light/dark images must be in two distinct anchors')

    def test_benchmark_guide_logos(self):
        parser = parse('docs/BENCHMARK_GAME.md')
        for theme in THEMES:
            fragment = f'#gh-{theme}-mode-only'
            src = f'branding/xengine_benchmark_logo_{theme}.svg?v=4{fragment}'
            self.assertEqual(
                [{'src': src, 'width': '184'}],
                [img for img in parser.images if img['src'] == src],
            )


if __name__ == '__main__':
    unittest.main()
