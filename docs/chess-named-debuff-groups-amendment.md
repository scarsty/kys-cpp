# Chess named-debuff groups and producer-local buffs amendment

## Document status

This is the normative delivery amendment for the status-authoring and runtime
work. It starts from behavior baseline `b87074e` and from the partially migrated
working tree. Phases 1 through 6 are implemented in the current working tree.
The final Debug build, all 15 schema/content checks, and the complete C++ suite
pass (205,561 assertions in 1,022 cases). Independent audits found no remaining
substantiated correctness or migration defect after the documented findings
were corrected.

This document supersedes the incompatible parts of:

- `chess-status-contributions-and-composable-behaviors-design.md`; and
- `chess-status-effect-authoring-and-description-design.md`.

In particular, it supersedes those documents wherever they say that every
same-name status is independently producer-owned, that `流血`, `寒毒`, `枯骨`,
`七星`, `化勁`, or `刺目` may keep multiple independently effective complete
debuff contributions, or that exclusive `化勁`/`刺目` contenders use
winner-only consumption.

The earlier documents remain normative for generic effect rules, structured
execution ordering, two-phase numeric binding, default single-target damage,
semantic descriptions, and producer-owned positive contributions unless this
amendment explicitly says otherwise.

Implementation must not resume from the independent-debuff model and then
patch balance locally. The storage and reducer pivot in this amendment happens
first; content, descriptions, and cleanup follow from that model.

## Executive decision

Status storage is deliberately asymmetric:

- positive statuses default to producer-local contributions, so buffs from
  genuinely different sources can coexist without stealing caps, values,
  provenance, or execution origins;
- negative statuses default to one holder-local named group, so applying the
  same player-facing debuff from another source does not multiply the complete
  debuff accidentally;
- controls use holder-local duration reducers;
- known exceptions such as `中毒`, `眩暈`, and `封內` retain explicit catalog
  reducers;
- triggers, targets, conditions, chance, formulas, provenance, and structured
  ordering remain reusable parts of the generic effect engine; and
- the named debuff catalog owns the behavior and aggregation contract that
  gives the debuff name its player-facing meaning.

This is not a global config file. The catalog is one C++ semantic registry used
by parsing, schema generation, validation, lowering, runtime reduction,
descriptions, and agreement tests. Config still says when, where, and with what
reviewed parameters a debuff is applied.

The design rejects both unsafe extremes:

- one generic same-name object that lets the newest producer overwrite
  unrelated buff layers; and
- independent complete debuff payloads whose caps and percentages multiply by
  the number of producers.

## Goals

1. Keep positive contributions independently safe.
2. Keep named debuffs bounded and balanceable per holder.
3. Make each debuff name tell the truth about its behavior.
4. Keep the generic trigger/action engine instead of recreating per-skill
   execution code.
5. Remove anonymous `強度`, secondary-strength slots, and unexplained identity
   percentages such as `百分比: 100`.
6. Make the YAML explain the authored choice without requiring the author to
   restate catalog-owned mechanics.
7. Preserve baseline combat behavior unless this document lists an intentional
   change.
8. Preserve exact source attribution and established pipeline positions.
9. Make Compact, Full, and Detailed descriptions derive from resolved semantic
   data rather than raw YAML spelling.
10. Delete superseded parser/schema/runtime compatibility paths after all
    shipped configs and fixtures migrate.

## Non-goals

1. This does not create a global YAML status-definition service.
2. This does not forbid new producers from applying an existing named debuff.
3. This does not hard-code a skill whitelist.
4. This does not make all negative attribute modifiers into named statuses.
5. This does not preserve removed authored names as aliases.
6. This does not infer a generic `max`, `average`, or `latest` reducer for
   arbitrary status payloads.
7. This does not move established damage/healing/speed calculations to more
   convenient pipeline phases without equivalence evidence.

## Core invariants

1. Every status group belongs to exactly one holder unit.
2. No capacity, replacement, duration clock, cleanse operation, or consumption
   crosses holder units.
3. A positive producer family is scoped to `(holder unit, producer family)`.
   The family key identifies the granting definition and logical owner; the
   holder is the storage boundary, not part of a global pool.
4. A named debuff group is scoped to `(holder unit, status identity)`.
5. The catalog supplies exactly one storage model and reducer for each status.
6. A catalog-owned debuff cannot also accept an authored generic behavior
   payload or a second authored reapplication authority.
7. Application protections run before the group reducer. A fully blocked
   application changes nothing.
8. An accepted selected-instance replacement changes its complete packet
   atomically: quantity, duration, bound parameters, effective source,
   provenance, and application sequence.
9. A rejected or zero-effect application cannot steal attribution, reset a
   clock, or reorder cleanse state.
10. Stored behavior dispatch uses snapshot-plus-liveness: new state does not act
    in the current dispatch, and removed state cannot act from a stale snapshot.
11. Runtime clones and borrowed rules resolve back to a stable authored family;
    a runtime instance ID may distinguish provenance but cannot manufacture
    extra family capacity.
12. Adding a new catalog row, storage variant, numeric base, or payload field
    must fail structurally until every exhaustive consumer is updated.

## Storage taxonomy

The holder owns one entry per status identity. Its group state is explicit:

```cpp
using BattleStatusGroupState = std::variant<
    ProducerOwnedContributions,
    SharedLayerDebuff,
    SelectedDebuffInstance,
    SharedDurationControl>;
```

The holder also retains one globally application-ordered contribution range.
Each group-state variant stores indices into that range; the indexed
`BattleStatusContribution` owns the packet data. This keeps provenance and
active-rule dispatch in exact application order without duplicating packet
state, while making the catalog-selected storage shape structurally visible.
The public contribution range remains mutable for packet fields used by the
runtime; if a caller changes a packet's status identity, the storage refreshes
its derived taxonomy at the next group query so cached group indices cannot go
stale.

`ProducerOwnedContributions` indexes one or more independently attributed
contributions. It is the default for positive buffs and is the only group state
that may index multiple packets.

`SharedLayerDebuff` indexes the single packet that owns one shared quantity, one
target-total ceiling, one clock, and group attribution. It is used by `流血`.

`SelectedDebuffInstance` indexes one complete selected application packet. Its
quantity may be marks, charges, or a periodic charge count; its packet may also
contain duration and named bound parameters. It is used where one complete
debuff instance must win or replace rather than stack.

`SharedDurationControl` indexes the single packet that owns the effective
holder-local control clock and its reducer state. It is used by `眩暈` and
`封內`.

A fifth `SharedChargeDebuff` top-level variant is not needed. `化勁` and `刺目`
are selected packets with charge quantity. If a later mechanic needs charges
from several simultaneous producers to coexist inside one debuff group, that is
a new reviewed storage/reducer design rather than an overloaded variant.

## Normative status matrix

| Status | Polarity | Storage | Authored quantity/parameter surface | Behavior contract | Reapplication authority |
| --- | --- | --- | --- | --- | --- |
| `中毒` | Negative | `SelectedDebuffInstance` with poison arbitration | charge count, duration, poison damage profile | periodic damage/charge lifecycle and comparator | authored choice: `保留較高傷害` or `取代現有中毒` |
| `流血` | Negative | `SharedLayerDebuff` | `增加層數`, `目標總層數上限` | one 10-frame clock and one combined max-HP transaction | catalog shared-layer reducer |
| `眩暈` | Negative control | `SharedDurationControl` | duration | control clock | authored choice: `延長持續時間` or `保留較長持續時間` |
| `封內` | Negative control-like | `SharedDurationControl` | duration | MP blocking and keep-longer clock | catalog reducer |
| `寒毒` | Negative | `SelectedDebuffInstance` | duration | healing blocked and speed −25% | catalog exact-duration refresh |
| `枯骨` | Negative | `SelectedDebuffInstance` | duration | damage taken +25% and received healing ×25% | catalog exact-duration refresh |
| `七星` | Negative | `SelectedDebuffInstance` | mark count and duration | allied-hit observation, defense ignore, mark consumption, depletion stun | catalog complete replacement |
| `化勁` | Negative | `SelectedDebuffInstance` | trigger charges and named shield formula | outgoing cast suppression and shield continuation | catalog complete replacement |
| `刺目` | Negative | `SelectedDebuffInstance` | trigger charges | outgoing cast suppression | catalog complete replacement |
| `下一次受到攻擊必定落空` | Positive | `ProducerOwnedContributions` | trigger charges and duration | incoming miss action may remain a bound generic behavior | producer-local quantity operation |
| `傷害抵擋` | Positive | `ProducerOwnedContributions` | add/set block-charge verbs and local ceiling | non-execute positive-damage block | quantity verb; no authored `重複套用` |
| `下次承傷上限` | Positive | `ProducerOwnedContributions` | trigger charges and application-bound cap | strongest eligible cap arbitration | producer-local contributions, value-aware winner |
| `戰意` | Positive | `ProducerOwnedContributions` | added layers and family ceiling | persistent modifiers remain locally authored | producer-family additive allocation |
| `真氣` | Positive | `ProducerOwnedContributions` | added layers and family ceiling | stored hit behavior remains locally authored | producer-family additive allocation |
| `無影` | Positive | `ProducerOwnedContributions` | duration | stored attack-generation behavior remains locally authored | authored `刷新持續時間` |
| `毒爆` | Positive/internal marker | `ProducerOwnedContributions` | added layers and family ceiling | stored death behavior remains locally authored | producer-family additive allocation |

Any additional status must choose one of these models in the single catalog. It
does not inherit behavior from “positive” or “negative” alone.

## One catalog, not parallel switches

The catalog is the authoritative row set for:

- authored and displayed status label;
- polarity and control/cleanse classification;
- storage model;
- quantity noun and legal quantity fields;
- duration requirement;
- legal authored reapplication choices, if any;
- catalog reducer;
- catalog-owned behavior profile and named tunable fields;
- parser/schema field descriptors;
- validation constraints and diagnostics;
- runtime lowering and projection;
- Compact/Full/Detailed description phrases; and
- generated positive and negative fixtures.

The parser and validator may dispatch by a catalog-owned descriptor, but they
must not maintain independent field lists. The schema renderer must visit the
same descriptors. Runtime lowering and description code must use exhaustive
visitors over the same semantic payload variant.

Agreement is structural, not a size assertion and not a hash pin. Tests walk
every catalog row and verify labels, fields, requiredness, scopes, policy sets,
lowering handlers, description handlers, and schema shapes. Adding a payload
variant without updating a numeric visitor must fail at compile time or in a
focused structural test; it must not silently evaluate to `{0, 0}`.

For catalog-owned named debuffs, “name truth” is stronger than a producer-side
minimum profile. Authors do not retype the profile at every application. The
catalog provides it once, and contract tests prove that lowering and all three
description styles expose that profile. This keeps the parser open to any
producer while preventing a `寒毒` that accidentally omits healing prevention.

## Locked `重複套用` vocabulary

### Surviving authored values

The final author-facing vocabulary is locked to these status-discriminated
choices:

| Status | Legal authored values |
| --- | --- |
| `中毒` | `保留較高傷害`, `取代現有中毒` |
| `眩暈` | `延長持續時間`, `保留較長持續時間` |
| `無影` | `刷新持續時間` |

The field is required for those shipped producer surfaces. A status-specific
schema branch exposes only the values legal for that status; a single broad enum
must not imply that, for example, poison can extend duration or `無影` can keep
the stronger poison.

Implementation should prefer typed semantic policies such as
`PoisonReapplicationPolicy`, `StunReapplicationPolicy`, and the single allowed
`無影` refresh mode. A generic enum containing impossible cross-status
combinations is not a design requirement.

### Catalog-owned statuses

Authors omit `重複套用`, and parser/schema validation rejects it, for:

| Status | Catalog rule replacing the authored field |
| --- | --- |
| `流血` | add into one shared holder-local layer group |
| `封內` | keep the longer effective duration |
| `寒毒` | replace remaining duration with accepted post-protection incoming duration |
| `枯骨` | replace remaining duration with accepted post-protection incoming duration |
| `七星` | replace the complete existing `七星` packet |
| `化勁` | replace the complete existing `化勁` packet |
| `刺目` | replace the complete existing `刺目` packet |
| `傷害抵擋` and other quantity-verb buffs | the add/set quantity verb defines the operation |

Descriptions still state these catalog-owned reapplication semantics where they
matter to the player. Omission from YAML does not mean omission from Full or
Detailed descriptions.

### Removed values

The following authored labels have no surviving use and are removed from the
parser, generated schema, examples, fixtures, descriptions, and semantic enum:

- `取代並重設`;
- `取代整組`;
- `取代同一效果`; and
- `取代持續時間`.

`取代並重設` migrates only to poison-specific `取代現有中毒`.
`七星` replacement is catalog-owned and therefore has no authored
`取代現有七星` value. That phrase remains valid player-facing prose, not an
authoring enum.

There is intentionally no generic surviving `取代同一效果` policy for
producer-local buffs. `傷害抵擋` is not a counterexample: its authored
`增加可抵擋次數` and `設定可抵擋次數` verbs already define whether charges are
added or set. Adding a second replacement field would create two authorities.

### Baseline and intermediate-worktree evidence

An audit of the three shipped top-level configs at baseline `b87074e` found:

- two poison sites using `取代並重設`;
- three poison sites using `保留較高傷害`;
- seven stun sites using `延長持續時間`;
- seven stun/MP-block sites using `保留較長持續時間`;
- `寒毒`, `枯骨`, and `無影` as the three `刷新持續時間` sites; and
- no authored `取代同一效果`, `取代整組`, or `取代持續時間` sites.

The baseline `化勁` and `刺目` applications contain no `重複套用` field. Their
migration therefore continues an existing omission; it does not remove an
authored policy from shipped content.

The current partially migrated working tree temporarily contains
`重複套用: 取代同一效果` for `七星` and replacement-poison sites. Those are
intermediate artifacts of the superseded design and must be removed/migrated;
they are not evidence for preserving the label.

## `流血`: one bounded holder-local group

### Canonical authoring

```yaml
套用狀態:
  狀態: 流血
  增加層數: 1
  目標總層數上限: 3
```

The explicit noun `目標總層數上限` is required. It says that the value is the
holder's shared bleed ceiling, not the applying producer's private capacity.

The author does not repeat the interval, maximum-HP percentage, rounding,
minimum, damage kind, or transaction count. Those are one reviewed catalog
profile:

```text
every 10 frames
damage = target maximum HP × total bleed layers × 1%
round toward zero
minimum 1
damage kind = 流血
one combined transaction
```

There is no per-producer percentage override. A future stronger bleed requires
a named reviewed catalog variant plus an explicit group aggregation rule.

### Creation and reapplication

On group creation:

```text
quantity = min(incoming layers, incoming target-total ceiling)
group ceiling = incoming target-total ceiling
frames until tick = 10
```

While the group is active:

```text
group ceiling = max(existing ceiling, incoming target-total ceiling)
added = min(incoming layers, group ceiling - existing quantity)
quantity = existing quantity + max(0, added)
```

The ceiling ratchets upward only while the current group exists. A lower-ceiling
application never clamps existing layers downward.

An application that adds at least one layer becomes the effective source for
future combined ticks. An application rejected at the ceiling does not steal
attribution, reset the clock, or change cleanse ordering.

### Lifecycle

- The clock begins at the first accepted application.
- Adding layers never resets the clock.
- If an application is ordered before a due tick in the same merged dispatch,
  the already-snapshotted tick still occurs on schedule using the dispatch-start
  layer count and effective source. The accepted layers and source apply to
  later dispatches; they do not act retroactively in the current snapshot.
- There is no authored duration.
- Ticks do not consume layers.
- The group persists until explicit removal/cleanse or until holder/battle state
  is discarded.
- Dead holders do not tick.
- Removing or clearing the complete group resets quantity, ceiling, clock, and
  effective source. A later application creates a fresh group and starts again
  at its own ceiling.

Therefore, after a cap-3 `刀客` bleed is fully removed, a later cap-1 `快刀`
application starts at cap 1. The old cap does not survive invisibly to the end of
battle.

### Shipped balance result

The `刀客` thresholds continue to declare target-total ceilings 1, 2, and 3.
The two `鴛鴦刀` spiral owners feed the same holder-local group and do not each
add another private ceiling. Their scripted path must lower to the same group
application command as config-authored bleed.

The former independent-contribution result—where aggregate effective ceilings
could reach 5, 12, or 23 at the corresponding thresholds—is rejected as too
powerful. One shared transaction also avoids multiplying minimum damage,
rounding, shield interaction, invincibility checks, modifier hooks, and damage
events by producer count.

## Selected fixed-profile debuffs

### `寒毒`

The catalog profile is exactly:

- healing is blocked; and
- speed is reduced by 25%.

Only one selected `寒毒` packet exists on a holder. Different producers do not
stack the speed penalty and cannot create a same-name instance that omits the
healing block.

An accepted reapplication replaces remaining duration with the incoming
application's duration **after** status-shield/protection resolution:

```text
existing 30 + accepted incoming 90 -> 90
existing 80 + protected incoming 70 -> 70
existing 80 + fully blocked incoming -> 80
```

This preserves baseline `刷新持續時間`, including the sometimes useful ability
for status protection to turn an incoming refresh into a shorter clock. It does
not use “keep the longer duration.”

### `枯骨`

The catalog profile is exactly:

- damage taken increases by 25%; and
- received healing is multiplied by 25%.

Only one selected packet exists. Different producers do not turn the damage
increase into +50% or ordered healing into ×6.25%.

Its accepted/blocked duration behavior is identical to `寒毒` and preserves
baseline refresh semantics. The existing ordered healing multiplication and
rounding point remain unchanged.

Because the shipped profiles are catalog-fixed, neither status needs an
“effective instance comparator.” Equal-profile comparison is trivial and the
accepted incoming packet refreshes duration exactly. If a future design gives
either status several reviewed strengths, the catalog must add a named total
ordering over the complete profile before such authoring is accepted; the
implementation may not improvise a field order.

### `七星`

There is one selected mark packet per holder. Any successful new application
atomically replaces the complete existing packet, including:

- marks;
- remaining duration;
- bound parameters;
- effective source/team observation;
- provenance and structured origin; and
- application sequence.

Replacement may be a downgrade: a successful three-mark application replaces
an existing seven-mark packet. That is deliberate parity with replacement
semantics, not an accidental “strongest wins” rule. A fully blocked or rejected
application leaves the old packet unchanged.

The catalog owns allied-hit observation, the reviewed defense-ignore value,
one-mark consumption, and depletion stun. No independent producer can stack a
second defense-ignore behavior under the same displayed debuff.

### `化勁` and `刺目`

Each status has one selected charge packet per holder. A successful same-name
reapplication replaces the complete packet; charges and per-application bound
values do not merge across producers.

`化勁` owns the outgoing-cast suppression behavior and a named, application-
bound shield formula. When it is consumed, the shield comes from the exact
active `化勁` packet; attribution cannot come from another producer.

The canonical parameter shape is:

```yaml
套用狀態:
  狀態: 化勁
  可觸發次數: 1
  化解後護盾:
    每星級: 100
```

`化解後護盾` is an `EffectNumber` evaluated under the closed binding-phase
catalog. The field name is status-specific and describes the value it controls;
there is no anonymous `強度` slot.

`刺目` owns outgoing-cast suppression without the shield continuation.

The two different debuffs may coexist. Preserve the shipped cross-status rule:

1. Gather all eligible attacker-side `化勁` and `刺目` packets for the cast.
2. If at least one is eligible, suppress the cast.
3. Consume every eligible attacker-side suppression packet for that cast.
4. If `化勁` was present, grant the active consumed `化勁` packet's shield.
5. Do not consume defender-side `下一次受到攻擊必定落空`, because the cast has
   already been stopped on the attacker side.

This section supersedes the earlier winner-only `化勁`/`刺目` arbitration. The
stable structured order still defines deterministic inspection and presentation
order, but does not change the all-eligible consumption rule for this named
cross-status suppression group.

## Existing named reducers

### `中毒`

Poison retains two authored, reviewed choices:

```yaml
重複套用: 保留較高傷害
```

uses the catalog-owned poison damage comparator and the reviewed compatible
same-event aggregation path.

```yaml
重複套用: 取代現有中毒
```

replaces the complete current poison packet and cannot opt into same-event
damage aggregation.

`結算剩餘中毒傷害` and `移除狀態: 中毒` remain first-class actions.
`玄冥神掌` must preserve its ordered sequence: settle remaining poison damage,
remove poison, then apply the replacement poison. Lowering may not subsume or
reorder those steps.

### `眩暈`

`延長持續時間` and `保留較長持續時間` are the only authored choices. Omission
is invalid. The obsolete, unshipped `取代持續時間` label is removed.

### `封內`

The catalog always retains the longer effective duration. Config supplies only
duration and omits `重複套用`.

## Producer-local positive statuses

Positive contributions retain the independent-safety work. On one holder,
several producer families may contribute the same positive identity without
overwriting each other's caps, bound values, sources, or execution origins.

Within one producer family:

```text
used = checked sum(active family quantity on this holder)
available = local family ceiling - used
allocated = min(requested increase, max(0, available))
```

Runtime generations or borrowed aliases do not receive another ceiling. The
same producer applied to another holder receives that holder's independent
allocation. This specifically guarantees that one action applying
`下一次受到攻擊必定落空` to three allies gives each ally one charge.

Different positive producers may stack when their mechanics permit it:

- `真氣` layers retain per-contribution hit values and origins;
- `戰意` layers retain per-contribution persistent modifier values;
- `傷害抵擋` charges coexist safely;
- `下一次受到攻擊必定落空` charges coexist safely; and
- `下次承傷上限` contributions coexist but use an explicit strongest-cap
  resolver on a damage transaction.

There is no group-wide `max` cap or potency for these buffs.

## Defensive priority and exact consumption

Eligible damage protection resolves in this order:

```text
execute bypass
-> invincibility
-> dual-wield/native full block
-> status-based 傷害抵擋
-> strongest 下次承傷上限
-> absorption/reduction layers
-> shield
-> HP
```

Full blocks return before single-hit-cap selection. Therefore a hit blocked by
`傷害抵擋` consumes the chosen block contribution and leaves every
`下次承傷上限` contribution untouched. Baseline `b87074e` already behaves this
way; it is parity and requires a characterization test.

For multiple live `下次承傷上限` contributions:

- resolve each application-bound absolute cap;
- choose the smallest positive value;
- break equal-value ties by oldest application sequence;
- consume only that exact contribution; and
- consume it on the next eligible positive non-execute transaction even when
  incoming damage is already below the cap.

Selection is value-aware and is not inferred from source registration order.

## Persistent modifier pipeline parity

Moving behavior ownership must not move arithmetic.

Every persistent status-derived modifier enters the same accumulator and at the
same relative pipeline point used at baseline unless Phase 0 proves an intended
move equivalent over mixed cases. This includes:

- `戰意` skill-damage increase at the existing `skillDamagePct` fold-in and
  scaling point;
- `戰意` damage reduction with the existing positive-reduction sign
  convention, even if author-facing prose says “每層減傷 1%”;
- `枯骨` incoming-damage increase;
- `枯骨` ordered received-healing multiplier; and
- `寒毒` speed reduction and healing block.

The implementation must not translate `戰意` into an arbitrary generic
“final-phase” modifier merely because the YAML AST can express one. Phase 0
records mixed defense, ignore-defense, shield, cap, rounding, and multiple-
modifier cases before changing storage.

## Numeric binding contract

The closed binding-phase catalog from
`chess-status-contributions-and-composable-behaviors-design.md` remains
normative:

- `Literal`: constants and internal bound ratios;
- `ApplicationBound`: source star/attack/max HP, source resource ratios, source
  status quantity, stored state values, and application-target max HP;
- `EventLive`: selected target HP/shield/cooldown/max HP and resolved event
  damage; and
- `ContributionLive`: the executing positive contribution's current quantity
  and `每層數值` scale.

Catalog-owned debuffs use named application fields rather than a generic
behavior-defined “current contribution value.” Mixed formulas capture only
application inputs; they evaluate event inputs and round/min/max once at the
final action boundary.

Adding a numeric base requires one catalog row covering label, legal contexts,
binding phase, binder, evaluator, compatibility behavior, schema, and
description. No consumer may infer the phase from the field name.

## Structured execution order and provenance

The existing structured ordering work remains. Ordering compares the established
source-precedence lane before registration/rule/action/target/command positions.
Phase 0 must compare mixed Combo, Equipment, EquipmentSynergy, Neigong, and
Magic scheduling against baseline; a raw enum ordinal or a renumbered rule-store
index is not an acceptable replacement for that order.

Stored positive behavior retains stable authored origin tokens through repeated
rule-store removal/rebuild cycles. No `origin->ruleOrder + 1` snapshot is allowed
to drift relative to live mixed-source rules.

Catalog-owned debuff actions retain the effective selected source or group
source specified above. Cleanse/removal atomically removes the corresponding
active rule view.

Clone/borrow audits must explicitly cover all three shipped `生成分身` sites.
If clone-executed rules can apply a status, tests must prove their authored
definition identity, logical granting owner, holder-local storage, and bounded
family behavior.

## Removal, cleanse, expiry, and presentation

- Status presence means the holder-local group has positive effective state.
- Removing a status name removes that holder's whole named group unless an
  action explicitly filters a positive contribution.
- Cleansing counts player-visible status groups, not internal positive
  contributions.
- Expiry of one positive contribution leaves the group visible while another
  remains.
- Selected debuff expiry removes its whole selected packet.
- Shared duration control expires when its effective clock reaches zero.
- `流血` has no duration and follows its explicit lifecycle above.
- Presentation is a projection; it cannot merge storage or choose a different
  reducer.

## Authoring and description examples

### `流血`

Compact:

```text
施加1層流血（目標總上限3層）
```

Full:

```text
施加1層流血，目標共享上限3層；每10幀造成一次目標最大生命1% × 流血總層數的流血傷害
```

Detailed:

```text
所有來源在此目標共享流血層數上限與10幀計時；增加層數不重置計時，每次結算為一筆合併傷害，向零取整且至少1點
```

### `七星劍法`

Target YAML:

```yaml
套用狀態:
  狀態: 七星
  設定印記層數: 7
  持續幀數: 150
```

Compact:

```text
施加7枚七星印記（150幀）
```

Full:

```text
對命中目標施加7枚七星印記，持續150幀；友方招式命中時無視50%防禦並消耗1枚，耗盡後眩暈30幀；再次施加會取代現有七星
```

Detailed additionally states that replacement atomically changes the complete
holder-local packet and may replace seven marks with fewer marks. It does not
show a fictional authored `重複套用` field.

### `降龍十八掌`

Target YAML remains producer-local:

```yaml
套用狀態:
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
            方位: 承受
            階段: 防禦前
            傷害種類: 全部
            方式: 百分比加算
            每層數值: -1
```

Compact:

```text
獲得1層戰意（此來源上限10層）
```

Full:

```text
獲得1層戰意，此來源上限10層；每層使招式傷害+5%、受到傷害減少1%
```

Detailed states that other producer families may contribute their own `戰意`
without borrowing this source's ten-layer ceiling or per-layer values, and that
the two modifiers enter their established damage-pipeline fold points.

### Damage `範圍`

The earlier decision remains unchanged:

- omitted `範圍` resolves to single target;
- Full/Compact omit redundant single-target wording;
- Detailed may show the resolved single-target default; and
- explicit `範圍: 單體` is rejected.

## Deliberate behavior changes

The implementation must expose and test these intentional changes relative to
baseline or the partially migrated independent-contribution tree:

1. Different positive producers no longer overwrite each other's cap, values,
   source, or origin.
2. Same-name `流血` producers share one holder-local quantity, ratcheting
   target-total ceiling, clock, and transaction rather than independent private
   caps/transactions.
3. The two `鴛鴦刀` owners no longer add private bleed ceilings above the active
   `刀客` target-total ceiling.
4. Same-name `寒毒` producers cannot stack speed penalties or omit healing
   blocking.
5. Same-name `枯骨` producers cannot stack damage-taken or healing multipliers.
6. Any accepted `七星` application replaces the complete existing packet even
   when the new mark count is lower.
7. Same-name `化勁` or same-name `刺目` applications replace selected packets
   instead of accumulating independently valued charges.
8. `化勁` and `刺目` preserve all-eligible cross-status consumption on one
   suppressed cast; this explicitly reverses the partially implemented
   winner-only experiment.
9. A contribution created during one dispatch cannot act during that dispatch;
   a contribution removed earlier loses remaining snapshotted executions.
10. Two authored status-application actions in one positive rule remain distinct
    producer families when their action orders differ.
11. Cleanse counts visible named groups rather than positive internal
    contributions.
12. Explicit `範圍: 單體` becomes invalid; omission is canonical.
13. Stronger-poison replacement reports `Replaced`, so the normal status-applied
    cue is emitted. This is a presentation correction.
14. Catalog-owned debuff behavior moves out of repeated producer YAML without
    moving its combat pipeline position.
15. Removed generic reapplication labels are rejected rather than accepted as
    aliases.

Ordinary `寒毒`/`枯骨` refresh is **not** listed as a change: the accepted
incoming post-protection duration replaces remaining duration exactly, matching
baseline behavior.

## Phase 0: mandatory characterization before migration

No storage rewrite begins until these baseline tests are recorded:

1. `寒毒` and `枯骨`: 30 + 90 -> 90, 80 + protection-reduced 70 -> 70, and a
   fully blocked application leaves 80.
2. `流血`: group clock creation, non-reset on reapplication, lack of duration,
   non-consumption on tick, explicit removal reset, dead-holder behavior, and
   the current multi-source balance delta.
3. `七星`: full packet replacement, including downgrade and blocked incoming
   application.
4. `化勁` plus `刺目`: one cast is suppressed, all eligible attacker-side
   packets are consumed, `化勁` grants its own bound shield, and defender-side
   incoming-miss protection remains.
5. `傷害抵擋` plus `下次承傷上限`: first hit consumes only block; second hit
   selects and consumes the cap.
6. Invincibility, dual-wield/native full block, and execute interactions with
   block/cap contributions.
7. Multiple caps: smallest positive value wins, equal values choose oldest, and
   only the selected contribution is consumed.
8. `戰意` mixed-pipeline parity against defense, ignore-defense, other damage
   modifiers, cap, shield, and rounding.
9. `枯骨` ordered healing rounding and incoming-damage parity.
10. `寒毒` speed/healing parity.
11. Poison strongest/replace/same-event behavior and `玄冥神掌` settle/remove/
    apply ordering.
12. Stun extend/keep-longer and `封內` keep-longer behavior.
13. Mixed-source scheduling across Combo, Equipment, EquipmentSynergy, Neigong,
    and Magic.
14. Repeated rule-store remove/rebuild cycles around active positive status
    behavior ordering.
15. All three shipped clone-count tiers and every borrow/rebind path that can
    apply statuses.
16. One action applying incoming-miss protection to three holders gives each
    holder an independent charge.
17. Baseline authoring inventory proves `化勁`/`刺目` omit
    `重複套用` and records every surviving policy label/site.

Phase 0 output is executable tests plus a short delta ledger. If a baseline
result differs from the normative rule above, the normative rule wins only when
it appears in the deliberate-change list; otherwise implementation pauses for a
design correction.

## Implementation phases

### Phase 1: catalog and semantic types

1. Introduce the single exhaustive status catalog and structural agreement
   tests.
2. Add the four group-state variants and status-to-storage mapping.
3. Split authored reapplication policies into status-discriminated semantic
   types.
4. Add named catalog payloads/parameters for fixed debuffs.
5. Keep generic trigger/action and numeric-binding descriptors shared.
6. Make all payload and numeric visitors exhaustive.

Exit gate: catalog tests cover every status row and every field; no parallel
label/field/policy list remains.

### Phase 2: named debuff reducers

1. Implement `流血` shared quantity/ceiling/clock/source and one transaction.
2. Implement selected `寒毒`, `枯骨`, `七星`, `化勁`, and `刺目` packets.
3. Preserve poison, stun, and MP-block reducers under the new group container.
4. Route config-authored and scripted/native applications through the same
   commands and protections.
5. Implement exact cleanse/removal/expiry/presentation rules.

Exit gate: all Phase 0 parity tests and deliberate-change tests pass through the
new storage without compatibility branches.

### Phase 3: positive contribution completion

1. Retain holder-scoped producer-family allocation and immutable bound values.
2. Finish stable active-rule integration for `真氣`, `戰意`, `無影`, and
   `毒爆`.
3. Implement value-aware cap selection and defensive priority.
4. Prove mixed-source order and repeated rule-store rebuild stability.
5. Complete clone/borrow family identity.

Exit gate: independent buffs stack safely, family capacity never crosses
holders or runtime generations, and established pipeline/order tests pass.

### Phase 4: authoring, schema, and descriptions

1. Migrate `流血` to `目標總層數上限` and remove repeated behavior YAML.
2. Remove catalog-owned behavior payloads and `重複套用` from fixed debuffs.
3. Migrate poison replacement to `取代現有中毒`.
4. Keep only the locked policy values and status-specific schemas.
5. Generate semantic Compact/Full/Detailed nodes from catalog plus authored
   parameters.
6. Update all config, C++, and schema fixtures together.

Exit gate: all shipped top-level configs use only canonical authoring; golden
descriptions include catalog-owned behavior; schemas reject every removed form.

### Phase 5: migration cleanup

Delete, rather than deprecate:

- `取代並重設`, `取代整組`, `取代同一效果`, and `取代持續時間` authored
  labels and enum cases;
- `重複套用` parsing for catalog-owned statuses;
- independent-debuff storage/transaction tests and code paths;
- generic `potency`, `secondaryPotency`, source-status strength/layer aliases,
  and old poison macro shapes already superseded by the earlier semantic
  migration;
- duplicate parser/validator/schema/description field lists;
- specialized True-Qi command synthesis and stale first-instance reads;
- compatibility parsing and copied generated config artifacts; and
- hash-only coverage pins where a structural catalog assertion can name the
  missing row or field.

Exit gate: repository search finds no removed authoring vocabulary outside
negative rejection tests and historical design quotations; no runtime path can
construct a catalog-owned debuff using an arbitrary local behavior payload.

### Phase 6: verification and independent audit (complete)

1. Build with `.github/build-command.ps1`.
2. Run all unit tests, including Python schema/content tests.
3. Ensure command-line assertion/codegen failures print to stdout/stderr and do
   not open Windows modal dialogs.
4. Run the requested independent subagent audit for design delivery, combat
   correctness, migration completeness, obsolete compatibility paths, and
   duplicated catalogs/switches.
5. Address every substantiated finding and rerun the complete verification.

Delivered verification: `.github/build-command.ps1 -Target kys_tests` completed
successfully, all 15 Python schema/content tests passed, the full C++ suite
passed with 205,561 assertions across 1,022 cases, and `git diff --check` plus
canonical-config cleanup scans are clean. Independent review covered design
delivery, combat ordering/correctness, holder-local capacity, bleed lifecycle,
化勁/刺目, 傷害抵擋/下次承傷上限 priority, descriptions, catalog duplication,
and obsolete compatibility paths; no unresolved finding remains.

Final linking alone may be treated as successful compilation only when the
running game holds the output binary, as permitted by `AGENTS.md`.

## Required verification matrix

At minimum, tests cover:

- one valid generated fixture for every status catalog row;
- forbidden fields and policy values named with the affected status;
- catalog/schema/parser/validator/lowering/description structural agreement;
- no unmatched payload variant or numeric visitor fallback;
- holder-local capacity across three selected targets;
- positive same-name two-producer stacking;
- every fixed debuff with two different producers;
- bleed cap ratchet/reset/non-resetting clock/one-transaction attribution;
- poison comparator and replacement vocabulary;
- stun and MP-block clocks;
- seven-star downgrade replacement;
- `化勁`/`刺目` all-eligible suppression and exact shield ownership;
- incoming-miss interaction;
- damage block/cap/invincibility/dual-wield/execute priority;
- persistent modifier phase and rounding parity;
- mixed-source structured order;
- repeated live-rule removal/addition cycles;
- clone/borrow producer-family stability;
- cleanse/removal/expiry/liveness;
- Compact/Full/Detailed goldens for `流血`, `七星`, `降龍十八掌`,
  `九陽神功`, and `九陰白骨爪`; and
- rejection of explicit `範圍: 單體` with omitted-range default coverage.

## Rejected alternatives

### Independent complete debuffs

Rejected because producer count becomes an unintended multiplier for caps,
periodic transactions, healing modifiers, speed penalties, and defense-ignore
effects.

### One generic same-name storage object

Rejected for positive buffs because incoming caps/values/origins can rewrite
unrelated existing layers.

### Generic max/latest/average payload resolution

Rejected because it hides a game rule behind storage mechanics and can upgrade
weak layers with a strong producer's value.

### Global status YAML

Rejected because the project has no global definition layer and because trigger
and authored parameter context should remain near the granting effect. The code
catalog centralizes invariants without adding a new external dependency.

### Producer whitelist

Rejected because a new skill may legitimately apply an existing debuff. The
catalog constrains the debuff packet, not the producer identity.

### Producer-authored fixed debuff profiles

Rejected because every producer could omit part of the named contract. Catalog
ownership makes `寒毒`, `枯骨`, `七星`, `化勁`, and `刺目` truthful by
construction.

### Keeping generic replacement labels for possible future buffs

Rejected because no shipped positive status needs them, quantity verbs already
encode add/set behavior, and speculative retention weakens status-discriminated
schemas. A future mechanic should add a specifically named reviewed policy when
its semantics are known.
