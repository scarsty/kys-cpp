# Chess status contributions and composable behaviors

## Document status

This is a design and implementation plan. It does not describe an already completed migration.

This document starts from the working-tree state after the semantic status-authoring migration described in `chess-status-effect-authoring-and-description-design.md`. It supersedes the following decisions from that document:

- that an effect-bearing status such as `真氣` should have exactly one runtime instance;
- that the latest additive application may replace the stored cap, payload values, source, or execution origin for all existing layers;
- that a status name owns a closed list of effect fields;
- that the runtime may reduce every status payload to anonymous `potency` and `secondaryPotency` slots; and
- that a stored status behavior needs a status-specific combat path such as `insertTrueQiHitDamage`.

The earlier document remains the record of the completed semantic-YAML and description migration. Where the two documents disagree about status instance ownership or name-specific effect ownership, this document is normative.

Implementation must not begin until this document has been reviewed. The first implementation phase is characterization only.

## Executive decision

The runtime will use **independent status contributions grouped by status identity**.

A status name such as `真氣` is a grouping identity used by conditions, cleansing, consumption, UI, and descriptions. It is not a declaration that every producer supplied an interchangeable cap and effect definition.

Each authored producer contributes its own runtime object containing:

- a stable producer key;
- a stable producer-family key and family-local capacity;
- source and attribution;
- quantity allocated from that producer family's capacity;
- duration and local reapplication state;
- bound behavior rules;
- execution origin and stable ordering information; and
- application sequence.

Reapplying a compatible contribution from the same producer may merge into that contribution. On each holder independently, several immutable-value generations or runtime aliases of the same authored producer remain independently attributed but share one producer-family capacity. Applying the same named status from another producer family does not overwrite the first family, and applying it to another holder never consumes this holder's capacity.

Conceptually:

```text
status group: 真氣
├─ 九陽神功 contribution
│  ├─ 7 / 10 layers
│  ├─ 9 pure hit damage per layer
│  └─ 九陽 source, origin, and execution position
└─ another-martial-art contribution
   ├─ 2 / 4 layers
   ├─ 15 pure hit damage per layer
   └─ its own source, origin, and execution position
```

The aggregate status has 9 visible layers, but the two contributions remain independently meaningful. On a qualifying hit they produce `7 × 9` and `2 × 15` at their own execution positions. Neither cap clamps the other contribution, and neither payload replaces the other.

There will be no global status-definition config. Caps and behaviors remain visible in the effect that grants them.

## Decisions at a glance

| Question | Decision |
| --- | --- |
| Where does a status behavior live? | In the local effect that applies the status. |
| May several effects apply the same status name? | Yes. Their contributions are independent by default. |
| Are layers shown together? | Yes. Group quantity is the sum of matching contributions. |
| Is there one group cap? | No. On each holder, each logical producer family owns one cap shared by its generations. A derived per-holder group capacity may be displayed as the sum of distinct family caps, never used to merge storage. |
| Do several holders share a producer's capacity? | No. Capacity and family-local replacement are scoped to `(producer family, holder unit)`; every selected target receives its own allocation. |
| Can repeated value drift or borrowing multiply a producer's cap? | No. On one holder, runtime instances and immutable-value generations remain distinct for attribution but draw from the same finite producer-family capacity. |
| Does the strongest per-layer value upgrade weaker layers? | No. Each contribution evaluates its own layers. |
| Is `max` used implicitly for cap or behavior? | No. Dominant/shared resolution would be a separate, explicit future mechanic. |
| May a status name use a behavior formerly associated with another status? | Yes, when the trigger and action are structurally and temporally valid and the producer still satisfies any reviewed minimum behavior profile for that player-facing status name. |
| What remains status-name metadata? | Label, quantity noun/family, polarity, control classification, cleanse grouping, genuinely intrinsic group semantics, and optional minimum behavior-profile capabilities used by whole-content validation. |
| How is per-layer scaling authored? | With a numeric `每層數值`, never `每層: true` and never an identity `百分比: 100`. |
| What does an omitted damage `範圍` mean? | Single target. Authors write `範圍` only for `圓形` or `方形`. Explicit `範圍: 單體` is rejected. |
| Do status behaviors use the generic effect engine? | Yes. Stored behavior rules reuse event, selector, condition, action, validation, ordering, and description infrastructure. |

## Why the current model is unsafe

### Same-name additive merge destroys ownership

`BattleStatusSystem::apply` currently locates the first stored instance by `BattleStatusKind`. Its generic `AddStack` branch then:

1. adds to that instance's quantity;
2. clamps the complete result using the incoming request's `stackLimit`;
3. replaces `sourceUnitId`;
4. replaces `potency` and `secondaryPotency`; and
5. replaces `origin`.

If 九陽 has 10 layers capped at 10 with per-layer damage 9, and another producer applies one `真氣` layer capped at 5 with per-layer damage 20, the existing object can become 5 layers whose value and origin both belong to the newest producer.

The inverse is also unsafe: an incoming higher cap can permit a merged status to exceed the earlier producer's intended cap.

This is not a cap-selection problem alone. It is loss of layer provenance.

### `max` does not provide independent safety

Taking the maximum cap is deterministic but creates a shared-cap game rule. Taking the maximum per-layer value is more consequential:

```text
10 weak layers × 9
+ 1 strong layer × 100
```

If maximum potency controls the shared pool, one strong layer upgrades all weak layers to 100. If only the strong layer remains worth 100, the runtime must remember its provenance, which is the contribution model.

Therefore, this design does not add an implicit `max`, `min`, latest-wins, or weighted-average resolver. Those may be valid explicit game mechanics later, but they are not neutral storage behavior.

### A name-indexed payload catalog overstates the invariant

The current status field catalog answers:

> Which fields may the C++ payload selected by this status name contain?

It does not answer the more important questions:

- who may produce the status;
- which applications may merge;
- who owns the cap;
- which layers a value applies to;
- which source and origin execute the stored behavior; or
- how several independently valid contributions interact.

This is why a diagnostic such as the following feels arbitrary:

```text
狀態「真氣」的效果不允許欄位「阻止本次施放」；此欄位屬於狀態「化勁」、「刺目」
```

Preventing a cast is valid or invalid because of the event phase, current-cast context, target relationship, and action semantics. It is not technically invalid because the displayed status label is `真氣`.

### The specialized True-Qi bridge removes the generality we already built

The generic effect system already has events, selectors, conditions, actions, source binding, ordering, and command reduction. The current True-Qi implementation exits that system after application, reads a single status instance on hit, synthesizes a pure-damage command, and requires one origin-bearing instance.

The generalized work was not mistaken. The missing abstraction is an active, source-owned status contribution that can expose its stored behavior to the same generic dispatcher.

### Current code reference map

Line numbers are deliberately omitted because the working tree is still changing. These symbols are the review anchors:

| Current responsibility | File and symbol | Design issue or retained behavior |
| --- | --- | --- |
| Stored status object | `src/battle/BattleStatusSystem.h`: `BattleTypedStatusInstance` | Stores one source, stacks, anonymous primary/secondary potency, optional origin, and sequence. It does not retain a producer key, cap, or typed bound behavior. |
| Status application | `src/battle/BattleStatusSystem.cpp`: `BattleStatusSystem::apply` | Generic merge selects first-by-kind; `AddStack` clamps with the incoming cap and overwrites source/potency/origin. Poison, stun, and MP block already have specialized reducers that require separate characterization. |
| Aggregate queries | `src/battle/BattleStatusSystem.cpp`: `BattleStatusQuerySnapshot::stacks`, `potency`, `secondaryPotency` | Stacks sum, while both potency accessors take maximum. The mixed reducer policy is implicit and unsafe for heterogeneous contributions. |
| True-Qi hit behavior | `src/battle/BattleCoreAttacks.cpp`: `insertTrueQiHitDamage` | Reads the first True-Qi instance, asserts exactly one, multiplies potency by stacks, and derives order arithmetically. |
| Status identity/effect metadata | `src/ChessBattleEffectSemantics.cpp`: `statusCatalog`; `src/ChessBattleEffectSemantics.h`: `statusEffectFieldCatalog` | The shared catalog work is retained, but effect fields currently belong to a status name and lower to anonymous runtime slots. |
| Status YAML parser | `src/ChessBattleEffectParser.cpp`: status effect payload parsing | Enforces the name-owned payload shape. It will be replaced by context/action validation after the contribution storage is safe. |
| Generic effect rules | `src/ChessBattleEffectTypes.h`: `EffectRule`, `EffectActionValue`; `src/battle/BattleEffectSystem.*` | Reused rather than replaced. Status behavior adds an active rule view and contribution-relative context. |
| Damage area default | `src/ChessBattleEffectTypes.h`: `DamageArea`; parser `造成傷害` branch | Internal default is already `SingleTarget`, and authoring `範圍` is optional. The requested cleanup is schema/content/diagnostic work, not a combat change. |
| Damage target expansion | `src/battle/BattleCoreDamage.cpp`: `effectDamageTargetIds` | Retains the existing single/circle/square runtime behavior. |
| Description pipeline | `src/ChessEffectDescription*.cpp/.h` | Semantic-before-lowering architecture is retained; its status node changes from name-owned payload facts to contribution-local behavior rules. |

## Goals

1. Make multiple same-name status producers independently safe.
2. Keep caps, values, triggers, and actions local to the producer config.
3. Preserve and reuse the generic effect trigger/action architecture.
4. Remove anonymous runtime value slots from status semantics.
5. Remove name-specific behavior ownership unless the name represents a genuinely intrinsic runtime rule.
6. Make quantity aggregation, consumption, removal, expiry, and presentation explicit.
7. Preserve stable source attribution and exact command ordering.
8. Make authored YAML explain the mechanic without detached Boolean multipliers or identity coefficients.
9. Keep Compact, Full, and Detailed descriptions semantic and source-correct.
10. Migrate shipped content without maintaining legacy parsing forms.

## Non-goals

1. This design does not create a global status-definition YAML file.
2. It does not make every modifier, resource, area, or private state machine a status.
3. It does not introduce an arbitrary user-named runtime script language.
4. It does not add implicit strongest-wins behavior for same-name contributions.
5. It does not guarantee that two independent contributions collapse into one damage transaction.
6. It does not preserve old YAML spellings after migration.
7. It does not infer cross-producer compatibility from coincidentally equal display text.

## Vocabulary

### Status identity

`BattleStatusKind`, such as `TrueQi` / `真氣`.

Identity owns only facts that must be shared for grouping:

- authored and displayed label;
- quantity family and noun (`層`, `次`, `枚印記`);
- positive/negative/control classification;
- default cleanse grouping;
- runtime-owned versus authorable classification; and
- a small number of genuinely intrinsic group reducers, such as the effective control clock for stun; and
- an optional reviewed minimum behavior profile when the player-facing name makes a stable mechanical promise.

Identity does not own authored cap values, exact numeric tuning, or a list of permitted generic behaviors. A minimum profile is a whole-producer content contract, not a parser whitelist: it can require capabilities such as both healing prevention and speed reduction for `寒毒`, while still allowing that producer to add other lifecycle-valid behaviors.

### Producer

The configured action that applied the status. A stable producer key consists conceptually of:

```cpp
struct StatusProducerKey
{
    EffectSourceBinding binding;
    EffectRuleId ruleId;
    std::uint32_t actionOrder{};
};
```

`ruleOrder` is not identity. It is an execution-order token. Runtime aliases remain distinct because `EffectSourceBinding::runtimeInstanceId` participates in the binding.

### Producer family

The finite-capacity identity of one authored status-applying action. It intentionally excludes transient runtime alias identity and resolved formula values:

```cpp
struct StatusProducerFamilyKey
{
    EffectSourceKind sourceKind{};
    int sourceId{};
    int logicalOwnerUnitId{};
    EffectRuleId ruleId;
    std::uint32_t actionOrder{};
};
```

Capacity is not global for that key. It is scoped by the unit that holds the contribution:

```cpp
struct StatusFamilyCapacityKey
{
    int holderUnitId = -1;
    StatusProducerFamilyKey family;
};
```

The invariant is enforced independently for every `(producer family, holder unit)` pair. `logicalOwnerUnitId` identifies and groups the granting effect owner; it does not group different status holders into one shared pool. A single multi-target action therefore gives every selected holder its own complete family allocation.

The concrete lowering may retain more stable source-definition identity when cloned or borrowed rules require it, but it must satisfy these rules:

- rebinding the same authored rule for another cast does not create another family capacity;
- `runtimeInstanceId` distinguishes execution attribution and runtime state, but not family capacity;
- two apply actions in one rule remain different families because `actionOrder` differs;
- a genuinely different configured producer remains a different family; and
- the set of family keys a battle can create is finite and cannot grow merely because casts repeat.

Clone and borrow lowering use stable source-definition identity explicitly:

- an inherited clone rule keeps the original `sourceKind`, `sourceId`, `ruleId`, and `actionOrder`, but uses the clone unit as `logicalOwnerUnitId`; each finite spawned clone is therefore its own granting family on each holder;
- a borrowed rule keeps the borrowed source definition identity, uses the borrower as `logicalOwnerUnitId`, and excludes the transient `runtimeInstanceId` from the family key; repeated borrows of that definition by the same borrower share one capacity on a given holder; and
- the runtime producer key still retains clone/borrow execution identity needed for attribution and ordering.

The authored local limit belongs to the family. Every active contribution generation in the family draws from the same capacity:

```text
sum(quantity of active generations on holder matching family) <= family.localLimit
```

A second application with a different bound cap for the same family key is a definition/invariant error, not permission to mint another capacity. Current layer/charge limits are structural integers, so ordinary stat drift changes behavior values, not family capacity.

### Contribution

One independently owned stored application of a status identity.

```cpp
struct BattleStatusContribution
{
    BattleStatusKind kind{};
    StatusProducerKey producer;
    StatusProducerFamilyKey family;
    int sourceUnitId = -1;
    StatusQuantity quantity;
    StatusDurationState duration;
    BoundStatusBehavior behavior;
    BattleStatusEffectOrigin origin;
    StatusContributionRuntimeState runtime;
    std::uint64_t appliedSequence{};
};
```

The exact types may differ, but family capacity, behavior, source, and origin must not be anonymous mutable fields that an unrelated producer can replace.

### Status group

The view of all contributions of the same identity on one unit.

The group is used for:

- `has status` conditions;
- aggregate quantity queries;
- group removal and cleansing;
- runtime/UI summaries; and
- behavior arbitration when an action is intrinsically exclusive.

The group is a view/reducer, not a single mutable contribution.

### Behavior rule

A rule stored by a contribution and active while that contribution exists. It reuses the generic effect-rule concepts:

- event or persistent timing;
- observation relationship;
- selector;
- conditions;
- actions;
- activation limits;
- source metadata; and
- execution ordering.

### Compatibility

Two applications are compatible for local reapplication only when they have:

- the same producer key;
- the same producer-family key;
- the same status identity;
- the same quantity family;
- the same family-local limit;
- the same immutable bound behavior signature; and
- a reapplication policy that permits merging.

Same name alone is never sufficient.

## Core runtime invariants

1. A contribution's producer key, family key, behavior, source attribution, and origin are immutable for its lifetime.
2. Reapplication may change only the fields named by its policy: quantity, remaining duration, or explicit replacement generation.
3. For each holder independently, the checked sum of active generation quantities matching one producer family never exceeds that family's authored local limit.
4. Different holders never consume, clamp, replace, or free one another's family capacity. Applying one family on a holder never clamps another family on that holder; allocation may be limited only by the matching `(family, holder)` pair's remaining capacity.
5. Applying one contribution never changes another contribution's behavior values or origin.
6. Every evented status behavior has a non-null origin established at the application boundary.
7. Every status query iterates all matching contributions or uses a named reducer; no combat path silently reads the first instance.
8. Aggregate quantity uses checked/saturating arithmetic according to the existing numeric safety policy.
9. A per-layer action evaluates `contribution quantity × per-layer value` for that contribution only.
10. Per-layer numeric scaling creates one action execution unless `交易次數` explicitly requests several transactions.
11. Contribution iteration order is stable and independent of vector compaction.
12. Execution ordering uses a structured key, not arithmetic such as `ruleOrder + 1`.
13. A removed or expired contribution cannot leave an active behavior rule behind.

## Authoring model

### Status application remains local

The producer continues to carry its own quantity, cap, duration, reapplication, and effects.

The intended 九陽 shape is:

```yaml
- 時機: 攻擊提交
  目標: 自身
  動作:
    - 回復生命:
        基準: 來源最大生命
        固定: 60
        百分比: 3
        取整: 向零
    - 套用狀態:
        狀態: 真氣
        增加層數: 1
        層數上限: 10
        效果:
          - 時機: 命中
            目標: 命中目標
            造成傷害:
              每層數值: 9
              傷害種類: 純粹
```

This says, without an external definition:

- this producer adds one layer;
- this producer may hold at most ten layers;
- while the contribution exists, holder hits cause pure damage to the hit target; and
- each layer from this contribution is worth 9 damage.

There is no `累計狀態值`, no identity `百分比: 100`, no `每層: true`, and no redundant `範圍: 單體`.

### Stored behavior rules reuse the generic rule AST

`效果` becomes a sequence of status-local behavior rules. Event names, selectors, conditions, actions, and action metadata reuse the existing descriptors wherever their semantics are identical.

The status context supplies these bindings:

| Term | Meaning |
| --- | --- |
| `狀態持有者` | Unit whose contribution collection contains the status. |
| `狀態來源` | Unit that applied the contribution. |
| `來源效果擁有者` | Owner from the stored `EffectSourceBinding`. |
| `此狀態貢獻` | The contribution whose behavior rule is running. |

For ordinary holder-originated events, observation defaults to the status holder. The config does not repeat that default. Cross-owner mechanics such as 七星 must state their broader observation relationship explicitly.

The status-context `觀察範圍` vocabulary is:

| Value | Meaning |
| --- | --- |
| omitted / `狀態持有者事件來源` | Observe events whose source is the status holder. This is the default used by 真氣. |
| `狀態持有者事件目標` | Observe events whose target is the status holder. |
| `狀態來源事件來源` | Observe events whose source is the applying unit. |
| `來源效果擁有者同隊事件來源` | Observe event sources on the original effect owner's team. This is used by 七星. |

These labels resolve the observer relationship only. The normal rule `目標` still selects the unit that receives each action.

The final parser should share `EffectRule`, selector, condition, action, and `EffectNumber` descriptors. Status context adds only contribution-relative selectors/references and per-layer number evaluation. It must not duplicate the complete generic parser in a second switch.

### Persistent behavior

`時機: 持續` is a status-only timing that contributes query-time modifiers while the contribution is active. Its actions must be reversible/query-derived actions; it cannot enqueue one-shot world mutations every snapshot.

For example, 戰意 can be expressed with generic damage modifiers:

```yaml
- 套用狀態:
    狀態: 戰意
    增加層數: 1
    層數上限: 10
    效果:
      - 時機: 持續
        目標: 狀態持有者
        動作:
          - 傷害修正:
              方位: 造成
              階段: 防禦前
              傷害種類: 招式
              方式: 百分比加算
              每層數值: 5
          - 傷害修正:
              方位: 受到
              階段: 防禦前
              傷害種類: 全部
              方式: 百分比加算
              每層數值: -1
```

These are not freely chosen presentation labels. In the current hit pipeline, `BattleStatusSystem::snapshot` folds 戰意 into `skillDamagePct` and `damageReductionPct`; `BattleCoreAttacks` inserts those values into the outgoing-before-critical and incoming-base lanes, while `BattleDamageSystem` folds the same values before its ordered defense/reduction/cap steps. The generic status behavior must lower to those exact existing semantic inputs. It must not become a generic final-stage percentage merely because that form looks similar.

### Persistent modifier pipeline parity

Every migrated persistent status modifier has a normative pipeline insertion point. Migration may replace the storage and query mechanism, but it may not move the modifier across critical calculation, defense, ignore-defense, typed reductions, healing rounding, shields, per-hit caps, integer/fixed-point rounding, or transaction creation.

The parity rule is:

> A status-derived persistent modifier enters the same semantic accumulator, phase, relative order, and rounding boundary used by the current status fold. Relocation is permitted only when a Phase 0 equivalence test proves equality for every interacting path and the relocation is recorded as a deliberate behavior change.

The initial mappings are:

| Status capability | Required current-equivalent insertion |
| --- | --- |
| 戰意 outgoing skill damage | `skillDamagePct`; hit path `outgoingBeforeCritical`; transaction path before flat damage and defense. |
| 戰意 incoming reduction | `damageReductionPct`; hit path `incomingBase`; transaction path at the existing typed reduction fold. |
| 枯骨 incoming damage increase | Existing `damageTakenPct` accumulator and its present position relative to reduction, cap, and rounding. |
| 枯骨 received-heal reduction | Existing ordered `receivedHealMultipliersPct` sequence, including rounding after each multiplier. |
| 寒毒 speed reduction | Existing status-derived speed percentage accumulator and attribute-query order. |
| 寒毒 healing prevention | Existing heal-attempt gate, before any applied-heal side effects. |

The 戰意 example's incoming `方式: 百分比加算` with `每層數值: -1` is the exact sign convention of the current generic modifier vocabulary, not placeholder prose. For incoming `PercentAdd`, a negative amount lowers to positive `damageReductionPct`; the hit path likewise inserts the current positive status reduction as a negative `incomingBase` modifier. Phase 2 must pin both representations in equivalence tests. If the final schema instead introduces a positive-valued operation such as an explicit percentage-reduction verb, the example, descriptor, description, and lowering must change together; silently flipping the sign under `百分比加算` is forbidden.

The generic `傷害修正`/heal/attribute vocabulary is acceptable only when its lowering reaches those slots exactly. Phase 0 owns mixed-case equivalence tests with critical hits, defense, ignore defense, other configured modifiers, shields, caps, poison/bleed transaction paths, heal multipliers, and rounding boundaries.

### Cross-owner lifecycle example: 七星

七星 can keep its complete producer/consumer lifecycle with the contribution instead of relying on a separate rule that searches by status name:

```yaml
- 時機: 主彈命中
  目標: 命中目標
  套用狀態:
    狀態: 七星
    設定印記層數: 7
    持續幀數: 150
    重複套用: 取代同一效果
    效果:
      - 時機: 命中
        觀察範圍: 來源效果擁有者同隊事件來源
        目標: 命中目標
        條件:
          - 目標為狀態持有者
        動作:
          - 傷害修正:
              階段: 防禦前
              傷害種類: 招式
              方式: 忽略防禦百分比
              數值: 50
          - 消耗此狀態:
              消耗數量: 1
              最後一次:
                套用狀態:
                  狀態: 眩暈
                  持續幀數: 30
                  重複套用: 保留較長持續時間
```

`目標為狀態持有者` and `消耗此狀態` are status-context condition/action descriptors. They reuse the ordinary condition/action pipeline but bind directly to the executing contribution, so they do not repeat `狀態: 七星` or perform a first-by-name lookup.

### Per-layer numbers

Actions that accept a number may accept exactly one of:

```yaml
數值: 9
```

or, inside a status behavior rule:

```yaml
每層數值: 9
```

`每層數值` is valid only for the `Layers` quantity family. Trigger charges and marks use their event/consumption semantics; they do not multiply an action by their remaining count merely because they are countable.

`每層數值` contains a normal `EffectNumber`, so complex values remain possible:

```yaml
每層數值:
  每星級: 60
```

Evaluation is:

```text
evaluate(per-layer EffectNumber in stored source context)
× current quantity of this contribution
```

The multiplication produces one numeric action value. It does not repeat the action once per layer. Separate transactions require the existing explicit transaction-count mechanism.

`每層: true` is not part of status behavior authoring.

### Formula binding and evaluation phases

A stored behavior is **bound**, not necessarily reduced to a final integer at application time.

At application:

- producer-definition inputs such as martial-art star scaling are resolved/captured according to current semantics;
- source/application snapshot inputs that are intentionally snapshot-based are captured;
- the behavior rule structure, selector, conditions, action kinds, and ordering metadata become immutable; and
- the resulting bound parameters participate in contribution compatibility.

At behavior trigger:

- event inputs such as triggering damage, hit target, current HP, and target maximum HP are evaluated from that event's live context;
- `每層數值` multiplies the resulting per-layer number by the executing contribution's current layer quantity; and
- the final action value is range-checked before command creation.

Conceptually, numeric action inputs use:

```cpp
enum class StatusNumberScale
{
    Once,
    PerContributionLayer,
};

struct BoundStatusNumber
{
    BoundEffectNumber number;
    StatusNumberScale scale = StatusNumberScale::Once;
};
```

The concrete implementation may reuse an existing bound-formula representation, but it must preserve this two-phase contract. Compatibility compares immutable structure and application-bound parameters; it never compares a later event's transient result.

#### Closed binding-phase catalog

Binding phase is catalog metadata, not an evaluator guess. Every numeric input available inside a stored behavior has exactly one `StatusNumberBindingPhase`:

```cpp
enum class StatusNumberBindingPhase
{
    Literal,
    ApplicationBound,
    EventLive,
    ContributionLive,
};
```

For the current `EffectNumberBase` surface, the status-behavior binding catalog is:

| Numeric base | Phase inside a stored behavior | Normative meaning |
| --- | --- | --- |
| `Constant` / `固定值` | `Literal` | No runtime lookup. |
| `SourceStar` / `來源星級` | `ApplicationBound` | Applying effect owner's star at application. |
| `SourceAttack` / `來源攻擊` | `ApplicationBound` | Applying effect owner's attack at application. |
| `SourceMaxHp` / `來源最大生命` | `ApplicationBound` | Applying effect owner's maximum HP at application. |
| `SourceMissingHpRatio` / `來源已損生命比例` | `ApplicationBound` | Applying effect owner's missing/current maximum-HP pair at application; do not pre-round the ratio. |
| `SourceCurrentMpRatio` / `來源目前內力比例` | `ApplicationBound` | Applying effect owner's current/maximum-MP pair at application; do not pre-round the ratio. |
| `SourceStatusEffectValue` / `來源狀態效果值` | `ApplicationBound` | Legacy migration input captured at application; removed after named contribution values replace it. |
| `SourceStatusQuantity` / `來源狀態數量` | `ApplicationBound` | Legacy migration input captured at application; new local rules prefer `此狀態貢獻`. |
| `StoredStateValue` / `狀態槽值` | `ApplicationBound` | State-slot value captured before a borrowed/cast-scoped producer can disappear. |
| `TargetMaxHp` / `目標最大生命` | `EventLive` | Current selected behavior target maximum HP at the triggering event. |
| `TargetCurrentHp` / `目標目前生命` | `EventLive` | Current selected behavior target HP at the triggering event. |
| `TargetCurrentShield` / `目標目前護盾` | `EventLive` | Current selected behavior target shield at the triggering event. |
| `TargetCurrentCooldown` / `目標目前冷卻` | `EventLive` | Current selected behavior target cooldown at the triggering event. |
| `FinalHpDamage` / `實際生命傷害` | `EventLive` | The triggering damage transaction's resolved HP damage; legal only at events that provide it. |
| `ApplicationTargetMaxHp` / `套用目標最大生命` | `ApplicationBound` | Status recipient's maximum HP when the contribution is applied; introduced only for characterized snapshot parity such as `下次承傷上限`. |
| `此狀態貢獻數量`, `此狀態貢獻行為值` and `每層數值` scale | `ContributionLive` | Read from the still-live executing contribution after liveness revalidation. |

`ApplicationTargetMaxHp` is the only new application-target base approved by this design. If characterization confirms that `下次承傷上限` snapshots maximum HP when granted, its canonical migration uses `套用目標最大生命`, not the event-live `目標最大生命`. No other `ApplicationTarget…` or `EventSource…` base is part of this migration; a newly required input must amend this closed table before implementation rather than silently changing a `來源…` or `目標…` base's phase.

`base` and `multiplierBase` are classified independently. A mixed formula may capture its application-bound inputs and retain its event-live input, but rounding/minimum/maximum are applied only after the complete trigger-time value is assembled. Adding a numeric base without a phase, legality set, description phrase, binder, evaluator, and compatibility treatment must fail structurally at compile/test time.

### Damage range default

For `造成傷害`:

- omitted `範圍` means `SingleTarget` internally;
- explicit authoring values are only `圓形` and `方形`;
- `圓形` requires a positive `半徑格數`;
- `方形` requires a positive odd `方形邊長`; and
- explicit `範圍: 單體` is rejected with a focused diagnostic.

```text
「單體」是造成傷害的預設範圍；請省略欄位「範圍」
```

The selected action target remains the center for an area. `範圍` does not choose the target and does not describe cast range.

Detailed descriptions may show `傷害範圍：單體（預設）` as a resolved default. Full and Compact omit it unless an area is authored.

### Status name does not own generic actions

Validation is capability- and event-based.

For example, a cast-prevention action requires:

- a timing at which the current or remaining attack contacts can still be suppressed;
- live cast/contact provenance in the event context;
- a selector/relationship that identifies the cast to change; and
- an action compatible with that phase.

It does not require the outer status name to be `化勁` or `刺目`.

The old wrong-status diagnostic is replaced by diagnostics such as:

```text
狀態效果「使本次施放攻擊落空」需要尚未結算完成的施放接觸；事件「單位死亡」不提供此內容
```

If game design intentionally wants a named restriction, that restriction must be declared as a reviewed intrinsic status rule with a direct runtime reason. It must not emerge accidentally from a payload struct name.

### Reviewed minimum behavior profiles

Open parser capability does not mean a player-facing status label may lie. A producer of `寒毒` that authors only a speed penalty but omits healing prevention would be structurally executable, yet would violate the established game meaning of `寒毒`.

The status identity catalog therefore classifies every row as exactly one of:

- `Intrinsic`: the runtime/group reducer itself supplies the named contract, as for the effective stun clock;
- `Profiled`: every authored producer must contain a reviewed minimum set of generic behavior capabilities; or
- `OpenMarker`: the name is only a grouping/link identity and has no implied behavior bundle.

A status qualifies for a minimum profile only when all of the following hold:

1. the label or shipped player-facing status presentation makes a stable mechanical promise without always showing the producer-local rule beside it;
2. omitting the capability while retaining the label would be misleading independent of numeric tuning;
3. the capability is expected from every producer of that identity, not merely one martial art; and
4. the contract can be expressed as generic capability tags and relationships rather than an exact payload struct or numeric value.

Profiles require minimum capabilities and allow additional lifecycle-valid behavior. They do not own values, caps, duration, source, action order, or a list of forbidden generic fields. The ordinary parser/schema remains open; a whole-content validation pass compares each complete producer (including contribution-local and explicitly linked rules) with the profile after parsing. Contract tests derive from the same profile catalog, so there is no second name-keyed parser switch.

The initial reviewed classification is:

| Status | Classification | Minimum player-contract capabilities |
| --- | --- | --- |
| `中毒` | `Profiled` | Periodic current-HP damage and trigger consumption; its separately cataloged group reducer owns strongest/replace/same-event arbitration. |
| `流血` | `Profiled` | Periodic maximum-HP bleed damage scaled by this contribution's layers. |
| `眩暈` | `Intrinsic` | Effective control clock and its reviewed cross-source duration policies. |
| `封內` | `Intrinsic` | MP-block presence for the effective duration group. |
| `寒毒` | `Profiled` | Persistent healing prevention **and** speed reduction. |
| `枯骨` | `Profiled` | Persistent incoming-damage increase **and** ordered received-heal reduction. |
| `七星` | `Profiled` | Observe the reviewed allied-hit relationship, modify the eligible hit, consume this producer's mark, and apply depletion stun. |
| `化勁` | `Profiled` | Suppress the eligible cast contacts and grant the original-target shield. |
| `刺目` | `Profiled` | Suppress the eligible cast contacts. |
| `下一次受到攻擊必定落空` | `Profiled` | Make one eligible incoming attack miss and consume only on success. |
| `傷害抵擋` | `Profiled` | Block one eligible positive non-execute damage transaction and consume only on success. |
| `下次承傷上限` | `Profiled` | Cap one eligible incoming damage transaction and consume only on success. |
| `戰意` | `Profiled` | Per-layer outgoing skill-damage increase **and** incoming damage reduction at their parity pipeline slots. |
| `真氣` | `Profiled` | Holder-hit pure damage scaled by this contribution's layers. |
| `毒爆` | `Profiled` | Contribution-local death explosion damage and the reviewed poison application. |
| `無影` | `OpenMarker` | No name-implied payload bundle; whole-content validation still requires every produced marker to have a source-correct linked consumer. |
| `下一次攻擊必定暴擊` | `Intrinsic` / runtime-owned | Existing runtime-owned next-attack critical behavior. |

Phase 0 must verify this table against player-visible descriptions and shipped behavior before the profiles become load-time contracts. Changing a profile after that is a game-design change, not parser maintenance.

### Cast-suppression naming

The current field `阻止本次施放` is itself misleading. The current runtime does not undo the cast animation, MP payment, or cooldown. At contact resolution it suppresses the current and remaining contacts belonging to that cast.

The canonical behavior action is therefore:

```yaml
效果:
  - 時機: 命中
    動作:
      - 使本次施放攻擊落空: {}
```

`化勁` attaches its shield as a success continuation of the exclusive action:

```yaml
- 使本次施放攻擊落空:
    成功後:
      - 原攻擊目標獲得護盾:
          每星級: 100
```

`刺目` needs only the suppression action. Suppression, original-target selection, shield grant, and success continuation are reusable behavior capabilities and are not parser-owned by those two status names.

## Contribution identity and reapplication

### Producer key

The command that applies a status must carry:

- full `EffectSourceBinding`;
- logical producer-family identity;
- producer `EffectRuleId`;
- producer `actionOrder`;
- stable execution-order key; and
- source unit attribution.

Two actions in one effect rule that apply the same status remain distinct because `actionOrder` participates in both the producer key and family key.

Borrowed/cast-scoped rules remain distinct contributions because their runtime binding instance participates in the producer key. The transient instance does not participate in the family key. Removing the borrowed rule does not invalidate a stored contribution's copied metadata, and rebinding the same authored producer cannot allocate another full family cap.

### Local additive reapplication

For a layer contribution:

```yaml
增加層數: 1
層數上限: 10
```

means:

> Add one layer to a compatible generation from this producer, capped by the remaining capacity of this producer family's local limit of ten.

It does not mean:

> Find any `真氣` object and clamp all of its layers to ten.

For a positive additive request, allocation is explicit:

```text
used = checkedSum(quantity of active generations on holder matching producerFamily)
available = family.localLimit - used
allocated = min(requestedIncrease, max(0, available))
```

The allocated quantity goes to the compatible producer generation on that holder or to a new immutable-value generation on that holder. A zero allocation creates no empty contribution. The family limit is stored with/validated against that holder's active family records; this is not a new global status-definition service.

### Payload changes from the same producer family

Bound behavior and its application-captured parameters are immutable. If the same producer family later binds a different behavior value:

- an additive policy may create a new contribution generation rather than rewriting old layers, but may allocate only the family's remaining capacity;
- an explicit `取代同一效果` policy removes older generations in that producer family **on the target holder only** and creates the replacement within that holder's family capacity; and
- a duration-only refresh changes duration only, not old behavior values.

This prevents a temporary stat change from silently upgrading or downgrading already stored layers.

The authored family limit is not a drifting behavior value. A request that resolves the same family key with a different limit is rejected as inconsistent producer definition. It does not fork another generation or choose max/latest.

Repeated borrowed generations illustrate the distinction:

```text
九陽 producer family capacity: 10
├─ borrowed runtime instance A: 6 layers at value 9
└─ borrowed runtime instance B: at most 4 additional layers at value 12
```

Instance B retains its own origin and immutable value, but the family total on this holder cannot exceed ten. Another holder has an independent ten-layer allocation for the same family. Expiry, removal, consumption, or explicit family replacement frees capacity only on the holder whose contribution changed.

### Explicit cross-group policies

Some existing mechanics intentionally arbitrate across sources. Examples include strongest poison and the effective stun clock. These remain explicit, named mechanics rather than generic same-name merge behavior.

The application semantic must say what it does to the group:

- `保留較高傷害` compares eligible poison contributions using the poison damage comparator;
- `取代整組` removes every selected same-name group contribution and creates the replacement;
- `取代同一效果` removes only generations in the applying producer family on the target holder and creates the replacement there;
- stun `延長持續時間`, `保留較長持續時間`, and `取代持續時間` modify the effective control clock according to their documented rules.

These operations may select/remove contributions, but they cannot reuse the generic `AddStack` branch or anonymous `(potency, secondaryPotency, stacks)` tuple.

No generic cross-group cap/effect resolver is added in this migration.

## Group queries and aggregation

### Presence

`自身有狀態: 真氣` is true when at least one matching contribution has positive effective quantity and has not expired.

### Quantity

The default group quantity is the checked sum of contribution quantities:

```text
groupQuantity(kind, filter) = Σ contribution.quantity
```

Source filters may restrict the sum to:

- all contributions;
- contributions applied by a unit;
- contributions owned by an effect binding; or
- the currently executing contribution.

The existing source-sensitive 七星 lifecycle must use an owner/binding filter, not merely the status name.

### Capacity

Storage never has a group cap. A presentation-only derived capacity may be:

```text
Σ distinct active producerFamily.localLimit on this holder
```

when every displayed family on the queried holder has the same quantity family and a finite local limit. Generations and runtime aliases in one family are counted once per holder. This derived group value must not be fed back into application, compatibility, or consumption logic, and no UI/query aggregates family capacity across holders.

When a concise `current / maximum` display would hide materially different behaviors, the UI shows total quantity without a single cap and provides a contribution breakdown.

### Behavior values

The APIs `potency(kind)` and `secondaryPotency(kind)` are removed. Their implicit maximum reducer is not meaningful for heterogeneous contributions.

Callers instead use one of:

- iterate matching contributions;
- sum a named behavior value with its declared aggregation;
- collect independent modifiers in deterministic order;
- select a contribution using an explicit behavior comparator; or
- execute every matching behavior rule.

For additive per-layer values:

```text
aggregate value = Σ (contribution.quantity × contribution.perLayerValue)
```

For sequential healing multipliers, the existing ordered-vector behavior remains sequential rather than being collapsed into one scalar.

For strongest-wins mechanics, the behavior action owns a strongest-wins reducer explicitly.

### Source status numeric references

`來源狀態數量` means the sum of matching contribution quantities after applying its source filter.

`來源狀態效果值` must name a behavior value and its reducer. Additive values default to sum only when the behavior catalog declares sum as their natural aggregation. No reference silently reads maximum potency.

Within a stored behavior rule, `此狀態貢獻` references are preferred because they cannot accidentally combine unrelated producers.

## Triggering and arbitration

### Additive behaviors

Additive behaviors execute once per matching contribution in stable execution order. Examples:

- True-Qi hit damage;
- persistent outgoing damage additions;
- persistent incoming damage modifiers; and
- independently ticking damage-over-time contributions.

Two contributions are not coalesced across different origins merely because their final numeric values could be added. Coalescing is allowed only when metadata, transaction semantics, origin, ordering position, modifier flags, invincibility behavior, and presentation are identical.

### Exclusive behaviors

Some actions are inherently exclusive for one event:

- prevent a cast;
- make one incoming attack miss;
- block one damage transaction; or
- consume one charge to cap one hit.

The dispatcher evaluates eligible contribution rules in deterministic order and revalidates live eligibility after each reduced command. Once an earlier rule prevents or consumes the transaction, later rules must not consume charges for an effect that no longer applies.

This arbitration belongs to the action/event contract, not to a list of status names.

The winner order is player-observable and normative. Configured rules and active contribution rules are sorted by the complete structured execution key described below, ascending. The first still-eligible command whose reduction makes the exclusive predicate false wins; every later contender is revalidated and skipped without consumption. There is no status-name priority between heterogeneous contenders.

Consequently, when `化勁` and `刺目` both contend for one attack contact, the earlier structured key determines whether the winning action includes 化勁's shield grant. The losing charge remains available for a later eligible event. This intentionally differs from the current typed loop, which consumes both statuses and grants the 化勁 shield regardless of their application order.

Dependent effects of an exclusive action are success continuations, not independent pre-gathered commands. Reduction either commits the exclusive action, its charge consumption, and its success continuation under one outcome, or commits none of them. Thus a losing 化勁 contribution cannot grant a shield after its suppression command fails live revalidation.

### Periodic behaviors

Periodic runtime state belongs to the contribution. Each contribution has its own tick countdown and remaining trigger charges. Expiring or removing one contribution cannot reset another contribution's tick.

Same-event poison aggregation remains a poison behavior policy. It must group only the applications selected by that policy and must not become a generic same-name overwrite.

### Death and depletion behaviors

Death and depletion effects should be stored with the contribution that supplies the values they use. This removes duplicated producer/consumer constants and makes source ownership explicit.

`毒爆` and `七星` are target migration cases:

- `毒爆` stores its death behavior with each contribution and uses that contribution's layer quantity/value;
- `七星` stores its hit observation, defense modification, mark consumption, and depletion stun with the mark contribution.

Generic explicit lifecycle rules may remain when they genuinely coordinate outside the status lifetime, but the description document must link them by contribution identity rather than status name alone.

## Execution order and provenance

### Structured order key

Status behavior commands need an order immediately associated with their producer without using `origin.ruleOrder + 1`.

Conceptually:

```cpp
struct EffectExecutionOrderKey
{
    int sourcePrecedence{};
    std::uint32_t producerRuleOrder{};
    EffectExecutionLane lane{};
    std::uint32_t producerActionOrder{};
    std::uint32_t behaviorRuleOrder{};
    std::uint64_t contributionSequence{};
};
```

Comparison is lexicographic in the field order shown, ascending. `sourcePrecedence` is derived by the authoritative existing mapping used by `effectSourceRuleOrderLess`: Combo, Equipment, EquipmentSynergy, Neigong, then Magic. It is not the raw `EffectSourceKind` enum ordinal. `producerRuleOrder` is the existing globally allocated, immutable registration-order token within that source precedence. The status-behavior lane sorts after its producer rule's normal lane and before the next producer rule order in the same source precedence. Action order, authored behavior order, and finally monotonic contribution sequence break all remaining ties. No two executable rules may remain unordered.

This preserves current mixed-source precedence while avoiding overflow, collision, enum-order accidents, and dependence on vector indices. Rebuilding event indices filters/reindexes lookup storage only; it must not renumber a live producer order token already copied into a contribution. A later borrowed binding receives a later producer order token within Magic precedence even when it shares family capacity with an earlier generation.

The existing monotonic rule-order allocation remains. Rebuilding event indices must never renumber live order tokens.

### Provenance

Every command created by a status behavior retains:

- original source kind and ID;
- original effect owner;
- source unit attribution;
- producer rule ID and action order;
- derived behavior rule identity;
- status identity and contribution sequence for diagnostics; and
- cast/hit provenance available from the triggering event.

Kill credit, presentation skill name, descendants, reflection lineage, damage modifiers, hurt invincibility, and event dispatch must be characterized before migration.

### Active rule integration

Active status behaviors should not be copied into the permanent rule store as ordinary configured rules. Instead, event dispatch gathers two rule views:

1. configured/bound effect rules; and
2. active status-contribution behavior rules.

Both views use the same eligibility, selector, condition, evaluation, command, ordering, and reduction helpers. Contribution removal makes its rule view disappear atomically.

This prevents leaked rules and avoids mutating the permanent rule store on every status application or expiry.

Event dispatch snapshots stable contribution identifiers, not references into the status vector. A contribution created during an event becomes eligible starting with the next event; it cannot retroactively join the dispatch that created it. Before each snapshotted contribution rule executes, the dispatcher confirms that the contribution still exists and remains eligible. This permits an earlier command to remove or consume a later contribution without iterator invalidation or same-event recursive activation.

## Removal, cleansing, consumption, and expiry

### Group removal

An unfiltered action such as:

```yaml
移除狀態: 真氣
```

removes the entire `真氣` group, including every contribution. This matches the player-visible meaning of removing that status.

Source-filtered removal may remove only matching contributions when a mechanic explicitly asks for it.

### Cleanse counts groups, not contributions

`移除狀態` with `僅負面`, `僅控制`, or a positive count chooses status identities/groups according to the authored order, then removes all selected contributions in each group. Adding another poison source must not make poison consume two cleanse slots.

### Quantity consumption

Consumption first filters contributions by identity and source relationship. It then removes quantity deterministically.

Default order is oldest matching contribution first, matching stable application order. When consuming heterogeneous contributions is player-observable, authoring must state an order such as newest, weakest, or strongest rather than relying on the default.

The current 七星 consumer remains source-owner filtered, so it consumes only the matching mark contribution.

`whenDepleted` / depletion behavior is evaluated for the contribution that actually reached zero, not for the aggregate group unless explicitly authored as a group depletion condition.

### Expiry

Each contribution tracks and expires its own duration. A group remains present while any contribution remains. Effective duration display defaults to the maximum remaining contribution duration, with a breakdown when contributions differ.

Stun and MP-block group-clock semantics remain dedicated reducers because their current reapplication policies intentionally operate on the effective clock.

Status shield, stagger shield, control immunity, and low-HP control immunity remain application-boundary protections. They run before a contribution is created or locally reapplied. When protection shortens a duration, only the surviving incoming duration enters the contribution/group reducer; an unrelated existing contribution is not rewritten.

## Status identity catalog after the migration

The catalog remains authoritative, but its responsibility narrows.

It owns:

- status label;
- authorability;
- quantity model and nouns;
- polarity/control flags;
- cleanse category;
- runtime-owned classification;
- group presence/duration presentation; and
- any reviewed intrinsic group reducer;
- `Intrinsic` / `Profiled` / `OpenMarker` classification; and
- minimum capability tags for `Profiled` identities.

It does not own:

- the cap used by every producer;
- the only legal behavior fields for a name;
- a producer whitelist;
- exact numeric values or an exact YAML payload shape for a minimum profile;
- anonymous runtime slots;
- a fixed execution origin; or
- the assumption that the group contains one instance.

The behavior/action catalog separately owns:

- authored labels;
- payload shapes;
- numeric constraints;
- compatible events and contexts;
- aggregation/arbitration semantics;
- description phrases;
- runtime lowering/evaluation; and
- schema probe values.

Parser, schema, validation, descriptions, and runtime visit the same action/behavior descriptors. Agreement tests are structural, not merely count or hash pins.

Whole-content profile validation runs after a complete producer and its explicitly linked rules are available. It compares generic semantic capability tags and relationships, not source field names. A profile catalog row must have generated positive/negative fixtures; adding a status without selecting `Intrinsic`, `Profiled`, or `OpenMarker` fails structurally.

## Status migration matrix

The characterization phase must confirm each row before changing behavior.

| Status | Contribution storage | Behavior target | Group-specific rule |
| --- | --- | --- | --- |
| `中毒` | Producer-owned periodic contribution | Generic periodic damage and charge consumption | Existing strongest/replace/same-event policies remain explicit poison arbitration. |
| `流血` | Producer-owned layered contribution | Periodic maximum-HP damage using `每層數值` | No implicit cross-source cap/value overwrite. |
| `眩暈` | Effective control-clock group | No arbitrary payload | Existing extend/keep-longer/replace duration rules remain the group reducer. |
| `封內` | Effective duration group | MP-block presence | Existing keep-longer/replace duration reducer. |
| `寒毒` | Producer-owned duration contribution | Persistent speed and healing-transaction behaviors | Aggregate compatible modifiers; no latest-source overwrite. |
| `枯骨` | Producer-owned duration contribution | Persistent incoming-damage and healing modifiers | Preserve ordered heal rounding. |
| `七星` | Producer-owned marks | Generic observed-hit rule, consume-this-contribution, depletion stun | Source-owner filtering is mandatory. |
| `化勁` | Producer-owned trigger charges | Generic cast-contact suppression and original-target shield actions | Exclusive action arbitration prevents duplicate charge waste. |
| `刺目` | Producer-owned trigger charges | Generic cast-contact suppression action | Same arbitration as any cast-suppression behavior. |
| `下一次受到攻擊必定落空` | Producer-owned trigger charges/duration | Generic incoming-attack modification | Consume only the winning contribution. |
| `傷害抵擋` | Producer-owned block charges | Generic damage-transaction block | Consume only when the block actually wins. |
| `下次承傷上限` | Producer-owned trigger charges | Generic incoming-damage cap | Preserve modifier phase and rounding. |
| `戰意` | Producer-owned layers | Persistent per-layer damage modifiers | Aggregate each contribution independently. |
| `真氣` | Producer-owned layers | Generic holder-hit damage rule | Execute every contribution at its own origin. |
| `毒爆` | Producer-owned layers | Generic holder-death behavior | Use the dying contribution's quantity/value. |
| `無影` | Producer-owned marker | Explicit linked or stored behavior | Link by producer identity. |
| `下一次攻擊必定暴擊` | Runtime-owned | Existing runtime system | Not authorable and not forced through this migration. |

Names in the final table must use the exact authoritative Traditional Chinese labels. The implementation audit should generate this table from the catalog or test every catalog row so future additions cannot bypass contribution semantics.

## Description AST and presentation

### Semantic description node

The description document receives the producer-local status application before runtime resolution:

```cpp
struct DescriptionStatusApplication
{
    BattleStatusKind identity{};
    DescriptionStatusQuantity quantity;
    std::optional<DescriptionDuration> duration;
    DescriptionReapplication reapplication;
    std::vector<DescriptionStatusBehaviorRule> behaviors;
    DescriptionContributionScope scope;
};
```

It never reconstructs behavior from `potency`, `secondaryPotency`, or the name `真氣`.

### Static descriptions describe the local contribution

An ability description answers:

> What does this ability contribute?

It does not claim that its cap or value is global for every same-name status.

Use wording such as:

```text
此效果提供的真氣最多10層
```

when another producer could coexist.

Full and Detailed always make a producer-family cap's scope clear with wording such as `此效果提供的` or `該來源最多`. The wording must not depend on whether shipped content happens to have a second producer today. Several immutable generations of one source share that displayed cap; they must not be shown as several independent maxima.

### Runtime status presentation

The runtime group summary may show:

```text
真氣 9層
```

When contributions differ, Detailed/tooltip expansion shows:

```text
九陽神功：7/10層；每層命中附加9點純粹傷害
另一武功：2/4層；每層命中附加15點純粹傷害
```

The UI must not show `9/10` by borrowing one producer family's cap for the complete group.

### 九陽 target descriptions

Full:

```text
攻擊提交時，回復60點加最大生命3%的生命，並獲得1層真氣；此效果提供的真氣最多10層。持有者命中時，每層真氣對命中目標附加9點純粹傷害。
```

Compact:

```text
攻擊提交：回血60+最大生命3%，真氣+1層
  此來源最多10層；每層命中附加9純粹傷害
```

Detailed excerpt:

```text
狀態貢獻：真氣
生產者：九陽神功／套用狀態動作
數量：增加1層
本效果來源共用上限：10層
重複套用範圍：同一效果來源內相容的貢獻世代
觸發：狀態持有者命中
目標：命中目標
動作：每層造成9點純粹傷害
傷害範圍：單體（省略欄位後的預設值）
```

### 降龍 target descriptions

Full:

```text
獲得1層戰意；此效果提供的戰意最多10層。每層使招式傷害提高5%、受到傷害降低1%。
```

When showing the configured producer family's full local capacity:

```text
此效果的戰意達10層時，招式傷害共提高50%、受到傷害共降低10%。
```

Compact:

```text
戰意+1層（此來源最多10層；每層增傷5%、減傷1%）
```

Detailed must identify two persistent per-layer modifier behaviors rather than reporting anonymous primary and secondary strength.

### 七星 target descriptions

Full:

```text
主彈命中時，對該敵人施加7枚七星印記，持續150幀；同一來源再次施加時會重設印記與持續時間。來源效果擁有者的任一友軍命中該敵人時，該次招式忽略50%防禦並消耗此來源的1枚印記；該貢獻的印記耗盡時，使敵人眩暈30幀。
```

Compact:

```text
主彈命中：施加7枚七星印記（150幀；同來源重設）
  友軍命中：破防50%、耗此來源1枚；耗盡時眩暈30幀
```

Detailed must show producer identity, broader observation relationship, source-filtered consumption, and contribution-local depletion.

### 九陰白骨爪 target description

Full:

```text
主彈命中時，使目標進入枯骨狀態120幀；同一來源再次施加會刷新持續時間。狀態期間，目標受到的傷害提高25%、受到的治療降低75%。
```

The description comes from the stored persistent behaviors, not a name-specific phrase that exists only because the status is called `枯骨`.

### Compact, Full, and Detailed contracts

- `Compact` may shorten nouns and split rows, but cannot drop cap scope, per-layer meaning, source-sensitive consumption, or area behavior.
- `Full` contains every player-observable rule in natural prose.
- `Detailed` includes resolved defaults, producer/contribution scope, behavior event context, source filters, execution ordering, and source-field coverage.
- None of the three styles infer behavior from status identity.

## Parser, schema, and diagnostics

### One behavior/action vocabulary

The parser and schema renderer consume the existing action descriptors plus status-context descriptors. Adding a new action numeric field or status-relative selector must update one authoritative descriptor consumed by:

- parser;
- validator;
- generated schema;
- description coverage;
- runtime evaluator; and
- fixture generation.

The exhaustive variant visitor remains the structural guarantee. Hash/count pins are supplemental regressions only.

### Structural validation

Validation checks:

- quantity field matches the status identity's quantity family;
- family-local limit is present exactly when required by the quantity operation and cannot vary across one producer family;
- duration policy is compatible with the quantity/lifecycle shape;
- behavior event has all required live context;
- selector is legal at that event;
- action is legal at that event;
- `每層數值` appears only in a contribution whose quantity family is `Layers`;
- `數值` and `每層數值` are mutually exclusive;
- persistent timing contains only persistent/query-derived behaviors;
- group-destructive reapplication is explicitly authored;
- source-sensitive consumers select contributions unambiguously; and
- behavior numeric constraints remain safe after maximum family quantity multiplication;
- every numeric base has exactly one binding-phase catalog row and is legal in its application/event context; and
- every complete `Profiled` producer satisfies its reviewed minimum capability set.

Validation does not reject a behavior merely because another status name currently uses it.

### Required diagnostics

Examples:

```text
「每層數值」只能用於以層數計量的狀態貢獻；狀態「眩暈」不使用層數
```

```text
「造成傷害」不可同時使用「數值」與「每層數值」
```

```text
狀態效果「使本次施放攻擊落空」需要尚未結算完成的施放接觸；事件「單位死亡」不提供此內容
```

```text
「單體」是造成傷害的預設範圍；請省略欄位「範圍」
```

Cap and value drift use separate diagnostics because they have different semantics:

```text
狀態「真氣」的同一效果使用不同的層數上限；同一效果來源的上限必須一致
```

This is a content/invariant error.

```text
狀態「真氣」的同一效果解析出不同的行為值；將建立共享原上限的新世代，不會覆寫既有層數
```

This is Detailed/debug instrumentation, not necessarily a content error; the design permits a value generation safely within remaining family capacity.

Minimum-profile diagnostics name both the status and missing capability:

```text
狀態「寒毒」缺少必要行為「禁止受到治療」；每個「寒毒」來源都必須同時提供治療阻止與速度降低
```

## Implementation phases

### Phase 0: characterization and review gates

No production behavior changes.

1. Add a two-producer test harness capable of applying the same status identity with different bindings, action orders, caps, values, durations, and origins.
2. Characterize every current status catalog row across application, query, tick, consume, remove, cleanse, expiry, shield interaction, description, and serialization surfaces.
3. Pin current shipped-content cases separately from hypothetical multi-producer cases.
4. Characterize mixed-source scheduling with rules from Combo, Equipment, EquipmentSynergy, Neigong, and Magic in the same event. Prove the current `(effectSourcePrecedence, registration order)` sequence and then characterize True-Qi relative to neighboring configured rules, main-projectile-before-damage rules, and repeated borrowed-rule add/remove cycles.
5. Characterize damage transaction metadata: modifier flags, hurt invincibility, kill credit, presentation, descendants, reflection lineage, and events.
6. Characterize poison strongest/equal/weaker/replace/same-event behavior.
7. Characterize stun and MP-block group clocks across different sources and policies.
8. Characterize source-sensitive 七星 consumption and depletion.
9. Characterize cleansing count semantics with several same-name independent instances.
10. Record current Compact, Full, and Detailed outputs for 九陽, 降龍, 七星, 九陰白骨爪, poison, and one charge-based interceptor.
11. Characterize every persistent status fold at its exact hit/transaction/heal/attribute pipeline position. For 戰意, cover critical, defense, ignore defense, configured before/after/final modifiers, shields, caps, pure/non-skill damage, and rounding; add corresponding mixed cases for 枯骨 and 寒毒.
12. Characterize every current status-payload `EffectNumberBase`, including `base`/`multiplierBase` mixtures, and approve the closed binding-phase table. Pin snapshot behavior for application-target values such as `下次承傷上限`.
13. Characterize the current single bleed transaction versus hypothetical independent bleed producers, including shield, invincibility, modifier, presentation, and tick-alignment interactions.
14. Pin current `化勁` + `刺目` behavior: both are consumed on one contact and 化勁 grants its shield. Separately approve the proposed winner-only behavior and its exact structured-order winner.
15. Audit every authoritative producer and every clone/borrow path for cross-source overwrite dependencies. `流血` and `傷害抵擋` are known non-poison/stun/MP-block users of current same-name storage and must be decided explicitly; poison, stun, and MP block are not declared the complete retained reducer set until this audit passes. Explicitly cover all three shipped `生成分身` threshold sites in `chess_combos.yaml` (one, two, and three clones): cloned owners inherit every non-opening, non-cast-scoped rule, so the audit must determine which inherited rules can apply status, prove their family source-definition identity, and prove each clone receives a finite distinct `logicalOwnerUnitId` rather than an unbounded runtime alias family.
16. Review every catalog row's `Intrinsic` / `Profiled` / `OpenMarker` classification and minimum capability set against player-facing descriptions.
17. Characterize current gather/reduce behavior for same-event status creation/removal, and approve the proposed snapshot-plus-liveness semantics as a deliberate change where it differs.
18. Review which observed behaviors are intentional. The implementation plan may change only after the intended behavior is recorded in this document.

Exit gate: tests describe every intentional behavior that later phases preserve or deliberately change.

### Phase 1: contribution storage behind the current authoring surface

1. Introduce `StatusProducerKey`, `StatusProducerFamilyKey`, and holder-scoped `StatusFamilyCapacityKey`; include producer action order, logical family identity, and target holder identity in `ApplyStatusEffectCommand` and `BattleStatusApplyRequest`.
2. Replace `BattleTypedStatusInstance` with a contribution representation that stores immutable identity, holder-scoped family-capacity ownership, source, resolved payload, origin, and sequence.
3. Change generic reapplication lookup from first-by-kind to holder/producer/family/compatibility lookup, enforcing the checked family-capacity invariant across generations on that holder only.
4. Preserve special poison, stun, and MP-block reducers as dedicated group operations.
5. Replace `find(kind)` combat access with contribution iteration or named group reducers.
6. Make remove/cleanse operate on groups and source-filtered removal operate on contributions.
7. Make consumption deterministic and contribution-aware.
8. Update snapshots and serialization/DTOs to retain contribution identity.
9. Keep the existing semantic YAML temporarily; lower it into the new contribution representation.

Exit gate: current shipped behavior remains characterized; hypothetical same-name different-family applications cannot overwrite each other; value drift and repeated borrowed aliases cannot multiply one producer family's capacity on a holder; and one multi-target application gives every holder an independent allocation.

### Phase 2: behavior/action catalog independent of status name

1. Split status identity metadata from behavior/action metadata.
2. Remove `BattleStatusKind status` ownership from generic behavior field entries.
3. Replace name-specific payload structs with composable behavior rules/actions or an equivalent generic resolved representation.
4. Add status-context selectors and references (`狀態持有者`, `狀態來源`, `來源效果擁有者`, `此狀態貢獻`).
5. Add `時機: 持續` with a closed set of persistent/query-derived actions.
6. Add `每層數值` to compatible numeric actions and remove Boolean per-layer contribution flags.
7. Derive parser, schema, validation, runtime evaluation, and description metadata from the shared action descriptors.
8. Add the closed numeric binding-phase catalog and structural exhaustiveness checks for binding, evaluation, compatibility, descriptions, and legality.
9. Add reviewed minimum behavior profiles as whole-content contracts derived from the identity/action catalogs.
10. Replace wrong-status diagnostics with event/action capability diagnostics plus focused missing-profile diagnostics.
11. Add a positive fixture proving that a profiled status can use an additional nontraditional behavior when its lifecycle is valid and its minimum profile remains complete.

Exit gate: adding a behavior to a status does not require editing a switch keyed by that status name.

### Phase 3: active status rules in generic dispatch

1. Represent evented contribution behaviors as active rule views.
2. Gather configured rules and active contribution rules in the same dispatch operation.
3. Introduce `EffectExecutionOrderKey` and replace arithmetic intrinsic ordering.
4. Reuse generic condition, selector, formula, action, activation, and command reducers.
5. Add sequential live revalidation for exclusive/consumptive actions.
6. Migrate True-Qi hit damage to an ordinary stored behavior rule.
7. Remove `insertTrueQiHitDamage`, the single-instance assertion, and intrinsic True-Qi rule-ID synthesis once parity tests pass.
8. Migrate periodic and death/depletion behaviors where doing so removes special bridges without changing intended behavior.

Exit gate: no combat path needs to read the first status instance, True-Qi uses the generic dispatcher, and heterogeneous exclusive winners follow the documented complete key.

### Phase 4: canonical YAML and description migration

1. Change `效果` to the reviewed sequence of local behavior rules.
2. Migrate all top-level authoritative `config/chess_*.yaml` status producers.
3. Replace per-layer Boolean flags with `每層數值`.
4. Remove redundant identity `百分比: 100` nodes where a direct status-relative value is intended; retain genuine 100-percent game values.
5. Omit `範圍` for every single-target damage action.
6. Restrict explicit damage-range schema values to `圓形` and `方形` and reject `範圍: 單體`.
7. Migrate Compact, Full, and Detailed descriptions to contribution-aware nodes and phrases.
8. Update schema examples, generated documentation, parser fixtures, and error goldens.
9. Migrate reapplication vocabulary to the scope-explicit `取代同一效果` and `取代整組`; do not retain `取代並重設` as an alias.
10. Remove the old four name-owned status effect-scope authoring forms after all content migrates.

Exit gate: shipped YAML contains the behavior in each producer, has one canonical spelling, and does not depend on name-specific payload ownership.

### Phase 5: runtime and metadata cleanup

1. Remove `potency` and `secondaryPotency` from status storage and requests.
2. Remove group `potency(kind)` / `secondaryPotency(kind)` maximum reducers.
3. Remove obsolete status-specific payload structs and field-to-runtime-slot mappings.
4. Remove old parser/schema aliases and temporary lowering adapters.
5. Remove unused accumulated-status bridges and identity formula paths.
6. Remove stale description archetypes that reconstruct stored behavior from names.
7. Make structural agreement tests enumerate every behavior variant, context, field, description phrase, validator, and runtime evaluator.
8. Run full content validation, all unit tests, Debug build, and `git diff --check`.

Exit gate: there is one canonical authoring and runtime model with no compatibility surface.

## File-level implementation map

Line numbers are intentionally omitted because these files are under active migration. Reviewers should use the named symbols.

| Area | Primary files and symbols | Expected work |
| --- | --- | --- |
| Canonical effect types | `src/ChessBattleEffectTypes.h`: `ApplyStatusAction`, `StatusEffectPayload`, `EffectActionValue` | Add composable status behavior rules and contribution-relative numeric inputs; retire name-specific payload variants. |
| Semantic catalogs | `src/ChessBattleEffectSemantics.h/.cpp`: `statusCatalog`, `statusEffectFieldCatalog` | Narrow identity catalog; introduce action/behavior, minimum-profile, and numeric binding-phase catalogs; remove field ownership by status name. |
| Parser | `src/ChessBattleEffectParser.cpp`: status payload and action parsing | Parse local behavior-rule sequences, contextual selectors, and `每層數值`; focused range-default diagnostic. |
| Authoring metadata | `src/ChessEffectAuthoringMetadata.h` | Share generic rule/action descriptors; expose only explicit area choices; generate behavior schemas without parallel labels. |
| Validation | `src/ChessBattleEffectValidation.cpp` | Validate event/action context, contribution quantity compatibility, group-destructive policies, and numeric products. |
| Description document | `src/ChessEffectDescription*.cpp/.h` | Add contribution scope and behavior rules; remove name-derived effect reconstruction. |
| Command evaluation | `src/battle/BattleEffectSystem.h/.cpp` | Carry holder/producer/family identity and action order, bind/evaluate phased status numbers, gather active contribution rule views, preserve explicit source precedence in structured ordering. |
| Status storage | `src/battle/BattleStatusSystem.h/.cpp` | Replace anonymous instance with contributions; per-`(family, holder)` capacity allocation; producer-compatible merge; group queries/removal/consume/expiry. |
| Hit integration | `src/battle/BattleCoreAttacks.cpp` | Remove True-Qi-specific insertion after generic active rule dispatch reaches parity. |
| Damage targeting | `src/battle/BattleCoreDamage.cpp` | Retain internal single-target default; no runtime change for omitted range. |
| Command reduction | battle command/core effect systems | Live revalidation and exclusive behavior arbitration; preserve transaction/provenance semantics. |
| Content | top-level `config/chess_*.yaml` | Migrate all status behaviors and omit redundant single-target ranges. |
| Schema generator | `tools/kys_effect_schema_codegen/ChessEffectSchemaRenderer.cpp` | Render contextual behavior schemas and circle/square-only explicit range enum. |
| Tests | C++ effect/status/description suites and Python schema/content tests | Characterization, contribution independence, behavior coverage, presentation, migration rejection. |

## Required tests

### Independent contribution safety

1. 九陽 applies 10 layers at cap 10 and value 9; another producer applies one layer at cap 5 and value 20. Result: two contributions, quantities 10 and 1.
2. Reapplying the second producer clamps only its contribution to 5.
3. Reapplying 九陽 changes only 九陽's contribution.
4. Neither reapplication changes the other's origin, source, cap, behavior, or sequence.
5. Holder hit executes `10 × 9` and `N × 20` at their respective order positions.
6. Removing one producer contribution leaves the other active.
7. Removing group `真氣` removes both.
8. Group quantity reports the checked sum.
9. On one holder, group capacity reports each distinct producer family once even when the family has several generations; another holder is queried independently.

### Holder-scoped family capacity

1. One `下一次受到攻擊必定落空` action selects three different allied holders. All three receive one trigger charge from the same producer family; none sees another holder's used capacity.
2. Consuming or removing the charge on one ally does not free, consume, or replace either other ally's charge.
3. The same 七星 producer applies seven marks to enemy A and seven marks to enemy B. Both allocations coexist at seven because capacity is per holder.
4. `取代同一效果` applied again to enemy B replaces only B's matching family generations and leaves enemy A's marks unchanged.
5. Per-holder group capacity and runtime display never aggregate another holder's family records.

### Same-producer compatibility

1. Same producer and identical bound behavior merges according to policy.
2. Same producer family with a changed evaluated value creates a new generation under additive policy only when family capacity remains.
3. All active generations in one family on one holder remain at or below that holder's family limit.
4. `取代同一效果` removes the selected older family generations on the target holder before creating the replacement; `取代整組` is separately tested as group-destructive on that same holder.
5. Duration refresh does not mutate immutable behavior.
6. Two apply actions in one rule remain distinct families through action order.
7. Runtime borrowed bindings remain distinct contributions through runtime instance ID while repeated re-borrows share the logical family cap on each holder independently.
8. A different limit for the same family is rejected rather than forked, maximized, or treated as a second capacity.

### Ordering and provenance

1. Status behavior sorts after its producer lane and before the next producer rule.
2. Multiple status behaviors from one contribution preserve authored behavior order.
3. Multiple contributions preserve structured source/rule/action/sequence order.
4. Repeated rule-store removal/rebuild cycles do not alter stored ordering tokens.
5. No `+1` arithmetic overflow or collision exists.
6. Damage metadata, kill credit, presentation, modifiers, invincibility, descendants, and event lineage match characterized intent.
7. Mixed Combo, Equipment, EquipmentSynergy, Neigong, and Magic rules retain the current explicit source-precedence order before registration order; the structured key does not depend on raw enum ordinal or registration token alone.
8. Rules executed by each of the three shipped clone-count tiers retain source-definition identity, use the clone as logical granting owner, and cannot create an unbounded family through repeated runtime aliasing.

### Exclusive behavior arbitration

1. Two cast-prevention contributions do not both consume charges after the first prevents the cast.
2. Two incoming-miss contributions consume only the winner.
3. Damage block and single-hit cap consume only on an eligible transaction.
4. The complete documented structured key decides equivalent and heterogeneous contenders; no status-name priority participates.
5. A later contribution remains for the next event when the earlier contribution wins.
6. A contribution created during an event does not execute during that same event.
7. A contribution removed earlier in the dispatch does not execute from a stale snapshot.
8. With both 化勁 and 刺目 present, only the earlier eligible contender consumes; the 化勁 shield occurs only when 化勁 wins.

### Query, removal, and consumption

1. `has` is any positive contribution.
2. quantity is a checked sum with source filters.
3. no generic potency maximum API remains.
4. cleanse count selects groups, not contributions.
5. source-filtered removal removes only matching contributions.
6. oldest-first consumption is deterministic.
7. source-sensitive 七星 consumes only its producer's marks.
8. depletion behavior runs for the contribution that reaches zero.
9. expiry of one contribution leaves the group present if another remains.

### Parser and schema

1. `每層數值` works for layered contributions and is rejected for trigger charges, marks, and quantity-less statuses.
2. ordinary `數值` and `每層數值` are mutually exclusive.
3. `每層: true` is rejected.
4. an additional behavior is not rejected merely because of the status name when the producer remains lifecycle-valid and satisfies its minimum profile.
5. lifecycle-incompatible actions produce contextual diagnostics.
6. omitted damage `範圍` parses as internal single target.
7. explicit `範圍: 單體` is rejected with the canonical diagnostic.
8. `圓形` and `方形` validate their dimensions.
9. shipped content has no explicit single-target range.
10. schema exposes only canonical forms.
11. Every numeric base has one binding phase, legality set, binder, evaluator, compatibility rule, and description path; mixed-phase formulas round only after trigger-time assembly.
12. Each `Profiled` status has generated complete/incomplete producer fixtures, including a 寒毒 fixture missing only healing prevention.
13. A profiled status may author additional lifecycle-valid behavior without expanding a status-name parser whitelist.

### Descriptions

1. 九陽, 降龍, 七星, and 九陰白骨爪 match reviewed Compact/Full/Detailed goldens.
2. Static descriptions say that caps belong to the producer family when necessary.
3. Runtime group display counts a producer-family cap once and never borrows one family's cap/value for another.
4. Heterogeneous contribution breakdown names each producer and local behavior.
5. Detailed shows omitted single-target range as a resolved default.
6. Full/Compact omit redundant single-target wording.
7. Every behavior field changes all styles where player-observable and has structural coverage.

### Existing parity suites

Retain and extend poison, bleed, stun, MP block, status shield, control immunity, heal rounding, damage cap, damage block, 七星, 毒爆, 玄冥神掌 settle/remove/reapply ordering, True-Qi multi-hit, death, execute, invincibility, reflection, and kill-order tests.

Add pipeline parity matrices for 戰意, 枯骨, and 寒毒. Add separate-transaction characterization for two bleed producers. Add repeated borrowed-generation capacity tests and application-bound versus event-live formula tests.

## Deliberate behavior changes

The following changes are intended and must be called out in release/review notes:

1. Same-name effect-bearing applications from different producers no longer overwrite cap, behavior values, source, or origin.
2. Generic aggregate potency no longer means maximum potency.
3. Cleanse counts player-visible status groups rather than internal contributions.
4. Explicit `範圍: 單體` becomes invalid authoring; omission is canonical.
5. Status behaviors are validated by lifecycle/action compatibility rather than a status-name field whitelist.
6. A same-producer-family additive application whose resolved immutable behavior changed creates a separate generation rather than retroactively changing old layers; all generations still share one family capacity.
7. Independent bleed producers create independently timed damage transactions instead of one merged maximum-HP-percent transaction. Shields, invincibility, modifiers, events, presentation, and rounding therefore interact with each contribution separately.
8. Exclusive contenders use winner-only consumption. In particular, one contact no longer consumes both `化勁` and `刺目`; only the earlier eligible structured-order winner consumes, and the 化勁 shield is granted only if 化勁 wins.
9. Two status-application actions in one configured rule are distinct producer families because their action orders differ, even when they currently author identical behavior. They no longer merge merely because they share rule ID and text.
10. Status behavior dispatch uses snapshot-plus-liveness semantics: a contribution created during a dispatch cannot act in that same dispatch, while a contribution removed or consumed by an earlier command loses its remaining snapshotted rule executions.

All other combat changes require an explicit amendment after Phase 0 characterization.

## Risks and mitigations

| Risk | Mitigation |
| --- | --- |
| Contribution model unintentionally changes poison or stun. | Keep their group reducers explicit; characterize cross-source matrices before storage changes. |
| Multiple stored rules double-consume exclusive effects. | Sequential live revalidation and winner-only consumption tests. |
| Generic status rules become a second parser/runtime. | Reuse existing rule/action descriptors and dispatcher helpers; status context adds only relative bindings. |
| Per-layer behavior creates many damage transactions. | Define numeric multiplication as one action; only independent origins remain separate transactions. |
| Runtime rule ordering drifts. | Structured execution key and source-category integration tests. |
| Same-producer dynamic values overwrite snapshots. | Immutable behavior plus separate contribution generations. |
| Value generations or repeated borrowed aliases multiply the authored cap. | Distinguish producer identity from producer-family identity; all generations/aliases on one holder share one checked family capacity. |
| A multi-target action accidentally shares capacity or replacement across holders. | Key allocation/replacement by `(holderUnitId, producerFamily)` and test three-target charges plus two-target 七星 replacement. |
| UI shows a misleading cap. | Aggregate quantity separately; show contribution breakdown when caps/behaviors differ. |
| Source-sensitive lifecycle consumes the wrong contribution. | Producer/binding filters and contribution-local depletion tests. |
| Catalogs drift again. | One action/behavior descriptor and exhaustive variant visitors; structural agreement tests. |
| Open behavior parsing lets a named status omit part of its player contract. | Catalog-owned minimum capability profiles checked against complete producers in whole-content validation, without parser whitelists or numeric ownership. |
| A persistent modifier silently moves calculation phase. | Exact semantic-accumulator/phase parity plus Phase 0 mixed-pipeline equivalence matrices. |
| A numeric base is captured or evaluated at the wrong time. | Closed binding-phase catalog shared by binder, evaluator, validator, compatibility, schema, and descriptions. |
| Independent DoT contributions change transaction interactions. | Treat the split as a deliberate change; characterize and test modifiers, shields, invincibility, timing, metadata, and presentation per contribution. |
| YAML becomes verbose. | Reuse single-action rule shorthand and omit defaults; evaluate Compact/Full human readability before final schema lock. |
| A broad behavior action is invalid in stored context. | Event/action capability validation, not status-name prohibition. |
| Existing dirty migration work is accidentally overwritten. | Implement in small phases, inspect overlapping diffs, and preserve unrelated user files. |

## Rejected alternatives

### Global status definitions

Rejected because the project has no global status-definition layer, it would hide behavior away from the granting effect, and it would create cross-file dependencies for caps and values.

### Exclusive producer whitelist

Rejected as the default because generic names such as `真氣` should remain reusable. A genuinely exclusive named mechanic may opt into a reviewed producer restriction, but that is not the general solution.

### Latest producer wins

Rejected because it retroactively changes old layers and execution origin.

### Maximum cap and maximum potency

Rejected because it creates cross-producer amplification and still loses layer provenance.

### Unbounded generations or one global status ceiling

Rejected as a false choice. Immutable-value generations and runtime aliases need separate attribution, but they do not each receive another authored cap. On each holder independently, a logical producer family owns one finite capacity shared by its generations. Distinct configured producers remain independently capped, and distinct holders never share the pool, so the status group needs no global numeric ceiling.

### Reintroducing behavior profiles as parser whitelists

Rejected. Minimum behavior profiles protect player-facing name truth through whole-content semantic validation. They require capabilities but do not forbid additional valid actions, own numeric values, or select payload structs in the parser/runtime.

### Structural equality followed by global merge

Rejected because equal values today do not establish shared ownership tomorrow, and source-sensitive consumption/order still differs. Equality is used only for same-producer local compatibility.

### One runtime instance per source unit only

Rejected because two effects owned by the same unit may define different caps and behaviors. Producer rule/action identity is required.

### Per-layer action repetition

Rejected as the meaning of `每層數值` because repeated transactions interact differently with modifiers, shields, retaliation, invincibility, and events.

### Keeping name-specific payload structs and adding more assertions

Rejected because assertions would only forbid new producers; they would not preserve the generalized effect model or explain behavior locally.

## Definition of done

The full migration is complete only when:

- all status storage is contribution-based;
- no generic merge finds only the first instance by status kind;
- same-name producers with different caps/values are independently safe;
- repeated value generations and borrowed runtime aliases cannot exceed one logical producer family's cap on a holder;
- multi-target application, family replacement, consumption, and capacity release are isolated per holder;
- every evented contribution has stable producer identity and origin;
- active status behavior rules use generic dispatch and structured ordering;
- the True-Qi specialized hit bridge and one-instance invariant are removed;
- caps and behaviors remain local to producer configs;
- status names no longer own arbitrary behavior field whitelists;
- every status row has an approved intrinsic/profiled/open classification, and profiled producers satisfy their minimum player contract;
- `potency`, `secondaryPotency`, and their maximum group queries are removed;
- persistent status modifiers preserve their characterized semantic accumulator, phase, relative order, and rounding boundary;
- every status-behavior numeric base has one closed binding-phase classification;
- per-layer authoring uses numeric `每層數值` and never `每層: true`;
- single-target damage omits `範圍`, and explicit `範圍: 單體` is rejected;
- group quantity, removal, cleanse, consumption, expiry, and source filters have documented semantics;
- Compact, Full, and Detailed descriptions are contribution-aware;
- every authoritative top-level `config/chess_*.yaml` uses canonical authoring;
- old forms are rejected rather than maintained as aliases;
- schema/content validation passes for easy, normal, and hard content;
- all C++ and Python tests pass;
- the required Debug build succeeds, except for the documented running-game final-link allowance; and
- `git diff --check` is clean.

## Review checklist

Reviewers should explicitly answer:

1. Is producer identity sufficiently stable across configured, cloned, borrowed, and runtime-scoped rules?
2. Does the producer-family key collapse repeated runtime aliases without collapsing genuinely different configured producers or action orders?
3. Are family capacity and immutable behavior ownership unambiguous, and can no generation mint another cap?
4. Is capacity/replacement unambiguously scoped to `(producer family, holder unit)`, including multi-target applications?
5. Do clone and borrow lowering preserve stable source-definition identity while assigning the correct logical granting owner, including all three shipped `生成分身` tiers?
6. Are any current mechanics unintentionally relying on cross-source `AddStack` overwrite, especially 流血 and 傷害抵擋?
7. After characterization, are poison, stun, and MP-block the complete retained group reducers, or does another mechanic require an explicit reducer?
8. Is oldest-first consumption acceptable, and which current actions need an explicit order?
9. Does group cleanse removing all contributions match player expectations?
10. Does every minimum profile express a real player contract without becoming a parser whitelist, especially 寒毒's two required behaviors?
11. Are action/event capability checks sufficient for additional behaviors once minimum profiles are satisfied?
12. Is `每層數值` clear that it scales one numeric action rather than repeats transactions?
13. Are the closed application-bound/event-live/contribution-live classifications correct for every numeric base and mixed formula?
14. Do 戰意, 枯骨, and 寒毒 retain their exact pipeline, sign convention, and rounding positions?
15. Is the separate-transaction behavior for independent bleed contributions acceptable?
16. Is winner-only heterogeneous arbitration acceptable when it determines whether 化勁 grants a shield?
17. Should any independent status damage commands be intentionally coalesced, and can metadata/order remain identical?
18. Does the complete structured order key preserve the explicit Combo → Equipment → EquipmentSynergy → Neigong → Magic precedence before registration order?
19. Are 九陽, 降龍, 七星, and 九陰白骨爪 descriptions understandable without external status definitions?
20. Is strict rejection of explicit `範圍: 單體` preferable to accepting it as redundant authoring?
21. Does the plan preserve the generic trigger/action work instead of recreating status-specific combat branches?
22. Are all intended behavior changes explicitly listed?
23. Is each implementation phase small enough to verify before deleting the previous representation?
