"""Tests for optional PNG -> Windows ICO converter and BlankGame defaults."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

try:
    from PIL import Image
except ImportError:
    Image = None

PROJECT_ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('xengine_png_to_ico_tool', PROJECT_ROOT / 'tools' / 'png_to_ico.py')
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


@unittest.skipUnless(Image is not None, 'Pillow is optional; install Pillow to run icon conversion tests')
class IconConversionTests(unittest.TestCase):
    def setUp(self):
        self.td = tempfile.TemporaryDirectory()
        self.addCleanup(self.td.cleanup)
        self.root = Path(self.td.name)
        self.game = self.root / 'games' / "Blob's Quest"
        self.icons = self.game / 'assets' / 'icons'
        self.icons.mkdir(parents=True)
        self.cfg = self.game / 'config.json'
        self.cfg.write_text(json.dumps({
            'project': {'name': "Blob's Quest", 'version': '0.1.0', 'icon': None},
            'build': {'configuration': 'Debug'},
        }), encoding='utf-8')
        self.src = self.icons / 'engine.png'
        Image.new('RGBA', (512, 512), (150, 40, 215, 110)).save(self.src)
        self.root_patch = mock.patch.object(tool, 'ROOT', self.root)
        self.root_patch.start()
        self.addCleanup(self.root_patch.stop)

    def test_convert_multiresolution_and_preserve_alpha(self):
        self.assertEqual(tool.main(["Blob's Quest", 'engine']), 0)
        result = self.icons/'engine.ico'
        self.assertTrue(result.is_file())
        with Image.open(result) as image:
            self.assertEqual(set(image.ico.sizes()), {(s, s) for s in tool.ICO_SIZES})
            # 110/255 alpha should remain, rather than being flattened to opaque.
            self.assertEqual(image.convert('RGBA').getpixel((128, 128))[3], 110)
        self.assertEqual(json.loads(self.cfg.read_text())['project']['icon'], None)
        with Image.open(self.src) as source:
            self.assertEqual(source.size, (512,512))

    def test_set_config_for_both_platforms_without_losing_fields(self):
        self.assertEqual(tool.main(["Blob's Quest", 'engine', '--set-config']), 0)
        data = json.loads(self.cfg.read_text())
        self.assertEqual(data['project']['name'], "Blob's Quest")
        self.assertEqual(data['build']['configuration'], 'Debug')
        self.assertEqual(data['project']['icon'], {
            'windows': 'assets/icons/engine.ico',
            'linux': 'assets/icons/engine.png',
        })

    def test_prevent_overwrite_unless_force(self):
        result = self.icons/'engine.ico'
        result.write_bytes(b'old')
        self.assertEqual(tool.main(["Blob's Quest", 'engine']), 1)
        self.assertEqual(result.read_bytes(), b'old')
        self.assertEqual(tool.main(["Blob's Quest", 'engine', '--force']), 0)
        self.assertNotEqual(result.read_bytes(), b'old')

    def test_custom_png_with_spaces_and_output_in_icons(self):
        tex = self.game/'assets'/'textures'/'My Logo.png'
        tex.parent.mkdir(parents=True)
        Image.new('RGBA', (128, 72), (80, 100, 200, 255)).save(tex)
        self.assertEqual(tool.main(["Blob's Quest", 'logo custom',
                                    '--png', 'textures/My Logo.png', '--set-config']), 0)
        self.assertTrue((self.icons/'logo custom.ico').is_file())
        data = json.loads(self.cfg.read_text())
        self.assertEqual(data['project']['icon']['linux'], 'assets/textures/My Logo.png')

    def test_reject_path_traversal_and_bad_icon_names(self):
        for name in ('../escape', 'CON', 'foo/bar', 'name.', 'hello\\other'):
            with self.subTest(name=name):
                self.assertEqual(tool.main(["Blob's Quest", name]), 1)
        self.assertEqual(tool.main(["Blob's Quest", 'engine', '--png', '../secret.png']), 1)
        self.assertEqual(tool.main(["Blob's Quest", 'missing']), 1)
        self.assertEqual(tool.main(['Non Existing Game', 'engine']), 1)

    def test_reject_file_disguised_as_png(self):
        self.src.write_bytes(b'not a PNG image')
        self.assertEqual(tool.main(["Blob's Quest", 'engine']), 1)
        self.assertFalse((self.icons/'engine.ico').exists())

    def test_no_arguments_refreshes_template_icon_in_place(self):
        template_icons = self.root/'tools/templates/BlankGame/assets/icons'
        template_icons.mkdir(parents=True)
        source = template_icons/'xengine_icon.png'
        original = Image.new('RGBA', (512, 512), (80, 40, 130, 100))
        original.save(source)
        destination = template_icons/'xengine_icon.ico'
        destination.write_bytes(b'OLD')
        self.assertEqual(tool.main([]), 0)
        self.assertEqual(source.read_bytes()[:8], b'\x89PNG\r\n\x1a\n')
        with Image.open(destination) as image:
            self.assertEqual(set(image.ico.sizes()), {(s, s) for s in tool.ICO_SIZES})
            self.assertEqual(image.convert('RGBA').getpixel((128, 128))[3], 100)
        self.assertEqual(tool.main([]), 0)  # Repeated no-argument runs must work.

    def test_png_alone_accepts_arbitrary_path_and_protects_existing_ico(self):
        outside = self.root/'Artwork outside games'/'my icon.png'
        outside.parent.mkdir()
        Image.new('RGBA', (240, 120), (20, 110, 180, 200)).save(outside)
        target = outside.with_suffix('.ico')
        self.assertEqual(tool.main(['--png', str(outside)]), 0)
        with Image.open(target) as image:
            self.assertEqual(set(image.ico.sizes()), {(s, s) for s in tool.ICO_SIZES})
        first = target.read_bytes()
        self.assertEqual(tool.main(['--png', str(outside)]), 1)
        self.assertEqual(target.read_bytes(), first)
        self.assertEqual(tool.main(['--png', str(outside), '--force']), 0)
        self.assertEqual(tool.main(['--png', str(outside.parent/'missing.png')]), 1)

    def test_reject_incomplete_positional_args_and_standalone_set_config(self):
        self.assertEqual(tool.main(["Blob's Quest"]), 1)
        self.assertEqual(tool.main(['--set-config']), 1)
        self.assertEqual(tool.main(['--png', str(self.src), '--set-config']), 1)


class TemplateIconTests(unittest.TestCase):
    def test_blankgame_default_icons_exist_and_are_valid(self):
        template = PROJECT_ROOT/'tools/templates/BlankGame'
        data = json.loads((template/'config.json').read_text())
        self.assertEqual(data['project']['icon'], {
            'windows': 'default',
            'linux': 'default',
        })
        # Brand assets are supplied by the project maintainer and are not
        # replaced by synthetic icons in patch archives.
        if not (template/'assets/icons/xengine_icon.png').is_file() or not (template/'assets/icons/xengine_icon.ico').is_file():
            self.skipTest('Maintainer must provide xengine_icon.png / .ico assets')
        if Image:
            with Image.open(template/'assets/icons/xengine_icon.png') as im:
                self.assertEqual(im.size, (512, 512))
                self.assertEqual(im.mode, 'RGBA')
                self.assertEqual(im.getpixel((0,0))[3], 0)
            with Image.open(template/'assets/icons/xengine_icon.ico') as im:
                self.assertGreaterEqual(len(im.ico.sizes()), 4)


if __name__ == '__main__':
    unittest.main()
