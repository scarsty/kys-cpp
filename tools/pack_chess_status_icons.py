"""將戰鬥狀態圖集打包為可著色的透明圖示。"""

from io import BytesIO
from pathlib import Path
import zipfile

from PIL import Image, ImageOps


ROOT = Path(__file__).resolve().parents[1]
ATLAS = ROOT / "tmp/asset-sources/status-icons/atlas.png"
ARCHIVE = ROOT / "work/game-dev/resource/chess-status.zip"
PREVIEW = ROOT / "output/status-icons/preview.png"
# 順序與 BattleEffectIcon 相同；不要依名稱排序。
NAMES = (
    "poison", "bleed", "stun", "mp-blocked", "cold-poison", "withered-bone",
    "seven-star-mark", "neutralize-force", "blinded", "next-attack-miss", "damage-block", "single-hit-cap",
    "battle-spirit", "true-qi", "poison-explosion", "shadowless", "sword-guard", "critical",
    "berserk", "shield", "health", "attack", "defence", "speed",
)


def main() -> None:
    atlas = Image.open(ATLAS).convert("RGB")
    preview = Image.new("RGBA", (6 * 112, 4 * 112), (25, 23, 22, 255))
    with zipfile.ZipFile(ARCHIVE, "w", compression=zipfile.ZIP_STORED) as archive:
        for index, name in enumerate(NAMES):
            column, row = index % 6, index // 6
            bounds = (
                round(column * atlas.width / 6) + 4,
                round(row * atlas.height / 4) + 4,
                round((column + 1) * atlas.width / 6) - 4,
                round((row + 1) * atlas.height / 4) - 4,
            )
            mask = ImageOps.grayscale(atlas.crop(bounds))
            mask = mask.point(lambda value: 255 if value >= 128 else 0)
            bounds = mask.getbbox()
            assert bounds, name
            mask = mask.crop(bounds)
            mask.thumbnail((80, 80), Image.Resampling.LANCZOS)
            alpha = Image.new("L", (96, 96))
            alpha.paste(mask, ((96 - mask.width) // 2, (96 - mask.height) // 2))
            glyph = Image.new("RGBA", alpha.size, (255, 255, 255, 0))
            glyph.putalpha(alpha)
            output = BytesIO()
            glyph.save(output, format="WEBP", lossless=True, method=6)
            archive.writestr(f"{index}.webp", output.getvalue())
            tinted = Image.new("RGBA", glyph.size, (221, 200, 141, 255))
            tinted.putalpha(alpha)
            preview.alpha_composite(tinted, (column * 112 + 8, row * 112 + 8))
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    preview.save(PREVIEW)
    print(f"Packed {len(NAMES)} icons into {ARCHIVE}")


if __name__ == "__main__":
    main()
