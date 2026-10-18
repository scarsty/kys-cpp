# 遊戲圖示

`app_icon.png` 是 1024×1024 共用來源，保留直排「金棋」、直劍與透視棋格。配色採用暖炭色背景、低飽和象牙色與灰褐色文字、灰色劍身及少量青色，避免金箔、亮邊與浮雕效果。

修改來源後，在專案根目錄執行：

```powershell
./tools/GenerateAppIcons.ps1
.venv/Scripts/python.exe tools/promo/build_page.py
```

生成器同步 Windows `src/kys.ico`、Android 啟動圖示、WASM favicon、Apple touch icon 與 PWA 圖示。平台圖示依各平台要求保留 PNG／ICO；宣傳頁內嵌 WebP。Android 前景在 432×432 畫布中央使用 264×264 內容區，對應[自適應圖示的 66/108 安全區域](https://developer.android.com/develop/ui/compose/system/icon_design_adaptive)；外圍透明區使用 `#121110` 背景。共用來源的劍柄與劍尖則延伸至畫面上下緣，避免主圖留下多餘留白。

## 圖像生成

2026-10-18 使用內建 image_gen 編輯既有標誌。先調低金色與金屬質感，再以以下最終提示詞放大主體，等比例縮放成共用來源：

```text
Edit only the framing and scale of this app icon. Keep the current understated matte ivory/grey-taupe 金棋 calligraphy, dark grey sword with its subtle cyan edge, and faint charcoal perspective chess grid. The current design has too much empty space. Zoom the emblem in substantially. The sword must extend almost all the way from the TOP edge to the BOTTOM edge: pommel at y=1% and sword point at y=99% of the square image. The full sword should occupy 98% of the image height, with no large empty margin above the pommel or below the point. The exact Chinese text 金棋 must remain complete and correctly written, 金 above 棋, enlarged along with the sword and spanning roughly 70 percent of the image height and 55 percent of its width. Let the faint grid run behind the emblem and bleed off every relevant edge. If necessary reposition the thin emphasized chess-square line behind the lower sword instead of leaving it below the point; it must not force empty padding at the bottom. Full-bleed square composition, NOT a small emblem centered inside a larger square. No generous safe-zone margins. No new decorative elements, no border, no bright gold, no metallic gilding, no glow, no new text, no rounded corner mask. Preserve the quiet palette and recognizable brushwork while correcting the framing.
```
