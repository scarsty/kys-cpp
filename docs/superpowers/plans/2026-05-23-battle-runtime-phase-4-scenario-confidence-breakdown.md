# Battle Runtime Phase 4 Scenario Confidence Breakdown Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add compact frame-level scenario confidence around battle runtime composition so ownership regressions are caught where subsystems interact.

**Architecture:** Keep existing unit tests for rule math and narrow reducers. Add a dedicated scenario test surface that runs `BattleRuntimeSession` or `BattleFrameRunner` for multiple frames and asserts a small digest of runtime state plus ordered frame facts. Do not move gameplay logic into test helpers; helpers may only build deterministic runtime input and summarize frame output.

**Tech Stack:** C++20, Catch2, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Research Summary

Phase 4 implementation is complete. The planning notes below record the coverage gap that drove the breakdown and the task sequence that was used to add the scenario tests without turning phase 4 into one broad "add scenario tests" pass.

Current coverage already has many frame-level tests:

- `tests/BattleCoreUnitTests.cpp`
  - many `BattleFrameRunner_AdvanceFrame_*` tests for ordering, action commit, damage lifecycle, rescue, projectile cancellation, bounce, death kick, and phase 3 reducer behavior
  - local helpers such as `runBattleFrame`, `configureRuntimeMovement`, `hitDamageFrameState`, `queuePendingDamage`, and `rescueDamageFrameState`
  - file is already large; do not add the new scenario surface here unless a test is tightly coupled to existing local helper code
- `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
  - runtime ownership and `BattleRuntimeSession` tests
  - status tick, team effects, projectile cancel command, frame application replay prevention
- `tests/BattleMovementRealStatsTests.cpp`
  - movement planner behavior and long-run movement stats
  - not a full runtime/session scenario surface
- scene tests such as `BattleSceneFrameDeltaBuilderUnitTests.cpp`
  - presentation mapping only; useful later in phase 5, not the phase 4 runtime scenario anchor

Main gap:

- Existing frame tests mostly protect one behavior at a time.
- There is no reusable scenario digest that says, "after N frames, these units are alive, these vitals changed, these attacks remain, and this ordered event/log shape was emitted."
- Phase 3 regressions are covered individually, but not by compact multi-frame runtime scenarios spanning damage, death effects, follow-up projectiles, action/presentation facts, and persistent runtime state.

Original first slice:

1. Add a small scenario digest helper.
2. Add a new `BattleRuntimeScenarioUnitTests.cpp` file instead of growing `BattleCoreUnitTests.cpp`.
3. Add one multi-frame damage lifecycle scenario that reuses current phase 3 bug surfaces:
   - pending damage kills a unit
   - ally death effects apply in the same damage lifecycle
   - death AOE queues a follow-up projectile
   - the next frame applies that follow-up damage

Stop after that first slice and review whether the digest is clear enough before adding cast/projectile/action scenarios.

---

## Files

- Create: `tests/BattleRuntimeScenarioTestHelpers.h`
  - reusable digest and deterministic runtime setup helpers for scenario tests only
- Create: `tests/BattleRuntimeScenarioUnitTests.cpp`
  - new high-level runtime scenario tests tagged `[battle][scenario][runtime]`
- Modify: `src/CMakeLists.txt`
  - add the new `.cpp` to `KYS_TEST_SOURCES`
- Modify: `tests/kys_tests.vcxproj`
  - add the new `.cpp` and helper header
- Modify: `docs/superpowers/plans/2026-05-21-battle-runtime-frame-direction.md`
  - link this phase 4 breakdown from Phase 4 Step 0

Do not modify production code in the first phase 4 slice unless a scenario exposes a real bug.

---

## Scenario Digest Rules

Digest assertions should be stable and intentionally small:

- frame number
- alive unit ids
- HP/MP/shield for key units
- active attack count
- pending attack spawn count
- HP-changing damage transaction defender ids and committed HP damage
- gameplay event type order when ordering matters
- log text order when ordering matters
- battle result only when the scenario is about battle end

Avoid brittle assertions:

- exact movement positions unless the scenario is about movement
- full visual event vectors
- every log/event field when only ordering or existence matters
- random outcomes without an explicit seed or 100% trigger chance

---

## Task 1: Add Scenario Digest Helper

**Files:**

- Create: `tests/BattleRuntimeScenarioTestHelpers.h`

- [x] **Step 1: Create the helper header**

Add:

```cpp
#pragma once

#include "battle/BattleCore.h"
#include "battle/BattleRuntimeRules.h"
#include "battle/BattleRuntimeSession.h"
#include "BattleLogTestHelpers.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace KysChess::Battle::Test
{

constexpr double ScenarioTileWidth = 36.0;
constexpr int ScenarioCoordCount = 18;
constexpr double ScenarioMinimumVectorNorm = 0.0001;

struct BattleScenarioFrameDigest
{
    int frame{};
    bool battleEnded{};
    int winningTeam = -1;
    std::vector<int> aliveUnitIds;
    std::map<int, int> hpByUnitId;
    std::map<int, int> mpByUnitId;
    std::map<int, int> shieldByUnitId;
    std::size_t activeAttackCount{};
    std::size_t pendingAttackSpawnCount{};
    std::vector<BattleGameplayEventType> gameplayTypes;
    std::vector<BattleLogEventType> logTypes;
    std::vector<std::string> logTexts;
    std::vector<int> damageDefenderIds;
    std::vector<int> committedHpDamage;
};

inline BattleRuntimeRulesConfig scenarioRules()
{
    auto rules = makeHadesBattleRuntimeRules(ScenarioTileWidth, ScenarioCoordCount);
    rules.movementCollisionWorld.walkableByCell.assign(ScenarioCoordCount * ScenarioCoordCount, 1);
    rules.minimumVectorNorm = ScenarioMinimumVectorNorm;
    return rules;
}

inline BattleRuntimeUnit scenarioRuntimeUnit(int id, int team, int hp, Pointf position)
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.realRoleId = id;
    unit.name = std::to_string(id);
    unit.team = team;
    unit.alive = hp > 0;
    unit.vitals = { hp, 100, 20, 100 };
    unit.stats = { 30, 5, 0 };
    unit.motion.position = position;
    unit.motion.facing = { 1, 0, 0 };
    return unit;
}

inline BattleStatusRuntimeUnit scenarioStatusUnit(const BattleRuntimeUnit& unit)
{
    BattleStatusUnitState status;
    status.id = unit.id;
    status.alive = unit.alive;
    status.hp = unit.vitals.hp;
    status.maxHp = unit.vitals.maxHp;
    return makeBattleStatusRuntimeUnit(status);
}

inline BattleAttackState scenarioAttackState()
{
    BattleAttackState state;
    state.hitRadius = ScenarioTileWidth * 2.0;
    state.minimumVectorNorm = ScenarioMinimumVectorNorm;
    state.bounceSpawnDistance = ScenarioTileWidth * 1.5;
    state.defaultProjectileSpeed = ScenarioTileWidth / 3.0;
    state.spendNonThroughOnHit = true;
    return state;
}

inline void seedScenarioRuntimeStores(BattleRuntimeState& state, std::vector<BattleRuntimeUnit> units)
{
    state.unitStore.gridTransform = { ScenarioTileWidth, ScenarioCoordCount };
    state.movement.frame = 0;
    state.movement.config = scenarioRules().movementConfig;
    state.attacks = scenarioAttackState();
    state.unitStore.units = std::move(units);
    state.combo.units.clear();
    state.status.units.clear();
    state.damage.unitExtras.clear();
    state.deathEffects.store.units.clear();
    state.movement.agents.clear();

    for (const auto& unit : state.unitStore.units)
    {
        state.combo.units.emplace(unit.id, RoleComboState{});
        state.status.units.push_back(scenarioStatusUnit(unit));
        state.damage.unitExtras.push_back(makeBattleDamageRuntimeUnit(
            makeBattleDamageUnitState(unit, static_cast<const BattleDamageRuntimeUnit*>(nullptr))));
        state.deathEffects.store.units.push_back({ .id = unit.id });

        if (unit.alive)
        {
            BattleMovementAgentState agent;
            agent.physics.position = unit.motion.position;
            agent.physics.velocity = unit.motion.velocity;
            agent.physics.acceleration = unit.motion.acceleration;
            state.movement.agents.emplace(unit.id, agent);
        }
    }
}

inline BattlePendingDamageIntent scenarioPreResolvedDamage(int attackerUnitId, int defenderUnitId, int damage)
{
    BattlePendingDamageIntent intent;
    intent.request.attackerUnitId = attackerUnitId;
    intent.request.defenderUnitId = defenderUnitId;
    intent.request.baseDamage = damage;
    intent.request.preResolvedDamage = true;
    intent.presentation.enabled = true;
    intent.presentation.normalDamageTextSize = 30;
    intent.presentation.emphasizedDamageTextSize = 44;
    intent.presentation.normalDamageColor = { 255, 90, 79, 255 };
    intent.presentation.emphasizedDamageColor = { 255, 45, 85, 255 };
    return intent;
}

inline BattleScenarioFrameDigest digestScenarioFrame(
    const BattleRuntimeState& runtime,
    const BattleFrameResult& result)
{
    BattleScenarioFrameDigest digest;
    digest.frame = result.frame.frame;
    digest.battleEnded = runtime.result.ended;
    digest.winningTeam = runtime.result.winningTeam;
    digest.activeAttackCount = runtime.attacks.attacks.size();
    digest.pendingAttackSpawnCount = runtime.pendingAttackSpawns.size();

    for (const auto& unit : runtime.unitStore.units)
    {
        if (unit.alive)
        {
            digest.aliveUnitIds.push_back(unit.id);
        }
        digest.hpByUnitId.emplace(unit.id, unit.vitals.hp);
        digest.mpByUnitId.emplace(unit.id, unit.vitals.mp);
        digest.shieldByUnitId.emplace(unit.id, unit.shield);
    }
    std::ranges::sort(digest.aliveUnitIds);

    for (const auto& event : result.frame.gameplayEvents)
    {
        digest.gameplayTypes.push_back(event.type);
    }
    for (const auto& event : result.frame.logEvents)
    {
        digest.logTypes.push_back(event.type);
        digest.logTexts.push_back(BattleLogTest::textOf(event));
    }
    for (const auto& transaction : result.damageTransactions)
    {
        if (transaction.finalHpDamage > 0)
        {
            digest.damageDefenderIds.push_back(transaction.defender.id);
            digest.committedHpDamage.push_back(transaction.finalHpDamage);
        }
    }

    return digest;
}

inline std::vector<BattleScenarioFrameDigest> runScenarioFrames(BattleRuntimeSession& session, int frameCount)
{
    assert(frameCount >= 0);

    std::vector<BattleScenarioFrameDigest> digests;
    digests.reserve(static_cast<std::size_t>(frameCount));
    for (int i = 0; i < frameCount; ++i)
    {
        auto result = session.runFrame();
        digests.push_back(digestScenarioFrame(session.runtime(), result));
    }
    return digests;
}

inline bool digestHasLogText(const BattleScenarioFrameDigest& digest, const std::string& text)
{
    return std::ranges::find(digest.logTexts, text) != digest.logTexts.end();
}

}  // namespace KysChess::Battle::Test
```

- [x] **Step 2: Verify the helper compiles through a temporary include**

Do not commit a fake include-only test. The helper will be compiled by Task 2 when the first scenario test file includes it.

Implementation note: `BattleRuntimeScenarioTestHelpers.h` also includes `scenarioSetupUnit(...)` so session-creation scenarios do not duplicate setup-unit field assignment. The digest filters zero-HP damage transactions because scripted stun can emit a status-only transaction before the HP damage transaction; scenarios that care about status-only transactions should add an explicit digest field.

---

## Task 2: Add Scenario Test File And Build Wiring

**Files:**

- Create: `tests/BattleRuntimeScenarioUnitTests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/kys_tests.vcxproj`

- [x] **Step 1: Add the new scenario test file**

Add:

```cpp
#include "BattleRuntimeScenarioTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>

using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;

namespace
{

BattleSetupUnitInput setupUnit(int id, int team, int hp, Pointf position)
{
    BattleSetupUnitInput unit;
    unit.unitId = id;
    unit.realRoleId = id;
    unit.name = std::to_string(id);
    unit.team = team;
    unit.alive = hp > 0;
    unit.vitals = { hp, 100, 20, 100 };
    unit.stats = { 30, 5, 0 };
    unit.motion.position = position;
    unit.motion.facing = { 1, 0, 0 };
    return unit;
}

BattleRuntimeSessionCreationInput basicSessionInput()
{
    BattleRuntimeSessionCreationInput input;
    input.rules = scenarioRules();
    input.randomSeed = 1;
    input.battleFrame = 0;
    input.units = {
        setupUnit(0, 0, 100, { 100, 100, 0 }),
        setupUnit(1, 1, 100, { 500, 100, 0 }),
    };
    input.comboStates.emplace(0, KysChess::RoleComboState{});
    input.comboStates.emplace(1, KysChess::RoleComboState{});
    return input;
}

}  // namespace

TEST_CASE("BattleRuntimeScenario_BasicSessionDigestTracksFramesAndRuntime", "[battle][scenario][runtime]")
{
    auto creation = BattleRuntimeSession::createInitialized(basicSessionInput());
    auto session = std::move(creation.session);

    auto digests = runScenarioFrames(session, 2);

    REQUIRE(digests.size() == 2);
    CHECK(digests[0].frame == 1);
    CHECK(digests[1].frame == 2);
    CHECK(digests[1].aliveUnitIds == std::vector<int>{ 0, 1 });
    CHECK(digests[1].hpByUnitId.at(0) == 100);
    CHECK(digests[1].hpByUnitId.at(1) == 100);
    CHECK(digests[1].activeAttackCount == 0);
    CHECK(digests[1].pendingAttackSpawnCount == 0);
    CHECK_FALSE(digests[1].battleEnded);
}
```

- [x] **Step 2: Add the `.cpp` to CMake**

In `src/CMakeLists.txt`, add this entry near the other battle tests:

```cmake
    "${KYS_ROOT}/tests/BattleRuntimeScenarioUnitTests.cpp"
```

- [x] **Step 3: Add the `.cpp` and helper header to the Visual Studio test project**

In `tests/kys_tests.vcxproj`, add:

```xml
    <ClCompile Include="BattleRuntimeScenarioUnitTests.cpp" />
```

near the other test `.cpp` entries, and add:

```xml
    <ClInclude Include="BattleRuntimeScenarioTestHelpers.h" />
```

near the other test helper headers.

- [x] **Step 4: Build the test target**

Run:

```powershell
.github\build-command.ps1 -Target kys_tests
```

Expected: MSBuild exits 0.

- [x] **Step 5: Run the new scenario smoke test**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleRuntimeScenario_BasicSessionDigestTracksFramesAndRuntime"
```

Expected: 1 test case passes.

---

## Task 3: Add Multi-Frame Damage Lifecycle Scenario

**Files:**

- Modify: `tests/BattleRuntimeScenarioUnitTests.cpp`

- [x] **Step 1: Add the damage lifecycle scenario**

Append to `tests/BattleRuntimeScenarioUnitTests.cpp`:

```cpp
TEST_CASE("BattleRuntimeScenario_DamageDeathEffectsAndFollowUpDigest", "[battle][scenario][runtime]")
{
    BattleRuntimeState state;
    seedScenarioRuntimeStores(state, {
        scenarioRuntimeUnit(0, 0, 100, { 100, 100, 0 }),
        scenarioRuntimeUnit(1, 1, 10, { 120, 100, 0 }),
        scenarioRuntimeUnit(2, 1, 50, { 140, 100, 0 }),
    });
    state.unitStore.requireUnit(2).stats.attack = 10;
    state.damage.pendingDamage.push_back(scenarioPreResolvedDamage(0, 1, 10));
    state.damage.unitEffects[1] = { 50, 6, 1 };
    state.projectileFollowUps.projectileSpeed = ScenarioTileWidth / 3.0;
    state.projectileFollowUps.minimumProjectileFrames = 20;
    state.projectileFollowUps.areaProjectileFramePadding = 15;
    state.projectileFollowUps.areaSpawnDistance = ScenarioTileWidth;

    BattleDeathEffectExtras dead;
    dead.id = 1;
    dead.comboIds = { 9 };
    dead.appliedEffects.push_back(
        { KysChess::EffectType::DeathMedical, 20, 0, "", KysChess::Trigger::Always, 0, 0, 0, 9 });

    BattleDeathEffectExtras ally;
    ally.id = 2;
    ally.shieldPctMaxHp = 30;
    ally.comboIds = { 9 };
    ally.appliedEffects.push_back(
        { KysChess::EffectType::AllyDeathStatBoost, 4, 0, "", KysChess::Trigger::Always, 0, 0, 0, 9 });
    ally.appliedEffects.push_back(
        { KysChess::EffectType::ShieldOnAllyDeath, 1, 0, "", KysChess::Trigger::Always, 0, 0, 0, 9 });

    state.deathEffects.store.units = { { 0 }, dead, ally };
    state.deathEffects.store.regularSynergyComboIds.insert(9);

    BattleRuntimeSession session(std::move(state));

    auto first = session.runFrame();
    auto firstDigest = digestScenarioFrame(session.runtime(), first);

    CHECK(firstDigest.aliveUnitIds == std::vector<int>{ 0, 2 });
    CHECK(firstDigest.hpByUnitId.at(1) == 0);
    CHECK(firstDigest.hpByUnitId.at(2) == 70);
    CHECK(firstDigest.shieldByUnitId.at(2) == 30);
    CHECK(firstDigest.pendingAttackSpawnCount == 1);
    CHECK(firstDigest.damageDefenderIds == std::vector<int>{ 1 });
    CHECK(firstDigest.committedHpDamage == std::vector<int>{ 10 });
    CHECK(digestHasLogText(firstDigest, "同袍之死（攻防+4）"));
    CHECK(digestHasLogText(firstDigest, "死亡醫療"));
    CHECK(digestHasLogText(firstDigest, "護盾重獲（30護盾）"));

    auto second = session.runFrame();
    auto secondDigest = digestScenarioFrame(session.runtime(), second);

    CHECK(secondDigest.hpByUnitId.at(0) == 50);
    CHECK(secondDigest.damageDefenderIds == std::vector<int>{ 0 });
    CHECK(secondDigest.committedHpDamage == std::vector<int>{ 50 });
    CHECK(secondDigest.pendingAttackSpawnCount == 0);
    CHECK_FALSE(secondDigest.battleEnded);
}
```

- [x] **Step 2: Run only the new scenario tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][scenario][runtime]"
```

Expected: the smoke scenario and damage lifecycle scenario pass.

- [x] **Step 3: Run focused battle runtime tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][scenario][runtime],[battle][core][breakthrough],[battle][frame_runner][runtime][unit]"
```

Expected: all selected tests pass.

---

## Task 4: Plan Remaining Phase 4 Scenario Slices

**Files:**

- Modify: `docs/superpowers/plans/2026-05-23-battle-runtime-phase-4-scenario-confidence-breakdown.md`

- [x] **Step 1: Review digest usefulness after Task 3**

After Task 3 passes, review whether the digest made the scenario easier to understand than the existing one-off assertions.

Keep the helper if:

- failures point at a small frame summary
- the scenario remains readable without duplicating runtime setup
- the helper does not hide gameplay behavior

Delete or simplify the helper if:

- assertions become less clear than direct checks
- the helper starts applying gameplay consequences
- the helper becomes a second runtime builder with too much policy

Review result: keep the helper. The damage lifecycle scenario reads as a compact two-frame state summary, and the one useful adjustment was to keep `damageDefenderIds` / `committedHpDamage` focused on HP-changing transactions rather than status-only zero-HP transactions.

- [x] **Step 2: Add all remaining Phase 4 scenario slices to the plan**

Phase 4 should include all three remaining scenario slices. Execute them one at a time, in this order, so a failure identifies the subsystem boundary that regressed:

1. **Action-to-projectile-to-damage scenario**
   - starts from `BattleRuntimeSession::createInitialized`
   - runs enough frames for runtime cast start, action commit, attack spawn, projectile hit, and damage reduction
   - digest should assert cast/action state cleared, one damage transaction occurred, and no stale pending cast remains
2. **Projectile cancellation scenario**
   - starts with active opposing projectiles in `BattleRuntimeState`
   - runs one frame
   - digest should assert attack event order, cancellation log text, and remaining projectile damage/active attack count
3. **Death/rescue scenario**
   - starts with a protected defender and legal rescue cells
   - runs one frame
   - digest should assert rescue result, runtime position/vitals, and damage lifecycle ordering

Do not batch the three implementations into one broad edit. Each slice gets its own test addition, scenario tag run, focused runtime/core run when relevant, and plan checkbox update.

---

## Task 5: Add Action-To-Projectile-To-Damage Scenario

**Files:**

- Modify: `tests/BattleRuntimeScenarioTestHelpers.h`
- Modify: `tests/BattleRuntimeScenarioUnitTests.cpp`
- Modify: `docs/superpowers/plans/2026-05-23-battle-runtime-phase-4-scenario-confidence-breakdown.md`

- [x] **Step 1: Extend the digest with action state**

In `BattleScenarioFrameDigest`, add only the fields needed to prove cast/action state does not remain stale:

```cpp
std::size_t pendingCastCount{};
std::map<int, bool> haveActionByUnitId;
std::map<int, BattleOperationType> operationTypeByUnitId;
```

In `digestScenarioFrame(...)`, populate:

```cpp
digest.pendingCastCount = runtime.action.pendingCasts.size();
for (const auto& unit : runtime.unitStore.units)
{
    digest.haveActionByUnitId.emplace(unit.id, unit.haveAction);
    digest.operationTypeByUnitId.emplace(unit.id, unit.operationType);
}
```

- [x] **Step 2: Add a deterministic session-created action scenario**

Add one test named:

```cpp
TEST_CASE("BattleRuntimeScenario_ActionProjectileDamageClearsCastState", "[battle][scenario][runtime]")
```

Use `BattleRuntimeSession::createInitialized(...)`, not a hand-built `BattleRuntimeState`. Seed:

- unit `0`, team `0`, HP `100`, position `{ 100, 100, 0 }`, equipped normal skill with `attackAreaType = 3`, `magicType = 1`, `visualEffectId = 77`, `selectDistance = 4`, and deterministic nonzero `magicPower`
- unit `1`, team `1`, HP `100`, position close enough for the skill to be selected and hit
- combo states for both units
- `input.rules = scenarioRules()`
- `input.randomSeed = 1`

Run frames until HP damage appears and the action state clears, with a hard cap of `80` frames:

```cpp
std::vector<BattleScenarioFrameDigest> digests;
BattleScenarioFrameDigest damageDigest;
bool sawDamage = false;
for (int i = 0; i < 80; ++i)
{
    auto result = session.runFrame();
    auto digest = digestScenarioFrame(session.runtime(), result);
    digests.push_back(digest);
    if (!sawDamage && !digest.committedHpDamage.empty())
    {
        damageDigest = digest;
        sawDamage = true;
    }
    if (sawDamage
        && digest.pendingCastCount == 0
        && !digest.haveActionByUnitId.at(0)
        && digest.operationTypeByUnitId.at(0) == BattleOperationType::None)
    {
        break;
    }
}
```

Assert:

- at least one digest was produced with HP damage
- `damageDigest.damageDefenderIds == std::vector<int>{ 1 }`
- committed HP damage is positive
- final digest has `pendingCastCount == 0`
- final digest has `haveActionByUnitId.at(0) == false`
- final digest has `operationTypeByUnitId.at(0) == BattleOperationType::None`
- final digest has no stale pending attack spawn after the projectile hit

- [x] **Step 3: Run action scenario verification**

Run:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "BattleRuntimeScenario_ActionProjectileDamageClearsCastState"
x64\Debug\kys_tests.exe "[battle][scenario][runtime]"
```

Expected: all commands exit 0.

---

## Task 6: Add Projectile Cancellation Scenario

**Files:**

- Modify: `tests/BattleRuntimeScenarioTestHelpers.h`
- Modify: `tests/BattleRuntimeScenarioUnitTests.cpp`
- Modify: `docs/superpowers/plans/2026-05-23-battle-runtime-phase-4-scenario-confidence-breakdown.md`

- [x] **Step 1: Extend the digest with attack event and remaining projectile facts**

In `BattleScenarioFrameDigest`, add:

```cpp
std::vector<BattleAttackEventType> attackTypes;
std::vector<int> activeAttackIds;
std::map<int, int> projectileCancelWeakenByAttackId;
```

In `digestScenarioFrame(...)`, populate attack event order from `result.attackEvents` and remaining projectile state from `runtime.attacks.attacks`:

```cpp
for (const auto& event : result.attackEvents)
{
    digest.attackTypes.push_back(event.type);
}
for (const auto& attack : runtime.attacks.attacks)
{
    digest.activeAttackIds.push_back(attack.id);
    digest.projectileCancelWeakenByAttackId.emplace(
        attack.id,
        attack.state.projectileCancelWeaken);
}
std::ranges::sort(digest.activeAttackIds);
```

- [x] **Step 2: Add a scenario helper for active cancel projectiles**

Add a helper in `BattleRuntimeScenarioTestHelpers.h`:

```cpp
inline BattleAttackInstance scenarioCancelProjectile(int id, int attackerUnitId, int cancelDamage)
{
    BattleAttackInstance attack;
    attack.id = id;
    attack.state.attackerUnitId = attackerUnitId;
    attack.frame = 5;
    attack.state.totalFrame = 30;
    attack.state.position = { 500, 500, 0 };
    attack.state.operationType = BattleOperationType::RangedProjectile;
    attack.state.projectileCancelDamage = cancelDamage;
    return attack;
}
```

- [x] **Step 3: Add the projectile cancellation scenario**

Add one test named:

```cpp
TEST_CASE("BattleRuntimeScenario_ProjectileCancellationDigest", "[battle][scenario][runtime]")
```

Seed a `BattleRuntimeState` with `seedScenarioRuntimeStores(...)`, two living ranged-side units, and two active projectiles:

```cpp
state.attacks.attacks.push_back(scenarioCancelProjectile(10, 0, 25));
state.attacks.attacks.push_back(scenarioCancelProjectile(20, 1, 12));
```

Run one frame through `BattleRuntimeSession`.

Assert:

- `attackTypes == std::vector<BattleAttackEventType>{ BattleAttackEventType::Moved, BattleAttackEventType::Moved, BattleAttackEventType::ProjectileCancel }`
- `gameplayTypes` contains `BattleGameplayEventType::ProjectileCancelled`
- `digestHasLogText(digest, "抵消彈道 #10 vs #20（25 - 12 = 13）")`
- `activeAttackCount == 2`
- `activeAttackIds == std::vector<int>{ 10, 20 }`
- `projectileCancelWeakenByAttackId.at(10) == 12`
- `projectileCancelWeakenByAttackId.at(20) == 25`

- [x] **Step 4: Run projectile cancellation verification**

Run:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "BattleRuntimeScenario_ProjectileCancellationDigest"
x64\Debug\kys_tests.exe "[battle][scenario][runtime],[battle][core],[battle][frame_runner][runtime][unit]"
```

Expected: all commands exit 0.

---

## Task 7: Add Death/Rescue Scenario

**Files:**

- Modify: `tests/BattleRuntimeScenarioTestHelpers.h`
- Modify: `tests/BattleRuntimeScenarioUnitTests.cpp`
- Modify: `docs/superpowers/plans/2026-05-23-battle-runtime-phase-4-scenario-confidence-breakdown.md`

- [x] **Step 1: Extend the digest with rescue result facts**

In `BattleScenarioFrameDigest`, add:

```cpp
std::size_t rescueResultCount{};
std::vector<int> rescuePulledUnitIds;
std::vector<int> rescuePullerUnitIds;
std::vector<Point> rescueDestinationCells;
```

In `digestScenarioFrame(...)`, populate from `result.rescueResults`:

```cpp
digest.rescueResultCount = result.rescueResults.size();
for (const auto& rescue : result.rescueResults)
{
    if (rescue.teleport)
    {
        digest.rescuePulledUnitIds.push_back(rescue.teleport->unitId);
        digest.rescuePullerUnitIds.push_back(rescue.teleport->pullerUnitId);
        digest.rescueDestinationCells.push_back(rescue.teleport->destinationCell);
    }
}
```

- [x] **Step 2: Add a scenario rescue cell helper**

Add:

```cpp
inline BattleRescueCellSnapshot scenarioRescueCell(int x, int y, bool walkable = true, bool occupied = false)
{
    BattleRescueCellSnapshot cell;
    cell.cell = { x, y };
    cell.walkable = walkable;
    cell.occupied = occupied;
    return cell;
}
```

- [x] **Step 3: Add the death/rescue scenario**

Add one test named:

```cpp
TEST_CASE("BattleRuntimeScenario_DeathRescueDigest", "[battle][scenario][runtime]")
```

Seed a `BattleRuntimeState` with:

- unit `0`, team `0`, HP `100`, position `{ 100, 100, 0 }`
- unit `1`, team `1`, HP `50`, position `{ 180, 100, 0 }`
- unit `2`, team `1`, HP `100`, position `{ 220, 100, 0 }`
- pending pre-resolved damage from unit `0` to unit `1` for `30`
- `state.combo.units[2].forcePullProtect = true`
- `state.combo.units[2].forcePullProtectRemaining = 1`
- `state.rescue.cells = { scenarioRescueCell(2, 3), scenarioRescueCell(3, 2), scenarioRescueCell(5, 5) }`

Run one frame through `BattleRuntimeSession`.

Assert:

- `damageDefenderIds == std::vector<int>{ 1 }`
- `committedHpDamage == std::vector<int>{ 30 }`
- `rescueResultCount == 1`
- `rescuePulledUnitIds == std::vector<int>{ 1 }`
- `rescuePullerUnitIds == std::vector<int>{ 2 }`
- `rescueDestinationCells == std::vector<Point>{ { 2, 3 } }`
- `hpByUnitId.at(1) == 30`
- runtime unit `1` has `invincible == 10`
- runtime combo for unit `2` has `forcePullProtectRemaining == 0`

- [x] **Step 4: Run death/rescue verification**

Run:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "BattleRuntimeScenario_DeathRescueDigest"
x64\Debug\kys_tests.exe "[battle][scenario][runtime],[battle][core][breakthrough]"
```

Expected: all commands exit 0.

---

## Task 8: Phase 4 Final Gate

**Files:**

- Modify: `docs/superpowers/plans/2026-05-23-battle-runtime-phase-4-scenario-confidence-breakdown.md`

- [x] **Step 1: Run the full phase gate**

Run:

```powershell
git diff --check
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected:

- `git diff --check` exits 0
- full test suite exits 0
- MSBuild exits 0, except final game executable linking can be treated as successful only if the executable is locked by a running game process

- [x] **Step 2: Mark Phase 4 done**

Mark the Phase 4 done criteria below only after Tasks 5, 6, 7, and 8 pass.

Phase 4 status: done. Tasks 5, 6, 7, and the full Phase 4 gate passed.

---

## Completion Gate

Run after each implemented task:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scenario][runtime]"
```

Run before marking phase 4 done:

```powershell
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected:

- `git diff --check` exits 0
- scenario tests pass
- full test suite passes before phase completion
- MSBuild exits 0, except final game executable linking can be treated as successful only if the executable is locked by a running game process

---

## Phase 4 Done Criteria

Phase 4 can be declared done when:

- at least five scenario tests exist under `[battle][scenario][runtime]`
- one scenario spans at least two frames
- one scenario starts from `BattleRuntimeSession::createInitialized`
- one scenario covers damage lifecycle plus follow-up frame effects
- one scenario covers action/cast/projectile/damage composition
- one scenario covers projectile cancellation ordering and remaining attack state
- one scenario covers death/rescue lifecycle ordering and runtime rescue consequences
- existing unit tests remain focused; no broad rule assertions are deleted just because a scenario exists
- no production behavior changes were made solely to satisfy brittle scenario assertions
