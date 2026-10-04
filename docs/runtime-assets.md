# 本機執行目錄與資源

`work/` 是遊戲執行目錄。根目錄保留執行所需的 DLL，`work/game-dev/` 保留遊戲資料、存檔、設定與正式資源。Debug 以 `work/game-dev` 為工作目錄並傳入 `.`；Release 以 `work` 為工作目錄並傳入 `game-dev`。執行檔使用目前建構的版本。

開發產物請依用途存放：

- `output/`：建構與測試紀錄、效能分析、重播證據、預覽圖、資源稽核與待檢查的資源包。
- `tmp/asset-sources/`：可重新製作資源的原稿、完整貼圖來源及外部資源包，不隨遊戲發佈。
- `tmp/tools/`：本機的 UPEdit、Very Sleepy 等輔助工具。
- `tmp/work-scratch/`：臨時重現程式及其他開發草稿。

## 貼圖套件

`TextureManager` 以群組路徑尋找同名 `.zip`，從 ZIP 根目錄讀取數字檔名的 WebP 圖片及 `index.txt` / `index.ka` 偏移。已打包的正式資源不保留另一份解壓目錄。來源圖片及生成提示留在 `tmp/asset-sources/`。

`resource/mmap.zip` 是大地圖套件；`resource/smap.zip` 是棋局使用的較小套件。完整 SMAP 來源留在 `tmp/asset-sources/smap/`，其中包含正式套件以外的貼圖。需要製作新的棋局套件時：

```powershell
python tools/pack_chess_smap_zip.py
```

預設讀取完整來源並輸出 `output/assets/smap.chess-battle.zip`。`--source-dir` 可指定另一份來源；確認內容後，以 `--output work/game-dev/resource/smap.zip` 更新正式套件。

## 天賦圖片

`resource/chess-talents.zip` 只包含 `ChessTalentId` 使用的 `0.webp` 至 `3.webp`。畫面顯示為 240 px，正式圖片最大為 512 px，使用 WebP quality 90。原始圖片與 atlas 位於 `tmp/asset-sources/chess-talents/`，重複的具名副本不保留。

## 棋局戰鬥特效

特效與原有法術共用 `resource/eft/`，每組是一個 ZIP，使用既有貼圖載入流程：

| 套件 | 用途 |
| --- | --- |
| `cue-positive` | 正面狀態與保護提示 |
| `cue-cleanse` | 淨化提示 |
| `cue-sword` | 劍意提示 |
| `cue-guardian` | 護衛攔截提示 |
| `cue-fire` | 火焰區域脈衝提示 |
| `area-sand` | 黃沙萬里鞭範圍 |
| `area-ward` | 伏魔杖法、羅漢伏魔功保護範圍 |
| `area-fire` | 火焰刀法範圍 |

中毒、流血、控制與詛咒的角色提示已在 `BattleFrameContext::queueSemanticCue` 被抑制，因此不保留或預載 `cue-negative`、`cue-bleed`、`cue-control`。

這八組素材直接併入既有 EFT 資源：ZIP 內保留 WebP 幀與需要的 `index.txt` 偏移，程式以 `eft/群組名稱` 載入。正式 ZIP 是遊戲使用的資源，不再維護生成或預覽腳本，也不保留另一份解壓目錄。

原稿與生成提示保存在 `tmp/asset-sources/chess-effects/`，供日後人工編修；執行與打包遊戲時只需 `resource/eft/` 中的正式 ZIP。
