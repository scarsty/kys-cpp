"""Preview prepared EFT frames; never substitute master images for runtime frames."""

from pathlib import Path

from PIL import Image, ImageDraw

from prepare_chess_battle_effect_assets import EXPECTED_GROUPS, ROOT, RUNTIME_ROOT


def main() -> None:
    output = ROOT / "work" / "chess-effect-preview"
    output.mkdir(parents=True, exist_ok=True)
    groups = {
        "area-fire": "Fire aura - 20-frame loop",
        "cue-fire": "Fire pulse - 12 frames",
        "cue-guardian": "Guardian interception - 12 frames",
        "cue-sword": "Sword intent - 12 frames",
    }
    rows = []
    for name, title in groups.items():
        count, role_cue = EXPECTED_GROUPS[name]
        frames = []
        for index in range(count):
            image = Image.open(RUNTIME_ROOT / name / f"{index}.webp").convert("RGBA")
            image.thumbnail((240, 240), Image.Resampling.LANCZOS)
            canvas = Image.new("RGBA", (280, 280), (27, 32, 40, 255))
            # A neutral character marker makes the clear center easy to judge.
            draw = ImageDraw.Draw(canvas)
            draw.ellipse((130, 126, 150, 150), fill=(85, 95, 110))
            if not role_cue:
                alpha = 110 + 70 * max(0, 6 - index % 20) // 6
                image.putalpha(image.getchannel("A").point(lambda value: value * alpha // 255))
            canvas.alpha_composite(image, ((280 - image.width) // 2, (280 - image.height) // 2))
            frames.append(canvas.convert("RGB"))
        playback = frames + ([Image.new("RGB", (280, 280), (27, 32, 40))] * 8 if role_cue else [])
        playback[0].save(output / f"{name}.gif", save_all=True, append_images=playback[1:],
                         duration=65, loop=0)
        sheet = Image.new("RGB", (280 * 5, 308), (27, 32, 40))
        draw = ImageDraw.Draw(sheet)
        draw.text((12, 8), title, fill="white")
        for slot in range(5):
            frame = round(slot * (count - 1) / 4)
            sheet.paste(frames[frame], (slot * 280, 28))
        sheet.save(output / f"{name}-frames.png")
        rows.append(f'<article><h2>{title}</h2><img src="{name}.gif"><p>Runtime WebP frames, slowed preview.</p></article>')
    (output / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>Battle effect animation preview</title>'
        '<style>body{background:#151a21;color:#e8edf5;font:16px system-ui;margin:32px}'
        'main{display:flex;flex-wrap:wrap;gap:24px}article{background:#1b2028;padding:20px;border-radius:12px}'
        'h2{font-size:18px}p{color:#a5b4c8;font-size:13px}</style>'
        '<h1>Prepared battle effect animations</h1><main>' + ''.join(rows) + '</main>',
        encoding="utf-8")
    print(output / "index.html")


if __name__ == "__main__":
    main()
