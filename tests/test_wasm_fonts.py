"""Verify text discovery, dictionary coverage and the actual subset font files."""
from contextlib import closing
import gzip
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from wasm.prepare_fonts import ASCII, collect_characters, conversion_rules, cpp_text, include_translations, prepare_fonts


def font_fixture(path):
    characters = ASCII | set('甲乙丙丁戊己庚辛壬癸龘囗')
    cmap = {ord(c): f'uni{ord(c):04X}' for c in sorted(characters)}
    names = ['.notdef', 'component', *cmap.values()]
    builder = FontBuilder(1000, isTTF=True)
    builder.setupGlyphOrder(names)
    builder.setupCharacterMap(cmap)
    glyphs = {}
    for name in names:
        pen = TTGlyphPen(None)
        pen.moveTo((100, 0))
        pen.lineTo((400, 0))
        pen.lineTo((250, 700))
        pen.closePath()
        glyphs[name] = pen.glyph()
    pen = TTGlyphPen(glyphs)
    pen.addComponent('component', (1, 0, 0, 1, 10, 20))
    glyphs[cmap[ord('甲')]] = pen.glyph()
    builder.setupGlyf(glyphs)
    builder.setupHorizontalMetrics({name: (600, 100) for name in names})
    builder.setupHorizontalHeader(ascent=800, descent=-200)
    builder.setupNameTable({'familyName': 'Fixture', 'styleName': 'Regular',
                            'copyright': 'Fixture copyright', 'licenseDescription': 'Fixture license'})
    builder.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200)
    builder.setupPost()
    builder.setupMaxp()
    builder.save(path)


def game_fixture(root):
    project = root / 'project'
    game = root / 'game'
    for directory in (project / 'src', project / 'config', game / 'config', game / 'cc',
                      game / 'save', game / 'font', game / 'list'):
        directory.mkdir(parents=True)
    (project / 'src/ui.cpp').write_text('draw("甲"); // 龘', encoding='utf-8')
    (project / 'config/chess_fixture.yaml').write_text('label: 乙\n# 龘', encoding='utf-8')
    (game / 'config/changelog.md').write_text('丙', encoding='utf-8')
    (game / 'list/leave.txt').write_text('己', encoding='utf-8')
    with closing(sqlite3.connect(game / 'save/game.db')) as database, database:
        database.execute('CREATE TABLE role (name TEXT, description TEXT)')
        database.execute("INSERT INTO role VALUES ('丁', '戊')")
        database.execute('CREATE TABLE chess_game_data_store (payload TEXT)')
        database.execute("INSERT INTO chess_game_data_store VALUES ('龘')")
    for direction in ('ST', 'TS'):
        for suffix in ('Phrases', 'Characters'):
            (game / 'cc' / f'{direction}{suffix}.txt').write_text('', encoding='utf-8')
    font_fixture(game / 'font/chinese.ttf')
    font_fixture(game / 'font/english.ttf')
    return project, game


class WasmFontTests(unittest.TestCase):
    def test_cpp_literals_handle_comments_raw_strings_and_unicode_and_utf8_escapes(self):
        source = r'''// "龘"
            /* "龘" */
            draw(u8"甲\u4E59\U00004E19\xE4\xB8\x81");
            auto text = R"text(戊 "己" // 庚)text";
            auto character = U'辛';
            auto escaped = "\"壬\"\\癸";
            auto continued = "一\
二";
        '''
        result = set(''.join(cpp_text(source)))
        self.assertTrue(set('甲乙丙丁戊己庚辛壬癸一二') <= result)
        self.assertNotIn('龘', result)

    def test_collection_covers_content_but_excludes_comments_and_player_saves(self):
        with tempfile.TemporaryDirectory() as directory:
            project, game = game_fixture(Path(directory))
            (game / 'config/staged.yaml').write_text('label: 庚', encoding='utf-8')
            result = collect_characters(project, game)
            self.assertTrue(ASCII | set('甲乙丙丁戊己庚') <= result)
            self.assertNotIn('龘', result)

    def test_dictionary_first_candidate_overrides_and_transitive_phrase_coverage(self):
        with tempfile.TemporaryDirectory() as directory:
            _, game = game_fixture(Path(directory))
            (game / 'cc/STPhrases.txt').write_text('甲乙\t丙 龘\n甲\t龘\n', encoding='utf-8', newline='\n')
            (game / 'cc/STCharacters.txt').write_text('甲\t丁\n甲\t龘', encoding='utf-8', newline='\n')
            (game / 'cc/TSCharacters.txt').write_text('丙\t戊\n戊\t己\n龘\t庚\n', encoding='utf-8', newline='\n')
            result = include_translations(set('甲乙'), conversion_rules(game))
            self.assertEqual(result, set('甲乙丙丁戊己'))

    def test_fonts_keep_required_glyphs_components_metrics_and_license_without_changing_native_fonts(self):
        with tempfile.TemporaryDirectory() as directory:
            project, game = game_fixture(Path(directory))
            originals = {name: (game / 'font' / name).read_bytes() for name in ('chinese.ttf', 'english.ttf')}
            prepare_fonts(project, game)
            with TTFont(game / 'font/wasm/chinese.ttf') as font, TTFont(game / 'font/chinese.ttf') as original:
                # 囗 is a FreeType hinting reference, retained without UI text.
                self.assertEqual(set(font.getBestCmap()), set(map(ord, ASCII | set('甲乙丙丁戊己囗'))))
                name = font.getBestCmap()[ord('甲')]
                self.assertTrue(font['glyf'][name].isComposite())
                self.assertEqual(font['hmtx'][name], original['hmtx'][original.getBestCmap()[ord('甲')]])
                self.assertEqual(font['glyf'][name].getCoordinates(font['glyf'])[0],
                                 original['glyf'][original.getBestCmap()[ord('甲')]].getCoordinates(original['glyf'])[0])
                self.assertEqual(font['name'].getDebugName(13), 'Fixture license')
            with TTFont(game / 'font/wasm/english.ttf') as font:
                self.assertEqual(set(font.getBestCmap()), set(map(ord, ASCII)))
            for name, content in originals.items():
                self.assertEqual((game / 'font' / name).read_bytes(), content)
                generated = game / 'font/wasm' / name
                self.assertEqual(gzip.decompress(generated.with_suffix('.ttf.gz').read_bytes()), generated.read_bytes())

    def test_repeated_generation_is_stable_and_content_changes_refresh_subsets(self):
        with tempfile.TemporaryDirectory() as directory:
            project, game = game_fixture(Path(directory))
            prepare_fonts(project, game)
            timestamps = {p: p.stat().st_mtime_ns for p in (game / 'font/wasm').iterdir()}
            prepare_fonts(project, game)
            self.assertEqual(timestamps, {p: p.stat().st_mtime_ns for p in timestamps})
            (project / 'src/ui.cpp').write_text('draw("甲庚");', encoding='utf-8')
            (project / 'config/chess_fixture.yaml').write_text('label: 乙辛', encoding='utf-8')
            with closing(sqlite3.connect(game / 'save/game.db')) as database, database:
                database.execute("UPDATE role SET name = '丁壬'")
            (game / 'cc/TSCharacters.txt').write_text('辛\t癸\n', encoding='utf-8', newline='\n')
            prepare_fonts(project, game)
            with TTFont(game / 'font/wasm/chinese.ttf') as font:
                self.assertTrue(set(map(ord, '庚辛壬癸')) <= set(font.getBestCmap()))
            self.assertEqual((game / 'font/wasm/english.ttf').stat().st_mtime_ns,
                             timestamps[game / 'font/wasm/english.ttf'])

    def test_manifest_selects_subsets_and_excludes_full_fonts_and_http_sidecars(self):
        with tempfile.TemporaryDirectory() as directory:
            project, game = game_fixture(Path(directory))
            prepare_fonts(project, game)
            manifest = Path(directory) / 'manifest.js'
            subprocess.run([sys.executable, str(ROOT / 'wasm/gen_manifest.py'), str(game), str(manifest)],
                           check=True, capture_output=True)
            files = json.loads(manifest.read_text(encoding='utf-8').split('files: ', 1)[1].split('\n', 1)[0])
            fonts = [path for path in files if path.startswith('/game/font/')]
            self.assertEqual(fonts, ['/game/font/wasm/chinese.ttf', '/game/font/wasm/english.ttf'])


if __name__ == '__main__':
    unittest.main()
