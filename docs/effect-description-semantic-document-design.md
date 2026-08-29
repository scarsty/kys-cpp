# 效果描述語意文件設計

狀態：現行設計，已實作。

本文件已取代 [Effect Description Three-Tier Rendering and Mechanical Coalescing Design](effect-description-three-tier-rendering-design.md) 的 presentation、renderer、API 與版面契約。

[效果描述語法設計](效果描述語法設計.md) 所建立的 typed payload、禁止手寫機制描述，以及執行器與描述器共用同一份解析結果等原則繼續有效；其中另外建立 `EffectDescriptionNode` AST 的方案已由本文件模型取代。本設計不回到每項武功手寫說明，而是把自動描述的輸入粒度從單一規則提升為整個效果容器，先理解角色、跨規則生命週期與常見機制，再產生結構化文件。

## 問題與證據

舊架構的 `effectDescription(const EffectRule&, ...)` 一次只看一條規則，並把中間語意壓成單一字串。即使 `Detailed／Full／Compact` 有不同可見性政策，三者本質上仍接近規則的縮寫序列化，而不是玩家可以閱讀的機制說明。

改造前最長的幾條 Compact 輸出如下。display unit 依 `displayTextWidth` 的概念計算，中文字約 2、ASCII 約 1：

| 長度 | 內容 | 改造前輸出 |
|---:|---|---|
| 245 | 慕容復 | `準備施放·借用所有敵人（隨機決定同順位）的大招規則·數量星級×50%·至少1·至多2·允許類別[...]·不含複製與借用遞迴·傳播借用規則` |
| 199 | 蕭中慧 | `原攻擊目標·若另一名武功62使用者存活·原樣式主彈×1·100%傷害·不觸發大招效果·追加至基礎攻擊·同目標·由友軍1人（不含自身）（使用武功62）出手／否則...` |
| 132 | 譚處端 | `命中·同隊來源·目標有此來源的七星·命中目標·造成招式傷害忽略50%防禦·防前·消耗七星1層·僅此來源→若七星最後一層已消耗·施加眩暈·刷新·30幀` |
| 122 | 虛竹 | `複製所有存活單位（不含自身）（隨機決定同順位）的絕招武功攻擊·數量1·條件[有絕招攻擊定義、排除複製與借用遞迴]·不傳播大招規則` |
| 117 | 歐陽鋒 | `死亡·自身有毒爆·依序×毒爆層數的100%·至少1·5格內所有敵軍·造成毒爆強度的100%純粹傷害→施加中毒4層·強度10·取代×4層·120幀` |

這些問題不是再縮短幾個標籤便能解決：

1. **輸入粒度錯誤。** 七星的施加與消耗位於不同規則；毒爆的累積與死亡引爆也位於不同規則。逐條描述必然只顯示生命週期的一半。
2. **先排版、後理解。** `原目標`、`自身`、`事件目標` 是 AST 的相對引用；若尚未解析誰施放、誰死亡、誰被命中，直接替欄位加標籤仍然含糊。
3. **以欄位重要性代替機制重要性。** `最後一層` 可能只是一個分支條件，卻是玩家理解七星的核心；依欄位種類刪除條件會破壞因果關係。
4. **把稽核資訊塞進玩家文字。** 完整 action allowlist、傳播 enum、同順位決定方式與 settlement phase 有稽核價值，但不應全擠在 Compact 列。
5. **字串不是 UI 結構。** 呼叫端只拿到一段文字，無法可靠地分組、換行、縮排、摺疊或提供詳細入口，只能不斷縮字。

## 目標

1. 以一個完整效果容器作為描述單位，能連結同容器內的狀態與 state slot 生命週期。
2. 先建立不含標點與版面的 typed `EffectDescriptionDocument`，再投影為 Detailed、Full 或 Compact。
3. Detailed 以多行、分節、縮排的方式完整呈現正式設定，並能逐欄位稽核。
4. Full 保留玩家作決策所需的機制、條件與例外，但不暴露實作詞彙。
5. Compact 成為數條短而完整的摘要列，不再是把整棵 AST 壓成一行。
6. 所有主詞與對象都由 typed role 解決，不顯示含糊的裸詞。
7. 以可重用的 typed mechanic archetype 組織說明，不依效果 ID、武功 ID 或顯示名稱分支。
8. 每個被省略的欄位都有可測試的正式理由；未知或非預設欄位不得靜默消失。
9. 讓 UI 直接取得區塊與列，能換行、分欄、捲動或進入詳細頁，不再解析描述字串。

## 非目標

- 不改變效果解析、驗證、執行順序、隨機消耗、戰鬥數值或重播語意。
- 不讓 YAML 新增 Full、Compact 或自由文字機制說明欄位。
- 不為慕容復、譚處端、虛竹、歐陽鋒等個別角色建立專用 formatter。
- 不從完成的字串反向猜測共同前綴、狀態關係或 action 順序。
- 不跨不同效果容器推論狀態生命週期；跨容器互動仍依每個容器可見的正式語意描述。
- 不以截斷、刪除關鍵條件或無限制縮小字體來滿足版面。
- 不保留舊的逐規則字串 API、legacy style 或兩套長期並存的 renderer。
- 不追求文學化文案；清楚、無歧義且可驗證的結構化中文優先。

## 描述單位：完整效果容器

描述輸入不再是一條 `EffectRule`，而是玩家視為同一項效果的最小完整容器：

| 來源 | 描述容器 |
|---|---|
| 大招效果 | 一個 `ChessMagicEffectDefinition` 的全部 `rules` |
| 裝備 | 一個 `EquipmentDef` 的全部 `rules` |
| 裝備羈絆 | 一個 `EquipmentSynergyDef` 的全部 `rules` |
| 內功 | 一個 `NeigongDef` 的全部 `rules` |
| 羈絆 | 一個 `ComboThreshold` 的全部 `rules` |

同一個容器可以產生多個描述區塊，但區塊是在分析所有規則後才形成。規則 ID 與原始順序仍保留為稽核與執行追蹤資料，不再強迫「一條規則等於一個顯示列」。

容器邊界是重要的安全限制。兩個不同武功碰巧使用同名狀態，不能因此被描述器合併；裝備與內功各自建立自己的 producer／consumer graph。若未來確實需要跨容器說明，必須先在 runtime schema 中建立可識別的 typed shared identity，而不是用顯示名稱配對。

## 處理流程

```text
包含全部規則的效果容器
  ↓
已解析的 typed EffectRule／EffectAction payload
  ↓
provenance 與欄位 coverage 擷取
  ↓
條件 action 提升為 typed DescriptionBranch
  ↓
容器內 status／state producer-consumer graph 與 closed archetype 歸約
  ↓
EffectDescriptionDocument
  ↓
Detailed／Full／Compact 結構化輸出
```

每一步只讀取上一層的 typed 結果。禁止 renderer 查詢效果 ID、角色名稱或已產生的句子來決定機制。

## 結構化文件模型

現行型別的責任與資料流如下：

```cpp
enum class EffectDescriptionContainerKind
{
    Magic,
    Equipment,
    EquipmentSynergy,
    Neigong,
    ComboThreshold,
};

struct EffectDescriptionInput
{
    EffectDescriptionContainerKind kind{};
    std::span<const EffectRule> rules{};
};

struct EffectDescriptionPresentationContext
{
    std::optional<EffectEvent> enclosingDefaultEvent{};
};

enum class DescriptionFactLevel
{
    Core,
    Decision,
    Audit,
};

struct DescriptionPlayerProjection
{
    bool full{};
    bool compact{};
};

enum class DescriptionPlayerFact
{
    Trigger,
    Target,
    Condition,
    RuleQualifiers,
    Action,
    StatusPotency,
    StatusSecondaryPotency,
    BorrowedRuleActionCategories,
    CopiedMagicConditions,
    StateMachinePropagation,
};

enum class DescriptionPhraseAbsorption
{
    None,
    UltimateEvent,
    OrdinaryHitTrigger,
    SameComboAllyDeathTrigger,
    CompactPositiveDamagePerspective,
};

struct EffectDescriptionFact
{
    DescriptionFactValue value; // closed typed variant
    std::vector<DescriptionSourceField> sources;
    DescriptionFactLevel level{};
    DescriptionPlayerProjection projection;
    DescriptionPlayerFact playerFact{};
    DescriptionPhraseAbsorption absorption{};
};

struct DescriptionAction
{
    std::variant<EffectAction, std::shared_ptr<DescriptionBranch>> value;
    std::vector<DescriptionSourceField> sources;
    DescriptionFactLevel level{};
    DescriptionPlayerProjection projection;
    DescriptionPlayerFact playerFact{};
};

struct EffectDescriptionBlock
{
    DescriptionArchetype archetype{};
    EffectRuleId sourceRuleId{};
    std::size_t sourceRuleOrder{};
    std::vector<EffectDescriptionFact> trigger;
    std::vector<EffectDescriptionFact> conditions;
    std::vector<EffectDescriptionFact> targets;
    std::vector<DescriptionActionGroup> actions;
    DescriptionCoverage coverage;
};

struct EffectDescriptionSection
{
    std::optional<EffectEvent> event;
    std::vector<EffectDescriptionBlock> blocks;
};

struct EffectDescriptionDocument
{
    EffectDescriptionContainerKind kind{};
    std::vector<EffectDescriptionSection> sections;
};
```

`DescriptionFactValue`、`DescriptionActionGroup` 與 `DescriptionBranch` 必須是 closed typed variants，不是預先組好的中文。`DescriptionAction` 直接保存正式 `EffectAction`，條件 action 則遞迴提升為 `DescriptionBranch`；不另建一份鏡像 subject／predicate／clause AST。中文只在最後 renderer 產生。狀態顯示名「七星」「毒爆」可作為 localization label，但層數、強度、來源、持續時間與耗盡行為仍來自 typed payload。

Core／Decision／Audit 與 Full／Compact projection 是 document 本身的 typed metadata，不是 renderer 臨時用字串判斷。相同的 `level`、closed `DescriptionPlayerFact` 與 `projection` 也記錄在 `DescriptionCoverageEntry`，因此來源欄位、fact、action 與玩家投影可以互相核對。Full 的 Core／Decision set 與 Compact 的 global mandatory set 都以 `DescriptionPlayerFact` 明列；非預設 observation／cast match 是 `Trigger` Decision，Full 與 Compact 都必須投影。Generic、StatusLifecycle、StackExplosion、ConditionalAttack、CopyAttack 與 BorrowRules 各有一份 `ArchetypeProjectionDescriptor`，用 closed fact ID 宣告額外 Audit projection，不比較 field path 字串；Generic 的狀態強度／次要強度也由 descriptor 明確授權，不能因 root action 可見便直接讀取未投影的數值。只有 `Visible` 或 `AbsorbedByPhrase` 欄位可被 promotion，`SchemaDefault` 即使 fact ID 相符也維持不投影，避免未 authored 的預設值重新滲入玩家文字。

renderer 只消費 projected condition/action view。所有帶 player fact 且 disposition 為 `Visible`／`AbsorbedByPhrase` 的 coverage entry，不論 Core、Decision 或 Audit，都必須由目前 style 授權；Generic 也受同一檢查。specialized archetype 在輸出前另驗證 trigger、target、condition、qualifier 與遞迴 action 都已由同一 projection view 授權；不完整時在 Debug／Release 都以 `logic_error` fail closed，不能退回 Generic raw-action renderer。`UltimateEvent`、普通命中、同羈絆死亡與 Compact 正傷害合併等省略／吸收，先在 document 標為 closed `DescriptionPhraseAbsorption`，renderer 不再用 variant 特判自行隱藏已 projected fact。測試會直接切換 condition/action projection 與 absorption metadata，驗證輸出隨之顯示或隱藏，並刻意破壞 Generic potency、Full trigger Decision 與 specialized root projection，驗證未授權資料不會在 Release fallback 洩漏。

每個 fact 保存 `DescriptionSourceField`，至少可追溯至 rule ID、condition/action variant 與欄位路徑。這份 provenance 供欄位覆蓋測試與診斷使用，不需要直接顯示給玩家。

每個 block 自己擁有且只擁有一份 `DescriptionCoverage`；document 不再維護第二份 root coverage mirror。文件是唯一的描述中間表示。三種 style 不得各自重新分析原始 `EffectRule`，也不得各自維護一份 mechanic matcher。Detailed 也只能從 document facts 與 provenance 產生；它不能為了 lossless 另行走訪舊 description AST，否則會形成第二套 formatter。

### 呈現 context 與 enclosing default

`EffectDescriptionDocument` 本身不含面板 context，同一份 document 必須能在 standalone 與巢狀 UI 重用。`EffectDescriptionPresentationContext` 只傳給最後的 renderer，而且只能影響重複 trigger 文字；不能改變 facts、graph、archetype matching 或 coverage。Magic 容器本身具有一個 intrinsic presentation default：玩家閱讀某項武功的效果時，外層語境已經是該武功，因此 Full／Compact 預設把 `UltimateCommitted` 視為 enclosing event；呼叫端不再重複傳入同一份 context mirror。Detailed 不使用這項省略。

enclosing-default 的省略規則如下：

1. Detailed 永遠顯示每個來源規則的 event 與 `EffectCastMatch`，不受 context 影響。
2. Full 與 Compact 只有在 effective enclosing event 等於 `sourceRule.event` 且該來源規則的 `castMatch == EffectCastMatch::BoundMagic` 時，才能省略該 trigger group 的最外層 event 詞。effective enclosing event 對 Magic 預設為 `UltimateCommitted`，其他容器預設為空；明確 presentation context 可為特殊巢狀顯示指定不同 enclosing event。
3. 容器內若有多個 event section 或一個 cross-rule block 含多個 trigger group，逐一按其來源規則判斷；context 不能一次隱藏整個 section。`MainProjectileBeforeDamage`、`CastSettled`、`UltimateCooldownFinished` 等不相符 event 仍必須顯示。
4. `EffectCastMatch::OwnerAnyCast` 永遠保留 event 與「任意施放」語意。context 也不能省略 condition、cast-match wording、propagation、duration、action qualifier 或 branch。
5. 非 Magic 呼叫端只能在 typed row model 把 block 與 enclosing source 關聯，而且 UI 以標題、縮排等可見 parent-child hierarchy 表達該關聯時設定 context。只因 call site、顏色、ID 或資料來源相同，不足以省略 event。Magic 的 intrinsic default 來自容器種類，不由 call site、magic ID 或面板名稱推測。

例如角色大招列可在「羅漢伏魔功」標題下，對一個 `BoundMagic` 的 `UltimateCommitted` trigger 省略重複的「施放大招」；同容器的主彈命中、施放結算或 `OwnerAnyCast` 規則仍保留各自主詞與觸發詞。換行後的 continuation row 必須保持縮排，不能與標題失去視覺關聯。

## 語意角色與明確主詞

描述器先把 selector 與事件中的相對引用解析成 typed semantic role。現行文件模型以效果持有者、事件來源、事件目標、原攻擊目標及 selector 選出單位作為 closed role；施法、命中、死亡及狀態來源等玩家措辭由這些 role 與 typed event／payload 組合產生：

- 效果持有者：裝備、內功、羈絆或大招規則所綁定的單位。
- 本次施法者：觸發目前施放事件的單位；通常但不保證等於效果持有者。
- 事件來源：造成命中、傷害、治療或死亡事件的單位。
- 事件目標：事件所作用的單位。
- 本次攻擊目標：該次基礎攻擊或絕招原本選定的目標。
- 被命中單位：實際接受該枚彈道或該次命中的單位。
- 被選單位：selector 在目前 scope 中選出的友軍或敵軍。
- 死亡單位：死亡事件所指的單位。
- 狀態施加者：建立特定 status instance 的來源單位。

trigger role resolver 以 `EffectObservationScope` 決定效果持有者、事件來源或事件目標；selector role resolver 以 closed `EffectSelectorKind` 決定效果持有者、事件來源、事件目標、原攻擊目標或被選單位。`EffectCastMatch` 是另一個 typed fact：`BoundMagic` 表示規則綁定本容器的武功，`OwnerAnyCast` 表示效果持有者的任意施放也可能觸發。兩者在 renderer 組合，但不能互相覆寫，因此不能假定事件施法者、效果持有者與被借用／代打的出手者相同。cast match 本身是 Detailed 的 Audit fact；非預設 `OwnerAnyCast` 同時產生 Full／Compact 可見的 Decision fact。

renderer 根據 role 與事件上下文產生完整關係詞，例如：

| 不可單獨輸出 | 必須解析成類似文字 |
|---|---|
| `死亡` | `施法者死亡時`、`任一友軍死亡時`、`帶有此效果的敵人死亡時` |
| `原目標` | `本次絕招原本選定的目標`、`本次基礎攻擊的目標` |
| `自身` | `效果持有者`、`施法者`、`造成該次命中的友軍` |
| `事件目標` | `被命中的敵人`、`受到治療的友軍`、`死亡的友軍` |

`EventTargetBelongsToBoundSourceCondition` 的內部 identity 仍是 event target，但玩家措辭必須依死亡 event 解析為「死亡單位屬於此羈絆（來源）」。`AllyDied` 的同羈絆條件可由 typed `SameComboAllyDeathTrigger` absorption 合併成「同羈絆友軍死亡」；`UnitDied` 不得沿用這項 absorption，必須明列死亡單位條件。兩條路徑的 Full／Compact 都不得輸出 `事件目標`。

同一個 runtime selector 在不同事件中可以有不同玩家措辭，但必須先解析成同一個 typed role，再由 phrase table 選詞。不得在 action formatter 看到 `Self` 便直接輸出「自身」。

role resolution failure 與 archetype mismatch 是不同錯誤。通過 parser／validator 的正式規則必須能解析唯一主詞與對象；無法解析代表 typed document model 或 resolver 違反 invariant，document builder 會 fail-fast／assert，測試直接失敗。generic fallback 仍需要 resolved role，因此只能承接 archetype mismatch，不能承接 role failure。

## 容器內狀態與 state slot 生命週期

### Canonical typed identity

graph 不以 localization label 或 runtime string 比較 identity。所有正式狀態引用已在 parse time canonicalize 為 `BattleStatusKind`：

- `ApplyStatusAction`、`ConsumeStatusAction` 與 depleted action 使用 `BattleStatusKind`；
- `EffectNumber::status`、`SourceHasStateCondition::state`、`TargetHasStateCondition::state`、`TargetHasStateFromEffectOwnerCondition::state` 與 `SourceStackAtLeastCondition::stack` 均使用 `BattleStatusKind`；
- YAML 的繁體中文 label 只在 parser 邊界透過同一個 enum descriptor 解析一次；`battleStatusLabel` 只負責顯示，不能再作 runtime 或 graph key；
- `EffectStateSlot` 是 typed identity，維持 enum 比較；未來其他具名 state 若不是 `BattleStatusKind`，必須建立自己的 enum 或 stable typed ID，不能復用 localization string。

繁體中文 label 只存在於 parser 與 renderer 邊界；狀態的 runtime 查詢、數值公式、condition、action 與 graph edge 全部使用同一 enum。來源限制與目標 domain 仍是 identity 之外的獨立相容條件。

### Graph 節點

對每個容器建立 producer／consumer graph。節點至少包含：

- status identity 或 state slot identity；
- 操作種類：建立、增加、替換、讀取、消耗、清除、耗盡分支或重設；
- 施加者／擁有者 role；
- 目標 domain；
- source scope 與 provenance 限制；
- 所在 rule、event 與 action 順序；
- lifetime、stack cap 與 source-death policy 等 typed qualifier。

### 連線條件

只有下列條件全部成立才可連線：

1. canonical `BattleStatusKind` 完全相同，或 typed state slot identity 完全相同；
2. producer 的目標 domain 與 consumer 可能讀取的 domain 相容；
3. consumer 要求「此來源」時，producer 的來源 role 與 binding 完全相容；
4. provenance、owner scope、team relation 與 source-death policy 沒有衝突；
5. 對 action order 有要求的 state cycle，連線後仍保留正式執行順序。

顯示名稱相同、數值相同或相鄰出現在 YAML 中，都不是 graph 的連線證據；連線只讀 canonical typed identity。不能確定其他相容條件時保留成兩個獨立 block，Detailed 仍完整顯示來源條件。

### Graph 歸約

graph 可把多條規則組成玩家理解的生命週期，但不得把不同事件合成一句沒有觸發點的文字。例如七星應保留「主彈命中時施加」與「任一友軍命中七星目標時消耗」兩列；毒爆應保留「絕招時累積」與「施法者死亡時引爆」兩列。

若一個 consumer 有多個相容 producer，必須在文件中保留其來源集合或拒絕單一生命週期 archetype，不能任選一個 producer。

## Core、Decision 與 Audit fact

欄位不是依「condition 是否太長」決定是否顯示，而是先轉成玩家語意 fact，再分級：

| 層級 | 定義 | 例子 |
|---|---|---|
| Core | 效果是什麼，以及何時、由誰、對誰發生 | `主彈命中時施加七星7層`、`施法者死亡時引爆毒爆` |
| Decision | 會改變玩家預期、觸發與否、次數、範圍或結果的條件與例外 | `最後一層消耗時眩暈`、`若另一名同武功友軍存活`、`最多5層` |
| Audit | 正式設定的完整執行與決定性細節 | 同順位隨機政策、action allowlist、傳播政策、精確 settlement phase |

Detailed 顯示 Core、Decision 與 Audit。Full 預設只顯示全部 Core 與 Decision；若某個 Audit fact 對特定 phrase family 不可缺少，archetype descriptor 必須逐一以 typed fact 名稱宣告 `PromotedToFull`、說明原因並加入 golden／mutation test，不能由 renderer 臨場判斷「理解機制所需」。Compact 預設不顯示 Audit；只有 archetype descriptor 以 closed fact ID 明確列入 Compact Audit projection 的欄位，才能顯示或由玩家詞組吸收。除此之外，Compact 顯示全部 Core，以及全域 mandatory set 或 descriptor 逐一命名的 Decision facts；mandatory set 至少包含分支、耗盡、上限、持續時間、間隔、機率、範圍、來源限制與選擇數量，不存在開放式的「重要例外」判斷。

「從候選中隨機選取單位」會改變實際對象與結果，屬於 Decision，Full／Compact 必須顯示「隨機」。只有候選已在玩法上同順位、而 tie-order 僅決定等價項目的列舉／抽樣次序時，該 tie-order policy 才屬 `DeterminismOnly`／Audit。兩者不能因都使用 RNG 而合併成同一省略規則。

`最後一層` 即使源自巢狀 condition，仍是 stack-consume archetype 的 Decision fact，因此 Compact 不得省略。反之，借用規則的完整 action allowlist 是 Audit fact，可在 Full 與 Compact 被「大招效果」這個已定義的玩家詞彙吸收。

## 明確省略契約

每個來源欄位都必須落入下列其中一種 disposition：

```cpp
enum class DescriptionFieldDisposition
{
    Visible,
    AbsorbedByPhrase,
    SchemaDefault,
    DeterminismOnly,
    SafetyInvariant,
};
```

- `Visible`：產生一個可追溯 fact，依 style contract 顯示。
- `AbsorbedByPhrase`：由一個有明確語意的較高階詞彙吸收，例如「大招效果」吸收已定義的標準 action 類別集合。matcher 必須列出被吸收的精確欄位和值。
- `SchemaDefault`：值為該 variant 的正式預設，且 archetype 明確宣告可省略。非預設值不能使用此理由。
- `DeterminismOnly`：只決定等價候選的穩定／隨機順序，不改變玩家可選擇的規則。Detailed 仍顯示。
- `SafetyInvariant`：由 parser／validator 保證且不是可設定的機制差異。必須引用實際 invariant，不能用來藏住未處理欄位。

disposition 規範 Full／Compact 如何吸收或省略來源欄位，不削弱 Detailed 的 lossless 契約。Detailed 必須從 fact 與 provenance 顯示 `SchemaDefault`、`DeterminismOnly` 及被高階 phrase 吸收的精確值；`SafetyInvariant` 若對應實際來源欄位，也必須出現在 audit trace。

`DescriptionCoverage` 記錄每個欄位的 disposition。任何非預設欄位未被 matcher 消耗時，該 archetype 匹配失敗；不得先匹配再漏掉欄位。失敗後使用結構化 fallback，並由內容測試列出該 typed shape。

## Typed mechanic archetype

archetype 是 reusable typed pattern，不是效果專用模板。現行 archetype library 支援：

1. 限時或永久屬性修正；
2. 狀態施加與堆疊；
3. 狀態消耗與耗盡行為；
4. stack 累積與逐層／一次引爆；
5. 記錄、讀取、消耗與重設；
6. 有條件的攻擊追加或代打；
7. 複製另一單位的攻擊；
8. 借用其他單位的規則；
9. 傷害吸收的建立、吸收、釋放與清除；
10. 持續區域的建立、週期作用與移除。

每個 archetype descriptor 必須宣告：

- 必要的 rule、condition、action 與 graph shape；
- 每個欄位允許的 typed value 或限制；
- 產生哪些 Core、Decision 與 Audit facts；
- 每個被吸收或省略欄位的 disposition 與理由；
- 哪些變化會使匹配失敗；
- block 的安全切分點與各 style 的 phrase family。

例如「狀態消耗與耗盡行為」只有在 consume action、source scope、消耗量、耗盡判定與耗盡 action 全部被處理後才可匹配。把 `耗盡判定` 改成不同條件，若 descriptor 未宣告該 shape，必須拒絕匹配，而不是繼續輸出看似相同的「消耗1層」。

matcher 不得接收 magic ID、role ID、item ID、combo ID 或顯示名稱作為判斷依據。除 localization label 外，相同 typed shape 必須產生相同結構與措辭。

### 屬性修正的有限共同 qualifier 提升

限時屬性修正 archetype 必須承接現行 duration-only coalescing 的安全邊界，不能因改為 container-level document 而放寬。兩個以上屬性 action 只有在下列條件全部成立時，才能把共同 duration 提升為 group qualifier：

1. 位於同一個 `DescriptionSimultaneous`，且 action 在原順序中相鄰；
2. effective target 完全相同；
3. 每一項都是 `ModifyAttributeAction`；
4. `AttributeOperation` 完全相同；
5. 每個 amount 都是 constant，且共用的 typed sign classifier 證明 `attributeModifierIsNegative(operation, amount) == false`；amount 可以不同，但 formula 或 sign-unknown 不合格；
6. `durationFrames` 完全相同且大於零；
7. `stack` 完全相同，而且只能是 `Independent` 或 `Refresh`；
8. 每一項都沒有 `stackLimit`、`perStack == false`，且 `stackScope == Shared`；因此 `Refresh` 必然是 uncapped；
9. group 內沒有 per-action condition，所有項目來自同一次 simultaneous dispatch；
10. 中間沒有 sequence、conditional、for-each 或 state-cycle boundary。

`durationFrames`、`stack`、`stackLimit`、`perStack` 與 `stackScope` 是目前 `ModifyAttributeAction` 的完整 lifecycle field 集合，該 action family 沒有 source-death policy；未來新增 lifecycle field 時，descriptor 在明確分類前必須預設拒絕。只有 `durationFrames` 可以提升。stack policy、cap、per-stack、scope、winner、counter 與 runtime stack identity 永遠附著在各 action；任何含 cap 或 counter 的 qualifier 都不能提升成共同文字。`AddStack`、`Replace`、`KeepStrongest`、runtime-negative、formula、sign-unknown、永久 modifier、不同 duration／target／operation／scope，以及被其他 action 隔開的 modifier 均不合併。status shield 可能使負值 sibling 的實際 lifetime 分歧，因此即使設定 duration 相同也不例外。

Detailed 不合併，每個 action 仍完整顯示。Full 與 Compact 可在上述邊界內顯示例如 `攻擊+20%、防禦+30%，持續100幀` 與 `攻+20%、防+30%（100幀）`；這只聲明共同顯示 duration，不聲明 sibling 共用 timer、stack key、cap 或 counter，也不進一步把共同 amount 因式分解成 `攻防+30%`。未來其他 action family 若要提升 qualifier，必須另有完整 lifecycle field 清單與獨立審查的 typed eligibility contract，不能使用文字相似度或沿用本規則。

## 關聯式 schema 優先於 ID 翻譯

`其他存活友軍使用武功: 62` 的玩家語意其實是「另一名同武功友軍存活」。同樣，舊式 `武功相符: 62` 若實際想表示目前效果來源的武功，也不是數字比較的玩家語意。描述器不應查詢 magic 62 的名稱，也不應對 62 特判。

正式設定已硬切換成能表達關係的 typed condition；以下是分屬不同規則的兩個例子：

```yaml
條件:
  - 其他存活友軍使用此武功
```

```yaml
條件:
  - 施放武功為效果來源
```

`OtherLivingAllyUsesBoundMagicCondition` 表示另一名存活友軍使用與目前效果容器綁定的同一武功；`CastUsesEffectSourceMagicCondition` 表示觸發 cast 的 `magicId` 必須等於規則 binding 的 `sourceId`。runtime 與描述器消費相同關係，不再保存 `requiredMagicId` 或 `MagicIdEqualsCondition`。凡是人類語意是「此武功」「目前羈絆」「此效果來源」的欄位，都應優先在 schema 中表達關係，不把數字 ID 留給 renderer 猜測。Full／Compact 可以輸出「同武功」或「施放武功為效果來源」，但不得洩漏 `武功62` 之類數字 ID。

武學類別也採相同硬切換。selector 使用 closed enum `EffectMartialCategory::{Fist, Sword, Knife, Unusual}` 與 `EffectSelectorKind::AlliesUsingMartialCategory`；設定寫成：

```yaml
目標:
  類型: 指定武學類別友軍
  武學類別: 御劍
```

舊 `指定武器友軍`、`武器類型: 1`、`AlliesUsingWeapon`、`requiredWeaponType` 與 `weaponType` 全部刪除，沒有 parser alias。Full 分別顯示「使用拳掌／御劍／耍刀／特殊類武功的友軍」，Compact 分別顯示「友方拳掌／御劍／耍刀／特殊角色」；玩家列不得出現數字 category code。

## Typed 數值基準與資源措辭

數值公式的 primary base 或 `multiplierBase` 若已經包含資源名稱，action phrase 都不得再次附加同一名稱。例如 `TargetCurrentShield` 的20%乘星級輸出 `目標目前護盾的20%×星級`，`TargetCurrentCooldown` 的30%輸出 `目標目前冷卻的30%`；反轉為「來源星級」primary 加「目標目前護盾／冷卻」multiplier 時也只能出現一次資源名。不能產生 `目標目前護盾的20%×星級護盾`、`星級×20%×目標目前護盾護盾` 或 `目標目前冷卻的30%目前冷卻`。HP 公式遵循同一原則。這由同一個 typed base-membership helper 同時檢查 `base` 與 `multiplierBase`，不靠完成後的字串去重。

## 結構化 generic fallback

無法匹配高階 archetype 不代表退回已刪除的 `·` 串接。fallback 仍從 role-resolved typed document facts 建立結構化 block：

```text
觸發：施法者死亡時
條件：死亡前仍有至少1層毒爆
對象：5格內所有敵人
效果：
  1. 依毒爆層數重複以下動作
  2. 造成純粹傷害
  3. 對受傷敵人施加中毒
限制：...
```

fallback 必須：

- 有明確主詞與對象；
- 保留 sequence、simultaneous、branch 與 for-each 的巢狀結構；
- Detailed 完整顯示所有正式欄位；
- Full 與 Compact 依 fact level 投影，而不是任意丟 condition；
- 在 `DescriptionCoverage` 標記 `GenericFallback` 與未匹配 typed shape。

正式內容中的 fallback 數量與 shape 要有測試清單。新增 fallback 必須讓測試失敗並要求檢視；已知清單只能隨 archetype 覆蓋增加而縮小。fallback 本身若通過可讀性與長度契約，不必為追求零數量而建立牽強 archetype。

## 三種輸出契約

三種 style 是同一份 `EffectDescriptionDocument` 的投影，不是三個獨立分析器。

### Detailed

Detailed 是 lossless、接近正式設定但使用 resolved role 的多行文件：

- 依觸發或生命週期分 section；
- 明列主詞、觀察範圍、施放匹配、條件、對象、動作、順序、機率、次數與 cooldown；
- 明列 duration、stacking、scope、source policy、propagation、rounding、phase 與 repetition formula；
- 巢狀 branch、sequence、for-each 與 state cycle 使用縮排和編號，不用一個符號承載關係；
- cross-rule block 顯示整合後生命週期，同時保留來源 rule／field trace 供稽核；
- 不設單列長度上限，UI 應換行或以連續列捲動。

Detailed 的 lossless 契約以欄位為準，不以目前舊 Full 曾否顯示為準。任何 player-visible 或 execution-relevant typed payload 欄位變更，都必須改變 Detailed 文件或其 audit trace。

### Full

Full 是有空間面板中的完整玩家說明：

- 顯示所有 Core 與 Decision facts；
- 用數個有標題或有明確觸發詞的段落呈現生命週期；
- 保留持續時間、上限、間隔、機率、範圍、來源限制、分支與耗盡行為；
- 可省略已被玩家詞彙吸收的 Audit 細節；
- 不顯示 action variant 名、完整 action allowlist、傳播 enum、`條件[...]`、`追加至基礎攻擊`、`原樣式` 或數字 magic ID；
- 需要表達順序時使用「先……，再……」；需要表達耗盡時使用「最後一層消耗時……」。

### Compact

Compact 是數條快速掃讀列，不是整個容器的一行摘要：

- 一列只描述一個觸發、分支或生命週期階段；
- 保留該列的明確主詞、事件對象、核心數值、duration、cap 與會改變結果的 Decision facts；
- 結構標點的封閉集合為冒號 `：`、逗號 `，`、頓號 `、`、分號 `；` 與全形括號 `（ ）`，不能用其他密集符號取代語意關係；
- 超過長度門檻時先按 branch、sequence 或 action group 切成多列，不截斷文字；
- 不能為了縮短而刪除「最後一層」「否則」「不複製大招效果」等核心例外。

## 標點與順序

玩家輸出的三種 style 都不使用 `→`。該符號無法表明「結果」「接著」「條件成立後」或「對每個目標」中的哪一種關係。

Detailed、Full 與 Compact 的每一列都不加 terminal `。`。semantic heading 可以使用 `：` 表示其下還有縮排子列，列內仍可使用 `，`、`、` 與 `；` 表達關係。renderer 不預留句號寬度、不追加句號，phrase family 也不產生列尾句號。外層 UI 不得追加、移除或猜測標點；被迫輸出單一字串的 protocol adapter 應按 block 使用換行，不能把列串成 `。；`、`：。` 或其他人工句尾。這是對早期 punctuation-neutral 契約的恢復；曾要求 Full prose row 以 `。` 結尾的中間版本已作廢。

`×` 不屬於結構標點，只能作為 typed arithmetic expression 中的乘號，例如 `星級×50%`；它不能分隔 trigger、condition、target、action 或 branch。現行 Compact 的全形 `／` coalescing／branch separator 與 ASCII `/` 都正式退役：同類項目使用頓號 `、`，分支使用「若……」與「否則……」的分行結構。`·` 也不再由 effect-description renderer 輸出；其他 UI 若有獨立的純數值 badge，可以在自身契約內使用 `·`，但不能把它帶回 description block。

| 語意 | 輸出方式 |
|---|---|
| 有順序的動作 | `先造成傷害，再施加中毒`，或 Detailed 編號清單 |
| 同時動作 | `獲得攻擊與防禦加成`，或同一 bullet group |
| 條件分支 | `若……：` 與 `否則：` 的分行區塊 |
| 耗盡分支 | `最後一層消耗時，使目標眩暈30幀` |
| 每層重複 | `逐層引爆；每層……` |
| 每個目標 | `對每名被選中的敵人……` |

## 代表性輸出

本節文字同時規範資訊層級與因果結構。實作可統一細部用詞，但不得退回 ID 特判或省略列出的機制。

### 譚處端：七星

Detailed：

```text
主彈道命中
  主詞：本次施法者
  對象：被主彈命中的敵人
  動作：施加「七星」7層
  上限：7層
  持續：150幀
  強度：50
  次要強度：30

任一友軍命中
  主詞：造成該次命中的友軍
  條件：被命中的目標帶有本次施法者所施加的「七星」
  動作：
    1. 該次招式忽略50%防禦
    2. 消耗該目標的「七星」1層
  耗盡：消耗最後一層時，使該目標眩暈30幀
```

Full：

```text
主彈命中時，對被命中的敵人施加七星7層，最多7層，持續150幀
任一友軍命中由效果持有者施加七星的敵人時，該次招式忽略50%防禦並消耗1層；最後一層消耗時，使該敵人眩暈30幀
```

Compact：

```text
主彈命中：施加七星7層（150幀）
友軍命中七星目標：破防50%，消耗1層；最後一層消耗時眩暈30幀
```

Compact 第二列的「七星」由第一條 producer rule 連入 consumer rule；不是 consumer formatter 看見 status 名稱後憑空猜出的補充。

### 慕容復：借用大招效果

Detailed：

```text
準備施放大招
  候選：所有敵人
  選擇數量：星級×50%，至少1名、至多2名
  同順位：隨機決定
  動作：借用被選敵人的大招規則
  允許類別：屬性修正、傷害修正、資源變更、治療交易修正、狀態、傷害、攻擊、強制移動、區域、修改施放、狀態值、傷害記憶、傷害吸收、狀態傷害結算
  遞迴限制：不借用複製或借用規則
  傳播政策：借用大招規則
```

Full：

```text
準備施放大招時，依星級隨機借用1～2名敵人的大招效果；不會遞迴借用或複製
```

Compact：

```text
準備施放：依星級隨機借用1～2名敵人的大招效果
```

Compact 的「大招效果」是 archetype 明確定義的吸收詞，只有 action allowlist 等於受支援集合時才成立；集合新增非預期類別時 matcher 必須失敗。

### 蕭中慧：夫妻刀法代打

Detailed：

```text
本次絕招攻擊
  對象：本次絕招原本選定的目標
  若另一名使用此武功的友軍存活：
    出手者：該名友軍
    攻擊：沿用主彈樣式，1枚，造成100%傷害
  否則：
    出手者：本次施法者
    攻擊：沿用副彈樣式，1枚，造成50%傷害
  共同限制：攻擊相同目標，且不觸發出手者的大招效果
  結算位置：加入本次基礎攻擊序列
```

Full：

```text
若另一名同武功友軍存活，該友軍會對本次絕招目標追加一枚100%傷害主彈；否則由施法者追加一枚50%傷害副彈；追加攻擊不觸發大招效果
```

Compact：

```text
同武功友軍存活：由該友軍對絕招目標追加100%主彈
否則：由施法者追加50%副彈（不觸發大招效果）
```

「同武功」必須來自關聯式 condition；不能在 renderer 中把 magic 62 翻成特殊文案。

### 虛竹：複製絕招攻擊

Detailed：

```text
施放大招
  候選：除施法者外，所有仍存活且有絕招攻擊定義的單位
  排除：會造成複製或借用遞迴的候選
  選擇：隨機1名；同順位隨機決定
  動作：複製該單位的絕招武功攻擊
  傳播：不複製該單位的大招規則
```

Full：

```text
隨機複製另一名存活單位的絕招攻擊，但不複製其大招效果
```

Compact：

```text
隨機複製另一名存活單位的絕招攻擊，不複製大招效果
```

### 歐陽鋒：毒爆生命週期

Detailed：

```text
施放大招
  主詞：本次施法者
  動作：獲得「毒爆」1層
  強度：星級×60
  上限：5層

施法者死亡
  條件：施法者至少有1層「毒爆」
  重複：依死亡時的毒爆層數逐層執行
  對象：施法者5格內所有敵人
  每層動作：
    1. 造成毒爆強度的100%純粹傷害
    2. 對受傷敵人施加中毒4層，強度為10，持續120幀，採取代政策
```

Full：

```text
獲得毒爆1層，強度為星級×60，最多5層
施法者死亡時，逐層引爆毒爆
  每層對5格內所有敵人造成毒爆強度的100%純粹傷害
  並施加中毒4層（強度為10，持續120幀）
```

Compact：

```text
獲得毒爆1層（強度為星級×60，最多5層）
施法者死亡：逐層引爆
  每層依毒爆強度傷害5格內所有敵軍並施加中毒4層（強度為10，120幀）
```

死亡主詞由事件 role 明確解析；毒爆上限與引爆次數則由同容器的 producer／consumer graph 串成同一生命週期。producer 的 `potency = 星級×60` 同時定義 consumer 的純粹傷害基準，因此必須提升到玩家投影；consumer 所施加中毒的 `potency = 10` 也必須與層數、持續時間一起顯示。兩者都是 typed producer／consumer 對應，不是為歐陽鋒手寫文案。

## API 與呼叫端

已刪除的舊 API 曾是：

```cpp
std::string effectDescription(
    const EffectRule& rule,
    EffectDescriptionStyle style,
    const EffectDescriptionContext& context);
```

現行唯一入口是兩階段 API：

```cpp
EffectDescriptionDocument buildEffectDescriptionDocument(
    const EffectDescriptionInput& input);

RenderedEffectDescription renderEffectDescription(
    const EffectDescriptionDocument& document,
    EffectDescriptionStyle style,
    const EffectDescriptionPresentationContext& context);
```

builder 產生與面板無關的 document；只有 renderer 按前述 matching-event 加 `BoundMagic` 契約消費 presentation context。`RenderedEffectDescription` 保留 section、block、row、row kind、indent 與 semantic break，不只是一個 `std::string`。catalog metadata 直接保存這個型別；JSON DTO 以同構結構輸出：

```text
description
  sections[]
    heading?
    blocks[]
      rows[]
        kind: field | list_item | heading | prose | summary
        text
        indent
        break_before: none | block | branch | sequence | action_group | qualifier
```

magic、裝備特殊效果、角色專屬裝備效果與羈絆 threshold 都走這個單一 structured contract，沒有平行的 `vector<string>` mirror。明確需要純文字的 adapter 才能在最外層呼叫 `effectDescriptionTextRows`／`joinEffectDescriptionRows`：目前限於 reward prose 與全武功 Markdown 審核報告。這些 adapter 依 section／block／row 順序及 indent 展平，不能讓其他 UI 再解析 join 後文字，也不能成為第二套 renderer。

所有 call site 在同一遷移中改用容器：

- `ChessMagicEffectDisplay` 一次傳入整個 `ChessMagicEffectDefinition`；
- catalog queries 依裝備、內功、羈絆 threshold 或 magic definition 建立 document；
- session adapter 與 JSON codec 消費 rendered blocks；JSON 保留 row kind、indent 與 semantic break；
- `ChessGameContent` 的內容指紋不得以 renderer 文字代替 typed rule serialization。

content fingerprint 已改為 canonical typed rule fingerprint，涵蓋 condition/action payload、activation limit、repetition formula 與所有 qualifier；Full／Compact 文字、magic 顯示名稱、purpose、release label、標點、換行及 wrapping 均不參與。canonical representation 切換刻意改變了一次 fingerprint，因此切換前建立的 replay 與 checkpoint 會因 content fingerprint mismatch 被拒絕；沒有舊 fingerprint 相容層。相同 typed semantics 的 content fingerprint 保持穩定，改標點或換行不會使正式內容不相容。

現行 API 不保留逐規則 wrapper。確實只含一條規則的容器仍使用同一個 document builder。

## UI 版面契約

call site 與 style 的映射是固定契約，不能由面板依當下字串長度臨時降級：

| 顯示位置 | Style 與 context | 版面責任 |
|---|---|---|
| 裝備詳細面板 | Full | 依 document block 分組並換行 |
| 內功詳細面板 | Full | 依 document block 分組並換行 |
| 完整羈絆瀏覽器 | Full | 依 threshold document 分組並換行 |
| 角色羈絆快速面板 | Compact | 依 container row 換行 |
| 角色大招效果列 | Compact，使用 Magic intrinsic `UltimateCommitted` context | 在大招武功標題下使用一個完整寬度欄，保留所有 continuation row 的縮排 |
| 棋子與升星獎勵中的角色大招預覽 | 與角色大招效果列相同 | 共用相同 row model 與可見 parent-child hierarchy，不另傳一份 context mirror |
| 玩家可見 catalog 與其他 reward metadata | Full | 優先輸出 structured blocks；受限 protocol 使用換行保留 block |
| CLI／診斷 catalog | Detailed，或明確提供三種 style | 不決定玩家面板預設值 |
| 效果詳細頁 | Detailed | 多行、可捲動並顯示全部 Audit facts |

UI 以 block 為排版單位，不以字元位置猜換行：

- 快速面板預設一個完整寬度的效果欄；空間足夠時最多兩個可讀欄，不能產生第三個狹窄欄；
- section heading、branch 與子 action 保留縮排，不把 wrapped continuation 誤認成新效果；
- 裝備、內功、完整羈絆與角色快速羈絆不分頁；先完整計算每一來源列換行後的物理高度，再在各面板的可讀範圍內選擇最大字體；
- 裝備與內功的 Full 下限是14px，完整羈絆 Full 下限是14px，角色快速羈絆 Compact 下限是12px；正式內容在下限仍放不下屬於內容／版面契約失敗，不能截斷、跳過列或退回舊描述；
- 角色快速羈絆只嘗試一欄或兩欄的完整配置，按最大共同字體、較少欄數、較平衡高度依序選擇；不能接受一個仍溢出的最終候選；
- Detailed 使用可捲動的詳細頁或 overlay，不要求塞進快速面板；
- 字體只在既定可讀範圍內調整，不能為容納任意長字串一路縮小；
- 任何 overflow 都不能重疊、越界或靜默消失。

角色大招與獎勵預覽保留兩個現有 concrete regression viewport：一般商店 `244×133`，升星獎勵 `196×133`。兩者都測試水平／垂直 bounds、武功數值欄分離、標題與子列的可見關聯、continuation indentation，以及所有 overflow 都能透過鍵盤／手把捲動到達。一般商店效果字體不得低於12px，升星獎勵不得低於10px；內容放不下時使用既有的 row scroll，不能分頁、截斷或再縮小。

非 magic 面板刻意不引入 page state。它們的正式 corpus test 使用 production 的 wrapping、indent、spacing 與一／兩欄選擇器，逐列證明：每個來源列至少產生一條物理列、每條物理列都在水平與垂直 bounds 內，而且所選字體不低於上述下限。magic 預覽保留的是既有連續 row scroll，不是 pagination。

目前正式 Normal corpus 的最壞實測結果如下；測試以比一般狀態更保守的快速羈絆組合（每個角色的每項羈絆都選物理高度最大的 threshold）計算：

| 面板 | Style | 偏好／下限 | corpus 中最小實際選擇 |
|---|---|---:|---:|
| 裝備詳細 | Full | 26／14px | 25px |
| 內功詳細 | Full | 24／14px | 24px |
| 完整羈絆 | Full | 24／14px | 15px |
| 角色快速羈絆 | Compact | 19／12px | 12px |

初始內容長度門檻：

| Style | 每個 rendered row 的上限 | 超限處理 |
|---|---:|---|
| Compact | 72 display units | 依 branch、sequence、action group 或 qualifier 前的 semantic break 拆列 |
| Full | 120 display units | 拆為同 block 的多個句子或子列 |
| Detailed | 不設內容上限 | 保留結構並由 UI wrap／scroll |

長度使用共用的 `displayTextWidth` 邏輯測試，不能用 `std::string::size()`。若一個不可再分的 semantic phrase 超限，內容測試應失敗並要求改善 phrase family 或 UI，不得截斷。

### Compact 最長列審核

最終正式 corpus 的 Compact 最大值是71 display units，低於72門檻。以下是完整掃描後最長的 distinct 正式列；長度測量的是 semantic row 本身，不含 UI continuation indentation：

| 長度 | 來源 | 內容 |
|---:|---|---|
| 71 | 裝備46「綠波香露刀」 | `對命中目標施加中毒3層，強度為4，只留最強，最多3層，90幀，同事件合計強度` |
| 71 | 毒宗2人 | `對命中目標施加中毒3層，強度為7，只留最強，最多3層，90幀，同事件合計強度` |
| 71 | 武功106「九陽神功」 | `回復效果持有者最大生命的3%＋60，直接治療、施加真氣，強度為9，可疊至10層` |
| 71 | 武功64「玄虛刀法」 | `對生命比例最低的3名友軍施加下一次攻擊落空，取代既有狀態，最多1層，120幀` |
| 70 | 左右互搏3人 | `出手：7幀後以60%傷害追擊最近的其他敵人；附近無其他敵人則追擊原攻擊目標` |
| 70 | 武功80「打狗棒法」 | `擊退4格，鎖1幀，遇障停止，受阻縮短、造成所有傷害-20%，最終，刷新，90幀` |
| 63 | 武功95「蛤蟆功」 | `每層依毒爆強度傷害5格內所有敵軍並施加中毒4層（強度為10，120幀）` |

被本次語意校正拆開的兩個案例固定為以下 deliberate rows，不允許重新壓回單列：

```text
出手：7幀後以60%傷害追擊最近的其他敵人；附近無其他敵人則追擊原攻擊目標  [70]
  追擊出手時：80%機率獲得1次傷害抵擋（最多1次）  [45]

對生命比例最低的5名友軍施加單次承傷上限，強度為目標最大生命的15%  [64]
  取代既有狀態，最多1層  [21]
```

### Compact 用詞變更審核表

下表是本次 cutover 與後續語意校正對 Compact 玩家文字所做的完整人類審核清單。文法與指涉修正也套用到 Full 的完整句型；純 Compact 縮寫則不改變 Full。

| typed 情境 | 先前 Compact 文字 | 現行 Compact 文字 |
|---|---|---|
| 主彈事件目標 | `被主彈命中的敵人` | `主彈目標` |
| 一般命中目標 | `被命中的單位` | `命中目標` |
| 治療事件目標 | `受到治療的單位` | `治療目標` |
| 其他事件目標 | `該次事件的目標` | `受影響單位` |
| 持有者生命條件 | `效果持有者生命…` | `持有者生命…` |
| 最後存活條件 | `效果持有者為最後存活單位` | `持有者為最後存活單位` |
| 持有者狀態條件 | `效果持有者有…` | `持有者有…` |
| 目標狀態條件 | `事件目標有…` | `目標有…` |
| 持有者施加的目標狀態 | `事件目標有本次施法者施加的…` | `目標有持有者施加的…` |
| 持有者層數條件 | `效果持有者的…至少…層` | `持有者…至少…層` |
| 傷害來源條件 | `傷害來自招式`（本次校正前曾縮成 `招式傷害`） | `為招式傷害` |
| 擊殺條件 | `該次傷害造成死亡` | `傷害致死` |
| 造成傷害視角 | `效果持有者造成的傷害` | `持有者造成傷害` |
| 承受傷害視角 | `效果持有者承受的傷害` | `持有者承受傷害` |
| 正傷害與造成視角並存 | `持有者造成傷害且正傷害` | `持有者造成正傷害` |
| 正傷害與承受視角並存 | `持有者承受傷害且正傷害` | `持有者承受正傷害` |
| 絕招條件命題 | `僅限絕招` | `為絕招` |
| 主彈道條件命題 | `僅限主彈道` | `為主彈道` |
| 根攻擊條件命題 | `僅限根攻擊` | `為根攻擊` |
| 攻擊序號條件命題 | `第N道攻擊` | `為第N道攻擊` |
| 施放武功與效果來源的關係 | 省略條件，或顯示 `武功N`／`武功相符N` | `施放武功為效果來源` |
| 常數百分比加算 | `百分比加算 48%` 類文字 | `+48%` 類帶符號記法 |
| 公式百分比加算 | `百分比加算` | `加` |
| 乘算 | `乘以` | `×` |
| 每次施放的同目標上限 | `每次施放對同一目標最多命中N次` | `每施放同目標最多N次` |
| 必要的存活事件來源 | `必含存活事件來源` | `含存活事件來源` |
| 預設攻擊樣式 | `沿用彈道樣式` | `原彈道` |
| 只傳播來源命中 | `僅觸發來源命中規則` | `僅來源命中` |
| 借用大招傳播 | `觸發借用的大招規則` | `觸發借用大招效果` |
| 不傳播效果 | `不觸發任何效果規則` | `不觸發效果` |
| 加入基礎攻擊 | `作為追加攻擊` | `追加攻擊` |
| 最低生命友軍 selector | `生命比例最低N人` | `生命比例最低的N名友軍` |
| 最低內力友軍 selector | `內力最低N人` | `內力最低的N名友軍` |
| 效果持有者最大生命公式 | `效果持有者N%最大生命` | `效果持有者最大生命的N%` |
| 目標最大生命公式 | `目標N%最大生命` | `目標最大生命的N%` |
| 狀態主要強度 | `強度X` | `強度為X` |
| 狀態次要強度 | `次要強度X` | `次要強度為X` |
| 狀態取代政策 | `取代` | `取代既有狀態` |
| 泛型效果取代政策 | `取代` | `取代既有效果` |
| 保留最強政策 | `取強` | `只留最強` |
| 增加層數及其上限 | `疊層×N層` 或 `×N層` | `可疊至N層` |
| 非增加層數的上限 | `×N層` | `最多N層` |
| 武學類別 selector | `友方武器N`／`武器類型N` | `友方拳掌角色`、`友方御劍角色`、`友方耍刀角色`、`友方特殊角色` |
| 目前護盾／冷卻數值公式 | `目標目前護盾的N%護盾`／`目標目前冷卻的N%目前冷卻`；反轉乘數時也可能重複資源名 | primary／multiplier 任一側已帶資源名時均只保留 `目標目前護盾的N%`／`目標目前冷卻的N%` 等公式本體 |
| runtime-only 彈射 | `原彈道主彈×1，100%傷害，彈射追加命中N次，P%，R像素` | `命中時有P%機率在R像素內彈射，最多追加命中N次` |
| runtime-only 附近追蹤彈 | `原彈道主彈×1，100%傷害，R像素內產生P%傷害追蹤彈` | `命中後在R像素內產生P%傷害追蹤彈` |
| 延遲追擊的目標與 fallback | `原彈道主彈×1，100%傷害，延遲N幀追擊替代目標，P%傷害` | `N幀後以P%傷害追擊最近的其他敵人；附近無其他敵人則追擊原攻擊目標` |
| 延遲追擊取得傷害抵擋 | `Q%獲得格擋` | `追擊出手時：Q%機率獲得1次傷害抵擋（最多1次）` |
| runtime-only 攻擊的 schema defaults | `原彈道主彈×1，100%傷害` | 玩家列省略；Detailed 稽核仍逐欄保留 schema 預設值 |
| Magic 綁定武功的預設大招事件 | `絕招：…` 或 `施放大招：…` | 省略重複前綴；非相符 event、`OwnerAnyCast` 與 Detailed 仍顯示 |
| 毒爆 producer 強度 | `獲得毒爆1層（最多5層）` | `獲得毒爆1層（強度為星級×60，最多5層）` |
| 毒爆施加中毒強度 | `施加中毒4層（120幀）` | `施加中毒4層（強度為10，120幀）` |
| 像素強制移動 | `擊退N像素並鎖定M幀` | `擊退N像素，鎖M幀` |
| 棋格強制移動 | `擊退N格並鎖定M幀` | `擊退N格，鎖M幀` |
| 佔位 collision | `在佔位前停止` | `遇佔位停止` |
| 地形 collision | `在阻擋地形前停止` | `遇障停止` |
| 受阻縮短 | `受阻時縮短位移` | `受阻縮短` |
| 受阻取消 | `受阻時取消位移` | `受阻取消` |
| 區域 outgoing all-damage channel | `所有傷害` | `全傷` |

另有五項結構化／schema 變更，不是 Compact 詞彙替換：可在72 units 內完整容納的同時 leaf actions 直接以 `、` 合併成一列，不再先輸出獨立的 `同時發生：` 標頭；semantic wrapping 後的列不殘留 `，` 或 `；`；多動作的 trigger／target lead 現在保留為帶 `：` 的 semantic heading，使下一列的縮排關係明確；Detailed、Full 與 Compact 的所有列都恢復 punctuation-neutral 契約，不再追加 terminal `。`；authoring label `指定武器友軍` 硬切換為 `指定武學類別友軍`。terminal punctuation 一項明確撤回中間版本「Full prose row 固定以 `。` 結尾」的改動；authoring label 一項是 typed schema 遷移，玩家 Compact 文字則使用上表的具名武學類別。

Full 的傷害視角也由名詞片段 `效果持有者造成的傷害`／`效果持有者承受的傷害` 改為條件命題 `效果持有者造成傷害`／`效果持有者承受傷害`，避免與下一條件串成 `……的傷害且傷害來自招式`。這不改變 Compact 用詞。

StatusLifecycle 的 Full source phrase 另由 `由施法者施加七星` 改成 `由效果持有者施加七星`。這不是 Compact 詞彙縮寫，而是修正 `OwnerTeamEventSource` 下效果持有者與目前出手友軍可能不同的 typed role identity；Compact 仍使用上表已審核的「持有者施加」。

`刷新` 在校正後的最終 Compact 用詞中刻意維持不變：它已有清楚的玩家語意，而且擴寫後會讓已驗證的角色快速羈絆最壞組合增加物理列。這不是保留舊 renderer；同一個 typed merge policy 仍只經新的 phrase family 輸出。

## 測試契約

### Lossless 與欄位覆蓋

- 對每種 condition/action payload 逐欄位 mutation；每個 execution-relevant 欄位必須改變 Detailed 文件或 audit trace。
- activation limit、chance、cooldown、interval、every-N、repetition formula、duration、stack policy、scope 與 propagation 都納入欄位測試。
- 每個來源欄位必須有唯一 `DescriptionFieldDisposition`；沒有 coverage、重複消耗或非預設值誤判為 default 都失敗。
- Detailed 測試只消費 document facts／provenance；不得以另一個 AST walker 才補齊欄位。

### Role 與主詞

- 每種死亡、命中、傷害、治療與施放事件測試明確主詞。
- player-facing 輸出不得出現裸 `死亡`、含糊 `原目標`、`事件目標` 或無先行詞的 `自身`。
- 本次施法者、效果持有者、代打友軍與被命中目標不同時，golden 必須能區分。
- `BoundMagic` 與 `OwnerAnyCast` 分別測試事件施法者、效果持有者和 cast-match wording；role resolution 失敗必須 assert，不能進入 generic fallback。
- Magic intrinsic `UltimateCommitted` default、明確 matching `BoundMagic` enclosing default、non-matching event 與 `OwnerAnyCast` context 各有測試。只有 matching `BoundMagic` trigger group 可省略 event；Detailed 永不省略，cross-rule section 的其他 event 也不受影響。

### Graph 連線

- 所有 status-reference YAML label 在 parse time 轉為 `BattleStatusKind`；未知 label 失敗，runtime、formula、condition、action 與 graph 使用相同 enum identity。
- status kind、state slot、source scope、owner binding 與 target domain 各有 positive／negative 配對測試。
- 同顯示名但不同 identity 不連線；同 identity 但不同來源限制不連線。
- 一個 consumer 有零個、一個與多個 producer 的行為都要明確測試。
- rule 順序與 action 順序在歸約後仍可由 audit trace 還原。

### Archetype 安全性

- 每個 archetype 對所有 payload field 做 mutation test，證明該欄位會顯示、被明確吸收，或使 matcher 拒絕。
- action allowlist、propagation、selector、branch condition 或 source policy 增加新 enum value 時，既有 matcher 預設拒絕。
- Full 可見 Audit fact 必須以 closed `DescriptionPlayerFact` 逐一出現在 descriptor 的 Full Audit 集合；Compact 可見 Decision fact 必須來自全域 mandatory set 或 descriptor 的 named projection。generic 與 specialized renderer 都只能消費 projected view；未宣告的 renderer 判斷使測試失敗。
- trigger／condition coalescing 或省略必須帶 closed `DescriptionPhraseAbsorption`；把 absorption 改回 `None` 應使該 projected fact 重新出現在玩家文字，不能仍被 renderer-local variant 分支隱藏。
- 屬性 duration promotion 有完整 positive／negative matrix：相鄰、target、variant、operation、constant sign、duration、stack、cap、perStack、scope、per-action condition 與各種 structural boundary 均各自變異。只有符合全部條件的 Full／Compact 提升 duration，Detailed 永不合併。
- 相同 typed shape 換成不同效果 ID、武功 ID、角色名或容器名，除 localization label 外應產生相同 document shape 與文字。
- renderer 或 matcher 不得含 production magic ID、role ID、item ID 或 combo ID 分支。

### Golden 與內容測試

- 慕容復、蕭中慧、譚處端、虛竹與歐陽鋒三種 style 的 golden tests。
- 全部正式 `config/chess_*.yaml` 建立 document，無 coverage error。
- generic fallback shape 使用明確 allowlist／snapshot；未審核的新 shape 使測試失敗。
- Compact 每列最多72 display units，Full 每列最多120；超限不能靠 truncation 通過。
- Full／Compact 禁止出現 action allowlist dump、`條件[`、propagation enum 名、`追加至基礎攻擊`、`原樣式`、`替代目標`、`強度目標`、`取代×`、`取強×`、`若僅限`、`若第`、`的傷害且`、數字 magic ID 或數字武學類別 code；所有 description row 禁止使用 `→`、全形 `／`、ASCII `/` 或 `·` 作結構分隔。含 `×` 的 golden／mutation test 必須證明它來自 typed arithmetic expression，而不是 renderer delimiter；stack limit 不得使用乘號記法。
- runtime-only attack behavior 的普通攻擊欄位若全為 schema default，Full／Compact 只描述實際 runtime behavior；Detailed 仍保留每個 default 的 audit row。普通攻擊欄位與 runtime behavior 的合法組合由同一 typed semantic predicate 判定，validation 與 renderer 不各自維護 default mirror。
- 資源變更的公式若在 primary base 或 multiplier base 帶有 HP、目前護盾或目前冷卻資源名，Full／Compact 都不得再次附加 resource noun；正向與反轉公式各有 regression。
- Detailed、Full 與 Compact row 均不以 `。` 結尾；semantic heading 保留 parent-child 關係並可以 `：` 結尾。外層 join 不得產生 `。；`、`：。` 或把 target heading 變成假句子。
- 驗證繁體中文詞彙，不新增簡體機制標籤。

### Runtime 與 UI

- 同一份 parsed rules 在描述遷移前後執行相同 scenario，battle event／replay digest 完全相同。
- 測試明確接受 migration build 的 content fingerprint 與舊 build 不同，且舊 replay／checkpoint 因 mismatch 被拒絕；遷移後的 canonical typed fingerprint 覆蓋所有正式欄位，且不受文案、標點或換行影響。
- UI 測試 section grouping、縮排、wrap、單欄／雙欄、non-overlap、非 magic corpus fit 與 magic row scroll；`244×133` 與 `196×133` 另測12px／10px下限、數值欄分離及所有 overflow 可到達。
- call-site mapping 測試裝備／內功／完整羈絆使用 Full、角色快速羈絆使用 Compact、角色大招與同型獎勵預覽使用 Magic intrinsic `UltimateCommitted` context 的 Compact，Detailed 只由詳細／診斷入口選用。
- JSON 與 catalog call site 測試 block 順序及 style，不再假設一條 rule 對應一個字串。

## 歷史實作依賴順序

下列編號記錄開發時的依賴分解，不是可合併、可發布或可長期停留的遷移階段。中間比較只在同一工作樹內作 characterization；最終切換一次完成，production 只保留新的 typed parser、document builder、renderer 與 structured UI consumption。舊 `ChessBattleEffects`、逐規則 API、雙 parser／renderer、facade、alias、鏡像 AST、feature flag 及 bridge 均未保留。

1. **盤點與 characterization**：列出所有正式容器、typed shape、目前 fallback 候選與最長列；固定五個案例的輸入 fixture 與預期資訊，但尚不要求新 renderer golden 通過。
2. **Typed semantic 前置遷移**：把所有 status reference canonicalize 為 `BattleStatusKind`；把 `使用武功: 62` 等實際代表「此武功」的設定遷移為 typed relational condition。解析器、schema、runtime、正式 YAML 與 structural equality tests 一次更新，不保留舊語法。同階段建立完整 canonical typed fingerprint，接受一次性 fingerprint 切換並固定舊 replay／checkpoint 拒絕測試。
3. **Document core**：建立 typed fact、provenance、coverage disposition、role resolver、presentation context 與 lossless Detailed；先證明所有欄位可追蹤，且 role failure 會 fail-fast。
4. **Lifecycle graph**：在 canonical identity 上加入 status／state slot producer-consumer graph，以及正反連線測試；完成七星、毒爆與 record／consume 類 shape。
5. **Archetype library**：依正式內容頻率實作 reusable matcher，包括屬性 duration promotion 的完整十項邊界。每加入一個 archetype，同時加入 field mutation、Audit promotion 與 identity-independence 測試。
6. **Full／Compact projection**：從同一 document 產生玩家文字、terminal punctuation、semantic breaks 與長度測試；移除 player-facing `→` 和實作詞彙。此時加入五個案例的三種 style golden；蕭中慧的「同武功」已由階段2的 relational condition 提供。
7. **Call-site 與 UI 遷移**：依固定 panel-to-style mapping 將 catalog、session、JSON、magic display 與其他面板改為容器和 blocks；完成 enclosing-default、非 magic calculated fit、magic row scroll、兩個 concrete viewport 與詳細入口。
8. **單次切換與清理**：移除舊 `effectDescription(EffectRule, ...)`、`ChessBattleEffects` monolith、逐規則 golden、legacy punctuation 與重複 formatter；把 [效果描述語法設計](效果描述語法設計.md) 的狀態標頭改指向本文件，並把三層文件標記為已由本文件取代，維持可追蹤的 supersession chain；最後執行完整 build、schema 驗證、單元測試與正式內容掃描。

characterization 比較未形成 production bridge；切換後只有一套 document pipeline，舊 renderer 與 compatibility fallback 已刪除。

## 驗收條件

完成實作必須同時滿足：

1. 所有效果描述由完整容器建立，七星與毒爆能顯示施加、累積、消耗或引爆的完整生命週期。
2. Detailed 以多行結構完整涵蓋每個正式欄位，所有欄位都有 provenance 與 disposition。
3. Full 與 Compact 都有明確主詞和對象，不出現含糊的 `死亡`、`原目標`、`事件目標` 或無先行詞的 `自身`。
4. Compact 保留「最後一層」等會改變結果的條件，不使用 `→` 表示因果或順序。
5. 五個代表案例符合本文件的資訊與結構，且不是以效果 ID 或顯示名稱特判。
6. 每個 archetype 遇到未宣告的非預設欄位都拒絕匹配，沒有靜默省略。
7. 正式內容沒有 coverage error；新增 generic fallback shape 會被內容測試攔截。
8. Compact 與 Full 符合72／120 display-unit 單列門檻，超限以 semantic block 拆分而非截斷或刪條件。
9. Magic intrinsic enclosing default 只省略 event 相符之 `BoundMagic` trigger group；Detailed、其他 event 與 `OwnerAnyCast` 不省略，且每個省略都有武功容器或可見的 UI parent-child hierarchy。
10. 屬性共同 duration 只在完整十項 eligibility boundary 內提升；cap、counter、stack identity 與其他 qualifier 不會被提升或誤認為共用。
11. panel-to-style mapping 固定；所有非 magic 正式內容經 production wrapping、indent、spacing 與最多兩欄的 calculated fit 後，每一來源列都有界內的物理列，裝備／內功／完整羈絆／快速羈絆字體分別不低於14／14／14／12px，且不分頁、不截斷。`244×133` 與 `196×133` magic viewport 則在12px／10px字體下維持正確分組、無重疊與 accessible row-scroll overflow。
12. Full 的 Audit promotion 與 Compact 的 Decision projection 都由 closed `DescriptionPlayerFact` descriptor／全域集合宣告，所有省略與合併由 `DescriptionPhraseAbsorption` 宣告，不含 renderer 的主觀逐案例判斷；generic 與 specialized renderer 消費同一 projected view，三種 row kind 符合 terminal punctuation 契約。
13. runtime 行為與 battle event／replay digest 不變。canonical typed representation 上線時 content fingerprint 刻意改變一次，遷移前 replay／checkpoint 因 mismatch 失效；其後 fingerprint 只由完整 typed semantics 決定，不受描述文案影響。
14. 所有 status identity 已在 parse time canonicalize，graph 不比較 localization string；「此武功」「施放武功為效果來源」等關係也以 typed condition 表達，武學類別以 closed enum 表達；玩家列不洩漏數字 magic ID 或 category code。
15. 所有 call site 已遷移，舊逐規則字串 API 與 renderer 已刪除，沒有相容層或重複邏輯，文件 supersession chain 已更新。
