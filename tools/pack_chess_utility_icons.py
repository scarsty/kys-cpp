"""把操作剪影、封禁與導覽圖示併入整備圖示包。"""

import argparse
from io import BytesIO
from pathlib import Path
import zipfile

from PIL import Image, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCES = ROOT / "tmp/asset-sources/preparation-icons"
ATLAS = ROOT.parent / "asset-sources/chess-preparation/chess-utility-atlas.png"
ARCHIVE = ROOT / "work/game-dev/resource/chess-preparation.zip"
PREVIEW = ROOT / "output/preparation-icons/utility-preview.png"
NAMES = ("character", "formation", "load", "save", "search", "transfer")
SOURCE_ICONS = {39: "ban", 40: "catalog", 41: "expedition", 42: "duel"}
CHARACTER_NAMES = ("luck", "victory", "light-strike", "heavy-strike", "ranged-strike")
CHARACTER_SOURCES = ROOT.parent / "asset-sources/chess-character-icons"
HELP_SOURCE = ROOT.parent / "asset-sources/chess-help/chess-help.png"
BATTLE_SOURCES = ROOT / "tmp/asset-sources/battle-controls"
BATTLE_NAMES = ("play", "pause", "speed", "camera-follow", "camera-free",
                "view-2d", "view-3d", "flag", "list", "map")


def encode_icon(glyph: Image.Image) -> bytes:
    output = BytesIO()
    glyph.save(output, format="WEBP", lossless=True, method=6)
    return output.getvalue()


def mask_atlas_icons(atlas: Image.Image, names: tuple[str, ...], columns: int,
                     rows: int, first_id: int, size: int = 96) -> dict[str, bytes]:
    icons = {}
    for index, name in enumerate(names):
        column, row = index % columns, index // columns
        bounds = (round(column * atlas.width / columns) + 9,
                  round(row * atlas.height / rows) + 9,
                  round((column + 1) * atlas.width / columns) - 9,
                  round((row + 1) * atlas.height / rows) - 9)
        mask = ImageOps.grayscale(atlas.crop(bounds)).point(lambda value: 255 if value >= 128 else 0)
        bounds = mask.getbbox()
        assert bounds, name
        mask = mask.crop(bounds)
        mask.thumbnail((size * 5 // 6, size * 5 // 6), Image.Resampling.LANCZOS)
        glyph = Image.new("RGBA", (size, size), (255, 255, 255, 0))
        alpha = Image.new("L", glyph.size)
        alpha.paste(mask, ((size - mask.width) // 2, (size - mask.height) // 2))
        glyph.putalpha(alpha)
        icons[f"{first_id + index}.webp"] = encode_icon(glyph)
    return icons


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--utility-only", action="store_true", help="從原始三列圖集重打包人物、佈陣、讀檔、存檔、搜尋與調度圖示")
    selection.add_argument("--ban-only", action="store_true", help="只更新封禁圖示，保留現有操作圖示")
    selection.add_argument("--navigation-only", action="store_true", help="只更新圖簽、遠征與離線對戰圖示")
    selection.add_argument("--character-only", action="store_true", help="只更新賭運、勝場與武學類型圖示")
    selection.add_argument("--battle-only", action="store_true", help="只更新戰鬥控制圖示（29–38）")
    selection.add_argument("--help-only", action="store_true", help="只更新操作教學圖示")
    parser.add_argument("--character-sources", type=Path, default=CHARACTER_SOURCES,
                        help="角色圖示來源目錄，預設使用儲存庫外的 asset-sources/chess-character-icons")
    args = parser.parse_args()
    source_only = args.utility_only or args.ban_only or args.navigation_only or args.character_only or args.battle_only or args.help_only
    new_icons = {}
    if args.utility_only or not source_only:
        atlas = Image.open(ATLAS).convert("RGB")
        # 原稿為 3×3；採用前兩列的六個圖示，第三列是生成時的額外變體。
        new_icons.update(mask_atlas_icons(atlas, NAMES, 3, 3, 22, size=192))
    if args.battle_only or not source_only:
        battle_atlas = Image.open(BATTLE_SOURCES / "atlas.png").convert("RGB")
        new_icons.update(mask_atlas_icons(battle_atlas, BATTLE_NAMES, 5, 2, 29))

    # 與既有圖示共用 96 像素畫布；保留生成來源的透明度及留白。
    for icon_id, name in (() if args.utility_only or args.character_only or args.battle_only or args.help_only else SOURCE_ICONS.items()):
        if args.ban_only and name != "ban" or args.navigation_only and name == "ban":
            continue
        glyph = Image.open(SOURCES / f"chess-{name}.webp").convert("RGBA")
        glyph = ImageOps.contain(glyph, (96, 96), Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", (96, 96))
        canvas.alpha_composite(glyph, ((96 - glyph.width) // 2, (96 - glyph.height) // 2))
        new_icons[f"{icon_id}.webp"] = encode_icon(canvas)

    if args.character_only or not source_only:
        atlas = Image.open(args.character_sources / "atlas.png").convert("RGBA")
        for index, name in enumerate(CHARACTER_NAMES):
            glyph = Image.open(args.character_sources / "luck.png").convert("RGBA") if name == "luck" else atlas.crop(
                (round(index * atlas.width / 5), 0, round((index + 1) * atlas.width / 5), atlas.height))
            bounds = glyph.getchannel("A").point(lambda alpha: 255 if alpha >= 128 else 0).getbbox()
            assert bounds, name
            glyph = glyph.crop(bounds)
            glyph.thumbnail((80, 80), Image.Resampling.LANCZOS)
            canvas = Image.new("RGBA", (96, 96))
            canvas.alpha_composite(glyph, ((96 - glyph.width) // 2, (96 - glyph.height) // 2))
            new_icons[f"{44 + index}.webp"] = encode_icon(canvas)

    if args.help_only or not source_only:
        glyph = Image.open(HELP_SOURCE).convert("RGBA")
        alpha = glyph.getchannel("A")
        bounds = alpha.point(lambda value: 255 if value >= 16 else 0).getbbox()
        assert bounds, "help"
        alpha = alpha.crop(bounds)
        alpha.thumbnail((96, 96), Image.Resampling.LANCZOS)
        # 只保留生成圖示的透明度，色彩由共用 UI 樣式決定。
        canvas = Image.new("RGBA", (96, 96), (255, 255, 255, 0))
        mask = Image.new("L", canvas.size)
        mask.paste(alpha, ((96 - alpha.width) // 2, (96 - alpha.height) // 2))
        canvas.putalpha(mask)
        new_icons["49.webp"] = encode_icon(canvas)

    with zipfile.ZipFile(ARCHIVE) as source:
        existing = {entry.filename: source.read(entry) for entry in source.infolist()
                    if entry.filename not in new_icons}
    with zipfile.ZipFile(ARCHIVE, "w", compression=zipfile.ZIP_STORED) as destination:
        for name, content in (*existing.items(), *new_icons.items()):
            destination.writestr(name, content)
    packed = existing | new_icons
    preview_ids = [icon_id for icon_id in (*range(22, 28), *range(29, 39), *SOURCE_ICONS, *range(44, 50)) if f"{icon_id}.webp" in packed]
    preview = Image.new("RGBA", (len(preview_ids) * 112, 112), (25, 23, 22, 255))
    for index, icon_id in enumerate(preview_ids):
        glyph = Image.open(BytesIO(packed[f"{icon_id}.webp"])).convert("RGBA")
        glyph.thumbnail((96, 96), Image.Resampling.LANCZOS)
        tinted = Image.new("RGBA", glyph.size, (221, 200, 141, 255))
        tinted.putalpha(glyph.getchannel("A"))
        preview.alpha_composite(tinted, (index * 112 + 8, 8))
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    preview.save(PREVIEW)
    print(f"Packed {len(new_icons)} icons into {ARCHIVE}")


if __name__ == "__main__":
    main()
