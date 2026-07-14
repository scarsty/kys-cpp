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

`seed` 的格式是 `0x` 前綴加上固定 16 位十六進位數字；`0x` 是必要部分，不是可省略的展示符號。

接著使用 `observe`、`legal_actions` 與 `act`。`observe` 同時列出遊戲狀態、存檔欄位及工作階段操作，並附上目前決策相關的角色名稱、數值、武學、羈絆、裝備及獎勵效果。未擁有任何成員的羈絆不會塞入觀察結果。難度規則直接以 `total_campaign_rounds`、`boss_interval`、`forced_bans_enabled` 與 `maximum_interest_gold` 呈現，例如簡單難度會明示 28 輪且不啟用強制禁棋。

`legal_actions` 的每一列都包含可直接提交的 `action_schema`、`example`、候選值名稱與說明。會花費金幣的操作另有 `economic_preview`，列出目前金幣、實際費用、操作後金幣與結果；購買經驗還會預估操作後等級及經驗，刷新類操作會列出受影響的欄位或選項數。商店棋子、出售棋子及神兵候選也各自列出 `gold_cost`／`gold_gain` 與 `projected_gold_after`。例如部署操作為：

```json
{"type":"set_deployment","chess_instance_ids":[1,2]}
```

欄位缺漏時，`invalid_action` 會指出缺少的欄位並回傳正確範例。遠征直接以唯一的繁體中文名稱選擇，例如 `{"type":"start_challenge","challenge_name":"聚賢莊內"}`，配置不再維護額外英文挑戰 ID。合法操作只提供由目前載入配置生成的敵人數、星級範圍、裝備覆蓋與獎勵摘要；需要完整權威陣容時使用 `inspect_challenge`，其角色、星級、武器、防具及獎勵皆直接來自已載入的挑戰定義。

`prepare_battle` 後的 compact 觀察只列具名單位、星級、裝備 ID、地圖候選及站位，不會用全零屬性或空武學陣列冒充完整資料；`inspect_prepared_battle` 會回傳地形板、完整屬性、武學、裝備及雙方羈絆。`start_battle` 的 summary 回應只列結果、階段變更及狀態雜湊；compact 回精簡戰後現況但不嵌入戰報，完整開局棋盤、單位統計及效果軌跡由 `inspect_last_battle` 或 `detail: "full"` 取得。

獎勵選項會在選擇前直接列出裝備或內功效果、計入羈絆及指定角色聯動，不必先選取才能理解。戰鬥預覽包含具名地圖、座標方向、ASCII 地形、雙方實際啟用羈絆、敵方預估屬性及當前星級武學；武學同時說明施放距離與單體／直線／十字／範圍幾何。

`new` 預設使用 `full`，`observe` 使用 `detail: "compact" | "full"`。`act` 另分為三層：`summary` 只回接受狀態、事件類型、欄位變更、目前階段、合法操作類型與狀態雜湊；`compact` 再附精簡現況，但不嵌入戰報；`full` 才包含完整定義、語意事件細節、完整戰報及驗證雜湊。MCP 的 `take_action` 預設為 `summary`，因此一般操作不會嵌入完整 `next_observation`。戰敗也不再暗中把 compact 升級成 full；戰鬥細節使用 `inspect_last_battle`，恢復分析使用其他聚焦檢視工具。

compact 現況固定保留階段、金幣、等級／經驗、主線進度、商店基本列、棋子實例／角色／星級／出戰狀態、啟用羈絆名稱與計數、裝備指派、完成遠征、待選獎勵摘要、合法操作類型及 `state_hash`。它不含每名棋子的計算屬性、羈絆成員／門檻／說明／貢獻、完整裝備效果、角色定義或完整戰鬥軌跡。

管理決策可按需使用：`inspect_shop_slot` 分析單格的價格、持有份數、預期合成、羈絆前後計數及當前費用機率；`inspect_shop` 一次分析全部欄位；`get_shop_odds` 列出指定或目前等級的費用機率與實際可用角色池；`inspect_chess_instance` 列出實際屬性、裝備、升星進度與羈絆貢獻；`inspect_bans` 列出禁棋、剩餘容量、依費用分組的可選角色及生效時機。`inspect_role` 繼續作為角色靜態定義的權威查詢。

compact 強制禁棋觀察只保留 `pending_reward` 的識別碼、種類與進度摘要；`option_count` 是目前仍可選的角色池數量，`maximum_selections` 與 `remaining_selections` 才是本次最多可選及尚可選的禁棋數。`selection_optional: true` 與 `decision_requirement` 明示「強制」指必須先解決此獨佔決策階段，可以選擇禁棋，也可以略過；略過不消耗之後仍可使用的禁棋容量。完整候選只由 `legal_actions` 的 `add_ban` 欄位提供，避免同一份大型角色清單重複兩次。其他獎勵仍在 compact 觀察保留選項與效果，因為裝備及內功比較本身就是目前決策所需資訊。若裝備候選超過 12 項，compact 觀察改以 `option_groups` 彙整各階武器／防具並保留 `option_count`，實際完整候選仍由合法操作提供。遠征獎勵的規則本來就是從全部合格裝備中選擇，因此通訊層只分組、不擅自過濾或改變遊戲機制。

summary 或 compact `act` 都不回傳 `pre_state_hash`、`post_state_hash`、`event_hash`、`rng_digest`、`chain_hash` 或 `last_battle_digest`。只保留 `state_hash` 作為不透明的狀態版本記號，用來確認拒絕操作沒有改變棋局或偵測快取過期；`detail: "full"` 才保留全部驗證雜湊。

裝備效果依來源分為 `base_stat_effects`、`special_effects`、`counts_as_combos` 與 `character_bonuses`。管理階段棋子的 `current_stats` 包含星級成長、勝場成長及裝備基礎屬性；`current_stats_note` 說明裝備特殊效果與羈絆效果要到戰鬥初始化才會套用。武學以單一項目的 `power_by_star` 列出各星威力。合法操作另有 `candidates_by_field`，例如 `equip` 會分別列出 `equipment_instance_id` 與 `target_chess_instance_id` 候選。未分配裝備排在已裝備項目前面，候選的 `assigned_chess_instance_id` 與 `assigned_to` 會明示目前持有者，自動範例優先選用未分配裝備；若只剩已分配裝備，範例會優先選擇另一名棋子，沒有其他目標時則由 `example_note` 明示該範例不會改變持有者。`equip` 本來就包含轉移已裝備項目的語意，因此不另增功能重複的移動操作。

羈絆的 `contributions` 逐一列出角色、棋子實例、採計星級、實體點數、星級加點、有效點數、是否為原生成員及提供成員資格的裝備。`count_explanation` 明示計算規則：裝備的「計作某羈絆」只讓原本不是成員的角色取得成員資格；若角色本身已是成員，例如俞岱巖裝備真武劍，仍只計一名，不會重複加點。這是既有遊戲規則的可觀察化，不是修改羈絆平衡。

觀察的 `interest_gold`、`next_interest_threshold`、`maximum_interest_gold`、`projected_base_victory_gold` 與 `projected_victory_income` 可直接用於經濟決策。預估勝利收入不包含依戰鬥存活狀態才能決定的羈絆額外金幣，此限制由 `projected_victory_income_excludes_conditional_bonuses` 明示。勝利事件以 `base_gold`、`interest_gold`、`other_gold`、`total_gold` 及可選的 `other_gold_source` 呈現，不必解讀通用 ID 欄位。

`chess_merged` 事件會列出角色名稱、被消耗實例、新實例、結果星級、繼承勝場、出戰狀態、轉移裝備及是否繼續觸發連鎖合成。`enemy_plan_rerolled` 會列出花費、前後敵方規劃鍵與實際影響，不再只回傳一個費用數字。

強制禁棋獎勵屬於獨佔決策階段：此時只有 `add_ban` 與 `skip_forced_bans` 合法。商店購買、合成、部署、裝備等管理操作即使內容本身有效，也會以階段錯誤拒絕，並且不改變狀態。觀察另列 `current_ban_count`、`maximum_ban_count` 與 `remaining_ban_capacity`。新禁棋只會排除之後生成或刷新的商店候選，不會刪除目前商店已經出現的棋子，因此該棋子在本次刷新前仍可購買。

戰報的每個單位另列治療、回內、吸取內力、格擋、閃避、中毒／流血、受擊硬直、眩暈、擊退、封內、冷卻操控、無敵及死亡庇護觸發。`magic_points_drained` 是從目標扣除的內力，`magic_points_restored` 是攻擊者受上限、封內及回復加成影響後實際得到的內力，因此兩者不一定相等。`hitstun_*` 只計一般受擊硬直，`stun_*` 才代表明確眩暈，不再混作同一種控制。`projectile_potential_damage_cancelled` 表示碰撞中被中和的潛在彈道傷害，`projectile_cancellations` 表示碰撞次數；此數值不是實際承傷或輸出，因此可能高於最終傷害。compact 戰報會省略值為零的上述選填指標；full 戰報則保留完整欄位。

毒宗的 `中毒7%×3次` 不是三層中毒，而是一次 7% 中毒負載預定最多三次傷害結算；狀態系統只保留較強的中毒。`poison_payload_events` 計算送出的施毒負載，`poison_application_events` 只計成功套用或強化目標狀態，`poison_ticks` 與 `poison_damage` 分別計實際毒傷次數與傷害。因此負載數可以高於成功套用數，不應解讀為堆疊層數。

full 戰報的 `initial_combat_stats` 是全部開戰效果套用後的實際屬性，`initial_stat_delta_from_special_effects` 顯示相對星級／勝場／裝備基礎值的淨變化，`enemy_attack_debuff` 與 `enemy_defence_debuff` 明列敵方開戰削弱。`damage_breakdown` 將傷害分為武學、普攻、狀態、羈絆、裝備與其他，總和必須等於 `damage_dealt`；無法可靠歸類的來源保留在 `non_skill_damage_sources`，不會被猜測性地歸入錯誤類別。

`important_effects` 是供決策使用的短摘要，例如陰險的開戰攻防削弱、各角色抵消的潛在彈道傷害、中毒／流血／眩暈次數，以及爆炸卡車的 `death_explosion_damage` 觸發次數與殉爆總傷害。陰險摘要及軌跡另列 `source_team: "我方" | "敵方"`、`source_kind: "combo"` 與 `source_name: "陰險"`。full 戰報的 `effect_activations` 是唯一的逐筆結構化軌跡，不再同時複製到每個單位。武學施放使用 `ability_cast` 並列出 `ability_id`、`ability_name`；施毒分為 `poison_payload` 與成功的 `poison_applied`；吸取及實際回復內力分為 `magic_points_drained` 與 `magic_points_restored`。彈道碰撞會列出雙方彈道 ID、碰撞前數值、中和數值與碰撞後數值；陰險動態調整會列出 `previous_value`、`new_value` 與 `delta`，因此由 -44 回復為 -22 會明確表示為 `delta: 22`。

存活摘要的分母只計開戰時的單位，`survivors[].summoned` 則標記召喚物。摘要會把初始存活數與召喚物存活數分開敘述，因此不會再出現「敵方存活 10/9」這類分子、分母口徑不同的結果。

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

MCP 工具對應即時決策所需的 JSONL 方法。`difficulty` 與 `detail` 在工具結構中是列舉值，`seed` 的說明及結構都明示必須符合 `^0x[0-9a-fA-F]{16}$`，也就是 `0x` 加上 16 位十六進位數字。遊戲規則、合法性、存檔及重播格式全部留在 C++ 程序內。

MCP 不再公開 `verify_replay`；完整重播驗證耗時且不參與即時決策。需要發行、除錯或回歸驗證時，仍可在 CLI 離線使用 `verify`，不會把大型重播往返塞進代理工作階段。

MCP 會持續緩衝 CLI 的標準錯誤。CLI 異常結束、沒有回應或回傳損壞的 JSONL 時，原工具回應會包含 `error_code: "cli_process_exited"`、`exit_code`、`diagnostics`、`restarted` 與 `session_lost`；服務會自動重啟子程序，但不會假裝原本的記憶體棋局仍存在。`get_diagnostics` 可隨時查看子程序狀態、持久存檔目錄及最近的原生診斷。

MCP 的具名存檔會在每次 `save_game` 後自動匯出並以原子替換寫到 `%LOCALAPPDATA%\kys_chess_mcp\saves`，也可用 `KYS_CHESS_MCP_SAVE_DIR` 指定位置。`list_saves` 直接讀取此持久目錄，不需要先建立棋局；此時會回傳存檔難度、版本及摘要，但 `compatible` 為 `null`，建立棋局後才依目前遊戲版本與難度判定相容性。新 CLI 子程序建立棋局後會自動匯入這些存檔，因此 MCP 或 CLI 子程序重啟後仍可 `load_game`。直接使用 JSONL CLI 時，程序內具名欄位仍只屬於該程序；需要跨程序保存時應使用 MCP 持久層或自行保存 `export_save`。

`kys_chess_cli.vcxproj` 的建置後步驟會把目前組態的 vcpkg DLL 複製到 CLI 執行檔旁，Debug 使用 `debug\bin`，Release 使用 `bin`，涵蓋 sqlite、yaml-cpp、zip、bz2 與 zlib 等執行期相依項目。
