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

`resource/chess-talents.zip` 只包含 `ChessTalentId` 使用的 `0.webp` 至 `4.webp`（神兵、晚成、賭徒、中堅、合縱）。畫面顯示為 240 px，正式圖片最大為 512 px，使用 WebP quality 90。原始圖片與 atlas 位於 `tmp/asset-sources/chess-talents/`，重複的具名副本不保留。

## 整備圖示

`resource/chess-preparation.zip` 使用與其他貼圖相同的載入流程，包含 `0.webp` 至 `27.webp`，順序對應 `ChessUiStyle::Icon`。每張圖為 96×96 透明 WebP，採用無損壓縮；白色遮罩在執行時套用暖象牙、青銅或狀態色。圖示由 `TextureManager` 共用及管理，不嵌入 C++，也不加入版本控制。新增的角色、陣形、讀取、儲存、搜尋與匯出圖示由 `tools/pack_chess_utility_icons.py` 自原稿圖集打包。

目前使用經過小尺寸檢查的生成圖示。原稿、生成提示及編號對照保存在 `tmp/asset-sources/preparation-icons/`；預覽圖放在 `output/preparation-icons/`。先前下載的外部圖示與相關工具已移除。這些來源不隨遊戲發佈，正式套件沿用既有資源複製與 WebAssembly 清單流程。

戰鬥控制亦使用此套件的 `29.webp` 至 `38.webp`，依序為繼續、暫停、倍速、跟隨鏡頭、自由鏡頭、平面視角、立體視角、投降、日誌及地圖。生成原稿與提示放在已忽略的 `tmp/asset-sources/battle-controls/`；以 `python tools/pack_chess_utility_icons.py --battle-only` 將五欄兩列的遮罩圖集轉為 96×96 透明無損 WebP，保留套件中的其他圖片。執行時共用圖示著色與控制狀態，不再以引擎線條繪製這些符號。

## 羈絆圖示

`resource/chess-synergies.zip` 包含 `0.webp` 至 `42.webp`，使用與整備圖示一致的生成剪影、96×96 透明無損 WebP 與執行時著色。每個 `config/chess_combos.yaml` 羈絆必須以 `圖示` 指定非負整數圖片編號，對應 ZIP 內的數字檔名；編號不隨羈絆排序改變，也不影響遊戲規則指紋。新增羈絆時，需同時指定圖示並將對應 WebP 放入套件。

生成原稿、提示、符號對照與打包腳本保存在 `tmp/asset-sources/synergy-icons/`；小尺寸預覽與驗證畫面放在 `output/synergy-icons/`。來源與正式圖片套件均不加入版本控制。整備的啟動羈絆列、羈絆清單與詳情共用此套件。

密宗沿用 `13.webp`，以交叉金剛杵剪影取代原蒙古馬頭。新原稿、提示與單張替換腳本保存在專案外的 `../asset-sources/chess-synergies/`；執行 `python ../asset-sources/chess-synergies/pack-mizong.py work/game-dev/resource/chess-synergies.zip` 可按上述規格重新打包，保留其他圖示。

## 棋局戰鬥特效

### 棋子狀態圖示

`resource/chess-status.zip` 包含 24 張 96×96 透明無損 WebP，固定編號對應 `BattlePieceInspector.h` 的 `BattleEffectIcon`。19 種狀態各有專用圖示；護盾、生命、攻擊、防禦與輕功圖示亦供屬性修正使用。檢視器以增益／減益顏色著色，名稱、數值、層數、來源和剩餘幀數分欄呈現。

生成原稿位於 `tmp/asset-sources/status-icons/atlas.png`（六欄四列），以 `python tools/pack_chess_status_icons.py` 打包；預覽輸出至 `output/status-icons/preview.png`。原稿與正式套件沿用既有資源管理規則，不加入版本控制；發佈時必須包含 `resource/chess-status.zip`。

特效與原有法術共用 `resource/eft/`，每組是一個 ZIP，使用既有貼圖載入流程：

格擋使用 `eft211.zip` 的半透明護盾，閃避使用 `eft212.zip` 的風痕。兩組各有 `0.webp` 至 `17.webp` 與 `index.txt`，每張圖片播放一個戰鬥幀，18 幀後結束。格擋錨點為 `(72, 80)`，閃避為 `(90, 90)`；透明度與動畫均已製作在圖片中。原稿留在專案外的 `../asset-sources/`，正式 ZIP 放在 `work/game-dev/resource/eft/`，沿用資源複製與 WebAssembly 清單流程。

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
