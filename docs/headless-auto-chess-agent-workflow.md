# 無介面自走棋代理流程

`kys_chess_cli` 是唯一的遊戲規則程序；Python MCP 服務只維護一個 JSONL 子程序並轉送要求。GUI、CLI、存檔與重播驗證都使用同一個 `ChessGameSession`。

## 啟動

```powershell
.\x64\Debug\kys_chess_cli.exe --jsonl
```

每行輸入一個 JSON 要求，每行輸出一個 JSON 回應。標準輸出只包含通訊協定；內容載入與警告寫到標準錯誤。第一個要求通常是：

```json
{"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000003039"}}
```

接著使用 `observe`、`legal_actions` 與 `act`。`observe` 同時列出遊戲狀態、存檔欄位及工作階段操作。所有遊戲決策都必須使用 `legal_actions` 提供的穩定識別碼。

## 存檔與時間線替換

`save_game`、`load_game`、`inspect_save`、`export_save` 與 `import_save` 是工作階段操作，不是遊戲行動，因此不消耗亂數，也不會加入最終重播。

載入較早存檔會：

1. 還原該存檔的遊戲狀態與完整亂數狀態；
2. 還原存檔內嵌的重播前綴；
3. 捨棄目前時間線中該前綴之後的行動；
4. 保留外部存檔目錄；
5. 在即時回應的 `discarded_active_actions` 明示捨棄數量。

之後的新行動會接在還原的前綴後方。最終 `export_replay` 只包含玩家選定的合法時間線；離線驗證不需要被捨棄的嘗試。

## 重播

`export_replay` 回傳權威 JSONL。未呼叫 `finish_run` 時頁尾為 `in_progress`；主線通關後仍可整備及挑戰遠征，只有明確提交 `finish_run` 才產生 `campaign_complete`。

可直接驗證 JSONL 或 `.kysreplay` 封裝：

```powershell
.\x64\Debug\kys_chess_cli.exe verify run.kysreplay
```

驗證器會從規則內容、根種子、選項及接受的決策建立全新棋局，逐筆比對前置狀態、事件、亂數、後置狀態及鏈式雜湊。

## MCP

安裝並啟動：

```powershell
pip install -e tools\kys_chess_mcp
$env:KYS_CHESS_CLI = "$PWD\x64\Debug\kys_chess_cli.exe"
kys-chess-mcp
```

MCP 工具與 JSONL 方法一一對應。遊戲規則、合法性、存檔驗證及重播驗證全部留在 C++ 程序內。
