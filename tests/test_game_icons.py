"""Windows .ico embedding: field validation, windres args, incremental rebuild."""
from pathlib import Path
import importlib.util
import json
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('xengine_icons_build', ROOT/'tools/build.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def tiny_ico(path: Path) -> None:
    # ICONDIR (6), ICONDIRENTRY (16) plus one byte of payload.
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(b'\x00\x00\x01\x00\x01\x00' +
                     b'\x10\x10\x00\x00\x01\x00\x20\x00\x01\x00\x00\x00\x16\x00\x00\x00' + b'X')


class GameIconTests(unittest.TestCase):
    def test_optional_icon(self):
        with tempfile.TemporaryDirectory() as td:
            self.assertIsNone(mod.project_icon(Path(td), {'project':{'icon':None}}))
            self.assertIsNone(mod.project_icon(Path(td), {'project':{}}))

    def test_relative_icon_in_project_with_spaces_and_apostrophe(self):
        with tempfile.TemporaryDirectory() as td:
            game = Path(td)/"Princess's Run"
            ico = game/'assets'/'icons'/'princess.ico'
            tiny_ico(ico)
            self.assertEqual(mod.project_icon(game, {'project': {'icon':'assets/icons/princess.ico'}}),ico.resolve())
            self.assertEqual(mod.project_icon(game, {'project': {'icon':r'assets\icons\princess.ico'}}),ico.resolve())

    def test_reject_absolute_traversal_wrong_file_and_non_ico(self):
        with tempfile.TemporaryDirectory() as td:
            game = Path(td)/'Demo'
            tiny_ico(game/'assets/icons/game.ico')
            for val in ('../outside.ico', 'C:/system.ico', '/tmp/wrong.ico', 'assets/image.png', 12):
                with self.subTest(val=val),self.assertRaises(mod.BuildError):
                    mod.project_icon(game, {'project':{'icon':val}})
            with self.assertRaisesRegex(mod.BuildError, 'not found'):
                mod.project_icon(game, {'project':{'icon':'assets/icons/missing.ico'}})
            bad=game/'assets/icons/bad.ico'
            bad.write_bytes(b'not ICO')
            with self.assertRaisesRegex(mod.BuildError, 'Invalid Windows ICO'):
                mod.project_icon(game, {'project':{'icon':'assets/icons/bad.ico'}})

    def test_windres_resource_and_incremental_rebuild(self):
        with tempfile.TemporaryDirectory() as td:
            game = Path(td)/"My Princess's Run"
            ico = game/'assets/icons/game.ico'
            tiny_ico(ico)
            config = {'project':{'icon':'assets/icons/game.ico'}}
            out = Path(td)/'build'
            calls=[]
            def run(args, **kwargs):
                calls.append(args)
                Path(args[-1]).write_bytes(b'COFF')
                return ''
            with mock.patch.object(mod, 'exe',return_value='/fake/windres'), \
                 mock.patch.object(mod, 'command',side_effect=run):
                obj,changed=mod.compile_windows_icon(game, config, out, 'windows')
                self.assertTrue(changed)
                self.assertTrue(obj.exists())
                self.assertEqual(len(calls),1)
                self.assertIn('--target=pe-x86-64',calls[0])
                self.assertIn(f'1 ICON "{ico.as_posix()}"', (out/'generated/GameIcon.generated.rc').read_text())
                obj2,changed2=mod.compile_windows_icon(game,config,out,'windows')
                self.assertEqual(obj,obj2)
                self.assertFalse(changed2)
                self.assertEqual(len(calls),1)
                with ico.open('ab') as fh: fh.write(b'NEW')
                _,changed3=mod.compile_windows_icon(game,config,out,'windows')
                self.assertTrue(changed3)
                self.assertEqual(len(calls),2)
                _,linux_change=mod.compile_windows_icon(game,config,out,'linux')
                self.assertFalse(linux_change)

    def test_link_signature_updates_when_icon_added_or_removed(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            (root/'main.o').write_bytes(b'main')
            (root/'libXEngine.a').write_bytes(b'engine')
            (root/'icon.o').write_bytes(b'ico')
            a=mod.link_signature([root/'main.o'],root/'libXEngine.a',None,'g++',['-lSDL2'])
            b=mod.link_signature([root/'main.o'],root/'libXEngine.a',root/'icon.o','g++',['-lSDL2'])
            self.assertNotEqual(a,b)
            c=mod.link_signature([root/'main.o'],root/'libXEngine.a',None,'g++',['-lSDL2'])
            self.assertEqual(a,c)

    def test_release_does_not_ship_embedded_icon_separately(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            game=root/'My Princess Run'
            tiny_ico(game/'assets/icons/princess.ico')
            (game/'assets/config').mkdir(parents=True)
            (game/'assets/config/engine.json').write_text('{}')
            (game/'assets/textures').mkdir()
            (game/'assets/textures/ground.png').write_bytes(b'png')
            (game/'config.json').write_text(json.dumps({
                'project':{'name':'My Princess Run','icon':'assets/icons/princess.ico'}
            }))
            binary=root/'My Princess Run.exe'
            binary.write_bytes(b'EXE')
            with mock.patch.object(mod,'package_windows_libs',return_value=[]):
                stage=mod.package_game(game,'My Princess Run','0.1.0','windows','x86_64',binary)
            self.assertTrue((stage/'My Princess Run.exe').is_file())
            self.assertTrue((stage/'textures/ground.png').is_file())
            self.assertFalse((stage/'icons/princess.ico').exists())
            self.assertFalse((stage/'config/engine.json').exists())

    def test_template_and_my_princess_run_have_optional_icon(self):
        for path in (ROOT/'tools/templates/BlankGame/config.json',ROOT/'games/My Princess Run/config.json'):
            self.assertIn('icon', json.loads(path.read_text())['project'])

if __name__=='__main__': unittest.main()
