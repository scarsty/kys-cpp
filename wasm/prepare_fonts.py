#!/usr/bin/env python3
"""Subset WASM fonts from game text and the game's own Chinese dictionaries."""
import argparse
from contextlib import closing
import gzip
import io
from pathlib import Path
import re
import sqlite3

from fontTools import subset
from fontTools.ttLib import TTFont
import yaml


SOURCE_SUFFIXES = {'.c', '.cc', '.cpp', '.h', '.hpp', '.inc'}
STATIC_TABLES = {'base', 'bag', 'role', 'magic', 'item', 'submap', 'shop'}
ASCII = set(map(chr, range(32, 127)))
# FreeType measures these reference glyphs even when drawing other characters.
# Keep them so subsetting does not change CJK auto-hinting at small UI sizes.
# Reference: FreeType 2.13.3 src/autofit/afblue.dat (CJK) and afscript.h (hani).
# https://gitlab.freedesktop.org/freetype/freetype/-/tree/VER-2-13-3/src/autofit
CJK_AUTOHINT = set(map(chr, (
    0x4E2A, 0x4E3A, 0x4E3B, 0x4E8B, 0x4E9B, 0x4EBA, 0x4ED6, 0x4EE5, 0x4EEC, 0x4F60, 0x4F86, 0x4F8B,
    0x500B, 0x5011, 0x519B, 0x5225, 0x522B, 0x5230, 0x5236, 0x524D, 0x52A8, 0x52D5, 0x5373, 0x540C,
    0x5417, 0x5427, 0x542C, 0x5462, 0x548C, 0x54C1, 0x54CD, 0x55CE, 0x56D7, 0x56E0, 0x5730, 0x589E,
    0x5927, 0x5979, 0x5B78, 0x5B83, 0x5BF9, 0x5C06, 0x5C07, 0x5C0D, 0x5C31, 0x5DF2, 0x5E08, 0x5E2B,
    0x5E2D, 0x5E74, 0x5F97, 0x60C5, 0x60F3, 0x610F, 0x613F, 0x6211, 0x6216, 0x6307, 0x6536, 0x653F,
    0x65AD, 0x65AF, 0x65B0, 0x65B7, 0x65E2, 0x65F6, 0x660E, 0x661F, 0x662F, 0x6642, 0x666F, 0x6700,
    0x6703, 0x6709, 0x671D, 0x671F, 0x6765, 0x6784, 0x6837, 0x6A23, 0x6C11, 0x6C92, 0x6CA1, 0x70BA,
    0x7136, 0x7167, 0x7269, 0x7279, 0x73B0, 0x73FE, 0x7403, 0x7406, 0x751F, 0x7528, 0x7530, 0x7576,
    0x770B, 0x773C, 0x7740, 0x786E, 0x79CD, 0x7B2C, 0x7D93, 0x7F6E, 0x8005, 0x80FD, 0x81EA, 0x8230,
    0x8457, 0x88E1, 0x8981, 0x8AAA, 0x8ABF, 0x8BF4, 0x8C01, 0x8C03, 0x8CBB, 0x8D39, 0x8D77, 0x8ECD,
    0x8FC7, 0x8FD8, 0x8FD9, 0x8FDB, 0x9019, 0x901A, 0x9032, 0x904E, 0x9053, 0x9084, 0x90A3, 0x90FD,
    0x914D, 0x91CC, 0x958B, 0x9593, 0x95F4, 0x9645, 0x9648, 0x9650, 0x9664, 0x9673, 0x968F, 0x969B,
    0x96A8, 0x96F7, 0x9732, 0x9762, 0x987E, 0x9F4A,
)))
# Match comments before literals, and raw strings before ordinary strings. This
# avoids packing Chinese comments or mistaking quotes inside raw strings for code.
CPP_TOKENS = re.compile(
    r'//[^\n]*|/\*[\s\S]*?\*/'
    r'|(?:u8|u|U|L)?R"(?P<delimiter>[^\s()\\]{0,16})\((?P<raw>[\s\S]*?)\)(?P=delimiter)"'
    r'|(?:u8|u|U|L)?"(?P<string>(?:\\[\s\S]|[^"\\])*)"'
    r"|(?:u8|u|U|L)?'(?P<char>\\(?:x[0-9a-fA-F]+|u[0-9a-fA-F]{4}|U[0-9a-fA-F]{8}|[0-7]{1,3}|.)|[^'\\\n])'"
)
CPP_ESCAPES = re.compile(r'\\(x[0-9a-fA-F]+|u[0-9a-fA-F]{4}|U[0-9a-fA-F]{8}|[0-7]{1,3}|[\s\S])')
SIMPLE_ESCAPES = dict(zip("abfnrtv\\\"'?", "\a\b\f\n\r\t\v\\\"'?"))
DICTIONARY_NEWLINE = re.compile(r'[\r\n]')


def decode_cpp_literal(text):
    """Decode UTF-8 byte escapes as well as C++ Unicode escapes."""
    result = bytearray()
    position = 0
    for match in CPP_ESCAPES.finditer(text):
        result.extend(text[position:match.start()].encode('utf-8'))
        escape = match[1]
        if escape[0] in 'uU':
            result.extend(chr(int(escape[1:], 16)).encode('utf-8'))
        elif escape[0] == 'x' or escape[0] in '01234567':
            value = int(escape[1:], 16) if escape[0] == 'x' else int(escape, 8)
            if value <= 255:
                result.append(value)
            else:
                result.extend(chr(value).encode('utf-8'))
        else:
            result.extend(SIMPLE_ESCAPES.get(escape, escape).encode('utf-8'))
        position = match.end()
    result.extend(text[position:].encode('utf-8'))
    # Binary byte strings are not display text; valid UTF-8 portions still count.
    return result.decode('utf-8', errors='ignore')


def cpp_text(source):
    source = re.sub(r'\\\r?\n', '', source)
    for token in CPP_TOKENS.finditer(source):
        if token['raw'] is not None:
            yield token['raw']
        elif token['string'] is not None:
            yield decode_cpp_literal(token['string'])
        elif token['char'] is not None:
            yield decode_cpp_literal(token['char'])


def yaml_text(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for key, child in value.items():
            yield from yaml_text(key)
            yield from yaml_text(child)
    elif isinstance(value, list):
        for child in value:
            yield from yaml_text(child)


def database_text(path):
    # Only static content is relevant. Legacy chess_* tables contain player saves.
    with closing(sqlite3.connect(path.resolve().as_uri() + '?mode=ro', uri=True)) as database:
        tables = {row[0] for row in database.execute("SELECT name FROM sqlite_master WHERE type='table'")}
        for table in sorted(tables & STATIC_TABLES):
            for row in database.execute(f'SELECT * FROM "{table}"'):
                yield from (value for value in row if isinstance(value, str))


def collect_characters(project_dir, game_dir):
    characters = ASCII.copy()  # Numbers, timestamps, keys and generated filenames.
    for directory in ('src', 'mlcc', 'others'):
        for path in sorted((project_dir / directory).rglob('*')):
            if path.suffix in SOURCE_SUFFIXES and 'unit_test' not in path.parts:
                for text in cpp_text(path.read_text(encoding='utf-8-sig')):
                    characters.update(text)

    # Canonical configuration plus staged/generated config and the changelog.
    for directory in (project_dir / 'config', game_dir / 'config'):
        for path in sorted(directory.rglob('*')):
            if path.suffix in {'.yaml', '.yml'}:
                for text in yaml_text(yaml.safe_load(path.read_text(encoding='utf-8-sig'))):
                    characters.update(text)
            elif path.suffix in {'.md', '.txt', '.ini'}:
                characters.update(path.read_text(encoding='utf-8-sig'))

    for text in database_text(game_dir / 'save/game.db'):
        characters.update(text)
    for path in [*sorted((game_dir / 'list').glob('*.txt')), game_dir / 'resource/talkutf8.txt']:
        if path.is_file():
            characters.update(path.read_text(encoding='utf-8-sig'))
    return characters


def conversion_rules(game_dir):
    for direction in ('ST', 'TS'):
        mappings = {}
        # SimpleCC::init loads phrases first, characters second, and uses only
        # the first space-separated candidate. Later duplicate keys overwrite.
        for suffix in ('Phrases', 'Characters'):
            path = game_dir / 'cc' / f'{direction}{suffix}.txt'
            text = path.read_bytes().decode('utf-8')
            position = 0
            while (tab := text.find('\t', position)) != -1:
                end = DICTIONARY_NEWLINE.search(text, tab + 1)
                if end is None:
                    break  # SimpleCC ignores an unterminated final entry.
                end = end.start()
                mappings[text[position:tab]] = text[tab + 1:end].split(' ', 1)[0]
                position = end + 1
        yield from mappings.items()


def include_translations(characters, rules):
    """Close the alphabet over both conversions, including dynamic phrases.

    If every character of a dictionary key can occur, its first translation can
    occur too. This conservatively covers longest matches across concatenated
    strings without packing the entire dictionaries' character sets.
    """
    characters = set(characters)
    pending = [(set(key), set(value)) for key, value in rules]
    while True:
        remaining = []
        previous_size = len(characters)
        for key, value in pending:
            if key <= characters:
                characters.update(value)
            else:
                remaining.append((key, value))
        if len(characters) == previous_size:
            return characters
        pending = remaining


def write_if_changed(path, content):
    if path.exists() and path.read_bytes() == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_bytes(content)
    temporary.replace(path)


def subset_font(source, output, characters):
    options = subset.Options()
    options.recalc_timestamp = False
    options.notdef_outline = True
    # Keep copyright and license records as well as the default font names.
    options.name_IDs += [13, 14]
    with TTFont(source, recalcTimestamp=False) as font:
        original_characters = set(font.getBestCmap())
        wanted = set(map(ord, characters)) & original_characters
        subsetter = subset.Subsetter(options=options)
        subsetter.populate(unicodes=wanted)
        subsetter.subset(font)
        assert wanted <= set(font.getBestCmap())
        stream = io.BytesIO()
        font.save(stream)
    content = stream.getvalue()
    write_if_changed(output, content)
    compressed = gzip.compress(content, compresslevel=9, mtime=0)
    write_if_changed(output.with_name(output.name + '.gz'), compressed)
    print(f'{source.name}: {len(wanted):,}/{len(original_characters):,} characters; '
          f'{source.stat().st_size:,} -> {len(content):,} bytes '
          f'({1 - len(content) / source.stat().st_size:.1%} smaller; gzip {len(compressed):,})')


def prepare_fonts(project_dir, game_dir):
    characters = include_translations(collect_characters(project_dir, game_dir), conversion_rules(game_dir))
    print(f'WASM text alphabet: {len(characters):,} characters including both Chinese translations')
    for name in ('chinese.ttf', 'english.ttf'):
        # ImGui uses chinese.ttf for ASCII too; SDL's English font only draws ASCII.
        wanted = characters | CJK_AUTOHINT if name == 'chinese.ttf' else ASCII
        subset_font(game_dir / 'font' / name, game_dir / 'font/wasm' / name, wanted)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project-dir', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--game-dir', type=Path, required=True)
    args = parser.parse_args()
    prepare_fonts(args.project_dir, args.game_dir)


if __name__ == '__main__':
    main()
