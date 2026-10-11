# 自走棋賽事報告

`generate-tournament-report.mjs` 與 `modern-tournament-template.html` 產生可離線開啟的互動 HTML。只需要 Node.js，沒有 npm 套件或外部網頁資源。

## 執行賽事

```powershell
& .\x64\Release\kys_chess_cli.exe tournament 'D:\saves' `
    --seed 0x20261011 --battle-seeds 3 --jobs 8 `
    --data-root 'D:\game' --config-root 'D:\game\config' `
    --output report.md --json-output results.json
```

`--jobs N` 指定 Asio 執行緒池大小。桌面 CLI 預設使用邏輯處理器數量的一半，最低為 1；`--jobs 1` 用於序列執行。每場戰鬥是一個工作，戰鬥在鎖外執行，完成後寫入固定結果位置並在短暫鎖內回報進度。進度依實際完成順序顯示，最終結果仍按配對、種子、場次排序；工作數不影響戰鬥種子、驗證碼或排名。程式庫預設仍為 1，WebAssembly 使用單執行緒。

## 產生 HTML

```powershell
node .\tools\tournament\generate-tournament-report.mjs `
    results.json report.html 'D:\saves' `
    'D:\projects\kys-cpp\kys-cpp\x64\Release\kys_chess_cli.exe' `
    'D:\game' 'D:\game\config'
```

參數順序：結果 JSON、輸出 HTML、原始存檔目錄、CLI 路徑、遊戲資料目錄、配置目錄。CLI 路徑預設為 `kys_chess_cli.exe`。若結果 JSON 含有 `run_metadata.data_root` / `config_root`，可以省略末兩個參數；配置目錄預設為資料目錄下的 `config`。

支援原生 `full_round_robin_mirrored` 結果。程式從原始存檔的 `replay.header.talent` 讀取天賦，並核對存檔 SHA256 與賽事紀錄，避免把不同存檔的天賦附到結果上。0.2.18 顯示名稱為 **神兵、中堅、晚成、賭徒**；新版本的 `alliance` 顯示為 **合縱**。未知天賦會回報錯誤。

HTML 包含完整排名、名稱搜尋、天賦篩選、任意兩份陣容的全部對戰、超時標記、戰鬥 SHA256，以及 PowerShell 重現指令。種子數量從 JSON 讀取；支援手機版面與系統深色主題。若需要顯示引擎版本，可在 JSON 加入 `run_metadata.game_version`，不會把存檔版本猜作引擎版本。

重現時須使用相同引擎、遊戲資料與配置。產生報告不會修改結果 JSON 或原始存檔。
