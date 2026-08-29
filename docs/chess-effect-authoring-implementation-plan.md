# Chess effect authoring implementation plan

## Objective

Make the effect authoring surface honest, canonical, and easy to validate while preserving the existing typed runtime model (`EffectRule`, selectors, conditions, actions, and state-machine variants).

The runtime validator remains authoritative for every typed rule, including rules constructed directly in C++. Authoring descriptors and generated JSON Schemas provide earlier structural and context-aware feedback, but they do not replace runtime invariants.

## Ownership boundaries

| Constraint | Owner |
| --- | --- |
| Field names, node shapes, required fields, and enum labels | Authoring descriptors, `PayloadView`, and generated schemas |
| Event payload availability and unconditional event/action compatibility | Shared event-capability metadata used by runtime validation and schema generation |
| Numeric, cross-field, policy, and typed-action invariants | Runtime validation |
| Exact runtime-stage and composition restrictions | Runtime validation, with shared metadata only where unconditional |
| Preferred spellings and recurring gameplay idioms | Timing intents and closed authoring macros |

## Phase 1: explicit content validation command

Add `kys_chess_cli validate-content`.

- Reuse `ChessContentLoader::load`; do not create a second YAML-only validator.
- Honor the existing `--data-root`, `--config-root`, and optional `--difficulty` arguments.
- Validate Easy, Normal, and Hard when no difficulty was explicitly supplied.
- Do not create a game session or enter interactive, JSONL, or MCP modes.
- Preserve existing diagnostics, including config context, YAML line, and validator message.
- Exit with zero only when every requested difficulty loads successfully.
- Add command/help coverage and valid/invalid content validation tests.

## Phase 2: honest named mechanism actions

Remove the author-facing catch-all form:

```yaml
- 狀態機:
    機制: 生成分身
    數量: 2
```

Promote every mechanism to a distinct named author action:

```yaml
- 生成分身:
    數量: 2
```

- Keep the runtime `StateMachineAction` representation unchanged.
- Give each named mechanism its own payload descriptor and exact field set.
- Attach unconditional event constraints to the mechanism descriptor.
- Migrate every top-level config use and remove the old author syntax without compatibility aliases.
- Generate schemas that reject fields belonging to another mechanism.
- Keep runtime validation for typed rules constructed outside YAML.

The authoring tiers are descriptor metadata, not runtime types:

- Tier A: closed idioms/macros that expand to canonical typed actions.
- Tier B: generic effect primitives, including generic state-memory operations.
- Tier C: named specialized mechanisms such as rule borrowing, attack-definition copying, poison settlement, clones, death protection, and rescue repositioning.

## Phase 3: shared event capabilities

Introduce one shared mapping from `EffectEvent` to capabilities such as hit payload, damage payload, damage origin, heal payload, cast provenance, cast aggregate, and initialization context.

- Use it in selector, condition, number-base, and unconditional action validation.
- Replace duplicated event lists where the rule is purely capability-based.
- Keep field-dependent and composition-dependent restrictions in runtime validation.
- Feed the same constraints into schema generation where representable.
- Ensure nested `條件分支` actions inherit the parent rule event context.
- Add agreement tests covering representative legal and illegal event/context pairs.

## Phase 4: canonical timing intent

The pre-damage timings remain distinct:

- `主彈命中`: the bound attack's main projectile before damage resolution.
- `命中`: any qualifying hit before damage resolution.

Canonicalize post-resolution authoring:

- Replace every shipped `時機: 傷害後` plus `傷害方位: 造成` with `時機: 造成傷害後`.
- Replace every shipped `時機: 傷害後` plus `傷害方位: 承受` with `時機: 受傷後`.
- Remove raw `傷害後` and author-specified `傷害方位` from the author grammar.
- Keep `DamageResolved` and `DamagePerspectiveCondition` in the runtime model because intent timings normalize to them.
- Preserve `觀察範圍` and `施放匹配` as independent behavior.
- Do not introduce lifecycle-section nesting.

## Phase 5: measured idioms

Measure repeated behavior from parsed YAML nodes or normalized typed rules, never by counting matching scalar text.

### Existing attribute macro

- Migrate repeated multi-attribute action groups to the existing `屬性加成` macro where all qualifiers are shared.
- Preserve exact normalized action ordering and values.

### Poison macro

The baseline is **five** shipped poison application actions:

- `chess_combos.yaml`: 2
- `chess_equipment.yaml`: 1
- `chess_magic_effects.yaml`: 2
- `chess_neigong.yaml`: 0

Add a closed `施毒` author macro because poison has an invariant-heavy policy, not because of raw frequency.

- Move the poison-only `同事件合計強度` switch out of generic `套用狀態` authoring.
- Encode the standard `保留最強`, `層數上限 == 層數`, and same-event potency aggregation coupling in `施毒`.
- Support the explicitly required replace/reset poison form without reopening arbitrary invalid combinations.
- Keep the typed runtime invariant in `ChessBattleEffectValidation.cpp` for C++-constructed rules.
- Add canonical-expansion equality tests and migrate all five shipped poison applications.

Do not add one-off macros solely for brevity. A one-off is eligible only when it protects an atomic domain invariant.

## Phase 6: top-level config language hygiene

- Migrate Simplified Chinese structural keys in the authoritative top-level combo, equipment, and neigong configs, their loaders, schemas, fixtures, and documentation to Traditional Chinese.
- Do not retain compatibility aliases.
- Remove explicit `條件: []`; omission is the canonical spelling.
- Keep this migration mechanically separate from runtime semantic changes where possible.

## Deferred runtime-language work

Do not add these without a concrete blocked gameplay design:

1. status-applied, expired, removed, or dispelled events;
2. general AND/OR/NOT condition expressions;
3. composable selector pipelines;
4. a general numeric expression AST.

These change runtime semantics, determinism, descriptions, hashes, and tests and are outside this authoring-surface implementation.

## Verification and definition of done

- `kys_chess_cli validate-content` succeeds for shipped Easy, Normal, and Hard content and fails with actionable diagnostics for invalid effect content.
- All four generated effect schemas accept shipped configs and reject representative invalid event/action, mechanism-payload, and removed author forms.
- Parser and runtime-validator tests cover every named mechanism and every author timing.
- Macro shorthand and canonical expansions produce identical typed rules.
- Shipped config migrations preserve normalized runtime rules and game-content behavior.
- All unit tests and schema tests pass.
- A debug build is attempted with `.github/build-command.ps1`; a final-link failure caused only by a running game is acceptable under repository instructions.
- After implementation and verification, an independent subagent reviews the changes against this plan. Its findings are either fixed and reverified or explicitly documented before completion.
