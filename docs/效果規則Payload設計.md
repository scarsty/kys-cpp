# 效果規則 Payload 設計

## 目的與邊界

本文件把 `EffectRule` 的「動作族群與參數例子」收斂成可實作、可驗證、可描述的型別契約。第二階段開始前，這些型別、合法組合及錯誤規則必須先通過評審。

這不是絕招專用 DSL。羈絆、裝備、裝備羈絆、內功與絕招都解析成同一個 `EffectRule`；商店、金幣、刷新、戰場選擇及計作羈絆等非戰鬥資料不進入本模型。

## 規則外殼

```cpp
enum class EffectEvent
{
    BattleInitialized,
    FrameAdvanced,
    CastPlanned,
    AttackCommitted,
    UltimateCommitted,
    AttackSpawned,
    MainProjectileBeforeDamage,
    HitBeforeDamage,
    DamageResolved,
    HealAttempted,
    HealApplied,
    CastContinuation,
    CastSettled,
    ShieldBroken,
    UnitDied,
    AllyDied,
};

enum class EffectSelectorKind
{
    Self,
    SourceUnit,
    HitTarget,
    OriginalAttackTarget,
    ComboMembers,
    AllLivingUnits,
    Allies,
    Enemies,
    LowestHpAllies,
    LowestMpAllies,
    HighestMpEnemy,
    NearestEnemies,
    FarthestEnemy,
    UnitsInRadius,
    UnitsInSquare,
};

struct EffectSelector
{
    EffectSelectorKind kind{};
    int count = 0;              // 0 表示不截斷；只供可排序的複數選擇器使用
    int radiusTiles = 0;        // 半徑選擇器必填
    int squareSideTiles = 0;    // 方形選擇器必填，必須為正奇數
    EffectTeamFilter team{};
    EffectTieBreak tieBreak{};
    bool excludeOwner = false;
};

struct EffectRule
{
    EffectRuleId id{};
    EffectEvent event{};
    EffectSelector selector{};
    std::vector<EffectCondition> conditions;
    int chancePct = 100;
    int maxActivations = 0;     // 0 表示無上限
    std::vector<EffectAction> actions;
};
```

`EffectRuleId` 是設定載入後依來源與條目順序產生的穩定 ID，不依顯示文字或容器位址。規則內 action 依 YAML 順序執行；規則之間依既有來源順序及條目順序執行。遷移不得以排序「整理」而改變現有事件順序。

## 事件資料契約

事件 context 不是可任意加欄位的 property bag。所有事件共享固定 header，payload 則是由 `EffectEvent` 決定的封閉 variant；語意相同的事件可共用 payload 型別，例如兩種 commit event 都使用 `CastCommitEventData`：

```cpp
struct EffectEventHeader
{
    int frame{};
    std::uint64_t eventOrdinal{};
    EffectSourceBinding binding{};
    EffectUnitSnapshot owner{};
    BattleEffectReadView battle{};
};

using EffectEventPayload = std::variant<
    InitializationEventData,
    FrameTickEventData,
    CastPlanEventData,
    CastCommitEventData,
    AttackEventData,
    HitEventData,
    DamageResultEventData,
    HealRequestEventData,
    HealResultEventData,
    CastAggregateEventData,
    ShieldBreakEventData,
    DeathEventData>;

struct EffectEventContext
{
    EffectEventHeader header{};
    EffectEventPayload payload{};
};
```

- `owner` 是事件建立時的來源單位快照，包含 unit ID、team、星級、存活、HP／MP、攻防速、位置及狀態 ID；來源死亡後仍可安全讀取事件值。
- `battle` 是規則評估時的唯讀 deterministic query view，供全隊、最近、最低生命等 selector 使用；它不提供寫入入口。
- transaction／cast 中間值只能來自對應 payload，不能回頭從目前 runtime 猜測。
- 舊效果原本採 enqueue-time snapshot 或 evaluation-time live read 的差異，必須由 canonical execution trace 保留；遷移不能為了統一而改變擷取時點。

可用 capability 與具體欄位如下。表中未列欄位對該事件即不存在，validator 必須拒絕依賴它的 condition、selector 或 formula。

| `EffectEvent` | Payload capability | 除共用 header 外的可用資料 |
|---|---|---|
| `BattleInitialized` | `Initialization` | 初始 roster／格位讀取權、戰鬥規則 ID；無 cast、attack、target 或 transaction |
| `FrameAdvanced` | `FrameTick` | frame delta、週期 ordinal；無特定 target |
| `CastPlanned` | `CastPlan` | 已保留的 cast ID、caster、magic ID、ultimate、preferred target、規劃前 MP／cost、base range／attack pattern |
| `AttackCommitted` | `CastCommit` | 完整 cast provenance、實付 MP、最終 target／range／pattern；所有正常與絕招 cast 各發一次 |
| `UltimateCommitted` | `CastCommit` | 與 `AttackCommitted` 相同且 `ultimate=true`；絕招在前者之後額外發一次，保留兩種舊 trigger 語意 |
| `AttackSpawned` | `Attack` | cast／attack provenance、attacker、original target、spawn position／velocity、skill payload、root／main／ordinal、spawn-time modifiers |
| `HitBeforeDamage` | `Hit` | `Attack` 全欄位、defender before snapshot、contact position、accepted-hit state、pre-defense damage／damage kind；無最終傷害 |
| `MainProjectileBeforeDamage` | `Hit` | 與 `HitBeforeDamage` 相同且 `mainProjectile=true`；主彈命中會額外發此事件，兩事件相對順序由 dispatcher golden 固定 |
| `DamageResolved` | `DamageResult` | damage transaction ID、typed damage origin、attacker／defender before-after、raw／resolved damage、shield absorbed、final HP／MP damage、blocked／executed／killed |
| `HealAttempted` | `HealRequest` | heal transaction ID、kind、source／target before snapshot、calculated amount、optional cast ID；無 modifier／applied amount |
| `HealApplied` | `HealResult` | `HealRequest` 全欄位、modified amount、HP before／after、applied amount；只代表 `appliedAmount > 0` |
| `CastContinuation` | `CastAggregate` | cast provenance、第一輪 aggregate、不同 target IDs、最高／總 final HP damage、per-ordinal results；每 cast 一次 |
| `CastSettled` | `CastAggregate` | 所有合法 child work 結束後，本 cast 自身的 final aggregate；child 數值不自動併入父 aggregate；每 cast 一次且不可再建立 tracked work |
| `ShieldBroken` | `ShieldBreak` | target before-after、破盾量、typed cause origin、optional cast／attack provenance |
| `UnitDied` | `Death` | dead unit before-after、optional killer、typed cause origin、death ordinal、optional cast／attack provenance |
| `AllyDied` | `Death` | `Death` 全欄位及相對於 rule owner 的 ally relation；按既有來源／owner 順序評估 |

`DamageResult`、`ShieldBreak` 與 `Death` 的 origin 是 `Attack／Status／Effect／Environment` variant，不是 nullable attack 欄位。`IsMainProjectile` 用於 `DamageResolved` 時必須同時有 `DamageOriginIsAttack` 條件；validator 不接受「非攻擊時當 false」的隱含行為。

每個 condition、selector 與 `EffectNumberBase` 都宣告所需 capability 集合。第二層驗證實作為 `requiredCapabilities ⊆ eventCapabilities`；例如 `FinalHpDamage` 只接受 `DamageResult`，`HealKindIn` 只接受 `HealRequest／HealResult`，`CastDistinctTargetCountAtLeast` 只接受 `CastAggregate`。

## 數值公式

所有可調數字只存在於具型別數值節點，不存在於說明字串。

```cpp
enum class EffectNumberBase
{
    Constant,
    SourceStar,
    SourceAttack,
    SourceMaxHp,
    TargetMaxHp,
    TargetCurrentShield,
    FinalHpDamage,
    AccumulatedStateValue,
    RecordedMaximum,
};

struct EffectNumber
{
    EffectNumberBase base{};
    int flat = 0;
    int percent = 0;
    EffectRounding rounding{};
    std::optional<int> minimum;
    std::optional<int> maximum;
};
```

計算順序固定為 `round(base × percent / 100) + flat`，最後套用最小／最大值。純固定值使用 `base: Constant, percent: 0, flat: N`；`星級×100` 使用 `base: SourceStar, percent: 10000, flat: 0`。解析器不得因某個武功方便而增加另一套算式欄位。

取整不是隱含預設。舊效果遷移時必須填入能重現舊行為的 `EffectRounding`；新規則若省略取整，只能使用該 action schema 明列的預設。

## 條件

```cpp
using EffectCondition = std::variant<
    IsUltimate,
    MagicIdEquals,
    IsMainProjectile,
    IsRootAttack,
    SourceHpRatioAtMost,
    TargetHpRatioAtMost,
    SourceHasState,
    TargetHasState,
    SourceStackAtLeast,
    OtherLivingAllyUsesMagic,
    CastDistinctTargetCountAtLeast,
    AttackOrdinalEquals,
    HealKindIn,
    DamageOriginIsAttack,
    DamageKindIn>;
```

每一 variant 只能讀取事件快照中已定義的欄位。載入時若事件不能提供條件所需資料，直接回報設定錯誤；執行時不以「資料不存在就當 false」隱藏接線錯誤。

## 十個動作 payload

正式 `EffectAction` 只包含本節列出的封閉 variant；同族的 apply／remove 或 state-machine 子型別也不提供 `std::map<string, value>` 擴充口。

### 1. 屬性修正

```cpp
struct ModifyAttributeAction
{
    BattleAttribute attribute{};
    EffectNumber amount{};
    AttributeOperation operation{}; // FlatAdd, PercentAdd, Override, Multiply
    int durationFrames = 0;          // 0 表示永久
    EffectStackPolicy stack{};
    std::optional<int> stackLimit;
};
```

- `Override` 與 `Multiply` 必須有正持續時間，除非該屬性明確支援永久覆寫。
- 有 `stackLimit` 時 `stack` 只能是 `AddStack`；`Refresh` 不接受層數上限。
- 攻擊、防禦、速度、暴擊率、暴擊傷害、閃避、格擋、減傷及技能傷害共用此 payload，但各屬性的最終封頂仍由所屬結算邊界負責。

### 2. 傷害修正

```cpp
struct ModifyDamageAction
{
    DamageModifierStage stage{}; // BeforeDefense, AfterDefense, Final
    DamageChannel channel{};     // Skill, Dot, Effect, Reflected, All
    EffectNumber amount{};
    DamageModifierOperation operation{};
    int durationFrames = 0;
    EffectStackPolicy stack{};
};
```

忽略防禦使用 `operation: IgnoreDefensePercent`，不是把防禦暫時設成零。減傷合計的 80% 上限只在最終傷害交易套用，payload 不自行截斷。單次承傷上限使用既有傷害交易中的具名 modifier，不冒充百分比減傷。

### 3. 資源變更／治療交易修正

```cpp
struct ChangeResourceAction
{
    BattleResource resource{}; // Hp, Mp, Shield, StatusShield, StaggerShield,
                               // ActiveCooldown, ControlImmunityFrames,
                               // InvincibilityFrames
    EffectNumber amount{};
    ResourceChangeKind kind{}; // Restore, Drain, Grant, Remove, Transfer,
                               // RefreshToAtLeast
    std::optional<EffectSelector> transferDestination;
    EffectHealKind healKind{};
    EffectHealSourcePolicy healSourcePolicy{};
};

enum class HealModifierOperation
{
    Block,
    MultiplyReceived,
};

struct ModifyHealTransactionAction
{
    HealModifierOperation operation{};
    HealKindFilter kinds{};
    int percent = 100; // 只供 MultiplyReceived 使用
};
```

- `Hp + Restore` 一律產生治療請求，不能直接改 HP。
- `Transfer` 必須有目的選擇器，實際轉交值等於成功扣除值。
- `Shield + Remove` 可按目標目前護盾比例計算，不能產生負護盾。
- 吸血不是 `ChangeResource` 自行讀傷害；它在 `DamageResolved` 以 `FinalHpDamage` 公式產生治療請求。
- `ModifyHealTransactionAction` 只接受 `HealAttempted`。`Block` 不讀 `percent`；`MultiplyReceived` 要求 0–100，依[治療交易設計](治療交易設計.md)的固定順序回傳 modifier，不直接寫 transaction。

### 4. 套用／移除狀態

```cpp
struct ApplyStatusAction
{
    BattleStatusKind status{};
    int durationFrames = 0;
    std::optional<EffectNumber> duration;
    std::optional<EffectNumber> applicationCount;
    int stacks = 1;
    EffectNumber potency{};
    EffectNumber secondaryPotency{};
    EffectStackPolicy stack{};
    std::optional<int> stackLimit;
    bool aggregatePotencyWithinEvent = false;
};

struct RemoveStatusAction
{
    StatusFilter filter{};
    int count = 0; // 0 表示全部
    StatusRemovalOrder order{};
};
```

`BattleStatusKind` 是穩定語意 ID；「寒毒」「枯骨」等名稱只是 localization label。中毒、流血等有專用 tick 語意的狀態保留具型別資料，不塞進無型別 property map。

### 5. 造成傷害

```cpp
struct SingleTargetDamageArea {};
struct CircleDamageArea { int radiusTiles{}; };
struct SquareDamageArea { int sideTiles{}; };
using DamageArea = std::variant<
    SingleTargetDamageArea,
    CircleDamageArea,
    SquareDamageArea>;

struct DealDamageAction
{
    EffectNumber amount{};
    std::optional<EffectNumber> transactionCount;
    BattleDamageKind kind{};
    DamageArea area{};
    PerCastHitPolicy perCast{};
    std::optional<AreaProjectileDamageDelivery> areaProjectiles;
};
```

`Square` 使用棋格方形並要求正奇數邊長；`Circle` 使用世界座標距離。純粹傷害只忽略防禦，仍進入護盾、格擋層、減傷及單次承傷上限。`perCast` 明確表示同一 cast 對同一單位的上限，不能以 `sharedHitGroupId` 猜測。

### 6. 修改／生成攻擊

```cpp
struct AttackPattern
{
    AttackPatternKind kind{}; // Preserve, Fan, Flanks, SamePointSequence, MultiTarget
    int projectileCount = 1;
    int spreadDegrees = 0;
    int intervalFrames = 0;
};

struct ModifyAttackAction
{
    AttackPattern pattern{};
    int strengthPct = 100;
    bool through = false;
    bool tracking = false;
    bool mainProjectile = true;
    int sameTargetHitLimit = 0;
    AttackTargetPolicy targets{};
    CastPropagationPolicy propagation{};
};
```

- `projectileCount` 是總數還是追加數由 `AttackPatternKind` 固定定義，不允許同一欄位兩種解讀。
- 每個生成攻擊都必須攜帶[施放來源與結算生命週期設計](施放來源與結算生命週期設計.md)定義的 provenance。
- 延遲、追蹤、側翼與反彈都不是新的效果族群；它們是同一攻擊 payload 與傳播政策的組合。

### 7. 強制移動

```cpp
struct ForceMoveAction
{
    ForceMoveDirection direction{}; // AwayFromSource, TowardSource, TowardPoint
    int distanceTiles = 0;
    int distancePixels = 0;
    int lockFrames = 1;
    ForceMoveCollision collision{};
    ForceMoveBlockedResult blocked{};
};
```

`distanceTiles` 與 `distancePixels` 必須恰有一個為正數，`lockFrames` 必須為正數。目前 runtime 只接受遠離來源與接近來源；接近任意指定點在載入時拒絕。拉近與擊退都交給既有移動／棋格合法性判斷；不得以瞬移近似。多目標移動依 selector 的穩定順序逐一結算，佔位結果也按該順序固定。

### 8. 建立區域

```cpp
struct CreateAreaAction
{
    AreaGeometry geometry{};
    AreaAnchor anchor{};
    int durationFrames{};
    AreaSourceDeathPolicy sourceDeath{};
    AreaMergePolicy merge{};
    std::vector<AreaModifier> modifiers;
};
```

```cpp
struct AreaModifyAttribute
{
    AreaUnitRelation relation{}; // Ally, Enemy
    BattleAttribute attribute{};
    EffectNumber amount{};
    AttributeOperation operation{};
    AreaOverlapPolicy overlap{};
};

struct AreaModifyOutgoingDamage
{
    AreaUnitRelation relation{};
    int percent{};
    DamageChannel channel{};
    AreaOverlapPolicy overlap{};
};

struct AreaModifyAttackSpawn
{
    AreaUnitRelation sourceRelation{};
    std::optional<bool> tracking;
    std::optional<int> speedPct;
    std::optional<int> projectilePressurePct;
    AreaOverlapPolicy overlap{};
};

struct AreaForcedMoveImmunity
{
    AreaUnitRelation relation{};
    ForceMoveDirectionFilter directions{};
    AreaOverlapPolicy overlap{};
};

using AreaModifier = std::variant<
    AreaModifyAttribute,
    AreaModifyOutgoingDamage,
    AreaModifyAttackSpawn,
    AreaForcedMoveImmunity>;
```

第一版只支援上述四個 modifier；不提供任意 callback。`AreaOverlapPolicy` 對數值只允許 `Add`、`KeepStrongest`，對布林免疫只允許 `Any`。黃沙與伏魔的實際選擇及 query phase 見區域契約。

完整欄位、幀生命週期與查詢語意由[持續區域效果設計](持續區域效果設計.md)定義。此 action 只建立／刷新區域，不在 dispatcher 內逐單位直接改屬性。

### 9. 修改施放

```cpp
struct ModifyCastAction
{
    std::optional<EffectNumber> mpCost;
    std::optional<CastRangeMode> rangeMode;
    std::optional<AttackPattern> replacementPattern;
    bool freeAdditionalCast = false;
    CastPropagationPolicy propagation{};
};
```

只允許 `CastPlanned` 使用。`mpCost` 表示本次實際消耗，不是結算後退還；免費追加施放也必須產生子 cast provenance，不能直接呼叫另一條武功流程並遺失來源。

### 10. 狀態機操作

```cpp
struct RecordMaximumDamage
{
    EffectStateSlot slot{};
    DamageChannel channel{};
    DamageMeasure measure{}; // FinalHpDamage
    StateResetBoundary reset{};
};

struct ConsumeRecordedMaximum
{
    EffectStateSlot slot{};
    StateValueDestination destination{}; // DamageAmount, ShieldAmount
    int percent = 100;
    bool clearAfterConsume = true;
};

struct StartDamageAbsorption
{
    EffectStateSlot slot{};
    int absorbedPct{};
    int durationFrames{};
    bool settleOnSourceDeath = false;
};

struct SettleDamageAbsorption
{
    EffectStateSlot slot{};
    EffectSelector target{};
    BattleDamageKind damageKind{};
    int returnedPct = 100;
    bool clearAfterSettle = true;
};

struct BorrowEffectRules
{
    EffectSelector sourceUnits{};
    EffectNumber sourceCount{};
    BorrowedRuleFilter filter{};
    CastPropagationPolicy propagation{};
};

struct CopyAttackDefinition
{
    EffectSelector sourceUnits{};
    CopiedMagicFilter filter{};
    int copyCount = 1;
    CastPropagationPolicy propagation{};
};

struct ChangeStateValue { EffectStateSlot slot{}; int delta{}; /* min/max */ };
struct TransferStateValue { EffectStateSlot sourceSlot{}; EffectStateSlot destinationSlot{}; };
struct SettleRemainingStatusDamage { BattleStatusKind status{}; }; // 目前只允許中毒
struct GenerateClones { int count{}; };
struct PreventDeath { int invincibilityFrames{}; };
struct ConfigureRescueReposition { RescueRepositionMode mode{}; int activations{}; };
```

`BorrowedRuleFilter`／`CopiedMagicFilter` 是 allow-list 型別：分別列出可借的 action category 與可選武功條件；禁止遞迴的 copy／borrow action 是 validator 的必要排除項，不能只在執行時跳過。

```cpp
using StateMachineAction = std::variant<
    ChangeStateValue,
    TransferStateValue,
    RecordMaximumDamage,
    ConsumeRecordedMaximum,
    StartDamageAbsorption,
    SettleDamageAbsorption,
    BorrowEffectRules,
    CopyAttackDefinition,
    SettleRemainingStatusDamage,
    GenerateClones,
    PreventDeath,
    ConfigureRescueReposition>;

using EffectAction = std::variant<
    ModifyAttributeAction,
    ModifyDamageAction,
    ChangeResourceAction,
    ModifyHealTransactionAction,
    ApplyStatusAction,
    RemoveStatusAction,
    DealDamageAction,
    ModifyAttackAction,
    ForceMoveAction,
    CreateAreaAction,
    ModifyCastAction,
    StateMachineAction>;
```

狀態機 variant 以可共用的機制定義，不以武功命名。每個 variant 都必須列出：狀態 key、寫入事件、讀取事件、重設時機、來源死亡行為、cast 傳播政策及描述 formatter。若一般事件＋前九類 action 能忠實表示，就不得新增狀態機 variant。

## 事件與動作合法矩陣

`✓` 表示可直接使用；`條件` 表示必須符合右側限制。未列出的組合載入失敗。

| 事件 | 合法動作 | 限制 |
|---|---|---|
| 戰鬥初始化 | 屬性修正、傷害修正、資源變更、狀態、限定狀態機 | 只允許 owner observation、確定性單次初始化；資源不接受 HP／作用中冷卻／吸取／轉移；狀態機只接受分身、免死與救援設定 |
| 每幀 | 屬性修正、資源變更、狀態、造成傷害 | 必須有週期或現存 runtime 狀態作門檻，禁止每幀無界建立物件 |
| 施放規劃 | 修改施放、修改攻擊 | 不得依命中目標或最終傷害 |
| 攻擊／絕招提交 | 屬性修正、資源、狀態、修改／生成攻擊、建立區域、狀態機 | 每個根 cast 最多評估一次 |
| 攻擊生成 | 修改攻擊 | 只能修改本次新生成 attack 的 spawn-time 性質 |
| 命中傷害前 | 傷害修正、資源、狀態、造成傷害、強制移動 | 直接治療仍須送治療交易；不得讀最終生命傷害 |
| 傷害結算後 | 資源、狀態、造成傷害、狀態機 | 可讀 `FinalHpDamage`；衍生傷害必須有來源分類防止遞迴 |
| `HealAttempted` | 治療交易修正 | 只能使用 `ModifyHealTransactionAction` 回傳 typed modifier，不直接覆寫 HP 或 transaction result |
| `HealApplied` | 屬性、資源、狀態、狀態機 | 只在實際回復量大於 0 後執行後續命令 |
| 施放延續 | 修改／生成攻擊、修改施放、狀態機 | 每個 cast 只發出一次；唯一可在攻擊耗盡後增加 tracked work 的事件 |
| 施放結算完成 | 屬性、資源、狀態、造成傷害、狀態機 | 可讀本 cast 聚合值，且只發出一次；不得再建立 tracked work |
| 護盾破裂／死亡 | 資源、狀態、造成傷害、建立區域、狀態機 | 死亡事件不得復用已失效的攻擊目標指標 |

由 hit resolver／movement 等消費端直接查詢的 exact-runtime 動作，不走一般 dispatcher 的觸發記帳。這類規則不可與一般動作或條件分支混合，也不可設定消費端沒有直接處理的規則機率、每 N 次事件、觸發限制、觸發次數或同來源冷卻；validator 會要求拆成可完整記帳的獨立規則。

## 必填、預設與禁止欄位

| 動作 | 必填 | 唯一 schema 預設 | 載入時拒絕 |
|---|---|---|---|
| 屬性修正 | 屬性、數值、運算 | `duration=0` 表永久 | 限時覆寫卻無期限；層數政策無上限 |
| 傷害修正 | stage、channel、數值、運算 | `duration=0` 表本事件即時 modifier | 不支援的 channel/stage；自行填 80% 全域 cap |
| 資源變更 | resource、kind、數值 | 無 | HP restore 繞過治療交易；transfer 無目的 selector；初始化使用 HP／作用中冷卻／吸取／轉移 |
| 治療交易修正 | operation、heal kinds；乘算時須填 percent | `percent=100` | 非 `HealAttempted`；Block 同時填 percent；比例超出 0–100 |
| 套用狀態 | status、duration／狀態專屬期限、合併政策 | `stacks=1`、套用次數 1 | 負面期限非正；AddStack 無上限；套用次數公式可能為負 |
| 移除狀態 | filter、order | `count=0` 表全部 | 隨機 order 卻無 battle RNG 語意 |
| 造成傷害 | amount、kind、area | 單次交易、single-target 的 per-cast 不限次 | 交易次數可能為負；圓半徑非正；方形邊長非正奇數 |
| 修改攻擊 | pattern、targets、propagation | `strength=100`、單彈、主彈 | 負數延遲；同目標上限負數；未知 copy policy |
| 強制移動 | direction、唯一 distance 單位、lock frames、collision、blocked result | `lockFrames=1` | 同時／都未填格數與像素；距離或鎖定非正；接近指定點；用 teleport 近似 pull／knockback |
| 建立區域 | geometry、anchor、duration、死亡與 merge policy、modifiers | 無 | 空 modifier；期限非正；modifier 無 overlap policy |
| 修改施放 | 至少一個變更、propagation | `freeAdditionalCast=false` | 非 `CastPlanned／CastContinuation`；免費 child 無 policy |
| 狀態機 | variant 的全部 state slot／生命週期欄位 | consume／return 百分比 100 | 以武功 ID 命名 variant；缺 reset/death/copy 排除規則 |

YAML 出現 schema 未列欄位一律報錯；不存在「讀到未知欄位但忽略」或用 `value/value2` 接住的 fallback。

## 合併與疊加規則

每個限時／可疊效果必須明寫 `EffectStackPolicy`：

| 政策 | 行為 |
|---|---|
| `Independent` | 每次啟用建立獨立 instance，各自到期 |
| `Refresh` | 同來源、同規則、同目標只刷新時間，數值不累加 |
| `Replace` | 以新 instance 完整取代舊 instance |
| `KeepStrongest` | 比較 canonical amount，保留較強者並依 schema 定義是否刷新時間 |
| `AddStack` | 增加具上限層數；每層共享或獨立期限必須由 payload 指明 |

省略政策只允許 schema 明定無歧義的情況：即時資源變更、即時傷害及永久單次計數。限時屬性、狀態、區域及 modifier 不得靠 runtime 預設猜測。

## 驗證責任

載入器依序做四層驗證：

1. **結構**：必要欄位、未知欄位、數值範圍、enum label。
2. **事件能力**：事件是否提供 selector、condition、formula 需要的資料。
3. **動作語意**：持續、層數、取整、合併政策與 action 內部組合是否合法。
4. **來源能力**：例如 `ComboMembers` 只能用於有羈絆上下文的來源，`MagicIdEquals` 只能用於武功來源。

錯誤必須包含 YAML 路徑、來源 ID、規則序號與欄位名稱。正式載入不接受未知欄位，也不接受以忽略錯誤繼續執行。

## 舊 `EffectType` 的 canonical 對應

每個舊型別都在遷移 fixture 中展開成完整 canonical rule；以下是映射規則，不是只供參考的例子：

| 舊型別族 | canonical 組合 |
|---|---|
| `FlatATK/PctATK/TeamFlatATK/TeamPctATK` | selector 決定自己／全隊；`ModifyAttribute(Attack)` 決定固定／百分比 |
| `OnSkillTeamHeal/OnSkillTeamHealPct/LowestAllyHeal` | event 決定施放時機；selector 決定全隊／最低生命；`ChangeResource(Hp, Restore)` |
| `HPOnHit/KillHealPct` | `DamageResolved` 加上命中／擊殺條件；以治療交易執行 |
| `ArmorPen/ArmorPenPct/ArmorPenChance` | 命中前事件＋機率欄位＋`ModifyDamage(IgnoreDefensePercent)` |
| `Knockback` | 命中事件＋`ForceMove(AwayFromSource)`；舊預設距離與鎖定幀數先正規化 |
| `ProjectileBounce` | 舊 parser 隱含的 `OnHit` 先展開成事件，再映射為 `ModifyAttack` |
| `OffensiveCharm` | 映射成同一規則內有順序的兩個 action，不再暗中生成第二條規則 |
| 金幣、刷新、戰場、計作羈絆 | 不映射為 `EffectAction`，遷移到各自領域的 typed config |

原計畫要求由舊 parser oracle 對 367 個啟用條目產生 canonical fixture；該 oracle 與 v1 fixture 已在遷移後刪除，現在無法從目前工作樹重建具有證明力的 367／367 execution trace。此永久差異與目前 365 節點的合併原因記錄於[大招效果實作差異與決議](大招效果實作差異與決議.md)，不再列為可由新系統自行補出的驗證工作。

## 四條垂直切片

| 武功 | 必須驗證的 schema 能力 |
|---|---|
| 青囊奇術 | `UltimateCommitted`、最低生命 selector、最大生命比例治療、治療事件描述 |
| 神照功 | `CastPlanned` 的實際內力消耗、護盾資源、星級公式 |
| 九陰白骨爪 | 命中傷害前狀態、全傷害 modifier、治療 modifier、刷新政策 |
| 五虎斷門刀 | 扇形貫穿、多主彈道、同 cast 目標命中上限、順序式複合描述 |

第二階段入口只要求把四條切片各寫成 schema-only YAML fixture，按本文件紙面驗證可表達；不要求 store、dispatcher 或戰鬥 hook 已存在。第二階段建立 parser／validator 後，這四份 fixture 必須先實際通過載入、驗證及 description renderer，才可遷移 367 條設定。第五階段接入 runtime 時才要求它們走同一 store／dispatcher，且不得為切片加武功 ID 分支。
