# Semantic status-effect authoring, runtime lowering, and descriptions

## Status

Implemented design and migration record. This document remains normative for the status/buff authoring model described here. It complements [chess-effect-authoring-implementation-plan.md](chess-effect-authoring-implementation-plan.md); it does not replace that document's work on content validation, event capabilities, named mechanisms, timing intents, or non-status macros.

The design intentionally drops backward compatibility as an end-state guarantee. Authoritative top-level `config/chess_*.yaml` content and parser surfaces migrate in coordinated phase slices; a small old-form slice may remain only when a later phase has not yet introduced its replacement consumer, and is deleted in that later phase. Removed forms are rejected after their migration slice. Copied Android/build-output configs are not migration targets.

## Executive summary

The current status authoring surface exposes runtime storage slots rather than gameplay meaning:

```yaml
套用狀態:
  狀態: 戰意
  層數: 1
  強度: 5
  次要強度: 1
  合併方式: 增加層數
  層數上限: 10
```

Neither a human nor the description renderer can determine from this action that each layer increases skill damage by 5% and reduces incoming damage by 1%. Different statuses reinterpret `層數`, `強度`, and `次要強度` as unrelated mechanics.

The new authoring surface is semantic and status-specific:

```yaml
套用狀態:
  狀態: 戰意
  增加層數: 1
  層數上限: 10
  效果:
    每層生效:
      招式傷害增加百分比: 5
      傷害減免百分比: 1
```

One closed status catalog defines, for every status:

- its permitted quantity model and quantity verbs;
- its permitted effect scope and named effect fields;
- its permitted reapplication policies;
- its validation rules and diagnostics;
- its natural-language description phrases; and
- how semantic values lower into the existing runtime systems.

The semantic form must remain available to the description builder. Runtime lowering happens on a separate branch after parsing and validation:

```text
semantic YAML
    |
    v
canonical semantic EffectRule
    |------------------------------|
    v                              v
description document          runtime lowering
    |                              |
    v                              v
Detailed / Full / Compact     existing status/modifier/command systems
```

This is not a universal `BuffInstance` redesign. Typed statuses, persistent modifiers, resources, areas, and private effect state remain separate runtime systems.

## Goals

1. Make every authored status application understandable without reading C++.
2. Give each numeric value one named gameplay meaning and one authored source of truth.
3. Reject the wrong quantity noun, effect scope, effect field, or policy for a status.
4. Preserve current combat behavior unless a change is explicitly called out.
5. Generate `Detailed`, `Full`, and `Compact` descriptions from the same semantic facts.
6. Remove identity `百分比: 100` coefficients from direct references.
7. Remove the special accumulated-state bridge used by 真氣 after exact runtime parity is proven.
8. Keep the implementation bounded by lowering semantic authoring into the existing runtime architecture.

## Locked design decisions

1. `ApplyStatusAction` becomes a semantic canonical action; anonymous status potency fields are not the canonical parsed representation.
2. One closed status catalog is shared by authoring metadata, parser validation, runtime validation, lowering, and descriptions.
3. Every status has exactly one catalog-defined quantity model. Wrong nouns and irrelevant quantity fields are errors.
4. Layer caps are required for multiplicative-layer statuses. Omission never means unlimited.
5. There is one outer `效果` container with four closed scopes: `持續生效`, `每層生效`, `每次觸發`, and `每層提供數值`.
6. Reapplication policies are closed per status; the generic runtime policy enum is not exposed directly.
7. Consume actions use `消耗數量` and `最後一次`; player text recovers the status-specific noun from the catalog.
8. Direct non-constant references default to 100% at parse time; evaluator math does not change.
9. The description document consumes semantic status facts before runtime lowering and renders all three styles from those facts.
10. 七星 and 毒爆 keep explicit cross-rule lifecycles, but duplicated producer values and numeric-correspondence matching are removed.
11. 真氣 moves to runtime hit-command ownership only after ordering and provenance parity is proven.
12. Typed statuses, modifiers, resources, areas, and private state remain distinct runtime systems.
13. Old authoring forms are removed after migration without compatibility aliases.

## Non-goals

This work does not:

- replace all stateful mechanics with one universal buff type;
- merge typed statuses, persistent modifiers, shields/resources, areas, or private state machines;
- add arbitrary nested action lists inside status effect scopes;
- add general status-applied/status-expired events;
- add a general expression AST;
- add named modifier groups without atomic refresh, source ownership, and cleanse semantics;
- change frame-based duration authoring;
- edit copied configs under Android or build outputs; or
- preserve the removed YAML spellings as compatibility aliases.

## Authoritative scope and inventory

The effect DSL covered by this design occurs in four authoritative top-level files:

- `config/chess_magic_effects.yaml`
- `config/chess_neigong.yaml`
- `config/chess_combos.yaml`
- `config/chess_equipment.yaml`

The audited authored construct counts are:

| Construct | Exact count |
| --- | ---: |
| `套用狀態` | 34 |
| `施毒` | 5 |
| `屬性修正` action blocks | 208 |
| `傷害修正` action blocks | 50 |
| `資源變更` | 13 |
| `獲得護盾` | 10 |
| Persistent `單次承傷上限` macro | 3 |
| `建立區域` | 2 |
| `死亡庇護` | 2 |
| `生成分身` | 3 |

The 208/50 modifier counts refer to the named action blocks. Larger raw text counts mix in other constructs: three `屬性加成` macros, three `忽略防禦` macros, three persistent `單次承傷上限` macros, and one area `造成傷害修正`.

## Current runtime architecture

Several distinct stateful-effect systems are legitimate and must remain distinct.

1. **Typed statuses.** `BattleTypedStatusInstance` currently stores generic `stacks`, `potency`, and `secondaryPotency`, while `BattleStatusSystem` interprets them per status.
2. **Persistent attribute and damage modifiers.** These have their own instance stores, stacking scope, refresh behavior, and cleanse handling.
3. **Resources and windows.** Shields, invincibility, status shields, and stagger shields have transaction/window semantics rather than status-layer semantics.
4. **Areas and auras.** These have spatial ownership, enter/leave behavior, projectile policy, and an area-specific merge policy.
5. **Private effect memory and initialization state machines.** These include recorded maxima, absorption state, cast progress, borrowed rules, and similar source-private state.

The authoring layer should express gameplay semantics and lower into these systems. The runtime systems should not be flattened merely to make YAML uniform.

### Pre-migration code reference map

The design was based on the following implementation seams before this migration. The references are retained as the reviewed baseline; the completed implementation intentionally moved or removed several of them.

| Concern | Current source |
| --- | --- |
| Status enum and generic `ApplyStatusAction` | `src/ChessBattleEffectTypes.h:136`, `src/ChessBattleEffectTypes.h:458` |
| Generic status authoring descriptor | `src/ChessEffectAuthoringMetadata.h:1697` |
| Generic status parser branch | `src/ChessBattleEffectParser.cpp:1099` |
| Poison macro lowering | `src/ChessBattleEffectParser.cpp:1764` |
| Typed rule validation | `src/ChessBattleEffectValidation.cpp:399` |
| Numeric formula evaluation | `src/battle/BattleEffectSystem.cpp:2186` |
| Poison same-event command aggregation | `src/battle/BattleEffectSystem.cpp:2497` |
| Status apply/merge, poison tick, and contribution behavior | `src/battle/BattleStatusSystem.cpp:359`, `:515`, `:1133` |
| True-Qi formula-input bridge and before-damage dispatch order | `src/battle/BattleCoreAttacks.cpp:757`, `:784`, `:1035`, `:1091`, `:1102` |
| Damage-block/next-hit-cap lookup, defense request, and charge consumption | `src/battle/BattleDamageSystem.cpp:273`, `:291`, `:304`, `:742` |
| Persistent modifier stores and generic-negative cleanse | `src/battle/BattleEffectCommandSystem.h:22`, `src/battle/BattleEffectCommandSystem.cpp:941`, `:959` |
| Description AST document | `src/ChessEffectDescription.h:231` |
| Generic status phrase | `src/ChessEffectDescriptionPhrases.cpp:982` |
| Lifecycle shape matching | `src/ChessEffectDescriptionArchetypes.cpp:624`, `:879` |
| Full pre-rendering in catalog queries | `src/ChessCatalogQueries.cpp:36` |
| Compact in-game magic display | `src/ChessMagicEffectDisplay.cpp:88` |

## Problems in the pre-migration schema

### One quantity field represents unrelated mechanics

`層數` currently means:

- multiplicative layers for 戰意 and 真氣;
- remaining periodic triggers for 中毒;
- max-HP damage percentage for 流血;
- damage-block charges for 傷害抵擋;
- marks for 七星; and
- death-explosion repetition count for 毒爆.

An author cannot know the correct meaning from the field name, and the validator cannot give status-specific guidance.

### Anonymous positional effect parameters

`強度` and `次要強度` are positional storage slots. Examples include:

- 寒毒: speed reduction percentage and healing block behavior;
- 枯骨: damage-taken increase and healing reduction;
- 戰意: skill-damage increase and damage reduction;
- 真氣: pure damage per hit;
- 七星: duplicated defense-ignore percentage and final stun duration;
- 毒爆: per-layer explosion damage and duplicated poison potency.

The mapping exists only in C++ switches and specialized description matching.

### Direct references require a meaningless identity multiplier

For a non-constant `EffectNumber`, omitted `百分比` currently means zero rather than direct use. A direct status or state reference must therefore write `百分比: 100`, which looks like an additional gameplay multiplier even when it is only an implementation identity coefficient.

### Generic stacking vocabulary has status-specific behavior

`獨立`, `刷新`, `取代`, `保留最強`, and `增加層數` do not denote one consistent operation across statuses and modifier stores. For example, several stun policies collapse to max-duration behavior, while standard poison has a precisely defined strongest-wins policy plus same-event aggregation.

This is the shared parser/runtime vocabulary, not a claim that every value is authored on shipped statuses. `保留最強` has no authored `合併方式` site; the `施毒` macro lowers standard poison to that internal policy in `ChessBattleEffectParser.cpp:1764-1776`. The two shipped authored `獨立` sites are modifier blocks (`chess_combos.yaml:91`, `:167`), not status applications. The migration tables below still map the relevant runtime behaviors, but implementation audits should not look for nonexistent authored status sites for those values.

### Behavior ownership is inconsistent

- Intrinsic status behavior: 寒毒, 枯骨, 戰意.
- Explicit lifecycle rules: 七星, 毒爆, 無影.
- Hybrid behavior: 真氣 stores a value in the status but 九陽 separately consumes a generic accumulated-state value on hit.

The design does not force all statuses into one ownership model. It makes the chosen ownership explicit and removes duplicate values.

### Names collide or use the wrong perspective

- `單次承傷上限` currently names both a consumable absolute next-hit cap status and a permanent max-HP-percentage cap modifier.
- `下一次攻擊落空` is stored on the defender and rejects an incoming attack, so the name is from the wrong perspective.

### Descriptions reconstruct semantics after lowering

The style-independent `EffectDescriptionDocument` is built from runtime-oriented `EffectRule` actions. `DescriptionAction` retains a raw `EffectAction`; it has no semantic status-effect node. Generic status phrases therefore print `強度` and `次要強度`.

Readable lifecycle text such as 七星 is produced only after a whole-document matcher recognizes an exact producer/consumer shape and verifies duplicated numeric correspondence. This is fragile and becomes unnecessary once the semantic form is preserved.

### Description style is selected too early in some queries

The renderer supports `Detailed`, `Full`, and `Compact`, but catalog metadata often pre-renders `Full`. As a result, an API request whose surrounding catalog detail is `compact` can still contain Full-style effect descriptions. Rendering must move to the final presentation boundary.

## Existing runtime behavior that must be preserved

The following behavior is normative for migration and characterization tests.

### Typed statuses

| Status | Current runtime behavior |
| --- | --- |
| 寒毒 | Blocks all healing. Speed reduction is `potency × stacks`. |
| 枯骨 | Damage-taken increase is `potency × stacks`. Healing is multiplied once per layer by `100 - secondaryPotency`. |
| 戰意 | Skill-damage increase is `potency × stacks`. Damage reduction is `secondaryPotency × stacks`. |
| 真氣 | Contributes `potency × stacks` pure damage per accepted hit. The status does not currently issue the damage command itself. |
| 中毒 | Every 30 frames deals current HP × potency%. One trigger is consumed per tick. It ends on duration or trigger exhaustion. |
| 流血 | Every 10 frames deals max HP × stack count%. The current 1% per layer is hardcoded. |
| 傷害抵擋 | One charge blocks one positive, non-execute damage transaction. |
| 下次承傷上限 (current `單次承傷上限`) | Potency is an absolute cap. One charge is consumed on an eligible hit even when the cap does not reduce the damage. |
| 刺目 | Consumed from the attacker and suppresses that attacker's cast. |
| 化勁 | Consumed from the attacker, suppresses the cast, and grants the original target a shield equal to the status value. |
| 下一次受到攻擊必定落空 (current `下一次攻擊落空`) | Stored on the defender and rejects the next incoming attack. |
| 下一次攻擊必定暴擊 | Runtime-only status granted after dodge and consumed on the next attack. |

### Standard poison reapplication

Standard poison behavior is already fully defined and must not be reinterpreted:

1. For the same target, same effect owner, and same event, poison damage percentages are summed before application. The sum uses widened arithmetic and saturates at the maximum representable positive status value rather than overflowing.
2. Across events or owners, a higher damage percentage replaces the existing poison.
3. Equal or lower damage leaves the existing poison unchanged.
4. Equal damage does not refresh duration.
5. A stronger replacement updates source, remaining triggers, duration, and damage percentage.
6. The replace/reset form always replaces and does not perform same-event aggregation.

`同事件合併` is therefore poison-specific. It is not a generic status option.

### True-Qi hit ordering

Moving 真氣's hit damage into runtime ownership must preserve more than the event label. Characterization and parity tests must lock:

- exact queue position relative to main-projectile and hit-before-damage rules;
- effect owner and source metadata;
- target and contact position;
- cast and attack provenance;
- pure damage kind;
- damage-modifier flags;
- hurt-invincibility behavior;
- descendant work;
- multi-hit behavior; and
- kill-before-base-hit ordering.

## Semantic authoring model

### One outer effect container

Every status application that carries gameplay values uses one outer container named `效果`. It contains one permitted lifecycle scope from a closed set:

| Scope | Meaning |
| --- | --- |
| `持續生效` | Active once while the status exists. |
| `每層生效` | Active while the status exists and multiplied by the current layer count. |
| `每次觸發` | Runs when one trigger/charge is consumed. |
| `每層提供數值` | Named per-layer data read by an explicit lifecycle rule. |

These scopes are not arbitrary action lists. The status catalog defines the exact fields permitted in the permitted scope. Unknown fields, the wrong scope, or a second scope are errors.

Examples:

```yaml
套用狀態:
  狀態: 枯骨
  持續幀數: 120
  重複套用: 刷新持續時間
  效果:
    持續生效:
      受到傷害增加百分比: 25
      受到治療減少百分比: 75
```

```yaml
套用狀態:
  狀態: 戰意
  增加層數: 1
  層數上限: 10
  效果:
    每層生效:
      招式傷害增加百分比: 5
      傷害減免百分比: 1
```

```yaml
套用狀態:
  狀態: 下次承傷上限
  可觸發次數: 1
  效果:
    每次觸發:
      傷害上限:
        目標最大生命百分比: 15
        取整: 向零
        最小: 1
```

```yaml
套用狀態:
  狀態: 毒爆
  增加層數: 1
  層數上限: 5
  效果:
    每層提供數值:
      死亡爆炸純粹傷害:
        每星級: 60
```

### Status-specific quantity models

Every status has exactly one catalog-defined quantity model. A wrong noun is rejected with a status-specific diagnostic and suggested spelling. A status with no quantity rejects all quantity fields.

#### Multiplicative layers

```yaml
增加層數: 1
層數上限: 10
```

- Used by 戰意, 真氣, 流血, and 毒爆.
- The cap is required.
- Omission never means unlimited.
- `重複套用` is forbidden because `增加層數` already defines the operation.

#### Damage-block charges

Incremental form:

```yaml
增加可抵擋次數: 1
可抵擋次數上限: 3
```

Replacement/initialization form:

```yaml
設定可抵擋次數: 6
```

#### Trigger charges

```yaml
可觸發次數: 1
```

Used by 中毒, 化勁, 刺目, 下一次受到攻擊必定落空, and 下次承傷上限. The status catalog defines whether reapplication replaces or uses a status-specific policy.

#### Marks

```yaml
設定印記層數: 7
```

Used by 七星. The verb means replacement; a separate generic merge policy is forbidden.

#### No quantity

眩暈, 封內, 寒毒, 枯骨, and 無影 do not accept quantity fields.

### Quantity-neutral consumption

The consume side does not force the producer's noun into a generic action schema:

```yaml
消耗狀態:
  狀態: 七星
  消耗數量: 1
```

`最後一層` is renamed to `最後一次`:

```yaml
消耗狀態:
  狀態: 七星
  消耗數量: 1
  狀態來源: 效果擁有者
  最後一次:
    套用狀態:
      狀態: 眩暈
      持續幀數: 30
      重複套用: 保留較長持續時間
```

Player descriptions use the catalog noun (`1枚印記`, `1次抵擋`) even though the authoring action uses `消耗數量`.

### Direct status references

References distinguish quantity from a named effect value.

Quantity:

```yaml
重複次數:
  來源狀態數量: 毒爆
```

Named effect value:

```yaml
數值:
  來源狀態效果值:
    狀態: 毒爆
    名稱: 死亡爆炸純粹傷害
```

The referenced name is validated against the source status catalog. The old generic references `來源狀態層數` and `來源狀態強度` are removed.

### Reapplication policies are closed per status

There is no globally valid generic policy enum on the authoring surface.

#### Stun

| New authoring policy | Existing runtime behavior represented |
| --- | --- |
| `延長持續時間` | Current independent applications; durations add. |
| `保留較長持續時間` | Current refresh/keep-strongest/add-stack behavior; remaining duration becomes the maximum. |
| `取代持續時間` | Current replace behavior. |

Existing stuns with omitted policy migrate to `延長持續時間`. Existing `刷新` stuns migrate to `保留較長持續時間`; the current behavior does not reset a longer timer to a shorter one.

#### MP block

Use `保留較長持續時間`.

#### Cold poison, withered bone, and shadowless

Use `刷新持續時間`. Incoming duration, value, and source replace current values and may shorten duration.

#### Layer, charge, and mark operations

The quantity verb defines reapplication. `重複套用` is forbidden.

#### Poison

Poison has two permitted policies:

- `保留較高傷害`
- `取代並重設`

Standard poison exposes its same-event aggregation explicitly:

```yaml
施加中毒:
  可觸發次數: 3
  持續幀數: 90
  重複套用: 保留較高傷害
  同事件合併: 合計傷害百分比
  效果:
    每次觸發:
      目前生命傷害百分比: 7
```

Replace/reset poison is:

```yaml
施加中毒:
  可觸發次數: 4
  持續幀數: 120
  重複套用: 取代並重設
  效果:
    每次觸發:
      目前生命傷害百分比: 10
```

`同事件合併` is permitted only with `保留較高傷害` and is forbidden for `取代並重設`.

`結算剩餘中毒傷害` and `移除狀態: 中毒` remain valid first-class actions. `施加中毒` replaces only poison application authoring. In particular, 玄冥神掌 must preserve its current ordered sequence: settle the remaining poison damage, remove the old poison, then apply the replacement poison. Poison-policy lowering must not combine, reorder, or subsume those three actions.

`同來源刷新` remains area-specific and is not added to the status policy vocabulary.

### Numeric percentage default

For a non-constant numeric `基準`, omitted `百分比` means direct use. The parser materializes `percent = 100`; evaluator math does not change.

```yaml
重複次數:
  來源狀態數量: 毒爆
```

is therefore equivalent to the old identity calculation without exposing `百分比: 100`.

An audit of all top-level `config/chess_*.yaml` found zero non-constant `基準` mappings that currently omit `百分比`, so this parse-time change does not silently reinterpret shipped formulas.

### Removed authoring fields and forms

The final authoring surface removes:

- `強度`
- `次要強度`
- generic status `層數`
- generic status `合併方式`
- `套用次數`
- author-facing `同事件合計強度`
- `來源狀態層數`
- `來源狀態強度`
- identity `百分比: 100` on direct references
- `每層: true` on modifiers after Phase 4

The internal poison aggregation field may remain temporarily during migration, but it must be renamed to poison-specific terminology or moved into the poison lowering path. It must not remain a generic status concept.

There are zero shipped authored sites for `套用次數` or author-facing `同事件合計強度`. Removing them is parser/schema/runtime-surface cleanup rather than config-content migration.

### Naming changes

- `下一次攻擊落空` becomes `下一次受到攻擊必定落空`.
- The consumable `單次承傷上限` status becomes `下次承傷上限`.
- The persistent cap modifier uses:

  ```yaml
  方式: 每次承傷不超過最大生命百分比
  ```

Neither this spelling nor `每次傷害不超過最大生命百分比` exists in current shipped config. This design introduces `每次承傷不超過最大生命百分比` as the canonical authoring name for the migrated persistent modifier.

The runtime cap calculation multiplies maximum HP by the authored percentage using widened arithmetic and clamps the resulting absolute cap to the representable positive damage range. A large valid HP/percentage pair must never overflow into an unintended one-point cap.

## Closed status catalog

| Status | Quantity model | Effect scope | Reapplication authoring | Behavior ownership |
| --- | --- | --- | --- | --- |
| 中毒 | Trigger count | `每次觸發` | Required poison-specific policy | Typed status tick |
| 流血 | Added layers plus required cap | `每層生效` | Implied by quantity verb | Typed status tick |
| 眩暈 | None | None | Explicit extend/max/replace | Typed status |
| 封內 | None | None | `保留較長持續時間` | Typed status |
| 寒毒 | None | `持續生效` | `刷新持續時間` | Typed status |
| 枯骨 | None | `持續生效` | `刷新持續時間` | Typed status |
| 七星 | Set mark count | None | Implied replacement | Explicit cross-rule lifecycle |
| 化勁 | Trigger count | `每次觸發` | Implied replacement | Typed status consumption |
| 刺目 | Trigger count | `每次觸發` | Implied replacement | Typed status consumption |
| 下一次受到攻擊必定落空 | Trigger count | `每次觸發` | Implied replacement | Typed status consumption |
| 傷害抵擋 | Set/add block count | `每次觸發` | Implied by quantity verb | Damage transaction |
| 下次承傷上限 | Trigger count | `每次觸發` | Implied replacement | Damage transaction |
| 戰意 | Added layers plus required cap | `每層生效` | Implied by quantity verb | Typed status contribution |
| 真氣 | Added layers plus required cap | `每層生效` | Implied by quantity verb | Runtime hit contribution after Phase 3 |
| 毒爆 | Added layers plus required cap | `每層提供數值` | Implied by quantity verb | Explicit death lifecycle |
| 無影 | None; marker | None | `刷新持續時間` | Explicit lifecycle marker |
| 下一次攻擊必定暴擊 | Internal only | Runtime-owned | Runtime-owned | Typed status consumption |

Every catalog row expands into a closed parser/validator descriptor. The table is not merely documentation.

### Closed effect payload fields

The following authored fields are the complete intended status-effect payload vocabulary. Fixed timing details such as poison's 30-frame tick and bleed's 10-frame tick are catalog semantics and appear in Full/Detailed descriptions; authors do not repeat those constants at every application.

| Status | Scope | Permitted/required authored fields |
| --- | --- | --- |
| 中毒 | `每次觸發` | `目前生命傷害百分比` |
| 流血 | `每層生效` | `最大生命傷害百分比` (migrates the currently hardcoded 1%) |
| 寒毒 | `持續生效` | `禁止受到治療: true`, `速度降低百分比` |
| 枯骨 | `持續生效` | `受到傷害增加百分比`, `受到治療減少百分比` |
| 化勁 | `每次觸發` | `阻止本次施放: true`, `原攻擊目標獲得護盾` |
| 刺目 | `每次觸發` | `阻止本次施放: true` |
| 下一次受到攻擊必定落空 | `每次觸發` | `使本次受到攻擊落空: true` |
| 傷害抵擋 | `每次觸發` | `抵擋非處決正傷害: true` |
| 下次承傷上限 | `每次觸發` | `傷害上限` with one permitted numeric formula |
| 戰意 | `每層生效` | `招式傷害增加百分比`, `傷害減免百分比` |
| 真氣 | `每層生效` | `命中附加純粹傷害` |
| 毒爆 | `每層提供數值` | `死亡爆炸純粹傷害` |

眩暈 and 封內 are fully described by their status identity plus duration/policy and therefore have no `效果` payload. 七星 and 無影 expose their behavior through explicit lifecycle rules and have no intrinsic payload. 下一次攻擊必定暴擊 remains internal.

Literal `true` fields above are required semantic declarations, not configurable switches. `false` is rejected; an effect that should not occur must use a different status/action rather than author a contradictory status payload.

### Duration model

`持續幀數` retains the existing scalar-or-`EffectNumber` authoring capability where the catalog permits it.

| Duration rule | Statuses |
| --- | --- |
| Required positive duration | 中毒, 眩暈, 封內, 寒毒, 枯骨, 七星, 下一次受到攻擊必定落空, 無影 |
| No authored duration; persists until quantity exhaustion, explicit removal, or battle lifecycle | 流血, 化勁, 刺目, 傷害抵擋, 下次承傷上限, 戰意, 真氣, 毒爆 |
| Runtime-owned | 下一次攻擊必定暴擊 |

A forbidden duration is an error rather than an ignored field. The catalog may later gain a new permitted form only in response to a concrete gameplay requirement and accompanying runtime semantics.

## Representative migrations

### 降龍十八掌 / 戰意

```yaml
- 時機: 攻擊提交
  目標: 自身
  套用狀態:
    狀態: 戰意
    增加層數: 1
    層數上限: 10
    效果:
      每層生效:
        招式傷害增加百分比: 5
        傷害減免百分比: 1
```

### 九陰白骨爪 / 枯骨

```yaml
- 時機: 主彈命中
  目標: 命中目標
  套用狀態:
    狀態: 枯骨
    持續幀數: 120
    重複套用: 刷新持續時間
    效果:
      持續生效:
        受到傷害增加百分比: 25
        受到治療減少百分比: 75
```

### 綿掌 / 化勁

The named trigger effect preserves the current star-scaled shield formula instead of reducing it to a boolean behavior flag:

```yaml
- 時機: 主彈命中
  目標: 命中目標
  套用狀態:
    狀態: 化勁
    可觸發次數: 1
    效果:
      每次觸發:
        阻止本次施放: true
        原攻擊目標獲得護盾:
          每星級: 100
```

### 九陽神功 / 真氣

Final authored 九陽 behavior contains only the application:

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
          每層生效:
            命中附加純粹傷害: 9
```

The separate hit rule that reads `累計狀態值` is removed after runtime parity is proven.

### 七星劍法 / external mark lifecycle

七星 remains an explicit two-rule lifecycle because “any ally hits a mark applied by this effect owner” is event behavior, not a continuously active intrinsic stat contribution.

```yaml
- 時機: 主彈命中
  目標: 命中目標
  套用狀態:
    狀態: 七星
    設定印記層數: 7
    持續幀數: 150

- 時機: 命中
  觀察範圍: 效果擁有者同隊事件來源
  目標: 命中目標
  條件:
    - 目標有此來源狀態: 七星
  動作:
    - 傷害修正:
        階段: 防禦前
        傷害種類: 招式
        方式: 忽略防禦百分比
        數值: 50
    - 消耗狀態:
        狀態: 七星
        消耗數量: 1
        狀態來源: 效果擁有者
        最後一次:
          套用狀態:
            狀態: 眩暈
            持續幀數: 30
            重複套用: 保留較長持續時間
```

The producer no longer duplicates 50 and 30 in anonymous fields. The lifecycle linker connects the producer and consumer by status identity, source ownership, quantity model, and unique compatibility rather than numeric correspondence.

### 蛤蟆功 / 毒爆

```yaml
- 時機: 攻擊提交
  目標: 自身
  套用狀態:
    狀態: 毒爆
    增加層數: 1
    層數上限: 5
    效果:
      每層提供數值:
        死亡爆炸純粹傷害:
          每星級: 60

- 時機: 單位死亡
  目標:
    類型: 半徑內單位
    半徑格數: 5
    隊伍: 敵方
  條件:
    - 自身有狀態: 毒爆
  重複次數:
    來源狀態數量: 毒爆
    最小: 1
  動作:
    - 造成傷害:
        數值:
          來源狀態效果值:
            狀態: 毒爆
            名稱: 死亡爆炸純粹傷害
        傷害種類: 純粹
        範圍: 單體
    - 施加中毒:
        可觸發次數: 4
        持續幀數: 120
        重複套用: 取代並重設
        效果:
          每次觸發:
            目前生命傷害百分比: 10
```

The poison damage percentage is authored once. The old producer `次要強度` duplication is removed.

### Persistent per-hit cap modifier

The non-consumable modifier remains a modifier rather than a status:

```yaml
傷害修正:
  階段: 最終
  傷害種類: 全部
  方式: 每次承傷不超過最大生命百分比
  數值: 15
```

## Canonical semantic types and runtime lowering

### Canonical action type

`ApplyStatusAction` should become semantic rather than retain author-facing generic slots. The exact C++ names may follow repository conventions, but the canonical shape is:

```cpp
struct ApplyStatusAction
{
    BattleStatusKind status{};
    StatusQuantityOperation quantity;
    StatusDuration duration;
    StatusReapplicationPolicy reapplication;
    StatusEffectPayload effects;
};
```

`StatusQuantityOperation` is a closed variant such as:

```cpp
using StatusQuantityOperation = std::variant<
    NoStatusQuantity,
    AddStatusLayers,
    SetStatusMarks,
    AddDamageBlockCharges,
    SetDamageBlockCharges,
    SetTriggerCharges>;
```

`StatusEffectPayload` should be typed, not a `map<string, number>`. Per-status structs or closed effect-field enums are preferable because they make invalid C++-constructed rules unrepresentable or directly rejectable. YAML names map through the catalog into those typed fields.

The status catalog verifies that the action's status, quantity variant, effect payload variant, duration, and policy correspond.

### Command evaluation boundary

At effect dispatch:

1. Evaluate every `EffectNumber` in the semantic payload against the existing formula context.
2. Produce a resolved status command with named resolved values.
3. Lower that command through one status-catalog adapter into the current `BattleStatusApplyRequest`/typed-status storage where the runtime still uses generic slots.
4. Keep poison event aggregation in the poison-specific command path.

The temporary adapter may map named semantic values to `potency`/`secondaryPotency` internally. Those slot names must not reappear in YAML, schema, validation diagnostics, or player descriptions.

This boundary limits the initial combat change: parser, validation, schemas, and descriptions become semantic while the established status application system continues to own merge and tick behavior.

### C++-constructed rules remain validated

Runtime validation remains authoritative for typed rules created outside YAML. It must reject:

- a quantity variant not allowed for the status;
- a missing required cap;
- a forbidden or missing effect scope;
- the wrong status effect payload;
- a forbidden reapplication policy;
- invalid duration combinations;
- invalid direct status-value references; and
- ambiguous explicit lifecycle links.

Generated schemas and parser diagnostics provide earlier feedback but do not replace these invariants.

### No compatibility parser

Do not retain old and new forms as compatibility aliases for the same migrated content slice. The phased implementation may temporarily keep only the old `施毒` path and 毒爆's old generic producer/reference fields while their Phase 2 consumers do not yet exist. That is sequencing inside one repository migration, not a supported compatibility surface. Delete each temporary path immediately after its Phase 2 configs and fixtures migrate; at completion, old spellings must be rejected. This avoids maintaining two semantic sources and ensures new descriptions cannot silently fall back to anonymous slots.

## Description AST and rendering amendment

### Current structure

There is one style-independent `EffectDescriptionDocument`, not three ASTs. It contains event sections, rule blocks, facts, raw actions, archetype classification, and source-field coverage. The three render styles are projections:

| Style | Contract |
| --- | --- |
| `Detailed` | Exhaustive developer/config audit: semantic author fields, resolved decisions/defaults, source coverage, ordering, and identity. |
| `Full` | Natural player prose containing all gameplay behavior and decisions. |
| `Compact` | Short player prose with the same behavior, status-specific nouns, and a 72-display-unit row limit. Split rows instead of dropping required semantics. |

### Required semantic node

The description document must receive a typed status application fact containing:

- status identity;
- quantity operation and catalog noun;
- duration and reapplication decision;
- effect scope;
- named effect values; and
- source coverage for each semantic field.

It must not infer gameplay meaning from lowered `potency` slots.

Conceptually:

```cpp
struct DescriptionStatusApplication
{
    BattleStatusKind status{};
    DescriptionStatusQuantity quantity;
    std::optional<DescriptionDuration> duration;
    DescriptionStatusReapplication reapplication;
    DescriptionStatusEffects effects;
};
```

This node is derived from the canonical semantic action. It is not separately authored and does not duplicate values.

### Explicit lifecycle linking

Cross-rule lifecycles such as 七星 and 毒爆 remain whole-document relationships, but matching changes:

- Link by status identity and source ownership.
- Require a unique compatible producer.
- Use typed quantity/reference facts.
- Use explicit named status-value references where applicable.
- Reject or render generically only when a valid typed relationship genuinely cannot be established.
- Do not compare duplicated producer/consumer values; the duplicates no longer exist.

The lifecycle archetype remains useful as a presentation composition, not as a mechanism for reverse-engineering runtime storage.

### Catalog-driven phrases

The renderer uses catalog nouns and effect phrases:

- 戰意: `層`, `每層使招式傷害提高…`.
- 七星: `枚印記`, `印記耗盡時…`.
- 傷害抵擋: `次抵擋`.
- 中毒: `可觸發…次`, `每次造成目前生命…`.
- `持續生效`: `狀態期間…` where a lead is needed.
- `每層提供數值`: normally absorbed into the explicit lifecycle description rather than exposed as “提供數值”.

When a per-layer value and cap are both simple constants, Full may show a derived maximum total. The derived total is presentation-only and is never a second configured value.

### Render at the presentation boundary

Catalog/query metadata should retain either the style-neutral document or enough canonical rules to render it later. `CatalogDetail::Compact` must select `EffectDescriptionStyle::Compact` for its contained effects; `Full` selects Full. Detailed remains an explicit diagnostic/audit choice rather than an accidental player mode.

### Target descriptions: 降龍十八掌

Current generic Full text is opaque:

```text
施加戰意，強度為5，次要強度為1，重複施加時會增加層數，最多10層
```

Target Full:

```text
獲得1層戰意，最多10層；每層使招式傷害提高5%、受到傷害降低1%
10層時，招式傷害共提高50%、受到傷害共降低10%
```

Target Compact:

```text
戰意+1層（每層增傷5%、減傷1%，最多10層）
  滿層增傷50%、減傷10%
```

Target Detailed semantic excerpt:

```text
施放大招
主詞：效果持有者
觀察範圍：效果持有者
施放匹配：本容器綁定武功
對象：效果持有者
動作：增加戰意1層
層數上限：10層
每層生效：招式傷害增加5%
每層生效：傷害減免1%
```

Detailed source coverage uses semantic paths such as `status.quantity.addLayers` and `status.effects.perLayer.skillDamageIncreasePct`. Legacy storage-slot paths are not the primary config audit.

### Target descriptions: 七星劍法

Target Full:

```text
主彈命中時，將該敵人的七星印記設為7枚，持續150幀；再次施加會重設印記與持續時間
任一友軍命中由效果持有者施加七星印記的敵人時，該次招式忽略50%防禦並消耗1枚印記
  印記耗盡時，使該敵人眩暈30幀
```

Target Compact:

```text
主彈命中：七星印記設為7枚（150幀；再施加時重設）
友軍命中七星目標：破防50%、耗1枚
  耗盡時眩暈30幀
```

Target Detailed semantic excerpt:

```text
主彈命中
動作：將七星印記設為7枚
持續時間：150幀
重複套用：重新設定印記數量並重計持續時間

任一友軍命中
觀察範圍：效果持有者同隊的事件來源
條件：目標有由效果持有者施加的七星印記
動作關係：依序
  - 該次招式忽略50%防禦
  - 消耗1枚由效果持有者施加的七星印記
    - 印記耗盡時，施加眩暈30幀
```

### Description coverage requirements

For every migrated status field:

1. Detailed changes when the authored semantic value changes.
2. Full and Compact change when the value affects player-observable behavior.
3. Schema defaults and safety invariants remain visible in Detailed coverage but do not pollute player prose.
4. A specialized renderer may absorb facts into a natural phrase, but coverage must mark the absorption explicitly.
5. No semantic field may become invisible merely because an archetype fails to match.

## Implementation phases

### Phase 0: characterization and target goldens

Before changing authoring types:

1. Pin current runtime behavior for every status catalog row.
2. Add focused poison tests for same-event summation, cross-event/source strongest selection, equal-value no-refresh, stronger replacement, and replace/reset.
3. Pin stun extend/max/replace behavior.
4. Pin 七星 producer/consumer ownership, mark consumption, final stun, and reapplication.
5. Pin 毒爆 layer count, per-layer damage, poison application, and source ownership.
6. Pin damage-block and next-hit-cap charge consumption, including eligible hits that do not reduce damage.
7. Pin status shield and cleanse interactions.
8. Pin 真氣 queue position and every parity property listed above.
9. Record current Detailed/Full/Compact outputs for representative configs.
10. Add target post-migration golden specifications for 降龍, 七星, 枯骨, 真氣, poison, one trigger-charge status, and one block-charge status.
11. Pin 玄冥神掌's ordered poison lifecycle: settle remaining poison damage, remove the old poison, then apply replacement poison.

Phase 0 changes tests only and must not intentionally change combat behavior.

### Phase 1: semantic status frontend and descriptions

1. Add the closed status catalog and typed quantity/effect/policy metadata.
2. Refactor canonical `ApplyStatusAction` into semantic fields.
3. Add authoring descriptors and generated schema support for the one `效果` root and four closed scopes.
4. Implement strict status-specific quantity and policy diagnostics.
5. Remove `套用次數` from authoring.
6. Add semantic runtime validation for C++-constructed rules.
7. Add the semantic status node and semantic coverage paths to the description document.
8. Add catalog-driven Full/Compact/Detailed phrases.
9. Move description style selection to the presentation boundary.
10. Add the central runtime-lowering adapter from semantic actions to the existing status request/storage.
11. Migrate statuses that lower directly without new lifecycle references.
12. Remove old generic fields and parsing forms only for statuses fully migrated in Phase 1. Keep 毒爆's old producer fields (`層數`, `強度`, `次要強度`, `合併方式: 增加層數`, and `層數上限`), its old numeric references, and `施毒` parsing only until their Phase 2 replacements exist; do not expose those temporary paths to already-migrated statuses.

Phase 1 should preserve normalized combat behavior even though the canonical typed action representation changes.

### Phase 2: numeric references and explicit lifecycle ownership

1. Implement parse-time direct-reference `percent = 100` materialization.
2. Add `來源狀態數量`.
3. Add `來源狀態效果值` and validate names against the status catalog.
4. Migrate all five poison applications to `施加中毒`.
5. Preserve poison's existing same-event and strongest-wins behavior exactly.
6. Remove 七星's duplicated producer values and link its lifecycle semantically.
7. Remove 毒爆's duplicated damage/poison values and use named references.
8. Delete 毒爆's old generic producer form, `來源狀態層數`, `來源狀態強度`, and the `施毒` macro parser/metadata after the corresponding configs and fixtures migrate.
9. Author 無影 as a marker with explicit lifecycle ownership.
10. Remove identity `百分比: 100` references.
11. Remove obsolete description-archetype numeric correspondence checks and their tests.

### Phase 3: True-Qi runtime ownership

1. Add a normal pure-damage command contribution for each accepted contact while 真氣 is present.
2. Preserve exact queue position, metadata, provenance, flags, invincibility, descendants, multi-hit behavior, and kill ordering.
3. Migrate 九陽 to the single semantic status application shown above.
4. Remove 九陽's explicit hit consumer.
5. Remove `EffectFormulaInputs::accumulatedStateValue`.
6. Remove `EffectNumberBase::AccumulatedStateValue` if no other use remains.
7. Remove the corresponding parser, schema, validator, description, and evaluator branches.

This is the only phase intended to change ownership on the combat path. Observable behavior must remain identical.

### Phase 4: modifier authoring, separately

After status migration is stable:

1. Make stack contribution implicit for attribute and damage modifiers.
2. Remove `每層: true` from the 11 shipped sites.
3. Split base-percentage operations from percentage-point operations so `百分比加算` has one meaning per author action.
4. Migrate the three persistent `單次承傷上限` macro sites (`chess_combos.yaml:1111`, `:1129`; `chess_equipment.yaml:765`) to `傷害修正` with `方式: 每次承傷不超過最大生命百分比`, preserving values 12, 8, and 8, then remove the old macro descriptor/parser path.
5. Add parser, lowering, runtime, and content-validation coverage for the new persistent-cap operation and rejection of the old macro.
6. Keep resources, areas, private memory, and initialization state machines separate.
7. Do not add named modifier groups until refresh, source ownership, and cleanse can operate atomically on the group.

## File-level implementation map

| Area | Primary files | Expected work |
| --- | --- | --- |
| Canonical types | `src/ChessBattleEffectTypes.h` | Semantic status quantity, policy, payload, references; remove anonymous author fields. |
| Authoring metadata/schema | `src/ChessEffectAuthoringMetadata.h`, schema generator/tests | Closed per-status fields, scopes, enums, and diagnostics; add the persistent-cap modifier operation and remove its macro. |
| Parser | `src/ChessBattleEffectParser.cpp` | Parse semantic forms, poison form, named references, direct-reference default; remove poison and persistent-cap macro paths after migration. |
| Validation | `src/ChessBattleEffectValidation.cpp` | Catalog correspondence, required caps/scopes, policy legality, reference legality, ambiguity checks. |
| Semantics | `src/ChessBattleEffectSemantics.cpp` | Shared status semantic helpers and later modifier-operation split. |
| Command evaluation | `src/battle/BattleEffectSystem.cpp`, `src/battle/BattleEffectSystem.h` | Evaluate named status values and produce resolved semantic commands; poison aggregation stays poison-specific. |
| Status application | `src/battle/BattleEffectCommandSystem.cpp`, `src/battle/BattleStatusSystem.cpp` | Central semantic-to-runtime lowering and Phase 3 True-Qi contribution. |
| Hit ordering | `src/battle/BattleCoreAttacks.cpp` | True-Qi parity and removal of accumulated-state bridge. |
| Damage charges/caps | `src/battle/BattleDamageSystem.cpp` | Characterization and noun-specific lowering; preserve consumption rules. |
| Description document | `src/ChessEffectDescription.h`, `src/ChessEffectDescriptionBuilder.cpp`, `src/ChessEffectDescriptionCoverage.cpp` | Semantic status fact and coverage. |
| Description composition | `src/ChessEffectDescriptionArchetypes.cpp` | Semantic lifecycle links; remove duplicate-value correspondence matching. |
| Description phrases/rendering | `src/ChessEffectDescriptionPhrases.cpp`, `src/ChessEffectDescriptionRenderer.cpp` | Catalog nouns/effects and target three-style prose. |
| Query/display plumbing | `src/ChessCatalogQueries.cpp`, `src/ChessJsonCodec.cpp`, GUI display callers | Defer rendering or pass the requested style through. |
| Config migration | Four authoritative top-level configs | Migrate every old status/poison/modifier form; do not edit copied configs. |

## Validation and diagnostics

Diagnostics should identify the status and the closed correction. Examples:

```text
狀態「七星」使用「印記」數量；不接受「增加層數」。請使用「設定印記層數」。
```

```text
狀態「戰意」需要「層數上限」；省略不代表無上限。
```

```text
狀態「枯骨」只接受「效果.持續生效」，不接受「效果.每層生效」。
```

```text
狀態「眩暈」不接受數量欄位。
```

```text
「同事件合併」只適用於「施加中毒」且重複套用為「保留較高傷害」。
```

```text
狀態「毒爆」沒有名為「強度」的效果值；可引用「死亡爆炸純粹傷害」。
```

Unknown fields must fail. The parser must not ignore a field merely because it is irrelevant to a selected status.

## Testing strategy

### Parser and validation

- One valid fixture for every status catalog row.
- Wrong quantity noun for every quantity family.
- Missing required cap.
- Forbidden cap or quantity on a no-quantity status.
- Wrong effect scope and wrong named field.
- Missing required effect payload.
- Forbidden/required reapplication policy combinations.
- Poison policy/aggregation matrix.
- Named status quantity/value reference success and failure.
- Direct-reference omission materializes 100%; constant formulas retain existing behavior.
- Old removed fields, including `來源狀態層數` and `來源狀態強度`, fail parsing/schema validation after their migration phase.
- The new persistent-cap modifier operation parses and the old `單次承傷上限` macro fails after Phase 4 migration.

### Canonical lowering

- Semantic actions lower to the same resolved status requests as current shipped behavior.
- Formula evaluation, rounding, duration, source metadata, and limits match.
- Poison event aggregation remains deterministic and poison-specific.
- Additive status quantities and same-event poison aggregation use widened arithmetic and saturate at their semantic cap (`層數上限` when present, otherwise the runtime `int` maximum).
- Persistent max-HP percentage caps use widened multiplication and clamp only after division.
- The three persistent-cap modifiers lower to the same 12%, 8%, and 8% caps as the old macro.
- C++-constructed invalid semantic actions fail runtime validation.

### Runtime characterization and parity

- Every behavior in the normative runtime table.
- Cross-source/source-owner behavior for 七星, poison, and cleanse.
- Status shield and control immunity interactions.
- Charge consumption edge cases.
- `INT_MAX` boundary cases for poison aggregation, poison/bleed/general layer addition, and additive stun duration.
- 玄冥神掌 settles remaining poison damage, removes the old poison, and applies replacement poison in that order.
- True-Qi ordering and multi-hit behavior.
- Death, execute, invincibility, and kill-order edge cases affected by status contributions.

### Descriptions

- Exact target rows for 降龍 and 七星 in all three styles.
- Semantic mutations change Detailed and every affected player projection.
- Status-specific nouns are used consistently.
- Derived full-stack totals appear only for simple constant layer/cap pairs.
- Compact rows obey the display-width constraint without dropping required facts.
- Specialized lifecycle composition remains source-correct.
- Catalog `compact` actually renders Compact effect descriptions.
- Coverage has no unmatched semantic status fields for shipped content.

Primary suites include:

- `tests/ChessBattleEffectParserUnitTests.cpp`
- `tests/ChessBattleEffectValidationUnitTests.cpp`
- `tests/ChessEffectDescriptionUnitTests.cpp`
- `tests/BattleStatusSystemUnitTests.cpp`
- `tests/BattleEffectSystemUnitTests.cpp`
- `tests/BattleDamageSystemUnitTests.cpp`
- `tests/BattleCoreAttacksIntegrationUnitTests.cpp`

### Content and build verification

After each non-trivial implementation phase:

1. Run all unit tests, not only focused effect tests.
2. Run schema/content validation for shipped Easy, Normal, and Hard content.
3. Attempt a debug build with `.github/build-command.ps1`.
4. Treat only a final-link failure caused by a running game as an acceptable build outcome.
5. Confirm only authoritative top-level configs were intentionally migrated.

## Risks and mitigations

| Risk | Mitigation |
| --- | --- |
| Semantic values are lowered too early and descriptions regress to `強度`. | Make the semantic action canonical and build the description document before runtime lowering. |
| A large migration silently changes merge behavior. | Characterize each policy first; centralize lowering; migrate all content without compatibility aliases. |
| Poison equality or same-event behavior changes. | Dedicated matrix tests for aggregation, strongest selection, equality, replacement, source, duration, and triggers. |
| True-Qi damage occurs at a different queue position. | Exact ordering/provenance characterization and integration tests before removing the bridge. |
| Cross-rule lifecycle linking becomes ambiguous. | Require a unique compatible producer and reject ambiguity with a semantic diagnostic. |
| Compact descriptions omit behavior to fit. | Split semantic rows; keep required facts; retain the 72-unit width test. |
| Catalog metadata continues to pre-render Full. | Defer rendering or pass style explicitly at every presentation boundary. |
| Catalog and runtime switches diverge. | Use shared typed catalog descriptors/helpers in parser, validation, lowering, and descriptions; add agreement tests. |
| Generic runtime slots leak back into authoring. | Remove old descriptor/parser paths and reject old fields after migration. |
| Phase 1 removes generic parsing needed by Phase 2 content. | Scope deletion per migrated status; temporarily retain only 毒爆's old producer/references and `施毒`, then delete them in Phase 2. |
| Persistent-cap macro migration changes its percentage basis or lifetime. | Characterize all three opening modifiers, preserve values and permanent lifetime through the new modifier operation, and test old/new lowering equivalence before removing the macro. |

## Definition of done

The migration is complete only when:

- all 34 status applications and all 5 poison applications in the authoritative configs use the new semantic forms;
- every status accepts exactly its catalog-defined quantity, scope, fields, and policies;
- `強度`, `次要強度`, generic status `層數`, generic status `合併方式`, `套用次數`, and author-facing `同事件合計強度` are absent from status authoring;
- `來源狀態層數` and `來源狀態強度` are absent from parser, schema, validation, descriptions, and evaluator branches;
- direct references no longer write identity `百分比: 100`;
- 七星 and 毒爆 have no duplicated producer/consumer values;
- 九陽 has no explicit hit consumer or accumulated-state bridge;
- all three persistent `單次承傷上限` macros use the new canonical `傷害修正` operation and the old macro is rejected;
- Full, Compact, and Detailed descriptions are produced from semantic status facts;
- catalog compact/full selection reaches the effect renderer;
- every migrated semantic field has description coverage;
- poison and all other characterized runtime behavior remains unchanged;
- all generated schemas and shipped content validation pass;
- all unit tests pass; and
- the required debug build is attempted successfully, subject only to the documented running-game final-link exception.

## Review checklist

Reviewers should explicitly confirm:

1. The four effect scopes are sufficient and closed rather than arbitrary action containers.
2. Each status has the correct quantity model and required/forbidden cap behavior.
3. Reapplication semantics, especially poison and stun, match current runtime behavior.
4. 七星 remains an explicit cross-rule lifecycle and no longer duplicates values.
5. 毒爆 uses named quantity/value references and no identity coefficient.
6. 真氣's Phase 3 parity requirements are complete enough to prevent ordering drift.
7. The semantic action is available to descriptions before runtime lowering.
8. Detailed/Full/Compact contracts and target wording are acceptable.
9. Query/display plumbing selects the requested effect-description style.
10. The design does not accidentally create a universal buff runtime or absorb modifiers/resources/areas/private state into typed statuses.
11. Phase 1 deletion is scoped so 毒爆 and `施毒` survive only until their Phase 2 replacements land.
12. The three persistent-cap macros have an explicit Phase 4 migration and equivalence tests.
13. 玄冥神掌 preserves settle, remove, and replacement-poison actions as three ordered first-class actions.
