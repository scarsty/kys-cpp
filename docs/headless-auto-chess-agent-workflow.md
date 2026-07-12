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

接著使用 `observe`、`legal_actions` 與 `act`。`observe` 同時列出遊戲狀態、存檔欄位及工作階段操作，並附上目前決策相關的角色名稱、數值、武學、羈絆、裝備及獎勵效果。未擁有任何成員的羈絆不會塞入觀察結果。

`legal_actions` 的每一列都包含可直接提交的 `action_schema`、`example`、候選值名稱與說明。例如部署操作為：

```json
{"type":"set_deployment","chess_instance_ids":[1,2]}
```

欄位缺漏時，`invalid_action` 會指出缺少的欄位並回傳正確範例。遠征直接以唯一的繁體中文名稱選擇，例如 `{"type":"start_challenge","challenge_name":"聚賢莊內"}`，配置不再維護額外英文挑戰 ID。

`prepare_battle` 後的觀察會列出具名敵方陣容、星級、裝備、地圖與站位。`start_battle` 的 compact 回應只保留明文結果、摘要、存活單位、彙總單位統計、傷害分類、重要效果與關鍵擊倒事件；不重複完整開局棋盤、武學定義或逐筆效果軌跡。需要完整資料時使用 `inspect_last_battle`，或在 `start_battle` 操作指定 `detail: "full"`。雜湊只保留作重播驗證用途。

獎勵選項會在選擇前直接列出裝備或內功效果、計入羈絆及指定角色聯動，不必先選取才能理解。戰鬥預覽包含具名地圖、座標方向、ASCII 地形、雙方實際啟用羈絆、敵方預估屬性及當前星級武學；武學同時說明施放距離與單體／直線／十字／範圍幾何。

`new` 預設使用 `full`。`observe` 與 `act` 可傳入 `detail: "compact" | "full"`；MCP 的一般觀察與操作預設為 `compact`，此時 `role_metadata_scope` 是 `omitted_compact`，可用 `inspect_role`、`inspect_combo`、`inspect_equipment`、`inspect_prepared_battle` 或 `inspect_last_battle` 按需取得完整定義。戰敗後的下一個觀察會強制改為 `full` 與 `complete_after_defeat`，但若呼叫端要求 compact，戰報本身仍維持 compact，確保恢復資訊與回應大小各自正確。

裝備效果依來源分為 `base_stat_effects`、`special_effects`、`counts_as_combos` 與 `character_bonuses`。武學以單一項目的 `power_by_star` 列出各星威力。合法操作另有 `candidates_by_field`，例如 `equip` 會分別列出 `equipment_instance_id` 與 `target_chess_instance_id` 候選。

戰報的每個單位另列治療、回內、格擋、閃避、中毒／流血、受擊硬直、眩暈、擊退、封內、冷卻操控、無敵及死亡庇護觸發。`hitstun_*` 只計一般受擊硬直，`stun_*` 才代表明確眩暈，不再混作同一種控制。`projectile_potential_damage_cancelled` 表示碰撞中被中和的潛在彈道傷害，`projectile_cancellations` 表示碰撞次數；此數值不是實際承傷或輸出，因此可能高於最終傷害。

full 戰報的 `initial_combat_stats` 是全部開戰效果套用後的實際屬性，`initial_stat_delta_from_special_effects` 顯示相對星級／勝場／裝備基礎值的淨變化，`enemy_attack_debuff` 與 `enemy_defence_debuff` 明列敵方開戰削弱。`damage_breakdown` 將傷害分為武學、普攻、狀態、羈絆、裝備與其他，總和必須等於 `damage_dealt`；無法可靠歸類的來源保留在 `non_skill_damage_sources`，不會被猜測性地歸入錯誤類別。

`important_effects` 是供決策使用的短摘要，例如陰險的開戰攻防削弱、各角色抵消的潛在彈道傷害及中毒／流血／眩暈次數。full 戰報的 `effect_activations` 是唯一的逐筆結構化軌跡，不再同時複製到每個單位。彈道碰撞會列出雙方彈道 ID、碰撞前數值、中和數值與碰撞後數值；陰險動態調整會列出 `previous_value`、`new_value` 與 `delta`，因此由 -44 回復為 -22 會明確表示為 `delta: 22`。

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

MCP 工具與 JSONL 方法一一對應。`difficulty` 與 `detail` 在工具結構中是列舉值，`seed` 具備 `^0x[0-9a-fA-F]{16}$` 格式限制，呼叫端可在送出前排除無效參數。遊戲規則、合法性、存檔驗證及重播驗證全部留在 C++ 程序內。
