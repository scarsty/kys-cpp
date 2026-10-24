# 自走棋賽事與使用率報告

這組工具產生可離線開啟的繁體中文互動 HTML。賽事戰績與使用歷程可以獨立產生，也可以整合為同一份報告的分頁。只需要 Node.js，沒有 npm 套件或外部網頁資源。

## 執行賽事

```powershell
& .\x64\Release\kys_chess_cli.exe tournament 'D:\saves' `
    --seed 0x20261011 --battle-seeds 3 --jobs 8 `
    --data-root 'D:\game' --config-root 'D:\game\config' `
    --output report.md --json-output results.json
```

`--jobs N` 指定 Asio 執行緒池大小，同時用於存檔重播驗證與賽事戰鬥。桌面 CLI 預設使用邏輯處理器數量的一半，最低為 1；`--jobs 1` 用於序列執行。每份存檔與每場戰鬥各是一個工作，執行時不持鎖；戰鬥完成後寫入固定結果位置並在短暫鎖內回報進度。進度依實際完成順序顯示，參賽順序仍為檔名排序，最終結果仍按配對、種子、場次排序；工作數不影響戰鬥種子、驗證碼或排名。程式庫預設仍為 1，WebAssembly 使用單執行緒。

## 產生 HTML

```powershell
node .\tools\tournament\generate-tournament-report.mjs `
    results.json report.html 'D:\saves' `
    'D:\projects\kys-cpp\kys-cpp\x64\Release\kys_chess_cli.exe' `
    'D:\game' 'D:\game\config'
```

參數順序：結果 JSON、輸出 HTML、原始存檔目錄、CLI 路徑、遊戲資料目錄、配置目錄。CLI 路徑預設為 `kys_chess_cli.exe`。若結果 JSON 含有 `run_metadata.data_root` / `config_root`，可以省略末兩個參數；配置目錄預設為資料目錄下的 `config`。

支援原生 `full_round_robin_mirrored` 結果。程式從原始存檔的 `replay.header.talent` 讀取天賦，並核對存檔 SHA256 與賽事紀錄，避免把不同存檔的天賦附到結果上。0.2.18 顯示名稱為 **神兵、中堅、晚成、賭徒**；新版本的 `alliance` 顯示為 **合縱**。未知天賦會回報錯誤。

HTML 包含完整排名、名稱搜尋與天賦篩選。選擇陣容後，可查看它與全部對手的積分、勝和敗與勝率；點選對手可查看該配對的全部戰鬥、超時標記、戰鬥 SHA256，以及 PowerShell 重現指令。種子數量從 JSON 讀取；支援手機版面與系統深色主題。若需要顯示引擎版本，可在 JSON 加入 `run_metadata.game_version`，不會把存檔版本猜作引擎版本。

對手列表與單組戰鬥詳情在寬螢幕並排顯示。從下拉選單或列表選擇對手，會同步選取狀態並將該列捲到列表可見範圍，保留列表內容與鍵盤焦點。窄螢幕點選列表中的對手會移至詳情，可用「返回對手列表」回到所選列。賽事種子與結果檔路徑收在頁尾的「賽事資料」，首頁以排名與陣容戰績為主。

重現時須使用相同引擎、遊戲資料與配置。產生報告不會修改結果 JSON 或原始存檔。

## 重播並統計使用歷程

使用支援 `--trace-output` 的原生 CLI，指定符合存檔版本與內容指紋的資料。分析器會逐份執行 `verify-pvp`，保留原始 SHA-256、驗證日誌與壓縮決策快照；只統計完整驗證成功且原始內容未變的存檔。

```powershell
node .\tools\tournament\analyze-save-usage.mjs 'D:\saves' `
    --cli 'D:\projects\kys-cpp\kys-cpp\x64\Release\kys_chess_cli.exe' `
    --data-root 'D:\game' --config-root 'D:\game\config' --jobs 4 `
    --output 'D:\reports\run-01\usage.json'

node .\tools\tournament\generate-usage-report.mjs `
    'D:\reports\run-01\usage.json' 'D:\reports\run-01\usage.html'
```

輸入是目錄中的 `.json` 檔案；輸出須放在原始存檔目錄之外。每次重播須使用新的輸出目錄；既有 `usage-traces/` 會使程式停止，避免混入舊快照。`--jobs` 控制同時執行的 CLI 行程數，預設為可用邏輯處理器的一半；`--source-commit` 可記錄所用引擎的提交。CLI 執行檔雜湊自動記錄，引擎版本及內容指紋直接取自快照目錄。歷史存檔應選相符版本的引擎及資料；舊 CLI 若沒有此選項，可將唯讀觀察介面與快照輸出移植到相符提交，保留所有原生驗證。不可跳過證據、尾端、最終狀態、RNG、快照雜湊或離線對戰資格檢查。

輸出包含 `usage.json`、UTF-8 BOM 的 `pieces.csv` / `synergies.csv` / `runs.csv`、`usage-traces.json` 與 `usage-traces/` 中每份存檔的 `.jsonl.gz` / `.log`。驗證失敗的檔名、錯誤及日誌保留於清單，統計摘要顯示未通過數量；沒有成功樣本時則停止，不產生使用率。快照清單與快照是分析證據，應與結果一起保存。

已完成原生驗證後，可以重用快照重算統計，不必再跑戰鬥：

```powershell
node .\tools\tournament\analyze-save-usage.mjs 'D:\saves' `
    --trace-manifest 'D:\reports\run-01\usage-traces.json' `
    --output 'D:\reports\run-01\usage.json'
```

快取模式仍核對所有原始檔 SHA-256、完整驗證終端記錄、決策序號、規則指紋與最終棋子快照。它信任清單中的既有原生驗證來源，不會重新證明清單本身的真實性。若遷移較早的實驗快照格式，必須保留原始快照、原生完整驗證清單及其雜湊，記錄遷移來源；不能只憑可解析的快照補寫「驗證成功」。同一份統計只接受相同的規則目錄。

## 整合至賽事分頁

```powershell
node .\tools\tournament\generate-tournament-report.mjs `
    'D:\reports\run-01\results.json' 'D:\reports\run-01\report.html' 'D:\saves' `
    'D:\game\bin\kys_chess_cli.exe' 'D:\game' 'D:\game\config' `
    --usage 'D:\reports\run-01\usage.json'
```

加入 `--usage` 後產生「賽事戰績、使用總覽、棋子使用、羈絆使用、存檔歷程、統計摘要」分頁。使用率來源可以包含更多上傳存檔；產生器依每位參賽者的檔名與 SHA-256 選出完全相符的子集，重新計算分母、天賦群組與種子家族權重。任一參賽者缺少相符歷程即停止，不混合不同母體。這項操作只重建 HTML，保留既有賽事結果，不重跑對戰。

## 統計定義

- **購買**：至少一次商店購買，依操作前商店欄位解析；售出或自動合星後仍計入。購買張數另計。
- **持有／入陣／參戰／最終**：曾在已驗證局面持有、曾放入陣容（包含短暫擺放）、實際進入戰鬥、匯出時入陣。獎勵取得也計入持有與入陣。
- **前／中／後期**：主線第 1–8／9–16／17 輪起；遠征獨立計算。各出現欄每份存檔至多一次，重複嘗試只增加戰鬥場次。
- **羈絆**：採用原生羈絆解析，包含裝備附加成員、同角色去重、星級點數、合縱門檻調整與獨行選擇；不以自然成員交集代替啟動。門檻顯示配置中的名義階數；達成高階亦涵蓋較低階。「裝備參與」不表示裝備是啟動的必要條件。
- **種子加權**：每個種子家族總權重 1，由同種子的變體平分；種子不是玩家身分。各天賦與賽事子集重新計算權重。
- **曾刷出／禁用**：實際觀察到商店出現、曾禁用的存檔數。`shop_slots` 計初始商店、刷新與戰後變動商店中的格數，不按金錢、鎖定欄位或備戰席空間校正，不能視為獨立購買機會。

成功上傳樣本可用來找低使用流派與早期過渡貢獻，不能單憑使用率推論因果強度、整體選取率或勝率。同費比較、禁用傾向與裝備附加成員也應一起檢查。

## 共用程式與快照格式

`report-common.mjs` 共用存檔讀取、SHA-256、天賦名稱與安全內嵌 JSON；`usage-analysis.mjs` 共用快照分析、子集重算及 CSV 輸出；`usage-view.mjs` / `usage-report.mjs` 共用繁體中文互動檢視，透過 Shadow DOM 隔離賽事樣式與事件。兩種報告均可直接離線開啟。

原生指令也可單獨匯出：

```powershell
& $cli verify-pvp $save --trace-output $trace --data-root $dataRoot --config-root $configRoot
```

JSONL 格式 `kys_chess_usage_trace`、版本 1：首列為實際引擎的版本、內容指紋、棋子與羈絆目錄；接著是序號 0 的初始局面及每次操作證據通過後的快照；末列為 `{"validation":{"valid":true,"sequence":N,"message":""}}`。快照包含棋子、商店、裝備、原生羈絆進度、輪數、禁用與最近戰果。即使操作全數通過，最終狀態或資格驗證仍可能失敗；消費端必須檢查成功終端記錄。清單格式為 `kys_chess_usage_traces`、版本 1；統計資料格式為 `kys_chess_usage`、版本 1。

修改分析或報告後執行合成資料回歸測試：

```powershell
node --test .\tools\tournament\usage-analysis.test.mjs
```
