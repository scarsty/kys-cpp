# Battle Output Ownership And Presentation Boundary

## Current Boundary

`src/battle` owns battle facts as value state and explicit output events:

- `BattleLogEvent` is authoritative log output for damage, healing, status, death, and battle end facts.
- `BattleVisualEvent` is rendering intent for floating text, role effects, damage numbers, camera focus, and projectile visuals.
- `BattlePresentationPlaybackPlanner` plans only visual commands. It does not plan tracker log commands.

`BattleSceneHades` applies committed deltas to legacy `Role*` state, plays explicit log and visual output, and renders. It should not infer battle facts by comparing deltas after core systems have already committed the frame.

`Role*`, `Magic*`, `Item*`, save data, and database records remain legacy boundary objects. They are imported by `BattleSceneHades` and `BattleSceneBattleAdapter`, then translated into battle value state. `src/battle` must not store or consume those legacy object pointers.

## What Changed

Damage, hit resolution, rescue, team effects, and frame command reduction now carry separate log and visual streams. Scene-side reconstruction of damage-frame tracker entries is removed from the core frame apply path:

- damage numbers stay visual output;
- damage/heal/status tracker entries are log output;
- movement status facts are log output;
- projectile lifecycle output stays visual output;
- unit death and battle end are emitted as gameplay facts and log facts once.

`BattleScenePresentationPlayer` is a thin adapter:

- `playLogs` writes `BattleLogEvent` entries to `BattleTracker`;
- `playVisuals` runs visual events through `BattlePresentationPlaybackPlanner`;
- `play` calls both without inspecting deltas or making gameplay decisions.

## Runtime Ownership

`BattleRuntimeState` is now the public long-lived owner for core frame-to-frame state. `BattleFrameRunner::runFrame()` advances that runtime directly; the old `BattleFrameState`/`advanceFrame()` API has been removed.

Runtime-owned state includes:

- active attack world and pending attack spawns;
- shared-hit targets;
- movement decisions, movement physics runtime, and static terrain/cell geometry;
- pending cast results and ultimate caster ids keyed by battle unit id;
- battle result emission state;
- RNG-derived frame roll streams;
- imported combo value state.

The scene still refreshes live legacy `Role*` values into per-frame core inputs because `Role`, `Magic`, `Item`, save data, and database records remain boundary models. That import path should continue to shrink, but persistent runtime ownership should stay in `BattleRuntimeState`, not in `BattleSceneHades`.

## Follow-Up Sequence

1. Replace the remaining action/role import structs with smaller phase-specific import functions.
2. Keep moving per-frame calculations from `BattleSceneHades` into adapter/core value builders.
3. Keep `Role*`, `Magic*`, and `Item*` at adapter boundaries only.
4. Delete import fields as each phase stops needing live legacy data.
