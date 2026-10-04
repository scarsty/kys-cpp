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

天賦適用難度、預設天賦、各難度裝備獎勵時程與絕招總數都由配置取得。天賦卡片以數值方塊、流程、成長倍率條與賭運機率階梯呈現，取可選難度中最難的一種，目前為困難。晚成的倍率示意以生命成長為例，未計其他加成與取整。

絕招使用者從 `full` 目錄的各星級武功威力判定：與遊戲相同，按（威力、武功 ID）選最高者；僅列真正以該招作絕招的角色。只在部分星級使用時顯示星級標籤，未標示即一至三星皆使用。完整目錄可按武功、絕招使用者或效果文字搜尋。

生成器測試使用獨立測試資料，不依賴正式配置數值：

```powershell
.venv/Scripts/python.exe -m unittest discover -s tests -p test_promo_page.py -v
```
