# BattleSceneHades Backend Exit Implementation Plan

> **Status:** Superseded on May 4, 2026. The completion claim in this document was rejected after source review because `BattleSceneHades` still owns substantial backend gameplay through delegated helper phases. Treat this file as historical checkpoint evidence only. Continue from `docs/battle_scene_hades_backend_exit_continuation_plan.md`.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move backend battle ownership out of `BattleSceneHades` until the scene only adapts legacy objects, applies committed results, and plays presentation.

**Architecture:** The core battle layer owns gameplay decisions and returns explicit result objects. `BattleSceneHades` builds snapshots from legacy `Role`/`Magic`/`Item` state, calls core orchestration APIs, applies returned deltas through narrow adapter helpers, and sends presentation events to playback. `BattlePresentation` remains visual/log output only; it is not a home for gameplay rules.

**Tech Stack:** C++20, existing `src/battle` systems, Catch2 tests under `tests`, MSBuild via `.github\build-command.ps1`.

---

## Current Checkpoint

Worktree: `D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion`

Branch: `battle-refactor-completion-nospaces`

Final implementation checkpoint before Task 14 cleanup: `2d3d5bc refactor: make battle scene frame path an adapter pipeline`

Task 14 cleanup renamed the remaining presentation shims away from old gameplay/log helper names and reran the final boundary verification on May 4, 2026.

What the branch has achieved:

- Core systems own battle hit resolution, frame runtime decisions, action commit decisions, damage application, death/result flow, and typed gameplay command construction.
- `BattleSceneHades::backRun1()` is now an adapter pipeline: build frame input, advance core frame, apply core result, play presentation, and clean up visual-only state.
- `BattlePresentationRecorder`, `BattlePresentationFrame`, and `BattleScenePresentationPlayer` carry visual/log semantics only.

Final Task 14 verification covered:

- No forbidden legacy/render dependencies in `src/battle`.
- No old scene hit transaction, team-effect wrapper, shield-break, `std::function`, or competing frame-orchestrator names remain.
- Full Debug x64 `kys_tests` and `kys` build verification passed, with existing compiler warnings.

## Non-Negotiable Boundary Rules

- No new `std::function` routing in `BattleSceneHades` or adapter helpers for gameplay dispatch. Use explicit command/result types and typed variant visitation.
- No paired legacy/core overloads. Legacy conversion happens once at the adapter boundary.
- No broad defensive no-op branches for invalid boundary data. Use `assert` for invariants.
- No copy-paste migration. If two branches need the same application path, introduce one explicit command type and one explicit application helper.
- No `Role*`, `Magic*`, `Item*`, `Scene`, `Engine`, `TextureManager`, or `Font` dependencies in `src/battle`.
- `AttackEffect` remains visual playback state only. Gameplay cannot read from `attack_effects_`.
- `BattlePresentation` carries visual/log semantics only. Gameplay outcomes travel as gameplay result objects, not as presentation events.

## Target Shape

The final frame path should read like this:

```cpp
void BattleSceneHades::backRun1()
{
    auto input = battle_adapter_.buildFrameInput();
    auto result = Battle::BattleFrameRunner().advanceFrame(input);
    battle_adapter_.apply(result.deltas);
    battle_adapter_.apply(result.commands);
    presentation_player_.play(result.presentation);
    cleanupVisualOnlyState();
}
```

Use the existing `BattleFrameRunner` as the one frame orchestration owner:

- `src/battle`: decisions, deterministic result construction, gameplay commands, presentation event facts.
- `BattleFrameRunner`: the full core frame pipeline. Extend it; do not add a competing `BattleFrameOrchestrator`.
- `BattleSceneBattleAdapter`: legacy snapshot construction and committed delta/command application.
- `BattleSceneHades`: timing, input, camera, render, pointer resolution, presentation playback, visual cleanup.

## Proposed Core Interfaces

Create composed orchestration types instead of more one-off wrapper functions.

```cpp
#include <variant>

namespace KysChess::Battle
{

struct BattleHitUnitSnapshot
{
    int id = -1;
    int team = 0;
    bool alive = true;
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;
    int attack = 0;
    double defence = 0.0;
    int speed = 0;
    int invincible = 0;
    int hurtFrame = 0;
    int cooldown = 0;
    int cooldownMax = 0;
    bool haveAction = false;
    BattleOperationType operationType = BattleOperationType::None;
    int actType = -1;
    Pointf position;
    Pointf facing;
};

struct BattleHitSkillSnapshot
{
    int id = -1;
    std::string name;
    int hurtType = 0;
    int magicType = 0;
    int effectId = -1;
    int attackerActProperty = 0;
    int defenderActProperty = 0;
    int magicPower = 0;
    int resolvedBaseDamage = 0;
};

struct BattleHitItemSnapshot
{
    int id = -1;
    std::string name;
    int hiddenWeaponEffectId = -1;
    int resolvedDamage = 0;
};

struct BattleHpDamageCommand
{
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int damage = 0;
    bool critical = false;
    bool ultimate = false;
    bool executed = false;
    std::string skillName;
    std::string detailText;
};

struct BattleMpDamageCommand
{
    int sourceUnitId = -1;
    int targetUnitId = -1;
    BattleDamageRequest damage;
};

struct BattleAcceptedHitSideEffectCommand
{
    int sourceUnitId = -1;
    int targetUnitId = -1;
    BattleDamageRequest damage;
};

struct BattleTeamHealCommand
{
    int sourceUnitId = -1;
    int flatHeal = 0;
    int pctHeal = 0;
    std::string reason;
};

struct BattleTeamMpRestoreCommand
{
    int sourceUnitId = -1;
    int amount = 0;
    std::string reason;
};

struct BattleTeamShieldCommand
{
    int sourceUnitId = -1;
    int amount = 0;
    bool refreshOnly = false;
    std::string reason;
};

struct BattleProjectileSpawnCommand
{
    BattleAttackSpawnRequest request;
    std::string reason;
};

struct BattleAutoUltimateCommand
{
    int unitId = -1;
    bool consumeMp = false;
};

struct BattleKnockbackCommand
{
    int targetUnitId = -1;
    Pointf velocityDelta;
    bool grantHurtFrame = false;
};

struct BattleTempAttackBuffCommand
{
    int unitId = -1;
    int attackBonus = 0;
    int durationFrames = 0;
    std::string reason;
};

struct BattleLastAttackerCommand
{
    int targetUnitId = -1;
    int attackerUnitId = -1;
};

struct BattleRumbleCommand
{
    int lowFrequency = 0;
    int highFrequency = 0;
    int durationMs = 0;
};

using BattleGameplayCommand = std::variant<
    BattleHpDamageCommand,
    BattleMpDamageCommand,
    BattleAcceptedHitSideEffectCommand,
    BattleTeamHealCommand,
    BattleTeamMpRestoreCommand,
    BattleTeamShieldCommand,
    BattleProjectileSpawnCommand,
    BattleAutoUltimateCommand,
    BattleKnockbackCommand,
    BattleTempAttackBuffCommand,
    BattleLastAttackerCommand,
    BattleRumbleCommand>;

struct BattleHitResolutionInput
{
    BattleAttackEvent attackEvent;
    BattleHitUnitSnapshot attacker;
    BattleHitUnitSnapshot defender;
    BattleHitSkillSnapshot skill;
    BattleHitItemSnapshot item;
    RoleComboState attackerCombo;
    RoleComboState defenderCombo;
    int pendingDefenderHpDamage = 0;
    int sharedBleedMaxStacks = 1;
    int randomDamageVariance = 0;
    std::vector<double> percentRolls;
};

struct BattleHitResolutionResult
{
    RoleComboState attackerCombo;
    RoleComboState defenderCombo;
    std::vector<BattleGameplayCommand> commands;
    std::vector<BattlePresentationEvent> presentationEvents;
    bool dodged = false;
    bool reflected = false;
    bool critical = false;
    bool executed = false;
    int finalHpDamage = 0;
    int finalMpDamage = 0;
};

class BattleHitResolver
{
public:
    BattleHitResolutionResult resolve(const BattleHitResolutionInput& input) const;
};

}  // namespace KysChess::Battle
```

The first implementation may split this into smaller files, but the public API must preserve the same principle: snapshots in, typed commands/events out. Do not replace these typed commands with a generic enum-plus-optional-fields payload.

## Task 1: Correct The Last Commit Direction

**Files:**

- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`

- [x] Remove `#include <functional>` from `BattleSceneHades.h`.
- [x] Delete `BattleSceneHades::commitTeamEffectEvents()`.
- [x] Delete adapter pass-through helpers `commitTeamRecovery()`, `commitTeamFocus()`, and `commitTeamBarrier()`.
- [x] Add an explicit local helper while this responsibility is still in the scene:

```cpp
enum class LegacyTeamEffectCommitType
{
    Heal,
    MpRestore,
    Shield,
};

struct LegacyTeamEffectCommitRequest
{
    LegacyTeamEffectCommitType type = LegacyTeamEffectCommitType::Heal;
    int sourceUnitId = -1;
    int flatHeal = 0;
    int pctHeal = 0;
    int amount = 0;
    bool refreshOnly = false;
};
```

- [x] Implement one `commitLegacyTeamEffect(const LegacyTeamEffectCommitRequest& request)` helper that switches on `request.type`, calls `BattleTeamEffectSystem`, writes the world back, and returns events.
- [x] Replace all lambda call sites with explicit request construction.
- [x] Run:

```powershell
rg -n "std::function|commitTeamEffectEvents|commitTeamRecovery|commitTeamFocus|commitTeamBarrier" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][team_effect],[battle][combo]"
```

Expected: no matches from the search, the test target rebuilds, and the focused team/combo tests pass.

- [x] Commit:

```powershell
git add src\BattleSceneHades.h src\BattleSceneHades.cpp src\BattleSceneBattleAdapter.h src\BattleSceneBattleAdapter.cpp
git commit -m "refactor: restore explicit battle scene team effects"
```

## Task 2: Add Core Hit Resolver Skeleton

**Files:**

- Create: `src/battle/BattleHitResolver.h`
- Create: `src/battle/BattleHitResolver.cpp`
- Modify: `tests/kys_tests.vcxproj`
- Modify: `tests/kys_tests.vcxproj.filters`
- Create: `tests/BattleHitResolverUnitTests.cpp`

- [x] Add the snapshot/result/command types from the Proposed Core Interfaces section.
- [x] Implement a minimal resolver that asserts valid attacker/defender ids and returns no commands for a non-hit event.
- [x] Write tests for invariant-free normal construction:

```cpp
TEST_CASE("BattleHitResolver_NonHitEvent_ReturnsNoGameplayCommands", "[battle][hit_resolver][unit]")
{
    BattleHitResolutionInput input;
    input.attackEvent.type = BattleAttackEventType::Moved;
    input.attacker.id = 1;
    input.defender.id = 2;

    auto result = BattleHitResolver().resolve(input);

    CHECK(result.commands.empty());
    CHECK(result.presentationEvents.empty());
}
```

- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][hit_resolver][unit]"
```

- [x] Commit:

```powershell
git add src\battle\BattleHitResolver.h src\battle\BattleHitResolver.cpp tests\BattleHitResolverUnitTests.cpp tests\kys_tests.vcxproj tests\kys_tests.vcxproj.filters
git commit -m "refactor: add battle hit resolver boundary"
```

## Task 3: Move Hit Base Damage And Shape

**Files:**

- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`

- [x] Add adapter helpers that snapshot attacker, defender, selected skill, selected hidden weapon, combo state, pending damage, and precomputed scalar values. Hidden weapon base damage and magic base damage can stay adapter-computed for this task because they still depend on legacy formulas and asset data.
- [x] Add resolver tests for:
  - hidden weapon uses `item.resolvedDamage / 5`;
  - magic uses `skill.resolvedBaseDamage`;
  - projectile cancel damage and strength multiplier shape final damage;
  - frozen side effect emits a `BattleAcceptedHitSideEffectCommand`;
  - knockback emits a `BattleKnockbackCommand` rather than mutating velocity.
- [x] Move the `BattleDamageSystem::shapeLegacyHitDamage()` call and immediate knockback/frozen/hurt-frame decisions out of `commitBattleHitImpact()`.
- [x] Scene application may still apply the returned commands, but it must not recompute hit shape.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][hit_resolver][unit]"
rg -n "shapeLegacyHitDamage|knockbackStrength|grantsHurtFrame" src\BattleSceneHades.cpp
```

Expected: no `shapeLegacyHitDamage` match in `BattleSceneHades.cpp`; remaining knockback text only in adapter command application.

- [x] Commit:

```powershell
git add src\battle\BattleHitResolver.h src\battle\BattleHitResolver.cpp tests\BattleHitResolverUnitTests.cpp src\BattleSceneBattleAdapter.h src\BattleSceneBattleAdapter.cpp src\BattleSceneHades.cpp
git commit -m "refactor: move legacy hit shaping into resolver"
```

## Task 4: Move Attacker Hit Decisions

**Files:**

- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `src/BattleSceneHades.cpp`

- [x] Move attacker-side damage modifier application, MP-ratio damage boost, crit/ramping stack, on-hit HP/MP/drain, poison, legacy stun, trigger stun, knockback proc, offensive cooldown extension, and triggered team heal into `BattleHitResolver`.
- [x] The resolver returns:
  - `BattleAcceptedHitSideEffectCommand` values for status/resource/cooldown transactions;
  - `BattleTeamHealCommand` values for triggered team heal;
  - presentation events for status/heal logs and role effects.
- [x] Tests must cover at least:
  - crit marks `result.critical` and emits a status log event;
  - HP-on-hit emits a heal presentation event and one accepted-hit command;
  - poison emits an accepted-hit command with poison fields;
  - triggered team heal emits a `BattleTeamHealCommand` without applying the team world.
- [x] Delete the migrated attacker decision branches from `commitBattleHitImpact()`.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][hit_resolver][unit]"
rg -n "mpRatioDmgBoostPct|shapeAttackerHitDamage|mpOnHit|hpOnHit|poisonDOTPct|resolveLegacyStunFrames|resolveOffensiveCooldownExtendPct|collectTriggeredTeamHeal" src\BattleSceneHades.cpp
```

Expected: no matches in `commitBattleHitImpact()`.

- [x] Commit:

```powershell
git add src\battle\BattleHitResolver.h src\battle\BattleHitResolver.cpp tests\BattleHitResolverUnitTests.cpp src\BattleSceneHades.cpp
git commit -m "refactor: move attacker hit decisions into resolver"
```

## Task 5: Move Defender Hit Decisions

**Files:**

- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `src/BattleSceneHades.cpp`

- [x] Move defender modifier application, defender adaptation events, late poison/max-hit cap modifiers, defensive cooldown extension, counter stun, projectile reflect, execute decision, invincible/first-hit/shield defense, defender block commands, skill reflect, bleed proc, damage-reduce debuff, and MP block into `BattleHitResolver`.
- [x] Return explicit typed commands for:
  - `BattleAutoUltimateCommand` when counter-block needs an ultimate;
  - `BattleHpDamageCommand` when skill reflect damages the attacker;
  - `BattleAcceptedHitSideEffectCommand` for status/cooldown/MP block;
  - `BattleLastAttackerCommand` for final attribution.
- [x] Return presentation events for block, reflect, execute, shield absorption, cap messages, and status logs.
- [x] Tests must cover:
  - reflected ranged projectile changes actual source/target;
  - execute turns into an executed HP damage command;
  - first-hit block zeros damage and emits block presentation;
  - skill reflect emits a reflected HP damage command;
  - bleed and MP block emit accepted-hit commands.
- [x] Delete migrated defender branches from `commitBattleHitImpact()`.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][hit_resolver][unit]"
rg -n "shapeDefenderHitDamage|resolveProjectileReflect|resolveExecuteCombo|collectDefenderBlockCommands|resolveDefense|resolveBleedProc|resolveDamageReduceDebuffProc|MpBlock" src\BattleSceneHades.cpp
```

Expected: no matches in `commitBattleHitImpact()`.

- [x] Commit:

```powershell
git add src\battle\BattleHitResolver.h src\battle\BattleHitResolver.cpp tests\BattleHitResolverUnitTests.cpp src\BattleSceneHades.cpp
git commit -m "refactor: move defender hit decisions into resolver"
```

## Task 6: Move On-Hit Follow-Up Requests

**Files:**

- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`

- [x] Move current-HP blast, team MP restore, team shield, spiral bleed projectile command selection, nearby tracking projectile command selection, and hit extra projectile count into resolver output.
- [x] Keep target geometry and actual projectile spawn construction in adapter helpers when map walkability or legacy role positions are required.
- [x] Replace direct scene calls to `spawnSpiralBleedProjectiles()`, `spawnNearbyTrackingProjectiles()`, and `spawnHitExtraProjectiles()` from hit resolution with application of typed `BattleGameplayCommand` alternatives.
- [x] Tests must prove each command becomes the correct variant alternative without mutating scene state.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][hit_resolver][unit]"
rg -n "CurrentHpPctBlast|TeamMpRestore|FlatShield|SpiralBleedProjectile|NearbyTrackingProjectiles|getHitExtraProjectileCount|spawnHitExtraProjectiles|spawnNearbyTrackingProjectiles|spawnSpiralBleedProjectiles" src\BattleSceneHades.cpp
```

Expected: command enum names no longer appear in `BattleSceneHades.cpp`; spawn helpers may remain only as command application or visual construction plumbing.

- [x] Commit:

```powershell
git add src\battle\BattleHitResolver.h src\battle\BattleHitResolver.cpp tests\BattleHitResolverUnitTests.cpp src\BattleSceneBattleAdapter.h src\BattleSceneBattleAdapter.cpp src\BattleSceneHades.cpp
git commit -m "refactor: move on-hit follow-up decisions into resolver"
```

## Task 7: Collapse `commitBattleHitImpact()`

**Files:**

- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`

- [x] Replace `commitBattleHitImpact()` with `applyResolvedBattleHit(const BattleHitResolutionResult& result)`.
- [x] The new function may only:
  - resolve unit ids to `Role*`;
  - apply returned typed commands through adapter helpers;
  - write returned combo states;
  - record returned presentation events;
  - update visual-only sets such as `criticalHitRoles_` from explicit result flags.
- [x] `applyResolvedBattleHit()` must not call `BattleComboTriggerSystem` or `BattleDamageSystem` directly.
- [x] Run:

```powershell
rg -n "commitBattleHitImpact|applyLegacyMagicHitTransaction|BattleComboTriggerSystem|BattleDamageSystem" src\BattleSceneHades.cpp
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][hit_resolver][unit]"
```

Expected: no old hit function names; no combo/damage system calls inside the hit application helper; the test target rebuilds and the hit resolver tests pass.

- [x] Commit:

```powershell
git add src\BattleSceneHades.h src\BattleSceneHades.cpp src\BattleSceneBattleAdapter.h src\BattleSceneBattleAdapter.cpp
git commit -m "refactor: collapse scene hit impact into result application"
```

## Task 8: Move Shield-Break Command Application

**Files:**

- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `src/BattleSceneHades.cpp`

- [x] Convert shield-break command collection into resolver commands.
- [x] Remove `playCommittedShieldBreakCommands()` from `BattleSceneHades`.
- [x] Apply shield-break commands through the same explicit command application path used by hit results.
- [x] Tests must cover shield explosion, auto ultimate command, temporary attack buff command, and MP restore command.
- [x] Run:

```powershell
rg -n "playCommittedShieldBreakCommands|collectShieldBreakCommands|BattleShieldBreakCommand" src\BattleSceneHades.cpp src\BattleSceneHades.h
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][hit_resolver][unit]"
```

Expected: no matches from the search, the test target rebuilds, and the hit resolver tests pass.

- [x] Commit:

```powershell
git add src\battle\BattleHitResolver.h src\battle\BattleHitResolver.cpp tests\BattleHitResolverUnitTests.cpp src\BattleSceneHades.h src\BattleSceneHades.cpp
git commit -m "refactor: move shield break commands out of scene"
```

## Task 9: Move Scripted Hit Application

**Files:**

- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `src/BattleSceneHades.cpp`

- [x] Route scripted projectile hits through `BattleHitResolver`.
- [x] Resolver emits accepted-hit side-effect commands for scripted stun and bleed, HP damage commands for scripted damage, and presentation events for scripted status logs.
- [x] Delete the scripted-impact branch from `advanceBattleFrameBeforeDamage()`.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][hit_resolver][unit]"
rg -n "hasScriptedImpact|scriptedStunFrames|scriptedBleedStacks|scriptedDamage|makeScriptedHitRequest" src\BattleSceneHades.cpp
```

Expected: scene may pass scripted fields into resolver input, but it no longer applies scripted effects itself.

- [x] Commit:

```powershell
git add src\battle\BattleHitResolver.h src\battle\BattleHitResolver.cpp tests\BattleHitResolverUnitTests.cpp src\BattleSceneHades.cpp
git commit -m "refactor: route scripted hits through hit resolver"
```

## Task 10: Extend `BattleFrameRunner` To Own Runtime Decisions

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Create: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
- Modify: `tests/kys_tests.vcxproj`
- Modify: `tests/kys_tests.vcxproj.filters`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`

- [x] Extend the existing `BattleFrameRunner`; do not create `BattleFrameOrchestrator` or any second full-frame coordinator.
- [x] Inputs include frame number, unit runtime snapshots, combo snapshots, active attack world, pending attack spawn requests, status snapshots, and deterministic roll values.
- [x] Outputs include unit runtime deltas, combo state deltas, attack events, hit resolution inputs, typed gameplay commands, presentation events, and visual-only projectile events.
- [x] Move skill-finished combo events and pending skill team heal into `BattleFrameRunner`.
- [x] Move projectile cancel damage adaptation into `BattleFrameRunner` as an explicit typed command for adapter-provided base damage scalars.
- [x] Scene remains responsible for movement collision callbacks until walkability is represented as core geometry input.
- [x] Tests must cover skill-finished team heal command and projectile cancel damage command.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][frame_runner][runtime][unit]"
rg -n "collectSkillFinishedRuntimeEvents|collectPendingSkillTeamHeal|applyProjectileCancelDamage|projectileCancelDamage|CancelDmg" src\BattleSceneHades.cpp
```

Expected: scene only adapts returned projectile cancel deltas and no longer chooses cancel damage.

- [x] Commit:

```powershell
git add src\battle\BattleCore.h src\battle\BattleCore.cpp tests\BattleFrameRunnerRuntimeUnitTests.cpp tests\kys_tests.vcxproj tests\kys_tests.vcxproj.filters src\BattleSceneBattleAdapter.h src\BattleSceneBattleAdapter.cpp src\BattleSceneHades.cpp
git commit -m "refactor: extend battle frame runner runtime ownership"
```

## Task 11: Move Action And Cast Commit Decisions

**Files:**

- Modify: `src/battle/BattleCastSystem.h`
- Modify: `src/battle/BattleCastSystem.cpp`
- Create: `tests/BattleActionCommitUnitTests.cpp`
- Modify: `tests/kys_tests.vcxproj`
- Modify: `tests/kys_tests.vcxproj.filters`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`

- [x] Extend cast/action core output so committed cast returns operation-count delta, pending skill-team-heal flag change, blink attack command, item use command, hidden-weapon projectile command, item-count delta, and presentation events.
- [x] Move blink target selection intent into core as a typed command; adapter resolves walkable destination cells and applies the selected teleport.
- [x] Move hidden-weapon projectile spawn command construction into core snapshots, using adapter-provided target/facing scalar input.
- [x] `Action()` may play audio, advance animation frame, apply returned deltas, and clear legacy pointers. It must not choose targets or mutate combo gameplay state.
- [x] Tests must cover:
  - ultimate cast emits pending skill-team-heal combo delta;
  - blink attack alternates weakest/random intent through combo state delta;
  - hidden weapon item emits a projectile command and item-count delta;
  - operation count advances in core output.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][cast][unit] [battle][action_commit][unit]"
rg -n "blinkAttack|blinkAttackUseWeakest|findFarthestEnemy|UsingItem|useItem|OperationCount|onSkillTeamHealPending" src\BattleSceneHades.cpp
```

Expected: matches in scene are adapter application only, not decision branches.

- [x] Commit:

```powershell
git add src\battle\BattleCastSystem.h src\battle\BattleCastSystem.cpp tests\BattleActionCommitUnitTests.cpp tests\kys_tests.vcxproj tests\kys_tests.vcxproj.filters src\BattleSceneBattleAdapter.h src\BattleSceneBattleAdapter.cpp src\BattleSceneHades.cpp
git commit -m "refactor: move action commit decisions into core"
```

## Task 12: Extract Damage Application And Death Result Flow

**Files:**

- Create: `src/battle/BattleDamageApplicationSystem.h`
- Create: `src/battle/BattleDamageApplicationSystem.cpp`
- Create: `tests/BattleDamageApplicationSystemUnitTests.cpp`
- Modify: `tests/kys_tests.vcxproj`
- Modify: `tests/kys_tests.vcxproj.filters`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`

- [x] Move pending pre-resolved HP damage application, kill rewards, death prevention, hurt invincibility, death AOE commands, ally-death effects, and battle-result decision into core result objects.
- [x] Keep camera slow/zoom/shake as presentation/application of explicit result events.
- [x] Adapter applies HP/MP/shield/dead/combo deltas and invokes tracker calls from explicit `UnitDied`, `KillRecorded`, and `BattleEnded` events.
- [x] Tests must cover:
  - fatal damage emits death and kill reward events;
  - death prevention leaves unit alive and emits invincibility presentation;
  - death AOE becomes projectile command;
  - ally-death effects become explicit buff/heal/shield commands;
  - battle result is returned without scene scanning team state.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][damage_application][unit]"
rg -n "recordKill|recordBattleEnd|resolveCoreBattleResult|deathAOE|collectAllyDeathEffects|calExpGot|setExit" src\BattleSceneHades.cpp
```

Expected: scene only reacts to explicit result events; no backend death/result decisions remain.

- [x] Commit:

```powershell
git add src\battle\BattleDamageApplicationSystem.h src\battle\BattleDamageApplicationSystem.cpp tests\BattleDamageApplicationSystemUnitTests.cpp tests\kys_tests.vcxproj tests\kys_tests.vcxproj.filters src\BattleSceneBattleAdapter.h src\BattleSceneBattleAdapter.cpp src\BattleSceneHades.cpp
git commit -m "refactor: move damage application flow into core"
```

## Task 13: Make `backRun1()` The Adapter Pipeline

**Files:**

- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
- Modify: `tests/BattleActionCommitUnitTests.cpp`
- Modify: `tests/BattleDamageApplicationSystemUnitTests.cpp`

- [x] Replace `advanceBattleFrameBeforeDamage()`, `applyPendingPreResolvedDamageFrame()`, and `advanceBattleFrameAfterDamage()` with clearly named adapter steps:
  - `buildBattleFrameInput()`;
  - `advanceCoreBattleFrame()`;
  - `applyCoreBattleFrameResult()`;
  - `playCorePresentationFrame()`;
  - `cleanupVisualOnlyBattleFrameState()`.
- [x] Preserve visual maintenance in the scene: hurt flash timers, text effect lifetimes, visual attack effect cleanup, camera/manual camera behavior, and render-facing state.
- [x] Move all remaining gameplay reads of `attack_effects_` to the core attack world or explicit adapter snapshots.
- [x] Run:

```powershell
rg -n "attack_effects_.*HP|attack_effects_.*MP|attack_effects_.*Damage|AttackEffect.*HP|AttackEffect.*MP|AttackEffect.*Damage" src\BattleSceneHades.cpp src\BattleSceneHades.h
rg -n "BattleComboTriggerSystem|BattleDamageSystem|BattleTeamEffectSystem|BattleDeathEffectSystem|BattleProjectileTargetingSystem" src\BattleSceneHades.cpp
rg -n "HP|MP|CoolDown|Dead|Invincible|LastAttacker|Poison|Bleed|Shield" src\BattleSceneHades.cpp
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core],[battle][hit_resolver],[battle][frame_runner],[battle][cast],[battle][action_commit],[battle][damage_application]"
```

Expected: remaining matches are adapter snapshot building, committed delta application, rendering, or visual cleanup. Any gameplay decision match blocks completion. The test target must rebuild before the focused tests run.

- [x] Commit:

```powershell
git add src\BattleSceneHades.h src\BattleSceneHades.cpp src\BattleSceneBattleAdapter.h src\BattleSceneBattleAdapter.cpp src\battle\BattleCore.h src\battle\BattleCore.cpp tests\BattleHitResolverUnitTests.cpp tests\BattleFrameRunnerRuntimeUnitTests.cpp tests\BattleActionCommitUnitTests.cpp tests\BattleDamageApplicationSystemUnitTests.cpp
git commit -m "refactor: make battle scene frame path an adapter pipeline"
```

## Task 14: Final Boundary Verification

**Files:**

- Modify: `docs/battle_scene_hades_backend_exit_plan.md`
- Modify only files needed to fix verification failures.

- [x] Run forbidden dependency search:

```powershell
rg -n "Role\*|Magic\*|Item\*|Engine|TextureManager|Font|Scene" src\battle
```

Expected: no matches.

- [x] Run scene blocker searches:

```powershell
rg -n "applyLegacyMagicHitTransaction|commitBattleHitImpact|applyScriptedHitTransaction|applyTeamHeal|applyTeamMP|applyTeamShield|triggerShieldBreakEffects|playCommittedShieldBreakCommands|commitTeamEffectEvents" src\BattleSceneHades.cpp src\BattleSceneHades.h
rg -n "std::function" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
rg -n "BattleFrameOrchestrator|frame_orchestrator" src tests
rg -n "logBattle|addFloatingText|addDamageNumber|addRoleEffect" src\BattleSceneHades.cpp
```

Expected: old blockers, `std::function`, and competing frame-orchestrator names have no matches. Presentation helpers may remain only as playback plumbing or event-recording shims.

- [x] Run full verification:

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

- [x] Commit:

```powershell
git add docs\battle_scene_hades_backend_exit_plan.md
git commit -m "docs: close battle scene backend exit plan"
```

## Completion Criteria

- `BattleSceneHades::backRun1()` reads as orchestration: build input, run core, apply result, play presentation, cleanup visuals.
- `BattleSceneHades` does not choose skills, resolve hits, roll combo effects, compute damage, tick statuses, apply team effects, decide battle result, or read visual projectile state for gameplay.
- `commitBattleHitImpact()` and the old hit transaction names are gone.
- `BattleSceneHades` has no gameplay dispatch using `std::function`.
- `src/battle` has no forbidden legacy/render dependencies.
- Every migrated behavior has a focused core unit test.
- Full Debug x64 `kys_tests` and `kys` build pass, except final game link may be blocked if the executable is running.
