# 操作教學圖示

生成的透明圖示來源保存在專案外的 `../asset-sources/chess-help/chess-help.png`，不納入 Git。使用 `python tools/pack_chess_utility_icons.py --help-only` 將其透明度緊貼畫布、縮放至 96×96 並寫入 `work/game-dev/resource/chess-preparation.zip` 的 `49.webp`。遊戲以共用樣式著色，按鈕位置與尺寸不變。

初稿由 gen-image 技能的 `gemini-3.1-flash-image` 生成，再使用內建 image_gen 移除初稿的棋盤背景。

初稿提示詞：

> Create one production game UI help icon: a bold clearly readable question mark centered inside a simple circular ring. Flat ivory-white monochrome silhouette, subtly hand-cut wuxia strategy game aesthetic matching a minimalist gold-tinted icon set. Thick clean strokes readable at 24 pixels. Front-facing, no perspective, no shadow, no glow, no gradients, no decoration, no words or letters other than the question mark. True transparent background including inside the ring. The circular silhouette should fill 96 percent of the square canvas, only 2 percent margin each side. Single icon only, no grid, no mockup, no checkerboard.

透明化提示詞：

> Remove the entire checkerboard background, including the interior of the circle. Preserve only the ivory circular ring and ivory question-mark glyph as a clean flat monochrome UI icon. Make all background truly transparent alpha. Preserve the exact bold glyph and ring shape. Crop/frame tightly so the circular icon fills 98 percent of the square canvas. No shadows, no glow, no texture, no checkerboard pattern. This is a production game UI help icon to be tinted gold at 24 pixels.
