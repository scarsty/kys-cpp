# 效果描述語意文件設計

狀態：提案，尚未實作。

本文件規劃下一代效果描述架構。實作完成後，將取代 [Effect Description Three-Tier Rendering and Mechanical Coalescing Design](effect-description-three-tier-rendering-design.md) 的 presentation、renderer、API 與版面契約；在完成遷移前，該文件仍代表目前已實作的行為。

[效果描述語法設計](效果描述語法設計.md) 所定義的 typed payload、共用 semantic AST、禁止手寫機制描述，以及執行器與描述器共用同一份解析結果等原則繼續有效。本設計不回到每項武功手寫說明，而是把自動描述的輸入粒度從單一規則提升為整個效果容器，先理解角色、跨規則生命週期與常見機制，再產生結構化文件。

## 問題與證據

目前 `effectDescription(const EffectRule&, ...)` 一次只看一條規則，並把 semantic AST 壓成單一字串。即使 `Detailed／Full／Compact` 有不同可見性政策，三者本質上仍接近 AST 的縮寫序列化，而不是玩家可以閱讀的機制說明。

目前最長的幾條 Compact 輸出如下。display unit 依現有 `displayTextWidth` 的概念計算，中文字約 2、ASCII 約 1：

| 長度 | 內容 | 現有輸出 |
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
- 不要求第一版產生文學化文案；清楚、無歧義且可驗證的結構化中文優先。

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
lossless semantic AST
  ↓
主詞、事件參與者與目標角色解析
  ↓
容器內 status／state producer-consumer graph
  ↓
typed mechanic archetype 歸約
  ↓
Core／Decision／Audit facts
  ↓
EffectDescriptionDocument
  ↓
Detailed／Full／Compact 結構化輸出
```

每一步只讀取上一層的 typed 結果。禁止 renderer 查詢效果 ID、角色名稱或已產生的句子來決定機制。

## 結構化文件模型

下列型別只規範責任與資料流，實作時可依現有 variant 與 header 邊界調整名稱：

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
    std::string_view displayName{};
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

struct EffectDescriptionFact
{
    DescriptionFactValue value; // closed typed variant
    DescriptionFactLevel level{};
    std::vector<DescriptionSourceField> sources;
};

struct EffectDescriptionBlock
{
    DescriptionArchetype archetype{};
    DescriptionSubject subject{};
    std::vector<EffectDescriptionFact> trigger;
    std::vector<EffectDescriptionFact> conditions;
    std::vector<EffectDescriptionFact> targets;
    std::vector<DescriptionActionGroup> actions;
    std::vector<DescriptionBranch> branches;
    DescriptionCoverage coverage;
};

struct EffectDescriptionSection
{
    std::optional<DescriptionHeading> heading;
    std::vector<EffectDescriptionBlock> blocks;
};

struct EffectDescriptionDocument
{
    std::vector<EffectDescriptionSection> sections;
    DescriptionCoverage coverage;
};
```

`DescriptionFactValue`、`DescriptionSubject`、`DescriptionActionGroup` 與 `DescriptionBranch` 必須是 closed typed variants，不是預先組好的中文。中文只在最後 renderer 產生。狀態顯示名「七星」「毒爆」可作為 localization label，但層數、強度、來源、持續時間與耗盡行為仍來自 typed payload。

`displayName` 只供文件標題與 localization label 使用，不得參與 role resolution、graph 連線、archetype matching 或 content fingerprint。它是非 owning `std::string_view`；呼叫端必須讓 backing string 至少存活至 `buildEffectDescriptionDocument` 返回，而 document 若要保留名稱，必須複製成自有字串。

每個 fact 保存 `DescriptionSourceField`，至少可追溯至 rule ID、condition/action variant 與欄位路徑。這份 provenance 供欄位覆蓋測試與診斷使用，不需要直接顯示給玩家。

文件是唯一的描述中間表示。三種 style 不得各自重新分析原始 `EffectRule`，也不得各自維護一份 mechanic matcher。Detailed 也只能從 document facts 與 provenance 產生；它不能為了 lossless 另行走訪舊 semantic AST，否則會形成第二套 formatter。

### 呈現 context 與 enclosing default

`EffectDescriptionDocument` 本身不含面板 context，同一份 document 必須能在 standalone 與巢狀 UI 重用。`EffectDescriptionPresentationContext` 只傳給最後的 renderer，而且只能影響重複 trigger 文字；不能改變 facts、graph、archetype matching 或 coverage。

enclosing-default 的省略規則如下：

1. Detailed 永遠顯示每個來源規則的 event 與 `EffectCastMatch`，不受 context 影響。
2. Full 與 Compact 只有在 `context.enclosingDefaultEvent == sourceRule.event` 且該來源規則的 `castMatch == EffectCastMatch::BoundMagic` 時，才能省略該 trigger group 的最外層 event 詞。
3. 容器內若有多個 event section 或一個 cross-rule block 含多個 trigger group，逐一按其來源規則判斷；context 不能一次隱藏整個 section。`MainProjectileBeforeDamage`、`CastSettled`、`UltimateCooldownFinished` 等不相符 event 仍必須顯示。
4. `EffectCastMatch::OwnerAnyCast` 永遠保留 event 與「任意施放」語意。context 也不能省略 condition、cast-match wording、propagation、duration、action qualifier 或 branch。
5. 呼叫端只能在 typed row model 把 block 與 enclosing source 關聯，而且 UI 以標題、縮排等可見 parent-child hierarchy 表達該關聯時設定 context。只因 call site、顏色、magic ID 或資料來源相同，不足以省略 event。

例如角色大招列可在「羅漢伏魔功」標題下，對一個 `BoundMagic` 的 `UltimateCommitted` trigger 省略重複的「施放大招」；同容器的主彈命中、施放結算或 `OwnerAnyCast` 規則仍保留各自主詞與觸發詞。換行後的 continuation row 必須保持縮排，不能與標題失去視覺關聯。

## 語意角色與明確主詞

描述器必須先把 selector 與事件中的相對引用解析成 typed semantic role。第一版至少包含：

- 效果持有者：裝備、內功、羈絆或大招規則所綁定的單位。
- 本次施法者：觸發目前施放事件的單位；通常但不保證等於效果持有者。
- 事件來源：造成命中、傷害、治療或死亡事件的單位。
- 事件目標：事件所作用的單位。
- 本次攻擊目標：該次基礎攻擊或絕招原本選定的目標。
- 被命中單位：實際接受該枚彈道或該次命中的單位。
- 被選單位：selector 在目前 scope 中選出的友軍或敵軍。
- 死亡單位：死亡事件所指的單位。
- 狀態施加者：建立特定 status instance 的來源單位。

role resolver 必須同時讀取 `EffectObservationScope` 與 `EffectCastMatch`。`BoundMagic` 表示目前規則綁定本容器的武功；`OwnerAnyCast` 表示效果持有者的任意施放也可能觸發，不能假定事件施法者、效果持有者與被借用／代打的出手者相同。cast match 本身是 Detailed 的 Audit fact；非預設 `OwnerAnyCast` 同時產生 Full／Compact 可見的 Decision fact。

renderer 根據 role 與事件上下文產生完整關係詞，例如：

| 不可單獨輸出 | 必須解析成類似文字 |
|---|---|
| `死亡` | `施法者死亡時`、`任一友軍死亡時`、`帶有此效果的敵人死亡時` |
| `原目標` | `本次絕招原本選定的目標`、`本次基礎攻擊的目標` |
| `自身` | `效果持有者`、`施法者`、`造成該次命中的友軍` |
| `事件目標` | `被命中的敵人`、`受到治療的友軍`、`死亡的友軍` |

同一個 runtime selector 在不同事件中可以有不同玩家措辭，但必須先解析成同一個 typed role，再由 phrase table 選詞。不得在 action formatter 看到 `Self` 便直接輸出「自身」。

role resolution failure 與 archetype mismatch 是不同錯誤。通過 parser／validator 的正式規則必須能解析唯一主詞與對象；無法解析代表 semantic AST 或 resolver 違反 invariant，document builder 應 fail-fast／assert，測試必須直接失敗。generic fallback 仍需要 resolved role，因此只能承接 archetype mismatch，不能承接 role failure。

## 容器內狀態與 state slot 生命週期

### Identity canonicalization 前置條件

graph 不得直接以 localization label 或 runtime string 比較 identity。第一版實作 graph 前，必須先把所有正式狀態引用在 parse time canonicalize 為 `BattleStatusKind`：

- `ApplyStatusAction`、`ConsumeStatusAction` 與 depleted action 已使用 `BattleStatusKind`，維持不變；
- `EffectNumber::status`、`SourceHasStateCondition::state`、`TargetHasStateCondition::state`、`TargetHasStateFromEffectOwnerCondition::state` 與 `SourceStackAtLeastCondition::stack` 應由字串遷移為 `BattleStatusKind`；
- YAML 的繁體中文 label 只在 parser 邊界透過同一個 enum descriptor 解析一次；`battleStatusLabel` 只負責顯示，不能再作 runtime 或 graph key；
- `EffectStateSlot` 已是 typed identity，維持 enum 比較；未來其他具名 state 若不是 `BattleStatusKind`，必須建立自己的 enum 或 stable typed ID，不能復用 localization string。

目前 label 與 `BattleStatusKind` 的映射雖為雙射，這只足以安全遷移，不代表 graph 可以繼續比字串。canonicalization 後，狀態的 runtime 查詢、數值公式、condition、action 與 graph edge 都使用同一 enum；來源限制與目標 domain 仍是 identity 之外的獨立相容條件。

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

Detailed 顯示 Core、Decision 與 Audit。Full 預設只顯示全部 Core 與 Decision；若某個 Audit fact 對特定 phrase family 不可缺少，archetype descriptor 必須逐一以 typed fact 名稱宣告 `PromotedToFull`、說明原因並加入 golden／mutation test，不能由 renderer 臨場判斷「理解機制所需」。Compact 不顯示 Audit，只顯示全部 Core，以及全域 mandatory set 或 archetype descriptor 的 Compact projection 逐一命名的 Decision facts；mandatory set 至少包含分支、耗盡、上限、持續時間、間隔、機率、範圍、來源限制與選擇數量，不存在開放式的「重要例外」判斷。

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

archetype 是 reusable typed pattern，不是效果專用模板。第一版應至少支援：

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

`其他存活友軍使用武功: 62` 的玩家語意其實是「另一名同武功友軍存活」。描述器不應查詢 magic 62 的名稱，也不應對 62 特判。

正式設定應遷移為能表達關係的 typed condition，例如：

```yaml
條件:
  - 其他存活友軍使用此武功
```

對應的 C++ condition 表示「與目前效果容器綁定同一武功」，讓 runtime 與描述器共用相同語意。凡是人類語意是「此武功」「目前羈絆」「此效果來源」的欄位，都應優先在 schema 中表達關係，不把數字 ID 留給 renderer 猜測。

## 結構化 generic fallback

無法匹配高階 archetype 不代表退回目前的 `·` 串接。fallback 仍從 role-resolved semantic AST 建立結構化 block：

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
- 不設單列長度上限，UI 應換行、捲動或分頁。

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

terminal punctuation 由 renderer 與 row kind 決定：Full 的 prose row 是完整句子，固定以一個 `。` 結尾；section heading 不加標點。Detailed 的 field／list row 與 Compact summary row 都不加 terminal punctuation。外層 UI 不得再追加、移除或猜測標點；被迫輸出單一字串的 protocol adapter 應按 block 使用換行，不能把已帶 `。` 的 Full row 再用 `；` 串成 `。；`。這項政策刻意取代現行三層文件的「所有 fragment 都不加句號」契約。

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
主彈命中時，對被命中的敵人施加七星7層，最多7層，持續150幀。
任一友軍命中由施法者施加七星的敵人時，該次招式忽略50%防禦並消耗1層；最後一層消耗時，使該敵人眩暈30幀。
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
準備施放大招時，依星級隨機借用1～2名敵人的大招效果；不會遞迴借用或複製。
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
若另一名同武功友軍存活，該友軍會對本次絕招目標追加一枚100%傷害主彈；否則由施法者追加一枚50%傷害副彈。追加攻擊不觸發大招效果。
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
施放大招時，隨機複製另一名存活單位的絕招攻擊，但不複製其大招效果。
```

Compact：

```text
大招：隨機複製另一名存活單位的絕招攻擊，不複製大招效果
```

### 歐陽鋒：毒爆生命週期

Detailed：

```text
施放大招
  主詞：本次施法者
  動作：獲得「毒爆」1層
  上限：5層

施法者死亡
  條件：施法者至少有1層「毒爆」
  重複：依死亡時的毒爆層數逐層執行
  對象：施法者5格內所有敵人
  每層動作：
    1. 造成毒爆強度100%的純粹傷害
    2. 對受傷敵人施加中毒4層，強度10，持續120幀，採取代政策
```

Full：

```text
施放大招時獲得1層毒爆，最多5層。
施法者死亡時逐層引爆毒爆；每層對5格內所有敵人造成純粹傷害，並施加4層中毒（強度10，持續120幀）。
```

Compact：

```text
大招：獲得毒爆1層（最多5層）
施法者死亡：逐層引爆；每層傷害5格內敵人並施加中毒4層（120幀）
```

死亡主詞由事件 role 明確解析；毒爆上限與引爆次數則由同容器的 producer／consumer graph 串成同一生命週期。

## API 與呼叫端遷移

舊 API：

```cpp
std::string effectDescription(
    const EffectRule& rule,
    EffectDescriptionStyle style,
    const EffectDescriptionContext& context);
```

應移除並改為兩階段 API：

```cpp
EffectDescriptionDocument buildEffectDescriptionDocument(
    const EffectDescriptionInput& input);

RenderedEffectDescription renderEffectDescription(
    const EffectDescriptionDocument& document,
    EffectDescriptionStyle style,
    const EffectDescriptionPresentationContext& context);
```

builder 產生與面板無關的 document；只有 renderer 按前述 matching-event 加 `BoundMagic` 契約消費 presentation context。`RenderedEffectDescription` 至少保留 section、block、row、indent 與 semantic break，不只是一個 `std::string`。需要 JSON 的呼叫端應輸出結構化 blocks；暫時只支援文字的外部協定，可在協定邊界按 block 使用換行，但不得讓其他 UI 再解析 join 後文字。

所有 call site 在同一遷移中改用容器：

- `ChessMagicEffectDisplay` 一次傳入整個 `ChessMagicEffectDefinition`；
- catalog queries 依裝備、內功、羈絆 threshold 或 magic definition 建立 document；
- session adapter 與 JSON codec 消費 rendered blocks；
- `ChessGameContent` 的內容指紋不得以 renderer 文字代替 typed rule serialization。

目前 content fingerprint 的 view 包含 Full 與 Compact 文字。新設計會使文案調整不再代表玩法變更，因此實作前必須補齊 canonical typed rule fingerprint，涵蓋 condition/action payload、activation limit、repetition formula 與所有 qualifier，再移除描述字串。這次 canonical representation 置換會刻意改變 fingerprint 值，因此遷移前建立的 replay 與 checkpoint 會因 content fingerprint mismatch 被拒絕；本設計不建立舊 fingerprint 相容層。遷移完成後，相同 typed AST 的 content fingerprint 應保持穩定，改標點或換行不得再次使正式內容不相容。

遷移完成後不保留逐規則 wrapper。確實只含一條規則的容器仍使用同一個 document builder。

## UI 版面契約

call site 與 style 的映射是固定契約，不能由面板依當下字串長度臨時降級：

| 顯示位置 | Style 與 context | 版面責任 |
|---|---|---|
| 裝備詳細面板 | Full | 依 document block 分組並換行 |
| 內功詳細面板 | Full | 依 document block 分組並換行 |
| 完整羈絆瀏覽器 | Full | 依 threshold document 分組並換行 |
| 角色羈絆快速面板 | Compact | 依 container row 換行 |
| 角色大招效果列 | Compact，`enclosingDefaultEvent = UltimateCommitted` | 在大招武功標題下使用一個完整寬度欄，保留所有 continuation row 的縮排 |
| 棋子與升星獎勵中的角色大招預覽 | 與角色大招效果列相同 | 共用相同 row model、context 與可見 parent-child hierarchy |
| 玩家可見 catalog 與其他 reward metadata | Full | 優先輸出 structured blocks；受限 protocol 使用換行保留 block |
| CLI／診斷 catalog | Detailed，或明確提供三種 style | 不決定玩家面板預設值 |
| 效果詳細頁 | Detailed | 多行、可捲動／分頁並顯示全部 Audit facts |

UI 以 block 為排版單位，不以字元位置猜換行：

- 快速面板預設一個完整寬度的效果欄；空間足夠時最多兩個可讀欄，不能產生第三個狹窄欄；
- section heading、branch 與子 action 保留縮排，不把 wrapped continuation 誤認成新效果；
- Full 與 Compact 超出可用高度時使用捲動、分頁或明確的詳細入口；
- Detailed 使用可捲動的詳細頁或 overlay，不要求塞進快速面板；
- 字體只在既定可讀範圍內調整，不能為容納任意長字串一路縮小；
- 任何 overflow 都不能重疊、越界或靜默消失。

角色大招與獎勵預覽保留兩個現有 concrete regression viewport：一般商店 `244×133`，升星獎勵 `196×133`。兩者都測試水平／垂直 bounds、武功數值欄分離、標題與子列的可見關聯、continuation indentation，以及所有 overflow 都能透過鍵盤／手把到達。一般商店效果字體不得低於12px，升星獎勵不得低於10px；內容放不下時啟用 scroll、paging 或詳細入口，不能再縮小。

本設計明確退役舊版「上述 viewport 不可分頁，而且最小字體仍放不下時以 Debug assert」的契約。新 document 會把一個生命週期拆成多個可讀列，強制單頁容納全部內容會再次鼓勵刪除條件；新的 regression 要求是可讀、無重疊、無靜默遺失且 overflow 可到達，而不是必須同時出現在一頁。

初始內容長度門檻：

| Style | 每個 rendered row 的上限 | 超限處理 |
|---|---:|---|
| Compact | 72 display units | 依 branch、sequence、action group 或 qualifier 前的 semantic break 拆列 |
| Full | 120 display units | 拆為同 block 的多個句子或子列 |
| Detailed | 不設內容上限 | 保留結構並由 UI wrap／scroll |

長度使用共用的 `displayTextWidth` 邏輯測試，不能用 `std::string::size()`。若一個不可再分的 semantic phrase 超限，內容測試應失敗並要求改善 phrase family 或 UI，不得截斷。

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
- standalone、matching `BoundMagic` enclosing default、non-matching event 與 `OwnerAnyCast` context 各有測試。只有 matching `BoundMagic` trigger group 可省略 event；Detailed 永不省略，cross-rule section 的其他 event 也不受影響。

### Graph 連線

- 所有 status-reference YAML label 在 parse time 轉為 `BattleStatusKind`；未知 label 失敗，runtime、formula、condition、action 與 graph 使用相同 enum identity。
- status kind、state slot、source scope、owner binding 與 target domain 各有 positive／negative 配對測試。
- 同顯示名但不同 identity 不連線；同 identity 但不同來源限制不連線。
- 一個 consumer 有零個、一個與多個 producer 的行為都要明確測試。
- rule 順序與 action 順序在歸約後仍可由 audit trace 還原。

### Archetype 安全性

- 每個 archetype 對所有 payload field 做 mutation test，證明該欄位會顯示、被明確吸收，或使 matcher 拒絕。
- action allowlist、propagation、selector、branch condition 或 source policy 增加新 enum value 時，既有 matcher 預設拒絕。
- Full 可見 Audit fact 必須逐一出現在 descriptor 的 `PromotedToFull` 集合；Compact 可見 Decision fact 必須來自全域 mandatory set 或 descriptor 的 named projection。未宣告的 renderer 判斷使測試失敗。
- 屬性 duration promotion 有完整 positive／negative matrix：相鄰、target、variant、operation、constant sign、duration、stack、cap、perStack、scope、per-action condition 與各種 structural boundary 均各自變異。只有符合全部條件的 Full／Compact 提升 duration，Detailed 永不合併。
- 相同 typed shape 換成不同效果 ID、武功 ID、角色名或容器名，除 localization label 外應產生相同 document shape 與文字。
- renderer 或 matcher 不得含 production magic ID、role ID、item ID 或 combo ID 分支。

### Golden 與內容測試

- 慕容復、蕭中慧、譚處端、虛竹與歐陽鋒三種 style 的 golden tests。
- 全部正式 `config/chess_*.yaml` 建立 document，無 coverage error。
- generic fallback shape 使用明確 allowlist／snapshot；未審核的新 shape 使測試失敗。
- Compact 每列最多72 display units，Full 每列最多120；超限不能靠 truncation 通過。
- Full／Compact 禁止出現 action allowlist dump、`條件[`、propagation enum 名、`追加至基礎攻擊`、`原樣式` 與數字 magic ID；所有 description row 禁止使用 `→`、全形 `／`、ASCII `/` 或 `·` 作結構分隔。含 `×` 的 golden／mutation test 必須證明它來自 typed arithmetic expression，而不是 renderer delimiter。
- Full prose row 恰以一個 `。` 結尾；Full heading、Detailed field／list row 與 Compact row 沒有 terminal punctuation。外層 join 不得產生 `。；`。
- 驗證繁體中文詞彙，不新增簡體機制標籤。

### Runtime 與 UI

- 同一份 parsed rules 在描述遷移前後執行相同 scenario，battle event／replay digest 完全相同。
- 測試明確接受 migration build 的 content fingerprint 與舊 build 不同，且舊 replay／checkpoint 因 mismatch 被拒絕；遷移後的 canonical typed fingerprint 覆蓋所有正式欄位，且不受文案、標點或換行影響。
- UI 測試 section grouping、縮排、wrap、單欄／雙欄、non-overlap、scroll／paging 與 accessible overflow；`244×133` 與 `196×133` 另測12px／10px下限、數值欄分離及所有 overflow 可到達。
- call-site mapping 測試裝備／內功／完整羈絆使用 Full、角色快速羈絆使用 Compact、角色大招與同型獎勵預覽使用帶 `UltimateCommitted` context 的 Compact，Detailed 只由詳細／診斷入口選用。
- JSON 與 catalog call site 測試 block 順序及 style，不再假設一條 rule 對應一個字串。

## 實作階段

1. **盤點與 characterization**：列出所有正式容器、typed shape、目前 fallback 候選與最長列；固定五個案例的輸入 fixture 與預期資訊，但尚不要求新 renderer golden 通過。
2. **Typed semantic 前置遷移**：把所有 status reference canonicalize 為 `BattleStatusKind`；把 `使用武功: 62` 等實際代表「此武功」的設定遷移為 typed relational condition。解析器、schema、runtime、正式 YAML 與 structural equality tests 一次更新，不保留舊語法。同階段建立完整 canonical typed fingerprint，接受一次性 fingerprint 切換並固定舊 replay／checkpoint 拒絕測試。
3. **Document core**：建立 typed fact、provenance、coverage disposition、role resolver、presentation context 與 lossless Detailed；先證明所有欄位可追蹤，且 role failure 會 fail-fast。
4. **Lifecycle graph**：在 canonical identity 上加入 status／state slot producer-consumer graph，以及正反連線測試；完成七星、毒爆與 record／consume 類 shape。
5. **Archetype library**：依正式內容頻率實作 reusable matcher，包括屬性 duration promotion 的完整十項邊界。每加入一個 archetype，同時加入 field mutation、Audit promotion 與 identity-independence 測試。
6. **Full／Compact projection**：從同一 document 產生玩家文字、terminal punctuation、semantic breaks 與長度測試；移除 player-facing `→` 和實作詞彙。此時加入五個案例的三種 style golden；蕭中慧的「同武功」已由階段2的 relational condition 提供。
7. **Call-site 與 UI 遷移**：依固定 panel-to-style mapping 將 catalog、session、JSON、magic display 與其他面板改為容器和 blocks；完成 enclosing-default、wrap、scroll／paging、兩個 concrete viewport 與詳細入口。
8. **切換與清理**：移除舊 `effectDescription(EffectRule, ...)`、逐規則 golden、legacy punctuation 與重複 formatter；把 [效果描述語法設計](效果描述語法設計.md) 的狀態標頭改指向本文件，並把三層文件標記為已由本文件取代，維持可追蹤的 supersession chain；最後執行完整 build、schema 驗證、單元測試與正式內容掃描。

實作期間可以用 characterization 比較新舊結果，但合併後只能有一套 document pipeline。舊 renderer 不作 compatibility fallback。

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
9. enclosing default 只省略 event 相符之 `BoundMagic` trigger group；Detailed、其他 event 與 `OwnerAnyCast` 不省略，且每個省略都有可見的 UI parent-child hierarchy。
10. 屬性共同 duration 只在完整十項 eligibility boundary 內提升；cap、counter、stack identity 與其他 qualifier 不會被提升或誤認為共用。
11. panel-to-style mapping 固定，`244×133` 與 `196×133` viewport 在12px／10px字體下維持正確分組、無重疊與 accessible overflow；不靠任意縮字塞入全部內容。
12. Full 的 Audit promotion 與 Compact 的 Decision projection 都由 descriptor／全域集合宣告，不含 renderer 的主觀逐案例判斷；三種 row kind 符合 terminal punctuation 契約。
13. runtime 行為與 battle event／replay digest 不變。canonical typed representation 上線時 content fingerprint 刻意改變一次，遷移前 replay／checkpoint 因 mismatch 失效；其後 fingerprint 只由完整 typed semantics 決定，不受描述文案影響。
14. 所有 status identity 已在 parse time canonicalize，graph 不比較 localization string；「此武功」等關係也以 typed condition 表達。
15. 所有 call site 已遷移，舊逐規則字串 API 與 renderer 已刪除，沒有相容層或重複邏輯，文件 supersession chain 已更新。
