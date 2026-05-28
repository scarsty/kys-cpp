# BattleRuntimeUnitRecord Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `BattleRuntimeUnitRecord` the owner of per-unit battle runtime data and delete the horizontal side stores for unit facts.

**Architecture:** `BattleRuntimeState` keeps global battle systems: projectiles, attack state, configs, queues, random, result state, rescue cells, terrain, reservations, and presentation/style globals. `BattleRuntimeUnits` owns append-stable `BattleRuntimeUnitRecord` rows; each row owns core unit data plus status, combo, damage extras, movement agent, death extras, rescue counters, and action ownership. Systems that operate per unit iterate records; systems that operate globally keep their existing global state and look up records by unit id when needed.

**Tech Stack:** C++23 ranges/views, Catch2 tests through `kys_tests`, PowerShell build command `.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal`.

---

## Target Ownership Model

Final per-unit record:

```cpp
struct BattleRuntimeUnitActionState
{
    std::optional<BattleActionPlanSeed> planSeed;
    std::optional<BattlePendingCastAction> pendingCast;
    bool ultimateCaster = false;
};

struct BattleRescueUnitRuntime
{
    int unitId = -1;
    int forcePullProtectRemaining = 0;
    int forcePullExecuteRemaining = 0;
};

struct BattleRuntimeUnitRecord
{
    BattleRuntimeUnit core;
    RoleComboState combo;
    BattleStatusRuntimeUnit status;
    BattleDamageRuntimeUnit damage;
    BattleMovementAgentState movement;
    BattleDeathEffectExtras deathEffects;
    BattleRescueUnitRuntime rescue;
    BattleRuntimeUnitActionState action;

    int id() const { return core.id; }
    bool alive() const { return core.alive; }

    const BattleActionPlanSeed* actionPlan() const;
    void setActionPlan(BattleActionPlanSeed seed);
    BattlePendingCastAction* pendingCast();
    const BattlePendingCastAction* pendingCast() const;
    void setPendingCast(BattlePendingCastAction action);
    void clearPendingCast();
    void markUltimateCaster();
    void clearUltimateCaster();
    bool isUltimateCaster() const;
    void clearActionOwners();

    void clearFrozen();
    void setMpBlockFrames(int frames);
    void addTempAttackBuff(int attackBonus, int durationFrames);
    BattleStatusUnitState statusDamageState() const;
    void writeStatusDamageResult(const BattleStatusUnitState& unit);

    BattleDamageUnitState damageState() const;
    void writeDamageResult(const BattleDamageUnitState& unit);
};
```

Final owning container:

```cpp
class BattleRuntimeUnits
{
public:
    void reserve(std::size_t count);
    void append(BattleRuntimeUnitRecord record);

    BattleRuntimeUnitRecord* find(int unitId);
    const BattleRuntimeUnitRecord* find(int unitId) const;
    BattleRuntimeUnitRecord& require(int unitId);
    const BattleRuntimeUnitRecord& require(int unitId) const;

    BattleRuntimeUnit& requireCore(int unitId);
    const BattleRuntimeUnit& requireCore(int unitId) const;

    std::size_t size() const;
    bool empty() const;

    auto all();
    auto all() const;
    auto live();
    auto live() const;
    auto dead();
    auto dead() const;

private:
    std::vector<BattleRuntimeUnitRecord> records_;
};
```

Final runtime state shape:

```cpp
struct BattleRuntimeState
{
    BattleRuntimeUnits units;
    BattleGridTransform gridTransform;
    BattleMovementState movement;
    BattleAttackState attacks;
    BattleRuntimeRandom random;

    struct DamageState
    {
        bool sortPendingDamageByDefenderMagnitude = false;
        std::map<int, BattleDamagePresentationStyle> presentationStylesByDefender;
    } damage;

    struct StatusState
    {
        BattleStatusSystemConfig config;
    } status;

    struct DeathEffectState
    {
        std::set<int> regularSynergyComboIds;
    } deathEffects;

    struct RescueState
    {
        std::vector<BattleRescueCellSnapshot> cells;
        double executeUnattendedRadius = 0.0;
        BattleFrameRescueCounterAttackConfig counterAttack;
    } rescue;

    BattleResultState result;
    TeamEffectState teamEffects;
    MovementPhysicsState movementPhysics;
    BattleRuntimeActions action;
    BattleProjectileFollowUpContext projectileFollowUps;
    BattleNextFrameQueues nextFrame;
};
```

The final state must not contain these unit side stores:

- `BattleRuntimeState::unitStore.units`
- `BattleRuntimeState::status.units`
- `BattleRuntimeState::combo.units`
- `BattleRuntimeState::damage.unitExtras`
- `BattleRuntimeState::movement.agents`
- `BattleRuntimeState::deathEffects.store.units`
- `BattleRuntimeState::rescue.units`
- `BattleRuntimeActions::planSeeds_`
- `BattleRuntimeActions::pendingCasts_`
- `BattleRuntimeActions::ultimateCasterUnitIds_`
- `BattleRuntimeUnitHandle`
- `BattleRuntimeUnit*View`

The stable-record invariant:

```cpp
// Runtime unit records are append-only during runtime creation and are not
// erased during battle frames. C++23 ranges returned by all/live/dead are
// frame-local views over this stable vector; do not store them across runtime
// creation or mutation that appends records.
```

Projectiles, attack instances, follow-up contexts, frame queues, configs, random state, terrain, reservations, battle result, rescue cells, and presentation accumulation remain global.

---

## File Structure

- Modify `src/battle/BattleRuntimeUnits.h`
  - Replace view-based `BattleRuntimeUnits` with an owning container.
  - Add `BattleRuntimeUnitRecord`, `BattleRuntimeUnitActionState`, and `BattleRescueUnitRuntime`.
  - Remove `BattleRuntimeUnitHandle` and view classes after migration.
- Modify `src/battle/BattleRuntimeActions.h`
  - Keep action configs/rules.
  - Remove per-unit maps and sets after action ownership moves into records.
- Modify `src/battle/BattleRuntimeUnitSpawn.h`
  - Make spawns produce one record instead of separate unit/status/damage/movement/combo/action rows.
- Modify `src/battle/BattleRuntimeUnitSpawn.cpp`
  - Build and append `BattleRuntimeUnitRecord`.
  - Seed presentation styles and global configs only.
- Modify `src/battle/BattleCore.cpp`
  - Replace side-store lookups with record access.
  - Update movement, damage, status, combo, death, rescue, and action phases.
- Modify `src/battle/BattleUnitStore.h`
  - Keep `BattleRuntimeUnit`, `BattleGridTransform`, and core helper declarations.
  - Remove `BattleUnitStore` only after all systems accept `BattleRuntimeUnits`.
- Modify `src/battle/BattleStatusSystem.h` and `src/battle/BattleStatusSystem.cpp`
  - Tick `BattleRuntimeUnitRecord` / `BattleRuntimeUnits` instead of a separate status vector.
- Modify `src/battle/BattleTeamEffectSystem.h` and `src/battle/BattleTeamEffectSystem.cpp`
  - Read status/combo/action facts from records.
- Modify `src/battle/BattleDeathEffectSystem.h` and `src/battle/BattleDeathEffectSystem.cpp`
  - Read/write death extras from records and keep only global synergy metadata in runtime state.
- Modify `src/battle/BattleAttackSystem.h` and `src/battle/BattleAttackSystem.cpp`
  - Accept `const BattleRuntimeUnits&` for target lookup while keeping projectile ownership in `BattleAttackState`.
- Modify `src/battle/BattleCastSystem.h` and `src/battle/BattleCastSystem.cpp`
  - Accept `const BattleRuntimeUnits&` in commit helpers that need unit lookup.
- Modify `src/battle/BattleRuntimeSession.cpp`
  - Build records through `appendRuntimeUnit`.
  - Derive global runtime stores without rebuilding per-unit side stores.
- Modify tests under `tests/`
  - Replace direct side-store setup/assertions with record setup/assertions.
  - Keep pure system unit tests focused on the system APIs after signature changes.

---

## Task 1: Baseline And Ownership Audit

**Files:**
- Read: `src/battle/BattleRuntimeUnits.h`
- Read: `src/battle/BattleRuntimeActions.h`
- Read: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Read: `src/battle/BattleCore.cpp`
- Read: `tests/BattleCoreUnitTests.cpp`
- Read: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Capture baseline test state**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core][ownership]"
.\x64\Debug\kys_tests.exe "[battle][runtime_session][ownership]"
.\x64\Debug\kys_tests.exe "[battle][core][movement]"
```

Expected: build succeeds, focused tests pass.

- [ ] **Step 2: Capture all side-store call sites**

Run:

```powershell
rg -n "status\.units|combo\.units|damage\.unitExtras|movement\.agents|deathEffects\.store\.units|rescue\.units|state\.unitStore|runtime\.unitStore|BattleRuntimeUnitHandle|BattleRuntimeUnit[A-Za-z]+View|planSeeds_|pendingCasts_|ultimateCasterUnitIds_|state\.units\(\)" src tests
```

Expected: matches exist. Save the output in the implementation notes for this branch; every match must either disappear or be explicitly classified as a global non-unit store before final verification.

- [ ] **Step 3: Commit the audit note if useful**

Only commit if an audit note file is added. Use:

```powershell
git add docs/superpowers/plans/2026-05-27-battle-runtime-unit-record-migration.md
git commit -m "docs: plan battle runtime unit record migration"
```

Expected: plan commit succeeds if the team wants the plan isolated.

---

## Task 2: Add The Record Type And Append-Stable Container

**Files:**
- Modify: `src/battle/BattleRuntimeUnits.h`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add a focused failing test**

Add this test near the existing `[battle][core][ownership]` tests in `tests/BattleCoreUnitTests.cpp`:

```cpp
TEST_CASE("BattleRuntimeUnitRecord_OwnsPerUnitRuntimeFacts", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 7;
    record.core.alive = true;
    record.combo.onSkillTeamHealPending = true;
    record.status.id = 7;
    record.damage.id = 7;
    record.movement.active = true;
    record.deathEffects.id = 7;
    record.rescue.unitId = 7;

    BattleActionPlanSeed plan;
    plan.unitId = 99;
    record.setActionPlan(plan);

    BattlePendingCastAction pending;
    pending.unitId = 99;
    pending.targetUnitId = 3;
    record.setPendingCast(pending);
    record.markUltimateCaster();

    CHECK(record.id() == 7);
    CHECK(record.alive());
    REQUIRE(record.actionPlan() != nullptr);
    CHECK(record.actionPlan()->unitId == 7);
    REQUIRE(record.pendingCast() != nullptr);
    CHECK(record.pendingCast()->unitId == 7);
    CHECK(record.pendingCast()->targetUnitId == 3);
    CHECK(record.isUltimateCaster());

    record.clearActionOwners();

    CHECK(record.pendingCast() == nullptr);
    CHECK_FALSE(record.isUltimateCaster());
}
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
```

Expected: compile fails because `BattleRuntimeUnitRecord` and its action methods do not exist.

- [ ] **Step 3: Add the record and action ownership methods**

In `src/battle/BattleRuntimeUnits.h`, add:

```cpp
#include <optional>
#include <set>
```

Then add the record types before `BattleRuntimeUnits`:

```cpp
struct BattleRuntimeUnitActionState
{
    std::optional<BattleActionPlanSeed> planSeed;
    std::optional<BattlePendingCastAction> pendingCast;
    bool ultimateCaster = false;
};

struct BattleRescueUnitRuntime
{
    int unitId = -1;
    int forcePullProtectRemaining = 0;
    int forcePullExecuteRemaining = 0;
};

struct BattleRuntimeUnitRecord
{
    BattleRuntimeUnit core;
    RoleComboState combo;
    BattleStatusRuntimeUnit status;
    BattleDamageRuntimeUnit damage;
    BattleMovementAgentState movement;
    BattleDeathEffectExtras deathEffects;
    BattleRescueUnitRuntime rescue;
    BattleRuntimeUnitActionState action;

    int id() const { return core.id; }
    bool alive() const { return core.alive; }

    const BattleActionPlanSeed* actionPlan() const
    {
        return action.planSeed ? &*action.planSeed : nullptr;
    }

    void setActionPlan(BattleActionPlanSeed seed)
    {
        seed.unitId = id();
        action.planSeed = std::move(seed);
    }

    BattlePendingCastAction* pendingCast()
    {
        return action.pendingCast ? &*action.pendingCast : nullptr;
    }

    const BattlePendingCastAction* pendingCast() const
    {
        return action.pendingCast ? &*action.pendingCast : nullptr;
    }

    void setPendingCast(BattlePendingCastAction pending)
    {
        pending.unitId = id();
        action.pendingCast = std::move(pending);
    }

    void clearPendingCast()
    {
        action.pendingCast.reset();
    }

    void markUltimateCaster()
    {
        action.ultimateCaster = true;
    }

    void clearUltimateCaster()
    {
        action.ultimateCaster = false;
    }

    bool isUltimateCaster() const
    {
        return action.ultimateCaster;
    }

    void clearActionOwners()
    {
        clearPendingCast();
        clearUltimateCaster();
    }
};
```

The `BattleRuntimeUnitRecord` fields may be public during this migration. That is intentional: do not add shallow getters that only return `status`, `combo`, `damage`, or `movement`. Add domain methods only where they replace hidden unit-id side-store mutation.

- [ ] **Step 4: Add an owning container in parallel with the current view**

Still in `src/battle/BattleRuntimeUnits.h`, add a staging container with a temporary name so the current `BattleRuntimeUnits` view can keep compiling:

```cpp
class BattleRuntimeUnitRecords
{
public:
    void reserve(std::size_t count)
    {
        records_.reserve(count);
    }

    void append(BattleRuntimeUnitRecord record)
    {
        assert(record.id() >= 0);
        assert(find(record.id()) == nullptr);
        records_.push_back(std::move(record));
    }

    BattleRuntimeUnitRecord* find(int unitId)
    {
        auto it = std::ranges::find_if(
            records_,
            [unitId](const BattleRuntimeUnitRecord& record)
            {
                return record.id() == unitId;
            });
        return it == records_.end() ? nullptr : &*it;
    }

    const BattleRuntimeUnitRecord* find(int unitId) const
    {
        auto it = std::ranges::find_if(
            records_,
            [unitId](const BattleRuntimeUnitRecord& record)
            {
                return record.id() == unitId;
            });
        return it == records_.end() ? nullptr : &*it;
    }

    BattleRuntimeUnitRecord& require(int unitId)
    {
        auto* record = find(unitId);
        assert(record != nullptr);
        return *record;
    }

    const BattleRuntimeUnitRecord& require(int unitId) const
    {
        const auto* record = find(unitId);
        assert(record != nullptr);
        return *record;
    }

    std::size_t size() const { return records_.size(); }
    bool empty() const { return records_.empty(); }

    auto all()
    {
        return records_ | std::views::all;
    }

    auto all() const
    {
        return records_ | std::views::all;
    }

    auto live()
    {
        return records_
            | std::views::filter([](const BattleRuntimeUnitRecord& record) { return record.core.alive; });
    }

    auto live() const
    {
        return records_
            | std::views::filter([](const BattleRuntimeUnitRecord& record) { return record.core.alive; });
    }

    auto dead()
    {
        return records_
            | std::views::filter([](const BattleRuntimeUnitRecord& record) { return !record.core.alive; });
    }

    auto dead() const
    {
        return records_
            | std::views::filter([](const BattleRuntimeUnitRecord& record) { return !record.core.alive; });
    }

private:
    std::vector<BattleRuntimeUnitRecord> records_;
};
```

Add the staging field to `BattleRuntimeState`:

```cpp
BattleRuntimeUnitRecords unitRecords;
```

- [ ] **Step 5: Run the focused test**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core][ownership]"
```

Expected: focused ownership tests pass.

- [ ] **Step 6: Commit**

Run:

```powershell
git add src/battle/BattleRuntimeUnits.h tests/BattleCoreUnitTests.cpp
git commit -m "refactor: add battle runtime unit record owner"
```

Expected: commit succeeds.

---

## Task 3: Make Spawning Populate Records

**Files:**
- Modify: `src/battle/BattleRuntimeUnitSpawn.h`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Add a failing spawn ownership test**

Add this test near existing runtime initialization ownership tests:

```cpp
TEST_CASE("BattleRuntimeUnitSpawn_AppendsUnitRecordWithPerUnitFacts", "[battle][runtime_session][ownership]")
{
    BattleRuntimeState runtime;
    BattleRuntimeUnit unit;
    unit.id = 4;
    unit.team = 1;
    unit.alive = true;
    unit.vitals.maxHp = 100;
    unit.motion.position = { 32.0f, 48.0f, 0.0f };

    RoleComboState combo;
    combo.onSkillTeamHealPending = true;

    BattleActionPlanSeed plan;
    plan.unitId = 99;

    appendRuntimeUnit(runtime, makeRuntimeUnitSpawn(std::move(unit), combo, plan));

    REQUIRE(runtime.unitRecords.size() == 1);
    const auto& record = runtime.unitRecords.require(4);
    CHECK(record.core.id == 4);
    CHECK(record.combo.onSkillTeamHealPending);
    CHECK(record.status.id == 4);
    CHECK(record.damage.id == 4);
    CHECK(record.movement.physics.position.x == 32.0f);
    CHECK(record.deathEffects.id == 4);
    CHECK(record.rescue.unitId == 4);
    REQUIRE(record.actionPlan() != nullptr);
    CHECK(record.actionPlan()->unitId == 4);
}
```

- [ ] **Step 2: Run and verify failure**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
```

Expected: test fails or compile fails because `appendRuntimeUnit` does not populate `unitRecords`.

- [ ] **Step 3: Change `BattleRuntimeUnitSpawn` to expose record construction**

In `src/battle/BattleRuntimeUnitSpawn.h`, extend the spawn:

```cpp
struct BattleRuntimeUnitSpawn
{
    BattleRuntimeUnit unit;
    RoleComboState combo;
    BattleStatusRuntimeUnit status;
    BattleDamageRuntimeUnit damage;
    BattleMovementAgentState movement;
    std::optional<BattleActionPlanSeed> actionPlan;

    BattleRuntimeUnitRecord makeRecord() &&;
};
```

In `src/battle/BattleRuntimeUnitSpawn.cpp`, add:

```cpp
BattleRuntimeUnitRecord BattleRuntimeUnitSpawn::makeRecord() &&
{
    BattleRuntimeUnitRecord record;
    record.core = std::move(unit);
    record.combo = std::move(combo);
    record.status = std::move(status);
    record.damage = std::move(damage);
    record.movement = std::move(movement);
    record.deathEffects = { .id = record.core.id };
    record.rescue = {
        record.core.id,
        sumAlwaysEffectCharges(record.combo, EffectType::ForcePullProtect),
        sumAlwaysEffectCharges(record.combo, EffectType::ForcePullExecute),
    };
    if (actionPlan)
    {
        record.setActionPlan(std::move(*actionPlan));
    }
    return record;
}
```

- [ ] **Step 4: Dual-populate records while side stores still exist**

In `appendRuntimeUnit`, create the record before moving spawn fields into side stores:

```cpp
auto record = BattleRuntimeUnitSpawn(spawn).makeRecord();
```

If copying `BattleRuntimeUnitSpawn` is not viable, move the existing side-store push logic behind local copies and end with:

```cpp
runtime.unitRecords.append(std::move(record));
```

Keep the current side-store writes in this task. This is a temporary branch-local bridge so every step can compile; the final cleanup task removes the side stores.

- [ ] **Step 5: Reserve record capacity during runtime creation**

In `buildRuntimeFromSpawns`, add:

```cpp
runtime.unitRecords.reserve(spawns.size());
```

- [ ] **Step 6: Run focused tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][runtime_session][ownership]"
.\x64\Debug\kys_tests.exe "[battle][core][ownership]"
```

Expected: focused ownership tests pass.

- [ ] **Step 7: Commit**

Run:

```powershell
git add src/battle/BattleRuntimeUnitSpawn.h src/battle/BattleRuntimeUnitSpawn.cpp src/battle/BattleRuntimeSession.cpp tests/BattleInitializationUnitTests.cpp tests/BattleFrameRunnerRuntimeUnitTests.cpp
git commit -m "refactor: seed runtime unit records from spawns"
```

Expected: commit succeeds.

---

## Task 4: Move Action Unit Ownership Into Records

**Files:**
- Modify: `src/battle/BattleRuntimeActions.h`
- Modify: `src/battle/BattleRuntimeUnits.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleCastSystemUnitTests.cpp`
- Modify: `tests/BattleRuntimeScenarioTestHelpers.h`

- [ ] **Step 1: Add failing action ownership test**

Add:

```cpp
TEST_CASE("BattleRuntimeUnitRecord_ActionOwnershipReplacesRuntimeActionMaps", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    BattleRuntimeUnit unit;
    unit.id = 0;
    unit.alive = true;

    BattleActionPlanSeed seed;
    seed.unitId = 0;
    appendRuntimeUnit(state, makeRuntimeUnitSpawn(std::move(unit), RoleComboState{}, seed));

    auto& record = state.unitRecords.require(0);
    REQUIRE(record.actionPlan() != nullptr);
    CHECK(record.actionPlan()->unitId == 0);

    BattlePendingCastAction pending;
    pending.targetUnitId = 1;
    record.setPendingCast(pending);
    record.markUltimateCaster();

    REQUIRE(record.pendingCast() != nullptr);
    CHECK(record.pendingCast()->unitId == 0);
    CHECK(record.isUltimateCaster());

    record.clearActionOwners();

    CHECK(record.pendingCast() == nullptr);
    CHECK_FALSE(record.isUltimateCaster());
}
```

- [ ] **Step 2: Replace direct action map access in gameplay code**

Search:

```powershell
rg -n "state\.action\.(planSeed|pendingCast|hasPendingCast|pendingCastCount|hasUltimateCaster|ultimateCasterCount|setPlanSeed|clearPlanSeeds)|unitHandle\.action\(\)" src tests
```

For gameplay code in `src/battle/BattleCore.cpp`, replace:

```cpp
const auto* seed = state.action.planSeed(unitId);
```

with:

```cpp
const auto* seed = state.unitRecords.require(unitId).actionPlan();
```

Replace:

```cpp
auto* pendingCast = unitHandle.action().pendingCast();
unitHandle.action().setPendingCast(action);
unitHandle.action().clearPendingCast();
unitHandle.action().markUltimateCaster();
unitHandle.action().clearUltimateCaster();
unitHandle.action().clearOwners();
```

with:

```cpp
auto* pendingCast = unit.pendingCast();
unit.setPendingCast(action);
unit.clearPendingCast();
unit.markUltimateCaster();
unit.clearUltimateCaster();
unit.clearActionOwners();
```

where `unit` is a `BattleRuntimeUnitRecord&`. If the surrounding code currently has only `unitId`, use:

```cpp
auto& unit = state.unitRecords.require(unitId);
```

- [ ] **Step 3: Add container count helpers for tests and diagnostics**

In `BattleRuntimeUnitRecords`, add:

```cpp
std::size_t pendingCastCount() const
{
    return static_cast<std::size_t>(std::ranges::count_if(
        records_,
        [](const BattleRuntimeUnitRecord& record)
        {
            return record.pendingCast() != nullptr;
        }));
}

std::size_t ultimateCasterCount() const
{
    return static_cast<std::size_t>(std::ranges::count_if(
        records_,
        [](const BattleRuntimeUnitRecord& record)
        {
            return record.isUltimateCaster();
        }));
}
```

Replace test assertions:

```cpp
CHECK(state.action.pendingCastCount() == 0);
CHECK(state.action.ultimateCasterCount() == 0);
```

with:

```cpp
CHECK(state.unitRecords.pendingCastCount() == 0);
CHECK(state.unitRecords.ultimateCasterCount() == 0);
```

- [ ] **Step 4: Remove per-unit maps from `BattleRuntimeActions`**

Delete from `BattleRuntimeActions`:

```cpp
std::map<int, BattleActionPlanSeed> planSeeds_;
std::map<int, BattlePendingCastAction> pendingCasts_;
std::set<int> ultimateCasterUnitIds_;
```

Delete methods that only mutate those maps:

```cpp
planSeed
setPlanSeed
clearPlanSeeds
pendingCast
hasPendingCast
pendingCastCount
hasUltimateCaster
ultimateCasterCount
pendingCastMutable
setPendingCast
clearPendingCast
markUltimateCaster
clearUltimateCaster
isUltimateCaster
clearActionOwners
BattleRuntimeUnitActionView
```

Keep action config fields:

```cpp
BattleCastConfig castConfig;
BattleCastGeometry castGeometry;
BattleActionRulesConfig actionRules;
std::vector<int> castFrames;
int actionRecoveryFrames = 0;
int dashRecoveryFrames = 0;
double blinkWeakTargetDefWeight = 0.0;
int strengthenedMeleeOperationCountThreshold = 0;
int projectileBounceRange = 0;
```

- [ ] **Step 5: Run action-focused tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core][ownership]"
.\x64\Debug\kys_tests.exe "[battle][cast]"
.\x64\Debug\kys_tests.exe "[battle][action_commit]"
```

Expected: action/cast tests pass.

- [ ] **Step 6: Commit**

Run:

```powershell
git add src/battle/BattleRuntimeActions.h src/battle/BattleRuntimeUnits.h src/battle/BattleCore.cpp src/battle/BattleRuntimeUnitSpawn.cpp tests
git commit -m "refactor: move runtime action ownership into unit records"
```

Expected: commit succeeds.

---

## Task 5: Move Movement Agent Ownership Into Records

**Files:**
- Modify: `src/battle/BattleTypes.h`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
- Modify: `tests/BattleRuntimeScenarioTestHelpers.h`

- [ ] **Step 1: Add failing movement ownership test**

Add:

```cpp
TEST_CASE("BattleRuntimeUnitRecord_MovementAgentLivesOnRecord", "[battle][core][movement][ownership]")
{
    BattleRuntimeState state;
    BattleRuntimeUnit unit;
    unit.id = 1;
    unit.alive = true;
    unit.motion.position = { 12.0f, 18.0f, 0.0f };

    appendRuntimeUnit(state, makeRuntimeUnitSpawn(std::move(unit), RoleComboState{}));

    auto& record = state.unitRecords.require(1);
    record.movement.targetId = 0;
    record.movement.assignedSlot = 2;

    CHECK(record.movement.active);
    CHECK(record.movement.physics.position.x == 12.0f);
    CHECK(record.movement.targetId == 0);
    CHECK(record.movement.assignedSlot == 2);
}
```

- [ ] **Step 2: Replace movement agent lookups in gameplay code**

Search:

```powershell
rg -n "movement\.agents|requireMappedById\(state\.movement\.agents|requireMappedById\(runtime\.movement\.agents" src tests
```

In `src/battle/BattleCore.cpp`, replace:

```cpp
auto& agent = requireMappedById(state.movement.agents, unit.id);
```

with:

```cpp
auto& agent = state.unitRecords.require(unit.id).movement;
```

Replace:

```cpp
requireMappedById(state.movement.agents, unitId).physics
```

with:

```cpp
state.unitRecords.require(unitId).movement.physics
```

In loops over units, prefer:

```cpp
for (auto& unit : state.unitRecords.all())
{
    auto& agent = unit.movement;
}
```

or:

```cpp
for (const auto& unit : state.unitRecords.live())
{
    const auto& agent = unit.movement;
}
```

- [ ] **Step 3: Update movement preparation**

Replace `prepareMovementAgents` with:

```cpp
void prepareMovementAgents(BattleRuntimeState& state)
{
    for (auto& unit : state.unitRecords.all())
    {
        auto& agent = unit.movement;
        agent.active = unit.core.alive || needsCorpsePhysics(unit.core);
        if (!unit.core.alive)
        {
            state.movement.movementReservations.erase(unit.id());
        }
    }

    for (auto it = state.movement.movementReservations.begin(); it != state.movement.movementReservations.end();)
    {
        const auto& unit = state.unitRecords.require(it->first);
        if (!unit.core.alive)
        {
            it = state.movement.movementReservations.erase(it);
            continue;
        }
        ++it;
    }
}
```

- [ ] **Step 4: Remove `BattleMovementState::agents`**

In `src/battle/BattleTypes.h`, delete:

```cpp
std::map<int, BattleMovementAgentState> agents;
```

Update tests from:

```cpp
state.movement.agents.at(1).physics.velocity
```

to:

```cpp
state.unitRecords.require(1).movement.physics.velocity
```

- [ ] **Step 5: Run focused movement tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core][movement]"
.\x64\Debug\kys_tests.exe "[battle][runtime_session][ownership]"
```

Expected: movement tests pass; no `movement.agents` matches remain.

- [ ] **Step 6: Commit**

Run:

```powershell
git add src/battle/BattleTypes.h src/battle/BattleRuntimeUnitSpawn.cpp src/battle/BattleCore.cpp tests
git commit -m "refactor: move movement agents into unit records"
```

Expected: commit succeeds.

---

## Task 6: Move Status Ownership Into Records

**Files:**
- Modify: `src/battle/BattleStatusSystem.h`
- Modify: `src/battle/BattleStatusSystem.cpp`
- Modify: `src/battle/BattleRuntimeUnits.h`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Add record status domain methods**

In `BattleRuntimeUnitRecord`, add:

```cpp
void clearFrozen()
{
    status.effects.frozenTimer = 0;
    status.effects.frozenMaxTimer = 0;
}

void setMpBlockFrames(int frames)
{
    status.effects.mpBlockTimer = frames;
}

void addTempAttackBuff(int attackBonus, int durationFrames)
{
    status.effects.tempAttackBuffs.push_back({
        attackBonus,
        durationFrames,
    });
}

BattleStatusUnitState statusDamageState() const
{
    return makeBattleStatusUnitState(status, core);
}

void writeStatusDamageResult(const BattleStatusUnitState& unit)
{
    writeBattleStatusRuntimeUnit(status, unit);
}
```

- [ ] **Step 2: Add failing status ownership test**

Add:

```cpp
TEST_CASE("BattleRuntimeUnitRecord_StatusDomainMethodsMutateOwnedStatus", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 2;
    record.core.vitals.hp = 50;
    record.core.vitals.maxHp = 100;
    record.status.id = 2;
    record.status.effects.frozenTimer = 5;
    record.status.effects.frozenMaxTimer = 8;

    record.clearFrozen();
    record.setMpBlockFrames(3);
    record.addTempAttackBuff(7, 11);

    CHECK(record.status.effects.frozenTimer == 0);
    CHECK(record.status.effects.frozenMaxTimer == 0);
    CHECK(record.status.effects.mpBlockTimer == 3);
    REQUIRE(record.status.effects.tempAttackBuffs.size() == 1);
    CHECK(record.status.effects.tempAttackBuffs.front().attackBonus == 7);
    CHECK(record.status.effects.tempAttackBuffs.front().remainingFrames == 11);
}
```

- [ ] **Step 3: Change `BattleStatusSystem` to tick records**

In `BattleStatusSystem.h`, replace:

```cpp
BattleStatusTickResult tick(BattleUnitStore& units, std::vector<BattleStatusRuntimeUnit>& statuses) const;
```

with:

```cpp
BattleStatusTickResult tick(BattleRuntimeUnits& units) const;
```

Keep the single-unit helper only if tests still need it:

```cpp
BattleStatusTickResult tick(BattleRuntimeUnitRecord& unit) const;
```

In `BattleStatusSystem.cpp`, make the multi-unit tick:

```cpp
BattleStatusTickResult BattleStatusSystem::tick(BattleRuntimeUnits& units) const
{
    BattleStatusTickResult result;
    for (auto& unit : units.all())
    {
        auto unitResult = tick(unit);
        result.events.insert(result.events.end(), unitResult.events.begin(), unitResult.events.end());
    }
    return result;
}
```

The single-unit tick should mutate `unit.core` and `unit.status` directly instead of looking up a status row by id.

- [ ] **Step 4: Replace `status.units` reads and writes**

Search:

```powershell
rg -n "status\.units|requireById\(state\.status\.units|ensureById\(state\.status\.units|writeBattleStatusRuntimeUnit" src tests
```

Replace gameplay code:

```cpp
auto statusTick = BattleStatusSystem(state.status.config).tick(state.unitStore, state.status.units);
```

with:

```cpp
auto statusTick = BattleStatusSystem(state.status.config).tick(state.unitRecords);
```

Replace:

```cpp
auto& status = requireById(state.status.units, unitId);
```

with:

```cpp
auto& status = state.unitRecords.require(unitId).status;
```

Prefer domain methods where the operation has meaning:

```cpp
state.unitRecords.require(unitId).clearFrozen();
state.unitRecords.require(unitId).setMpBlockFrames(frames);
state.unitRecords.require(unitId).writeStatusDamageResult(result.defenderStatus);
```

- [ ] **Step 5: Remove `BattleRuntimeState::StatusState::units`**

Delete:

```cpp
std::vector<BattleStatusRuntimeUnit> units;
```

from `BattleRuntimeState::StatusState`.

- [ ] **Step 6: Run status tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][status]"
.\x64\Debug\kys_tests.exe "[battle][core][ownership]"
.\x64\Debug\kys_tests.exe "[battle][frame_runner]"
```

Expected: status and frame runner tests pass; no `status.units` matches remain.

- [ ] **Step 7: Commit**

Run:

```powershell
git add src/battle/BattleStatusSystem.h src/battle/BattleStatusSystem.cpp src/battle/BattleRuntimeUnits.h src/battle/BattleRuntimeUnitSpawn.cpp src/battle/BattleCore.cpp tests
git commit -m "refactor: move runtime status into unit records"
```

Expected: commit succeeds.

---

## Task 7: Move Combo Ownership Into Records

**Files:**
- Modify: `src/battle/BattleRuntimeUnits.h`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `src/battle/BattleTeamEffectSystem.h`
- Modify: `src/battle/BattleTeamEffectSystem.cpp`
- Modify: tests under `tests/`

- [ ] **Step 1: Add combo domain methods that already have clear meaning**

In `BattleRuntimeUnitRecord`, add:

```cpp
void clearPendingSkillHeal()
{
    combo.onSkillTeamHealPending = false;
}

int sumAlways(EffectType type) const
{
    return sumAlwaysEffectValue(combo, type);
}

int maxAlways(EffectType type) const
{
    return maxAlwaysEffectValue(combo, type);
}

bool hasAlways(EffectType type) const
{
    return firstAlwaysEffect(combo, type) != nullptr;
}

const AppliedEffectInstance* firstAlways(EffectType type) const
{
    return firstAlwaysEffect(combo, type);
}

BattleDamageModifierState damageModifiers() const
{
    return makeBattleDamageModifierState(&combo);
}

void applyComboEffect(const AppliedEffectInstance& effect, int comboId)
{
    KysChess::ChessBattleEffects::applyEffect(combo, effect, comboId);
}
```

- [ ] **Step 2: Add failing combo ownership test**

Add:

```cpp
TEST_CASE("BattleRuntimeUnitRecord_ComboDomainMethodsUseOwnedCombo", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 1;
    record.combo.onSkillTeamHealPending = true;
    record.combo.appliedEffects.push_back({
        .type = EffectType::MPRecoveryBonus,
        .trigger = Trigger::Always,
        .value = 25,
        .sourceComboId = -1,
    });

    CHECK(record.sumAlways(EffectType::MPRecoveryBonus) == 25);
    CHECK(record.hasAlways(EffectType::MPRecoveryBonus));

    record.clearPendingSkillHeal();

    CHECK_FALSE(record.combo.onSkillTeamHealPending);
}
```

- [ ] **Step 3: Replace combo map lookups**

Search:

```powershell
rg -n "combo\.units|requireMappedById\(state\.combo\.units|requireMappedById\(runtime\.combo\.units|state\.combo\.units|runtime\.combo\.units" src tests
```

Replace:

```cpp
auto& combo = requireMappedById(state.combo.units, unitId);
```

with:

```cpp
auto& combo = state.unitRecords.require(unitId).combo;
```

Prefer record methods for meaningful operations:

```cpp
state.unitRecords.require(deadUnitId).clearPendingSkillHeal();
const auto* forceRanged = state.unitRecords.require(unitId).firstAlways(EffectType::ForceRangedAttack);
auto modifiers = state.unitRecords.require(unitId).damageModifiers();
```

- [ ] **Step 4: Change team effect APIs to accept records**

In `BattleTeamEffectSystem.h`, replace:

```cpp
std::vector<BattleTeamEffectEvent> applyTeamMp(BattleUnitStore& units,
                                               const std::vector<BattleStatusRuntimeUnit>& statuses,
                                               const std::map<int, RoleComboState>& combos,
                                               int sourceUnitId,
                                               int amount) const;
```

with:

```cpp
std::vector<BattleTeamEffectEvent> applyTeamMp(BattleRuntimeUnits& units,
                                               int sourceUnitId,
                                               int amount) const;
```

In the implementation, read:

```cpp
if (target.status.effects.mpBlockTimer > 0)
{
    continue;
}
const int bonusPct = target.sumAlways(EffectType::MPRecoveryBonus);
```

instead of receiving separate status/combo collections.

- [ ] **Step 5: Remove `BattleRuntimeState::ComboTriggerState`**

Delete:

```cpp
struct ComboTriggerState
{
    std::map<int, RoleComboState> units;
} combo;
```

- [ ] **Step 6: Run combo and team effect tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][combo]"
.\x64\Debug\kys_tests.exe "[battle][team_effect]"
.\x64\Debug\kys_tests.exe "[battle][core][ownership]"
```

Expected: combo and team effect tests pass; no `combo.units` matches remain.

- [ ] **Step 7: Commit**

Run:

```powershell
git add src/battle/BattleRuntimeUnits.h src/battle/BattleRuntimeUnitSpawn.cpp src/battle/BattleCore.cpp src/battle/BattleRuntimeSession.cpp src/battle/BattleTeamEffectSystem.h src/battle/BattleTeamEffectSystem.cpp tests
git commit -m "refactor: move combo runtime state into unit records"
```

Expected: commit succeeds.

---

## Task 8: Move Damage Runtime Ownership Into Records

**Files:**
- Modify: `src/battle/BattleRuntimeUnits.h`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleDamageSystem.h`
- Modify: `src/battle/BattleDamageSystem.cpp`
- Modify: tests under `tests/`

- [ ] **Step 1: Add damage domain methods**

In `BattleRuntimeUnitRecord`, add:

```cpp
BattleDamageUnitState damageState() const
{
    auto unit = makeBattleDamageUnitState(core, &damage);
    unit.mpBlocked = status.effects.mpBlockTimer > 0;
    unit.mpRecoveryBonusPct = sumAlways(EffectType::MPRecoveryBonus);
    return unit;
}

void writeDamageResult(const BattleDamageUnitState& unit)
{
    writeBattleDamageRuntimeUnit(damage, unit);
}
```

- [ ] **Step 2: Add failing damage ownership test**

Add:

```cpp
TEST_CASE("BattleRuntimeUnitRecord_DamageStateComposesOwnedFacts", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 3;
    record.core.alive = true;
    record.core.vitals.hp = 40;
    record.core.vitals.maxHp = 100;
    record.core.vitals.mp = 5;
    record.core.vitals.maxMp = 20;
    record.damage.id = 3;
    record.damage.blockFirstHitsRemaining = 2;
    record.status.effects.mpBlockTimer = 4;
    record.combo.appliedEffects.push_back({
        .type = EffectType::MPRecoveryBonus,
        .trigger = Trigger::Always,
        .value = 30,
        .sourceComboId = -1,
    });

    const auto damage = record.damageState();

    CHECK(damage.id == 3);
    CHECK(damage.blockFirstHitsRemaining == 2);
    CHECK(damage.mpBlocked);
    CHECK(damage.mpRecoveryBonusPct == 30);
}
```

- [ ] **Step 3: Replace damage extras lookups**

Search:

```powershell
rg -n "damage\.unitExtras|requireById\(state\.damage\.unitExtras|writeBattleDamageRuntimeUnit\(requireById" src tests
```

Replace:

```cpp
auto& damage = requireById(state.damage.unitExtras, unitId);
writeBattleDamageRuntimeUnit(requireById(state.damage.unitExtras, unit.id), unit);
```

with:

```cpp
auto& damage = state.unitRecords.require(unitId).damage;
state.unitRecords.require(unit.id).writeDamageResult(unit);
```

Replace transaction input composition:

```cpp
makeBattleDamageUnitState(state.unitStore.requireUnit(unitId), &requireById(state.damage.unitExtras, unitId))
```

with:

```cpp
state.unitRecords.require(unitId).damageState()
```

- [ ] **Step 4: Remove `BattleRuntimeState::DamageState::unitExtras`**

Delete:

```cpp
std::vector<BattleDamageRuntimeUnit> unitExtras;
```

Keep:

```cpp
bool sortPendingDamageByDefenderMagnitude = false;
std::map<int, BattleDamagePresentationStyle> presentationStylesByDefender;
```

- [ ] **Step 5: Run damage tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][damage]"
.\x64\Debug\kys_tests.exe "[battle][core][ownership]"
.\x64\Debug\kys_tests.exe "[battle][frame_runner]"
```

Expected: damage tests pass; no `damage.unitExtras` matches remain.

- [ ] **Step 6: Commit**

Run:

```powershell
git add src/battle/BattleRuntimeUnits.h src/battle/BattleRuntimeUnitSpawn.cpp src/battle/BattleCore.cpp src/battle/BattleDamageSystem.h src/battle/BattleDamageSystem.cpp tests
git commit -m "refactor: move damage runtime state into unit records"
```

Expected: commit succeeds.

---

## Task 9: Move Death Effect And Rescue Unit Ownership Into Records

**Files:**
- Modify: `src/battle/BattleDeathEffectSystem.h`
- Modify: `src/battle/BattleDeathEffectSystem.cpp`
- Modify: `src/battle/BattleRuntimeUnits.h`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `src/battle/BattleRescueRepositionSystem.h`
- Modify: `src/battle/BattleRescueRepositionSystem.cpp`
- Modify: tests under `tests/`

- [ ] **Step 1: Add record methods for death extras and rescue counters**

In `BattleRuntimeUnitRecord`, add:

```cpp
void transferDeathAppliedEffect(const AppliedEffectInstance& effect)
{
    deathEffects.appliedEffects.push_back(effect);
}

int forcePullProtectRemaining() const
{
    return rescue.forcePullProtectRemaining;
}

int forcePullExecuteRemaining() const
{
    return rescue.forcePullExecuteRemaining;
}

void clearForcePullProtect()
{
    rescue.forcePullProtectRemaining = 0;
}

void applyRescueCounterDelta(const BattleRescueCounterDelta& delta)
{
    assert(delta.unitId == id());
    rescue.forcePullProtectRemaining = std::max(
        0,
        rescue.forcePullProtectRemaining + delta.protectRemainingDelta);
    rescue.forcePullExecuteRemaining = std::max(
        0,
        rescue.forcePullExecuteRemaining + delta.executeRemainingDelta);
}
```

- [ ] **Step 2: Add failing rescue/death ownership test**

Add:

```cpp
TEST_CASE("BattleRuntimeUnitRecord_DeathAndRescueFactsLiveOnRecord", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 5;
    record.deathEffects.id = 5;
    record.rescue.unitId = 5;
    record.rescue.forcePullProtectRemaining = 2;
    record.rescue.forcePullExecuteRemaining = 3;

    AppliedEffectInstance effect;
    effect.sourceComboId = 11;
    record.transferDeathAppliedEffect(effect);

    BattleRescueCounterDelta delta;
    delta.unitId = 5;
    delta.protectRemainingDelta = -1;
    delta.executeRemainingDelta = 2;
    record.applyRescueCounterDelta(delta);

    REQUIRE(record.deathEffects.appliedEffects.size() == 1);
    CHECK(record.deathEffects.appliedEffects.front().sourceComboId == 11);
    CHECK(record.forcePullProtectRemaining() == 1);
    CHECK(record.forcePullExecuteRemaining() == 5);

    record.clearForcePullProtect();
    CHECK(record.forcePullProtectRemaining() == 0);
}
```

- [ ] **Step 3: Change death effect store shape**

In `BattleDeathEffectSystem.h`, replace:

```cpp
struct BattleDeathEffectStore
{
    std::vector<BattleDeathEffectExtras> units;
    std::set<int> regularSynergyComboIds;
};
```

with:

```cpp
struct BattleDeathEffectStore
{
    std::set<int> regularSynergyComboIds;
};
```

Change death effect system APIs to accept records:

```cpp
std::vector<BattleDeathEffectEvent> applyAllyDeathEffects(BattleRuntimeUnits& units,
                                                          BattleDeathEffectStore& effects,
                                                          int deadUnitId) const;
```

Inside the implementation, replace death extra lookups with:

```cpp
auto& extras = units.require(unitId).deathEffects;
```

- [ ] **Step 4: Replace rescue side-store lookups**

Search:

```powershell
rg -n "rescue\.units|requireRescueRuntime|BattleRuntimeUnitRescueView|forcePullProtectRemaining|forcePullExecuteRemaining" src tests
```

Replace:

```cpp
requireRescueRuntime(state, unitId).forcePullProtectRemaining
```

with:

```cpp
state.unitRecords.require(unitId).forcePullProtectRemaining()
```

Replace counter mutation:

```cpp
state.units().require(unitId).rescue().applyCounterDelta(delta);
```

with:

```cpp
state.unitRecords.require(unitId).applyRescueCounterDelta(delta);
```

- [ ] **Step 5: Remove rescue and death unit side stores**

Delete:

```cpp
std::vector<BattleDeathEffectExtras> units;
std::vector<RescueUnitRuntime> units;
```

from runtime state and death effect store. Keep rescue cells/config and death global synergy metadata.

- [ ] **Step 6: Run rescue and death tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][rescue]"
.\x64\Debug\kys_tests.exe "[battle][death]"
.\x64\Debug\kys_tests.exe "[battle][core][ownership]"
```

Expected: rescue/death tests pass; no `rescue.units` or `deathEffects.store.units` matches remain.

- [ ] **Step 7: Commit**

Run:

```powershell
git add src/battle/BattleDeathEffectSystem.h src/battle/BattleDeathEffectSystem.cpp src/battle/BattleRuntimeUnits.h src/battle/BattleRuntimeUnitSpawn.cpp src/battle/BattleCore.cpp src/battle/BattleRuntimeSession.cpp src/battle/BattleRescueRepositionSystem.h src/battle/BattleRescueRepositionSystem.cpp tests
git commit -m "refactor: move death and rescue unit state into records"
```

Expected: commit succeeds.

---

## Task 10: Replace `BattleUnitStore` Runtime Usage With Record Container

**Files:**
- Modify: `src/battle/BattleUnitStore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleAttackSystem.h`
- Modify: `src/battle/BattleAttackSystem.cpp`
- Modify: `src/battle/BattleCastSystem.h`
- Modify: `src/battle/BattleCastSystem.cpp`
- Modify: `src/battle/BattleTeamEffectSystem.h`
- Modify: `src/battle/BattleTeamEffectSystem.cpp`
- Modify: tests under `tests/`

- [ ] **Step 1: Add core helpers to `BattleRuntimeUnitRecords`**

In `BattleRuntimeUnitRecords`, add:

```cpp
BattleRuntimeUnit* findCore(int unitId)
{
    auto* record = find(unitId);
    return record == nullptr ? nullptr : &record->core;
}

const BattleRuntimeUnit* findCore(int unitId) const
{
    const auto* record = find(unitId);
    return record == nullptr ? nullptr : &record->core;
}

BattleRuntimeUnit& requireCore(int unitId)
{
    return require(unitId).core;
}

const BattleRuntimeUnit& requireCore(int unitId) const
{
    return require(unitId).core;
}

void writeDamageUnit(const BattleDamageUnitState& source)
{
    auto& unit = requireCore(source.id);
    unit.alive = source.alive;
    unit.vitals = source.vitals;
    unit.stats.attack = source.attack;
    unit.invincible = source.invincible;
    unit.shield = source.shield;
}

void setPosition(int unitId, Pointf position, const BattleGridTransform& gridTransform)
{
    auto& unit = requireCore(unitId);
    unit.motion.position = position;
    unit.grid = gridTransform.toGrid(position);
}

void setMotion(int unitId, Pointf position, Pointf velocity, Pointf acceleration, const BattleGridTransform& gridTransform)
{
    auto& unit = requireCore(unitId);
    unit.motion.position = position;
    unit.motion.velocity = velocity;
    unit.motion.acceleration = acceleration;
    unit.grid = gridTransform.toGrid(position);
}
```

- [ ] **Step 2: Migrate runtime state from `unitStore` to `unitRecords`**

In `BattleRuntimeState`, replace:

```cpp
BattleUnitStore unitStore;
BattleRuntimeUnitRecords unitRecords;
```

with:

```cpp
BattleRuntimeUnitRecords units;
BattleGridTransform gridTransform;
```

Then replace runtime usage:

```cpp
state.unitStore.requireUnit(unitId)
state.unitStore.findUnit(unitId)
state.unitStore.units
state.unitRecords
state.unitStore.gridTransform
```

with:

```cpp
state.units.requireCore(unitId)
state.units.findCore(unitId)
state.units.all()
state.units
state.gridTransform
```

Do not leave both `unitStore` and `units` in final runtime state.

- [ ] **Step 3: Update pure system signatures**

Change `BattleAttackSystem` methods from:

```cpp
std::vector<BattleAttackEvent> tick(BattleAttackState& world, const BattleUnitStore& units) const;
```

to:

```cpp
std::vector<BattleAttackEvent> tick(BattleAttackState& world, const BattleRuntimeUnitRecords& units) const;
```

Change private helpers similarly. Replace loops:

```cpp
for (const auto& unit : units.units)
```

with:

```cpp
for (const auto& record : units.live())
{
    const auto& unit = record.core;
}
```

Change `BattleActionCommitSystem::commit` from:

```cpp
const BattleUnitStore& units
```

to:

```cpp
const BattleRuntimeUnitRecords& units
```

and replace unit lookup with `units.requireCore(unitId)`.

- [ ] **Step 4: Update free helper functions**

Change declarations in `BattleUnitStore.h`:

```cpp
int findNearestEnemyUnitId(const BattleRuntimeUnitRecords& units, int sourceUnitId);
int findFarthestEnemyUnitId(const BattleRuntimeUnitRecords& units, int sourceUnitId);
```

Update implementations to iterate records:

```cpp
for (const auto& candidateRecord : units.live())
{
    const auto& candidate = candidateRecord.core;
}
```

Keep `makeBattleMovementPlanUnit(const BattleRuntimeUnit& unit, double moveSpeedDivisor)` unchanged because it is a pure conversion from core unit state.

- [ ] **Step 5: Remove `BattleUnitStore` struct**

Delete:

```cpp
struct BattleUnitStore
{
    BattleGridTransform gridTransform;
    std::vector<BattleRuntimeUnit> units;

    BattleRuntimeUnit* findUnit(int unitId);
    const BattleRuntimeUnit* findUnit(int unitId) const;
    BattleRuntimeUnit& requireUnit(int unitId);
    const BattleRuntimeUnit& requireUnit(int unitId) const;
    void writeDamageUnit(const BattleDamageUnitState& source);
    void setPosition(int unitId, Pointf position);
    void setMotion(int unitId, Pointf position, Pointf velocity, Pointf acceleration);
};
```

Delete its method implementations from `BattleCore.cpp`.

- [ ] **Step 6: Run system tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][attack]"
.\x64\Debug\kys_tests.exe "[battle][cast]"
.\x64\Debug\kys_tests.exe "[battle][team_effect]"
.\x64\Debug\kys_tests.exe "[battle][core][ownership]"
```

Expected: tests pass; no runtime usage of `BattleUnitStore` remains. The file `BattleUnitStore.h` may still contain `BattleRuntimeUnit`, `BattleGridTransform`, and pure helper declarations.

- [ ] **Step 7: Commit**

Run:

```powershell
git add src/battle/BattleUnitStore.h src/battle/BattleCore.cpp src/battle/BattleAttackSystem.h src/battle/BattleAttackSystem.cpp src/battle/BattleCastSystem.h src/battle/BattleCastSystem.cpp src/battle/BattleTeamEffectSystem.h src/battle/BattleTeamEffectSystem.cpp tests
git commit -m "refactor: make runtime units the core unit store"
```

Expected: commit succeeds.

---

## Task 11: Rename Staging Container And Delete Unit Views

**Files:**
- Modify: `src/battle/BattleRuntimeUnits.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: all remaining call sites under `src/` and `tests/`

- [ ] **Step 1: Rename `BattleRuntimeUnitRecords` to final `BattleRuntimeUnits`**

In `BattleRuntimeUnits.h`, rename:

```cpp
class BattleRuntimeUnitRecords
```

to:

```cpp
class BattleRuntimeUnits
```

Update `BattleRuntimeState`:

```cpp
BattleRuntimeUnits units;
```

Replace all:

```cpp
state.unitRecords
runtime.unitRecords
BattleRuntimeUnitRecords
```

with:

```cpp
state.units
runtime.units
BattleRuntimeUnits
```

- [ ] **Step 2: Delete old view classes**

Remove these classes entirely:

```cpp
BattleRuntimeUnitComboView
BattleRuntimeUnitStatusView
BattleRuntimeUnitDeathEffectsView
BattleRuntimeUnitRescueView
BattleRuntimeUnitHandle
```

Remove the old view-based `BattleRuntimeUnits` constructor:

```cpp
explicit BattleRuntimeUnits(BattleRuntimeState& state);
```

Remove:

```cpp
BattleRuntimeState::units()
BattleRuntimeUnits::makeHandle
BattleRuntimeUnits::state
BattleRuntimeUnits::require returning handle
```

Final call sites must use:

```cpp
state.units.require(unitId)
state.units.live()
state.units.dead()
state.units.all()
```

- [ ] **Step 3: Run the deletion audit**

Run:

```powershell
rg -n "BattleRuntimeUnitHandle|BattleRuntimeUnit[A-Za-z]+View|state\.units\(\)|runtime\.units\(\)|unitRecords" src tests
```

Expected: no matches.

- [ ] **Step 4: Run focused tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core][ownership]"
.\x64\Debug\kys_tests.exe "[battle][runtime_session][ownership]"
.\x64\Debug\kys_tests.exe "[battle][core][movement]"
```

Expected: focused tests pass.

- [ ] **Step 5: Commit**

Run:

```powershell
git add src tests
git commit -m "refactor: replace runtime unit views with owned records"
```

Expected: commit succeeds.

---

## Task 12: Final Side-Store Removal Audit

**Files:**
- Modify: any remaining source or test files reported by the audit commands.

- [ ] **Step 1: Run final ownership audit**

Run:

```powershell
rg -n "status\.units|combo\.units|damage\.unitExtras|movement\.agents|deathEffects\.store\.units|rescue\.units|BattleRuntimeUnitHandle|BattleRuntimeUnit[A-Za-z]+View|state\.unitStore|runtime\.unitStore|unitStore\.units|planSeeds_|pendingCasts_|ultimateCasterUnitIds_|state\.units\(\)" src tests
```

Expected: no matches.

- [ ] **Step 2: Run global-store sanity audit**

Run:

```powershell
rg -n "BattleAttackState|BattleProjectileFollowUpContext|BattleNextFrameQueues|movementReservations|terrainCells|BattleRuntimeRandom|BattleRuntimeActions action|BattleRuntimeState::RescueState|BattleRuntimeState::DamageState" src\\battle
```

Expected: matches remain for global systems. Confirm none of these contain per-unit owned rows except global maps keyed by projectiles, presentation styles, reservations, or queues.

- [ ] **Step 3: Run all battle tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle]"
```

Expected: all battle-tagged tests pass.

- [ ] **Step 4: Run the full test executable**

Run:

```powershell
.\x64\Debug\kys_tests.exe
```

Expected: all tests pass.

- [ ] **Step 5: Commit final cleanup**

Run:

```powershell
git add src tests
git commit -m "test: verify runtime unit record ownership migration"
```

Expected: commit succeeds.

---

## Migration Rules

- Do not add fake wrappers like `unit.status()` if the method only returns `status`. Use `unit.status` directly until there is a real domain verb.
- Do add domain verbs for operations that previously required a unit id and a side store: `clearPendingCast`, `markUltimateCaster`, `clearFrozen`, `damageState`, `writeDamageResult`, `applyRescueCounterDelta`.
- Do not move projectile attack instances, queues, terrain, movement reservations, random state, or config into unit records.
- Do not erase unit records during battle. Death flips `unit.core.alive` and movement participation flips `unit.movement.active`.
- Do not store `all/live/dead` ranges across appending records. Ranges are frame-local.
- Prefer iterating `state.units.live()` for per-unit systems. Use `state.units.require(id)` for projectile and command resolution paths that naturally start from an id.

## Final Done Criteria

- `BattleRuntimeUnitRecord` owns core, combo, status, damage, movement, death extras, rescue counters, and action ownership.
- `BattleRuntimeState` contains no horizontal per-unit stores for those facts.
- `BattleRuntimeActions` contains only action config/rules and no per-unit maps/sets.
- `BattleMovementState` contains no `agents` map.
- `BattleDeathEffectStore` contains no per-unit `units` vector.
- `BattleRuntimeUnitHandle` and all unit view classes are deleted.
- Runtime code uses `state.units.require(id)` and `state.units.live()/dead()/all()`.
- Projectiles, configs, queues, random state, terrain, reservations, rescue cells, and result state remain global.
- Final audit command in Task 12 returns no migration-artifact matches.
- `.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal` succeeds.
- `.\x64\Debug\kys_tests.exe` succeeds.

## Self-Review

- Spec coverage: The plan migrates all per-unit side stores into `BattleRuntimeUnitRecord` and explicitly leaves projectiles, configs, queues, terrain, reservations, random state, and other global systems outside records.
- Placeholder scan: The plan contains concrete target types, commands, file paths, replacement patterns, and verification steps.
- Type consistency: The staging name is `BattleRuntimeUnitRecords`; the final owner name is `BattleRuntimeUnits`. The final per-unit type is consistently `BattleRuntimeUnitRecord`.
