"""把新增的六個操作剪影併入整備圖示包。"""

from io import BytesIO
from pathlib import Path
import zipfile

from PIL import Image, ImageOps


ROOT = Path(__file__).resolve().parents[1]
ATLAS = ROOT / "tmp/asset-sources/preparation-icons/utility-atlas.png"
ARCHIVE = ROOT / "work/game-dev/resource/chess-preparation.zip"
PREVIEW = ROOT / "output/preparation-icons/utility-preview.png"
NAMES = ("character", "formation", "load", "save", "search", "transfer")


def main() -> None:
    atlas = Image.open(ATLAS).convert("RGB")
    preview = Image.new("RGBA", (len(NAMES) * 112, 112), (25, 23, 22, 255))
    new_icons = {}
    for index, name in enumerate(NAMES):
        column, row = index % 3, index // 3
        left = round(column * atlas.width / 3) + 9
        top = round(row * atlas.height / 3) + 9
        right = round((column + 1) * atlas.width / 3) - 9
        bottom = round((row + 1) * atlas.height / 3) - 9
        mask = ImageOps.grayscale(atlas.crop((left, top, right, bottom)))
        mask = mask.point(lambda value: 255 if value >= 128 else 0)
        bounds = mask.getbbox()
        assert bounds, name
        mask = mask.crop(bounds)
        mask.thumbnail((80, 80), Image.Resampling.LANCZOS)
        glyph = Image.new("RGBA", (96, 96), (255, 255, 255, 0))
        alpha = Image.new("L", glyph.size)
        alpha.paste(mask, ((96 - mask.width) // 2, (96 - mask.height) // 2))
        glyph.putalpha(alpha)
        output = BytesIO()
        glyph.save(output, format="WEBP", lossless=True, method=6)
        new_icons[f"{22 + index}.webp"] = output.getvalue()
        tinted = Image.new("RGBA", glyph.size, (221, 200, 141, 255))
        tinted.putalpha(glyph.getchannel("A"))
        preview.alpha_composite(tinted, (index * 112 + 8, 8))

    with zipfile.ZipFile(ARCHIVE) as source:
        existing = {entry.filename: source.read(entry) for entry in source.infolist()
                    if entry.filename not in new_icons}
    with zipfile.ZipFile(ARCHIVE, "w", compression=zipfile.ZIP_STORED) as destination:
        for name, content in (*existing.items(), *new_icons.items()):
            destination.writestr(name, content)
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    preview.save(PREVIEW)
    print(f"Packed {len(new_icons)} icons into {ARCHIVE}")


if __name__ == "__main__":
    main()
