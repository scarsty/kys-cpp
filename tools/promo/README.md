# 宣傳頁生成

`page_template.html` 是繁體正文來源；`build_page.py` 生成根目錄的 `金群自走棋.html`，內嵌簡繁兩版、截圖、遊戲圖示與角色頭像，可直接開啟或隨遊戲分發。

在專案根目錄執行：

```powershell
.venv/Scripts/python.exe -m pip install -r tools/promo/requirements.txt
./.github/build-command.ps1
.venv/Scripts/python.exe tools/promo/build_page.py
```

預設讀取頂層 `config/chess_*.yaml`，角色資料與頭像取自 `work/game-dev`。絕招效果描述透過 `x64/Debug/kys_chess_cli.exe --jsonl` 的 `compact` 目錄取得，與遊戲共用精簡描述器；查詢明確指定同一份資料與配置，不讀取建構目錄中複製的配置。變更遊戲描述器後需先重新建構 CLI。

可用 `--game-dir`、`--config-dir`、`--cli`、`--output`、`--play-url` 指定其他來源、執行檔或輸出位置。所有配置欄位使用目前的繁體結構鍵；查詢失敗或模板留有未解析欄位時，生成器會停止，不覆寫既有頁面。

天賦適用難度、預設天賦、各難度裝備獎勵時程與絕招總數都由配置取得。天賦卡片以數值方塊、流程與賭運機率階梯呈現，取可選難度中最難的一種，目前為困難。晚成顯示四星培養條件與各費開場狀態護盾。

絕招使用者從 `full` 目錄的各星級武功威力判定：與遊戲相同，按（威力、武功 ID）選最高者；僅列真正以該招作絕招的角色。只在部分星級使用時顯示星級標籤，未標示即一至三星皆使用。完整目錄可按武功、絕招使用者或效果文字搜尋。

生成器測試使用獨立測試資料，不依賴正式配置數值：

```powershell
.venv/Scripts/python.exe -m unittest discover -s tests -p test_promo_page.py -v
```

## 視覺與實機截圖

頁面沿用 [UI 設計語言](../../docs/ui-design-language.md) 的暖炭色面板、象牙色正文、金色操作與青色資訊提示。主頁只展示一張整備商店截圖，以三個編號標出「招募 → 選人 → 開戰」；自動戰鬥截圖由讀者自行展開。玩法手冊與各章節預設收合，不在宣傳正文堆疊規則、表格或相似截圖。角色頭像卡片最大寬度為 120px，窄螢幕也不放大填滿整列，並保留完整頭像。

截圖均可點選放大，支援鍵盤開啟、Escape 關閉及返回原按鈕；簡繁兩版各有對應的預覽文字。頭版與頁尾皆提供主要遊戲入口及 `https://kyschess.pages.dev/kyschess` 備用入口。

「圖鑑」與「Windows 啟動問題」是與玩法手冊並列的獨立收合區塊；導覽列可直接開啟圖鑑。難度說明只列差異，不預先推薦選擇。手冊保留關鍵規則與配置數值，刪去重複建議與絕招範例；完整絕招仍可展開搜尋。

`shots/` 使用 1280×720 WebP 截圖，以品質 82、method 6 壓縮，保留密集表格的解析度。生成器直接內嵌檔案位元組，不再次有損壓縮。圖說與導覽說明寫在模板，更新畫面時一併核對。

瀏覽器擷取的暫存 JPEG 可放在 `work/`，再壓縮成正式資產，例如：

```powershell
.venv/Scripts/python.exe -c "from PIL import Image; Image.open('work/capture.jpg').convert('RGB').save('tools/promo/shots/preparation.webp', 'WEBP', quality=82, method=6)"
```

遊戲圖示由共用的 [`assets/app_icon.png`](../../assets/app_icon.png) 產生；平台圖示與生成提示詞見 [圖示資產說明](../../assets/README.md)。宣傳頁的圖示和角色頭像也使用 WebP。

本次截圖於 2026-10-18 以本機 `wasm/build.ps1` 建構，透過瀏覽器操作正式 `kyschess.html` 擷取。滿編畫面使用已存在的整備測試存檔，經遊戲「外部存檔 → 匯入存檔／對戰 JSON」載入；第 13 關戰報與日誌來自實際執行的戰鬥。測試陣容用於展示介面，不代表自然進度的養成範例。

重新擷取時先建構並啟動本機 WASM：

```powershell
./wasm/build.ps1
Set-Location wasm/build
python serve.py 8080
```

開啟 `http://localhost:8080/kyschess.html`，透過遊戲操作到目標畫面，待資源載入完成後擷取。保留完整畫布與控制項，避免裁切、拼接或改繪遊戲內容。主頁僅內嵌 `shop.webp` 與 `battle.webp`，其餘畫面保留作素材參考。更新所用畫面後，再回專案根目錄執行 `build_page.py`：

| 圖片 | 畫面 |
| --- | --- |
| `title.webp`、`difficulty.webp`、`talents.webp` | 主選單、難度選擇、天賦選擇 |
| `preparation.webp`、`shop.webp`、`lineup.webp`、`piece.webp` | 整備、商店、候補及固定檢視器 |
| `equipment.webp`、`neigong.webp`、`combos.webp` | 裝備效果、內功一覽、羈絆一覽 |
| `help.webp`、`challenge.webp` | 分頁說明、遠征挑戰 |
| `prebattle.webp`、`formation.webp` | 雙方總覽、地圖佈陣 |
| `battle.webp`、`victory.webp`、`log.webp` | 戰鬥、戰後報告、戰鬥日誌 |

WASM 首頁另以相對遊戲連結生成：

```powershell
.venv/Scripts/python.exe tools/promo/build_page.py --output wasm/build/index.html --play-url kyschess.html
```
