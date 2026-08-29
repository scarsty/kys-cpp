# Effect Description Three-Tier Rendering and Mechanical Coalescing Design

Status: superseded by [效果描述語意文件設計](effect-description-semantic-document-design.md).

This document is retained as the historical predecessor of [效果描述語意文件設計](effect-description-semantic-document-design.md). Its per-rule API, renderer, punctuation, and layout contracts are no longer current.

## Background

`EffectDescriptionStyle` currently provides `Full` and `Compact`. Both are rendered from the same semantic AST, but Compact primarily shortens labels and punctuation while retaining most events, targets, conditions, settlement phases, stacking policies, and source policies. Consequently:

- Full is close to an exhaustive serialization of the rule. It is useful for auditing, but does not read like player-facing copy.
- Compact uses `·`, `／`, and `→`, but can still be too long for quick-overview rows.
- Defaults such as battle start, self, accepted hit, and ordinary damage phases appear repeatedly.
- Several attribute modifiers with an identical lifetime repeat `持續 N 幀` and the same stacking policy for every action.
- Full appends `。`, after which outer containers may append `；`, producing `。；`.

The UI has three distinct needs: an exhaustive explanation, a readable description for spacious panels, and a quick summary that still includes important timing. Two styles cannot serve all three without compromising one of them.

## Goals

1. Provide `Detailed`, `Full`, and `Compact` description styles.
2. Preserve everything shown by the current Full renderer in Detailed, for a future detailed-effect view and diagnostics.
3. Make Full a player-facing description for panels with wrapping space, omitting defaults and implementation terminology while retaining decision-relevant mechanics.
4. Make Compact a fixed-structure quick summary and always retain the duration of temporary effects.
5. Generate all three styles from the same parsed `EffectRule` and semantic AST. YAML gains no description fields.
6. Remove repeated duration text through bounded, provable display coalescing without claiming that sibling actions share runtime stack state or building a natural-language inference engine.
7. Return description fragments without a terminal `。`; containers own rows, lists, and paragraph layout.
8. Correct existing presentation errors involving attribute units and event names.

## Non-goals

- Changing effect parsing, validation, runtime behavior, settlement order, or gameplay semantics.
- Rewriting arbitrary rules into the most natural possible Chinese sentence.
- Selecting phrasing by effect ID, equipment name, neigong name, combo name, or magic name.
- Parsing completed strings to discover common prefixes or suffixes.
- Coalescing across action families, branches, sequences, or target scopes.
- Truncating text to satisfy a panel width. The UI must still wrap, clip with a detailed entry point, or allocate sufficient space.
- Adding `LegacyFull`, old enum aliases, or parallel description pipelines.

## Description styles

```cpp
enum class EffectDescriptionStyle
{
    Detailed,
    Full,
    Compact,
};
```

The responsibility of the existing Full renderer moves to Detailed. The new Full renderer becomes the lean player-facing form. All call sites and tests migrate in the same change; no compatibility alias remains.

### Detailed

Detailed is the exhaustive, auditable description:

- Show event, observation perspective, cast match, target, all conditions, and chance.
- Show damage or healing phase, accepted-hit conditions, selector details, and activation limits.
- Show duration, stacking, refresh behavior, stack scope, source-death policy, and propagation policy.
- Preserve sequence, conditional, for-each, and state-cycle structure.
- Except for correctness fixes to labels and units, retain all information shown by current Full.
- Do not append terminal `。`.

Detailed may use `，`, `、`, `；`, `：`, parentheses, and complete conjunctions. These express AST structure; an outer container must not infer that structure.

### Full

Full is the player-facing description for equipment, neigong, and combo panels with wrapping space:

- Use limited natural punctuation: `，`, `、`, `；`, and parentheses where needed.
- Do not append terminal `。`.
- Omit `戰鬥開始時` for `BattleInitialized` rules. Omit the self target whenever the effective target is the effect owner.
- Omit an ordinary `已接受命中` condition automatically supplied by timing shorthand, along with redundant damage perspective.
- Translate ordinary phases and operations into gameplay language, such as `減傷6%` instead of `承受的所有傷害（防禦結算前）百分比加算-6%`.
- Show non-self targets, chance, interval, duration, stack cap, per-cast limits, and exceptions that change player expectations.
- Do not require every Detailed field to remain visible. Detailed is the lossless presentation tier.

### Compact

Compact is a fixed-structure quick summary, not a complete sentence:

- `·` separates trigger, target, ordinary simultaneous actions, and shared qualifiers.
- `／` (U+FF0F FULLWIDTH SOLIDUS) separates mechanically coalesced action items in one simultaneous group.
- `→` only represents ordered actions.
- `×` represents the repetition or cardinality of the immediately preceding unit. Include a classifier such as `層` or `次` where omission could be ambiguous.
- `-` is only a numeric minus sign, never a separator, avoiding ambiguity with text such as `防-25%`.
- Do not append terminal punctuation.
- Always show duration for temporary effects, interval for periodic effects, and cap for limited stacking effects.
- Do not invent a duration for permanent or battle-long effects; instant actions omit time.

Compact may omit default policies and uncommon settlement detail, but it must retain the primary magnitude, special target, trigger chance, duration, interval, and stack cap.

## Punctuation and fragment contract

`effectDescription` returns one text fragment for one effect rule. No style may end that fragment with `。`. Call sites must not remove or infer terminal punctuation according to style.

| Structure | Detailed | Full | Compact |
|---|---|---|---|
| Ordinary clause | `，` | `，` | `·` |
| Simultaneous actions | `、` | `、` | `·` |
| Mechanically coalesced items | Not coalesced | `、` | `／` (U+FF0F) |
| Sequence | `，接著` or `，再` | `，再` | `→` |
| Branch | `；若…則…，否則…` | `；若…則…，否則…` | `若…·…／否則…` |
| Shared qualifier | Complete conjunction | `，持續…` | `·…幀` |
| Terminator | None | None | None |

Compact's structural punctuation set is `·`, `／`, `→`, and `×`; `-` and `+` remain arithmetic signs. A branch uses `／否則` and does not introduce `；`. A mechanically coalesced group must promote at least one non-empty qualifier. Siblings with no promotable qualifier remain ordinary simultaneous actions and therefore use `·` in Compact.

The UI should represent multiple effects as rows or list items. If a protocol field temporarily requires one string, its joiner may concatenate punctuation-free fragments with `；`; it must not produce `。；`.

## Information visibility

The following table defines the default policy. A semantic node may still require visibility in Full or Compact when its value is non-default or changes player expectations. The enclosing-default context in Display mapping may additionally suppress a matching top-level trigger in Full and Compact under its stricter event-and-cast-match contract.

| Information | Detailed | Full | Compact |
|---|---:|---:|---:|
| Battle-start event | Show | Omit for `BattleInitialized` rules | Omit |
| Self target | Show | Omit | Omit |
| Non-self or group target | Show | Show | Show abbreviated label |
| Automatic accepted-hit condition | Show | Omit | Omit |
| Additional positive-damage or reflected-hit condition | Show | Show | Show abbreviated label |
| Ordinary damage settlement phase | Show | Use the reviewed player phrase; otherwise show the phase | Omit only when a reviewed player phrase absorbs it; otherwise show an abbreviated phase |
| Chance | Show | Show | Show |
| Interval | Show | Show | Show |
| Duration | Show | Show | Show |
| Stack cap | Show | Show | Show |
| Ordinary refresh policy | Show | Omit only when declared an ordinary default by the action-family visibility contract | Omit |
| Non-default stacking or source scope | Show | Show | Show abbreviated label when necessary |
| Source-death policy | Show | Show when non-default for that action family | Show an abbreviated label when non-default; otherwise omit |
| Propagation or per-cast limit | Show | Show when it changes trigger count | Show abbreviated label |
| Rounding | Show when relevant | Normally omit | Omit |

The renderer does not receive the panel name or content-source name to decide semantics. It may receive the typed enclosing-default context defined below when that default is visibly communicated by the container. The same rule, style, and context must produce the same text at every call site.

Phase visibility is deterministic and formatter-owned. Each action-family phrase table declares which typed phase it absorbs. For example, the reviewed incoming, all-damage, before-defence, percentage-add template may render `-6` as `減傷6%`; that phrase already fixes the phase, so Full and Compact do not append it. A combination without a reviewed phrase retains `（防禦結算前）`, `（防禦結算後）`, or `（最終結算）` in Full and `·防前`, `·防後`, or `·最終` in Compact. Call sites never choose whether to translate or omit a phase.

## Mechanical coalescing

### Principle

Coalescing promotes only a duration that is distributively true of every item to one common suffix. It is not a natural-language optimization, does not reinterpret multiple actions as a new gameplay concept, and does not assert that the actions share one runtime timer or counter.

Detailed never coalesces. Full and Compact may coalesce adjacent actions within a `DescriptionSimultaneous` node when their action family has explicitly opted in.

The first implementation opts in only timed `ModifyAttributeAction` siblings under the boundary below. Other action families remain independent. A family may be added later only when it has the same clear repeated-duration problem and a separately reviewed typed eligibility rule; there is no generic text-similarity mechanism.

Runtime attribute stack domains include `ruleId`, `actionOrder`, target, attribute, and applicable event source. Sibling actions therefore have distinct stack keys even when their duration and policy values match. This proposal does not treat value equality as shared stack-key identity. Instead, it permits duration-only display promotion in the narrow cases where those separate domains are not observable in the wording: every eligible sibling is a constant, runtime-non-negative modifier applied by the same simultaneous dispatch, and `Independent` or uncapped `Refresh` produces the same application and expiry schedule for each item. Any qualifier that exposes a counter, winner, cap, or partition remains attached to its action.

Runtime-negative persistent modifiers are deliberately excluded. Status-shield protection is applied separately per action and can block or shorten one sibling before the next sibling is reduced, so equal configured durations do not prove equal applied lifetimes. A formula whose sign is not statically known is excluded for the same reason.

### Coalescing boundary

Two attribute modifiers may enter the same group only when all of the following are true:

1. They are adjacent inside the same `DescriptionSimultaneous` node.
2. Their effective targets are identical.
3. Both variants are `ModifyAttributeAction`.
4. Their `AttributeOperation` values are identical.
5. Every amount is constant and `attributeModifierIsNegative(operation, amount)` is false. Amounts may differ, but formula or sign-unknown amounts are ineligible.
6. Their `durationFrames` values are identical and greater than zero.
7. Their `stack` values are identical and are either `Independent` or `Refresh`.
8. `stackLimit` is absent, `perStack` is false, and `stackScope` is `Shared` on every item.
9. No per-action condition exists inside the candidate group; all items execute from the same simultaneous dispatch.
10. No sequence, conditional, for-each, or state-cycle boundary separates them.

These are all lifecycle fields on `ModifyAttributeAction`: `durationFrames`, `stack`, `stackLimit`, `perStack`, and `stackScope`. `ModifyAttributeAction` has no source-death policy. Future action families must enumerate their own complete lifecycle fields rather than rely on an open-ended “other qualifier” clause.

Only `durationFrames` is promoted. Stack policy, cap, per-stack behavior, scope, and runtime stack identity are never promoted by this first implementation. Attribute and amount remain group items, so different attributes and amounts may share the same displayed duration:

```text
ModifyAttribute(Attack, PercentAdd, 20, Duration=100, Stack=Refresh)
ModifyAttribute(Defence, PercentAdd, 30, Duration=100, Stack=Refresh)
```

- Full: `攻擊+20%、防禦+30%，持續100幀`
- Compact: `攻+20%／防+30%·100幀`

The first implementation does not further factor an identical amount into `攻擊、防禦+30%` or `攻防+30%`. Common-value factoring is unnecessary to remove repeated qualifiers and introduces additional grammar ambiguity. It requires a separate reviewed structural rule if desired later.

### Cases that must not coalesce

Different durations remain separate:

```text
攻擊+20%，持續60幀；防禦+30%，持續100幀
```

Different stack scope, refresh policy, operation, or target also prevents coalescing, even when rendered text appears similar. `AddStack`, `Replace`, and `KeepStrongest` never coalesce in the first implementation. In particular, two `AddStack` actions that both say `最多8層` still have distinct `actionOrder` stack domains; combining their items under one cap would be ambiguous between a joint cap and one cap per item.

Runtime-negative, formula, and sign-unknown modifiers also remain separate because status-shield protection can make their applied lifetimes diverge. Permanent modifiers have no non-empty promotable duration. They remain ordinary simultaneous actions: Full may join them with `、`, while Compact joins them with `·`. Attribute actions separated by another action must not move across it because action order remains part of the formal semantics.

Sequences never coalesce:

```text
結算中毒→清除中毒→重新施加中毒
```

The true and false sides of a conditional are processed independently; no action may be promoted outside its branch.

### Implementation shape

Coalescing operates on typed semantic fragments, never on completed strings. A conceptual representation is:

```cpp
struct DescriptionActionItem
{
    DescriptionPredicate predicate{};
    std::vector<DescriptionArgument> arguments;
};

struct DescriptionActionGroup
{
    DescriptionActionFamily family{};
    DescriptionSubject subject{};
    std::vector<DescriptionActionItem> items;
    std::vector<DescriptionQualifier> sharedQualifiers;
};
```

The implementation may continue using the existing `DescriptionClause` and `DescriptionSimultaneous` types rather than introduce a parallel AST solely to match this illustration. The required contract is that both the group key and item come from typed fields; the renderer never compares localized strings.

Eligibility and runtime reduction must share one typed attribute-sign classifier; do not copy the `AttributeOperation` sign switch into the renderer. The coalescer invokes it only after proving the `EffectNumber` is constant.

The existing `sharedActionDescriptionQualifiers` stub is replaced by the typed eligibility rule above and may return only the common duration. `renderSharedActionQualifiers` remains the single rendering path for that promoted duration; it is not replaced or left as dead code. Its current safeguard comment must be rewritten to record both facts: sibling runtime stack domains remain distinct, and only display-unobservable duration promotion is allowed. A future feature that promotes caps, policies, scope, or other counter-bearing qualifiers still requires an explicit shared runtime stack key in the payload and runtime.

Conceptual flow:

```text
EffectRule
  → semantic AST
  → adjacent opt-in action coalescing (Full and Compact only)
  → style visibility projection
  → localized renderer
  → fragment without terminal punctuation
```

Eligibility always compares the typed fields above before style visibility is applied. Hidden output must never make an ineligible pair appear eligible. An empty promoted-qualifier result is an ordinary `DescriptionSimultaneous`, not a mechanically coalesced group.

## Units and correct terminology

Leaner output must not sacrifice correctness.

### Percentage-point attributes

The display unit belongs to `BattleAttribute` metadata rather than being inferred solely from `AttributeOperation`. Block chance, critical chance, dodge chance, projectile reflection chance, skill reflection percentage, and other probability or multiplier attributes must display `%` even under `FlatAdd`:

```text
格擋率+6%
```

The current `格擋率+6` output is incorrect.

### Sign-aware player phrases

Sign-absorbing verbs are permitted only for reviewed typed patterns. They do not apply by string substitution:

| Typed pattern | Constant amount | Full and Compact core phrase |
|---|---:|---|
| Incoming all-damage, before-defence, `PercentAdd` | `N < 0` | `減傷abs(N)%` |
| Incoming all-damage, before-defence, `PercentAdd` | `N >= 0` | `受傷{N:+}%` |
| Outgoing damage, reviewed ordinary stage/channel, `PercentAdd` | `N > 0` | `增傷N%` |
| Outgoing damage, reviewed ordinary stage/channel, `PercentAdd` | `N <= 0` | `造成傷害{N:+}%` |
| `BattleAttribute::DamageReduction`, `PercentAdd` | any `N` | `減傷{N:+}%`, retaining the arithmetic sign |

Thus an incoming modifier of `-6` is `減傷6%`, its `+6` inverse is `受傷+6%`, and a `DamageReduction PercentAdd +6` attribute modifier is `減傷+6%`. For zero, the non-directional branch renders `受傷+0%` or `造成傷害+0%`; it does not claim an increase or reduction. Other attributes, including speed, retain signed arithmetic such as `速度-25%`.

Detailed always retains the structural perspective, stage, operation, and signed amount. Formula or state-derived amounts use structural non-absorbing wording in Full and Compact unless their sign is statically guaranteed by typed metadata. Each reviewed sign-absorbing row requires positive, negative, zero, and non-constant tests where those inputs are valid.

### Trigger vocabulary

The following is the canonical event vocabulary after this proposal is approved; it supersedes the trigger table in the base design. Event labels are source-neutral. Cast match, observation scope, propagation, and conditions may add context, but an event enum alone must not imply it.

| `EffectEvent` | Detailed | Full | Compact |
|---|---|---|---|
| `BattleInitialized` | `戰鬥開始時` | Omit | Omit |
| `FrameAdvanced` | `每幀` | `每幀` | `每幀` |
| `UltimateCooldownFinished` | `絕招冷卻完成時` | `絕招冷卻完成時` | `絕招冷卻完成` |
| `CastPlanned` | `規劃施放時` | `準備施放時` | `準備施放` |
| `AttackCommitted` | `出手時` | `出手時` | `出手` |
| `UltimateCommitted` | `施放絕招時` | `施放絕招時` | `絕招` |
| `AttackSpawned` | `攻擊生成時` | `攻擊生成時` | `彈道生成` |
| `MainProjectileBeforeDamage` | `主彈道命中、傷害結算前` | `主彈道命中時` | `主彈命中` |
| `HitBeforeDamage` | `命中且傷害結算前` | `每次命中時` | `命中` |
| `DamageResolved` | `傷害結算後` | `傷害結算後` | `傷害後` |
| `HealAttempted` | `嘗試治療時` | `嘗試治療時` | `治療前` |
| `HealApplied` | `實際治療後` | `治療生效後` | `治療後` |
| `CastContinuation` | `本次施放第一輪攻擊完成後` | `第一輪攻擊後` | `施放延續` |
| `CastSettled` | `本次施放全部攻擊結算完成後` | `本次施放結算後` | `施放結算` |
| `ShieldBroken` | `護盾破裂時` | `護盾破裂時` | `破盾` |
| `UnitDied` | `自身死亡時` | `死亡時` | `死亡` |
| `AllyDied` | `友軍死亡時` | `友軍死亡時` | `友軍死亡` |

Two deterministic projections refine these base labels:

- A positive `intervalFrames=N` on `FrameAdvanced` replaces the standalone Full and Compact event label with `每N幀`; it is not appended to `每幀`.
- `DamageResolved` with outgoing perspective and the automatic accepted-hit condition renders `每次命中後` in Full and `命中後` in Compact. Detailed retains `傷害結算後`, perspective, and accepted-hit wording.

`MainProjectileBeforeDamage`, `CastContinuation`, and `CastSettled` are not exclusive to ultimates. A base label may use `絕招` only when the event contract is itself ultimate-specific (`UltimateCommitted` or `UltimateCooldownFinished`), or when typed cast match or condition data establishes ultimate context. Runtime propagation or current content placement is not sufficient. Likewise, “every eligible projectile” propagation is rendered from propagation metadata, never inferred from `HitBeforeDamage` alone.

### Cardinality marker

Whenever `×N` is used, it describes the immediately preceding unit. It is not a stack-specific operator. Compact should prefer `增傷4%×8層` for a stack cap and `中毒10%×5次` for tick count so the classifier removes ambiguity.

### Time

The first implementation retains the runtime and current-display unit of frames. It does not convert frames to seconds. A future seconds display requires a separate contract for tick rate, rounding, and non-integral durations.

## Representative output

### Passive fixed attribute

聖火神功:

- Detailed: `戰鬥開始時，對自身攻擊+25`
- Full: `攻擊+25`
- Compact: `攻+25`

### Same-combo death

明教教眾:

- Detailed: `友軍死亡時，事件目標屬於此效果來源，對自身攻擊+50、防禦+50`
- Full: `同羈絆友軍死亡時，攻擊+50、防禦+50`
- Compact: `同羈絆友軍死亡·攻+50·防+50`

The Full and Compact `同羈絆` label comes from the player-facing representation of `EventTargetBelongsToBoundSourceCondition`; it is not a Ming-specific special case.

These permanent modifiers have no non-empty shared duration, so they are not mechanically coalesced. Their Compact separator is the ordinary simultaneous-action `·`, not `／`.

### Attribute modifiers with a shared lifetime

Three adjacent actions use percentage addition, last 100 frames, and refresh:

- Detailed: show target, duration, and refresh policy for every action.
- Full: `攻擊+30%、防禦+30%、速度+30%，持續100幀`
- Compact: `攻+30%／防+30%／速度+30%·100幀`

Full omits the ordinary refresh policy under its action-family visibility contract while Detailed retains it. Duration is promoted once, but each action keeps its own attribute and amount. The slash does not imply a shared runtime timer.

### MP restoration on hit

- Detailed: `傷害結算後，自身造成的傷害且已接受命中，對自身回復12內力`
- Full: `每次命中後回復12內力`
- Compact: `命中後回內12`

### Conditional branch

一氣化三清 evaluates the branch separately for each beneficiary:

- Full: `施放絕招時，依序為自身及目前內力最低的兩名友軍，若施放前已滿內則獲得160護盾，否則回復20內力`
- Compact: `絕招·自身＋低內2人·若滿內·護盾160／否則回內20`

The Compact `／否則` is branch syntax, not action coalescing. The branch remains nested under the per-beneficiary scope in every style.

### Temporary stacking

羅漢伏魔功（內功被動，magic 96）:

- Detailed retains damage phase, additive stacking, eight-stack cap, and 91-frame duration.
- Full: `每次命中使招式傷害+4%，最多8層；最後一次命中91幀後清除層數`
- Compact: `命中增傷4%×8層·91幀`

This is the real rule at `config/chess_neigong.yaml` magic 96: outgoing skill damage `+4%`, 91 frames, `AddStack`, cap 8. It is separate from the same magic's team-defence ultimate in `config/chess_magic_effects.yaml`. Compact must not omit `91幀` to satisfy a length target.

### Persistent area

黃沙萬里鞭:

- Full: `在命中位置建立半徑6格的沙塵區域，持續100幀；區域內敵人速度-25%，彈道無法追蹤，彈速及壓制傷害-35%`
- Compact: `沙塵區6格·100幀·敵速-25%·彈速／壓制-35%·禁追蹤`

This example fixes the desired information level; it does not authorize a magic-name special case. Shape, radius, duration, and modifiers must come from the typed area payload. If the generic area formatter cannot produce this Full or Compact structure, the area semantic formatter must be improved rather than adding a 黃沙 branch.

Here `彈速／壓制` is a formatter-owned homogeneous argument-label list inside one area modifier. It uses the same U+FF0F list mark for compactness but does not invoke action coalescing or shared-qualifier logic.

## Display mapping

| Display location | Style | Layout responsibility |
|---|---|---|
| Equipment detail panel | Full | Wrap; one row per rule |
| Neigong detail panel | Full | Wrap; one row per rule |
| Full combo browser | Full | Wrap; one row per rule |
| Role combo quick panel | Compact | Wrap to column width |
| Role ultimate-effect rows | Compact with enclosing default `UltimateCommitted` | Use one full-width column, indent and wrap beneath the ultimate magic heading, and select the largest font that fits the available width and height |
| Player-visible catalog and reward metadata | Full | Prefer structured rows; join with `；` only when constrained to one string |
| Future CLI or diagnostic catalog (aspirational) | Detailed, or expose all three | No call site exists today; must not dictate player-panel defaults |
| Future detailed-effect view | Detailed | Show all rule semantics; location remains TBD |

The location of the Detailed UI is outside this design. The API and tests must still provide Detailed before that view exists so that lean rendering does not remove the auditable representation.

### Enclosing default event

An enclosing layout may communicate an event strongly enough that repeating it on every child row is redundant. This is represented as typed presentation context, not by inspecting or trimming rendered text:

```cpp
struct EffectDescriptionContext
{
    std::optional<EffectEvent> enclosingDefaultEvent;
};
```

The omission rule is deliberately small:

1. Detailed always renders the rule event, regardless of context.
2. Full and Compact omit only the top-level event token when `context.enclosingDefaultEvent == rule.event` and `rule.castMatch == EffectCastMatch::BoundMagic`.
3. Every non-matching event and every non-default cast match remains visible. Context never suppresses cast-match wording, conditions, propagation, duration, or action qualifiers.
4. A caller may set the field only when its typed row model associates the rule with the enclosing source and the UI presents that association as a visible parent-child hierarchy. A content source, magic ID, row color, or call-site identity alone is insufficient; visible nesting backed by the typed association is sufficient and does not require a literal `絕招` marker.

The role character panel satisfies this contract when ultimate-effect rows are visibly nested below the ultimate magic name. Its builder already finds the definition by that heading's magic ID, emits effect rows only for the selected ultimate, retains the same magic pointer on every child row, and draws those rows with an inset. It passes `UltimateCommitted` as the enclosing default for those child rows; no additional `絕招` heading marker is required:

```text
羅漢伏魔功
  全隊·防+66·100幀
```

The same rule rendered standalone remains `絕招·全隊·防+66·100幀`. Within the nested panel, only an `UltimateCommitted` event token on a `BoundMagic` rule disappears. `EffectCastMatch` cannot name an arbitrary different magic; its only broader alternative is `OwnerAnyCast`. A `MainProjectileBeforeDamage` rule still starts with `主彈命中`; `CastSettled` still starts with `施放結算`; and `UltimateCooldownFinished` still starts with `絕招冷卻完成`. A hypothetical `UltimateCommitted` rule with `OwnerAnyCast` also retains both `絕招` and its `任意施放` wording.

Because omission relies on visible nesting, wrapping must not orphan an effect row from its magic heading. The implemented role-detail layout uses one full-width column and preserves indentation for every continuation line. It assumes the game invariant of at most two skills per role and chooses from a bounded set of readable font metrics; it neither paginates nor silently drops rows. Piece and star-upgrade rewards use the same role-detail panel geometry. If the minimum metric cannot contain the content, Debug asserts so a content or layout regression cannot remain hidden.

## API and data flow

The public entry point remains singular:

```cpp
std::string effectDescription(
    const EffectRule& rule,
    EffectDescriptionStyle style,
    const EffectDescriptionContext& context);
```

All call sites migrate to the context-taking entry point; do not preserve a context-free compatibility overload or use a default argument. Standalone callers pass an empty context. Do not add panel-specific booleans, source names, or manually authored overrides. If a catalog or protocol needs multiple representations, it requests each style explicitly rather than making the renderer inspect its caller.

Description text does not participate in runtime rule identity. The style change may update description golden files and player-visible metadata, but it must not change effect IDs, action execution order, or battle digests.

## Implementation order

1. Add Detailed, move current Full rendering into it, and remove terminal `。` from all styles.
2. Create the three-style golden-test harness, explicit per-variant style-visibility metadata or contracts, and systematic visible-field mutation-test infrastructure. Existing tests do not provide these facilities.
3. Correct percentage-point attribute units, sign-aware phrase metadata, deterministic phase projection, and the canonical trigger vocabulary.
4. Implement Full visibility policy, starting with generic omission of battle start, self, accepted hit, and ordinary phase terminology.
5. Implement Compact labels, fixed separators, and mandatory duration, interval, and cap visibility.
6. Add typed enclosing-default context with the matching-event plus `BoundMagic` omission rule. Migrate the character-panel ultimate rows with `UltimateCommitted` as their default and preserve the existing indented parent-child grouping across columns; do not add a redundant visible `絕招` marker.
7. Add duration-only adjacent `ModifyAttributeAction` display coalescing at the typed-fragment layer. Replace the `sharedActionDescriptionQualifiers` stub and retain `renderSharedActionQualifiers` as the rendering path.
8. Migrate equipment, neigong, combo, ultimate, catalog, and reward call sites to the designated styles and explicit standalone or enclosing context.
9. Give Compact ultimate rows a layout strategy that handles long text.
10. Populate representative goldens and migrate the existing substring/content, UI formatting, and protocol tests to the three-style contract.

No step may introduce a separately maintained old and new action formatter. Detailed, Full, and Compact share semantic formatters, enum labels, numeric units, and typed qualifiers.

## Test contract

### Basic three-style contract

Every loadable action, condition, selector, and state-machine variant must:

1. Produce non-empty Detailed, Full, and Compact output.
2. Produce no terminal `。`.
3. Contain no unresolved token or raw schema-key dump.
4. Use canonical Traditional Chinese labels.
5. Produce deterministic text for the same rule, style, and context.

### Visible-value mutation

Detailed is exhaustive: changing any player-visible typed payload field must change Detailed output.

Full and Compact require a text change only for fields visible under their respective visibility contracts. Tests must derive this from explicit style-visibility metadata or a per-variant contract; they must not retain the old assertion that every style displays every field.

Changing only runtime IDs, source-container indices, or other invisible identity must not affect any style.

### Positive coalescing tests

- Adjacent constant, runtime-non-negative timed attribute modifiers with the same operation, target, duration, and eligible `Independent` or `Refresh` policy display their common duration once in Full and Compact.
- Each item's attribute and amount remain present and in original order.
- Different amounts may still share identical qualifiers.
- Detailed does not coalesce and continues to show exhaustive per-action information.
- The typed group contains only a shared duration; it contains no shared policy, cap, scope, or runtime stack key.

### Negative coalescing tests

Any of the following must prevent coalescing:

- duration;
- operation;
- target;
- `AddStack`, `Replace`, or `KeepStrongest` policy;
- any cap, true per-stack flag, or non-shared scope;
- a runtime-negative, formula, or otherwise sign-unknown amount;
- no duration or an otherwise empty promotable qualifier set;
- sequence, branch, for-each, or state-cycle boundary;
- an intervening action;
- different action family.

One negative test must use two same-valued `AddStack` siblings with the same cap and prove that each cap remains attached to its own action; their distinct `actionOrder` runtime stack domains must not be described as a shared cap. Another must cover same-duration negative siblings under status-shield protection and prove that the display does not promote their configured duration. Negative tests should inspect the typed group result and final text rather than relying only on snapshots.

### Punctuation and units

- No style ends with `。`.
- Multiple reward descriptions contain no `。；`, `；；`, or empty item.
- A percentage-point attribute under `FlatAdd` still displays `%`.
- Compact clearly distinguishes a negative value from its delimiters, for example `防-25%·90幀`.
- `MainProjectileBeforeDamage` does not mention `絕招` without an ultimate condition.
- Base labels for `CastContinuation` and `CastSettled` use `施放`, not `絕招`; neither event is ultimate-only.
- Compact uses U+FF0F `／`, never ASCII `/`, for coalesced items and homogeneous argument-label lists.
- Compact branches follow `若…·…／否則…` and contain no `；`.
- Sign-aware phrase tests cover the inverse sign: incoming `-6%` becomes `減傷6%`, incoming `+6%` becomes `受傷+6%`, and `DamageReduction PercentAdd +6` becomes `減傷+6%`.

### Golden coverage

At minimum, fix representative output for:

- 聖火神功: omission of passive battle-start and self defaults;
- 明教: player-facing same-combo death label;
- 南海鱷甲: percentage-point unit;
- 羅漢伏魔功（內功被動，magic 96）: Compact retains cap and duration;
- adjacent same-duration attributes: positive mechanical coalescing;
- different-duration attributes: no coalescing;
- 玄冥神掌: sequence is not coalesced;
- 一氣化三清: branch scope remains intact;
- 黃沙萬里鞭: area Full and Compact output plus long-row layout.

### Enclosing-context coverage

- A `BoundMagic` `UltimateCommitted` rule begins with `絕招` under standalone Compact context and omits only that event token under `enclosingDefaultEvent=UltimateCommitted`.
- An `OwnerAnyCast` `UltimateCommitted` rule retains `絕招` and `任意施放` under the same context.
- `MainProjectileBeforeDamage`, `CastContinuation`, `CastSettled`, and `UltimateCooldownFinished` remain visible under that same context because their events do not match the enclosing default.
- Detailed renders the event under both contexts.
- Character-panel layout tests prove wrapped continuation rows remain inset beneath the ultimate magic heading and stay inside the same single-column viewport.

## Acceptance criteria

- `EffectDescriptionStyle` contains only `Detailed`, `Full`, and `Compact`, without a legacy compatibility branch.
- All three styles share one semantic formatter and typed label and unit metadata.
- Contextual omission uses only a typed enclosing default event plus the rule's typed `BoundMagic` cast match; it contains no panel, effect-ID, magic-ID, or source-name branch.
- Detailed retains all currently visible semantics and has no terminal full stop.
- Full omits ordinary schema and settlement terminology while retaining targets, chance, duration, interval, cap, and other decision-relevant information.
- Compact shows duration for every temporary effect and interval for every periodic effect.
- Mechanical coalescing promotes only a non-empty common duration for adjacent simultaneous, constant, runtime-non-negative `ModifyAttributeAction` items that satisfy the explicit `Independent` or uncapped `Refresh` eligibility boundary.
- Cap-, counter-, winner-, or scope-bearing qualifiers are never promoted without explicit shared runtime stack-key identity.
- The renderer contains no effect-ID or effect-name branch and performs no completed-string similarity or post-processing merge.
- Equipment, neigong, combo, and ultimate panels use their designated styles.
- Character-panel ultimate rows omit only the event token of a matching `BoundMagic` `UltimateCommitted` rule and remain visibly grouped with their magic heading, without adding an explicit `絕招` marker.
- Compact ultimate rows wrap in one full-width column with adaptive font metrics; they do not paginate, create overlapping columns, or silently discard overflow.
- Piece and star-upgrade reward previews use the same role-detail panel layout. Content regression tests cover the actual `244×133` general-shop viewport and the narrower `196×133` star-upgrade viewport, including horizontal bounds, vertical bounds, skill-value separation, and the current minimum effect fonts of 12px and 10px respectively.
- Description, UI, and protocol tests pass while battle runtime behavior and digests remain unchanged.
