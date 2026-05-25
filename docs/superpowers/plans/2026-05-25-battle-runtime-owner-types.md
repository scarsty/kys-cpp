# Battle Runtime Owner Types Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace public battle-runtime bags with unit-centered handles and small owner types whose APIs encode phase ownership and domain transitions.

**Architecture:** Keep `BattleRuntimeState` as the aggregate during this refactor, but stop making every operation reassemble unit state from parallel `unitId` stores. Add a `BattleRuntimeUnits` facade that is bound to the runtime aggregate and yields per-unit handles for unit-centered phases, then add verb-only owners for next-frame queues and phase command routing. Status, combo, and death-effect changes move through unit subviews where the operation is genuinely per-unit; unrelated horizontal stores stay exposed until a real owner verb exists.

**Tech Stack:** C++20/C++23, Catch2, PowerShell, ripgrep, Visual Studio/MSBuild via `.github/build-command.ps1`.

---

## Scope And Direction

This is a staged ownership refactor, not a rewrite of battle runtime. Each task must preserve behavior and land with focused tests.

The target model is:

- Per-unit phase loops iterate C++23 view pipelines from `state.units().live()` / `state.units().all()` instead of repeatedly calling `runtimeUnit(state, unitId)`. `BattleRuntimeUnits` is a cheap view returned by `BattleRuntimeState::units()`, so callers do not pass `state` back into a member.
- Event-driven code such as projectile hits may still resolve by `unitId`, but it should resolve attacker/defender once at the top of the operation and pass handles downward.
- Runtime carry-over queues are owned by `BattleNextFrameQueues`.
- Frame queue routing is explicit. Hide only the one-shot queues that have clear ownership semantics; leave unrelated frame output fields exposed until there is a real verb for them.
- Command reduction receives phase-specific sinks so queue routing is visible at the type level.
- Action mutation goes through `unit.action()` for per-unit ownership operations. `BattleRuntimeActions` owns the containers; the unit action view supplies the unit id.
- Status, combo/death-effect, action, and rescue mutations should move toward unit-handle subviews such as `unit.status().clearFrozen()`, `unit.combo().clearPendingSkillHeal()`, `unit.action().clearOwners()`, and `unit.rescue().applyCounterDelta(...)`. Do not create standalone wrappers that only rename raw store access.
- Movement and damage wrappers are not introduced unless they can expose real operations such as `prepareForFrame`, `makePlanInput`, `commitTick`, `requireAgentPhysics`, `clearReservation`, `makeTransactionInput`, `writeTransactionResult`, `presentationStyle`, or `requireUnitExtra`; do not wrap them with `stateForSystem()` style getters. Movement may become a cleaner unit subview later if dead-unit agent erasure is removed.

Do not introduce a giant `BattleRuntime` class in this plan. A huge object with methods that can mutate everything would hide the same dependency problem behind `this`.

## File Structure

- Create `src/battle/BattleRuntimeQueues.h`
  - Own next-frame attack and damage queues with private vectors and drain/queue methods.

- Create `src/battle/BattleRuntimeUnits.h`
  - Provide `BattleRuntimeUnitHandle` and unit subviews. `all()`, `live()`, and `dead()` use C++23 ranges/views over `BattleUnitStore::units`; storage migration is out of scope for the first pass.

- Create `src/battle/BattleRuntimeActions.h`
  - Own pending casts and ultimate casters with private containers and transition methods. Keep rules/config outside this class.

- Do not create `src/battle/BattleRuntimeStatus.h` in this plan.
  - Add status verbs to the unit handle only where they remove a concrete per-unit mutation. Leave `state.status.units` exposed for system-wide ticking/setup until a real owner operation exists.

- Do not create `src/battle/BattleRuntimeCombos.h` in this plan.
  - Add combo verbs to the unit handle only where the operation is naturally per-unit.

- Do not create `src/battle/BattleRuntimeDeathEffects.h` in this plan.
  - Add death-effect verbs to the unit handle only for per-unit extras. Keep the broader `BattleDeathEffectStore` exposed until the death-effect lifecycle has a proper owner verb.

- Do not create `src/battle/BattleRuntimeRescue.h` in this plan.
  - Add rescue verbs to the unit handle for force-pull runtime counters only. Keep rescue cells/config/counterattack settings on the existing state until a broader rescue lifecycle owner exists.

- Defer `src/battle/BattleRuntimeMovement.h`
  - Only create it once the API can be expressed as verbs such as `prepareForFrame`, `makePlanInput`, `commitTick`, `requireAgentPhysics`, and `clearReservation`.

- Defer `src/battle/BattleRuntimeDamage.h`
  - Only create it once the API can be expressed as verbs such as `makeTransactionInput`, `writeTransactionResult`, and `presentationStyle`.

- Modify `src/battle/BattleCore.h`
  - Replace public nested state bags with owner types as each task lands.

- Modify `src/battle/BattleCore.cpp`
  - Use phase sinks and owner APIs inside `BattleFrameRunner`.

- Modify `src/battle/BattleRuntimeSession.cpp`
  - Initialize new owner types from runtime spawns.

- Modify tests:
  - `tests/BattleCoreUnitTests.cpp`
  - `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
  - `tests/BattleInitializationUnitTests.cpp`
  - add focused owner unit tests only when the owner has meaningful behavior independent of `BattleFrameRunner`.

## Task 1: Introduce BattleRuntimeUnits Handle Facade

**Files:**
- Create: `src/battle/BattleRuntimeUnits.h`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add a unit-handle centered regression**

In `tests/BattleCoreUnitTests.cpp`, keep the existing death cleanup regression, but update it to assert through `state.units().require(...)` once the facade exists:

```cpp
TEST_CASE("BattleFrameRunner_AdvanceFrame_DeadUnitActionCleanupClearsAllActionOwners", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.unitStore.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
    };
    
    auto deadBefore = state.units().require(1);
    deadBefore.core().animation.cooldown = 4;
    deadBefore.core().animation.actFrame = 2;
    deadBefore.core().animation.actType = 3;
    deadBefore.core().operationType = BattleOperationType::Melee;
    deadBefore.core().haveAction = true;
    state.action.pendingCasts.emplace(1, BattlePendingCastAction{});
    state.ultimateCasters.insert(1);
    state.deathEffects.store.units = { { 0 }, { 1 } };
    queuePendingDamage(state, lethalDamageInput(0, 1));

    runBattleFrame(state);

    auto dead = state.units().require(1);
    CHECK(dead.core().animation.cooldown == 0);
    CHECK(dead.core().animation.actFrame == 0);
    CHECK(dead.core().animation.actType == -1);
    CHECK(dead.core().operationType == BattleOperationType::None);
    CHECK_FALSE(dead.core().haveAction);
    CHECK(state.action.pendingCasts.count(1) == 0);
    CHECK(state.ultimateCasters.count(1) == 0);
}
```

This test should fail to compile before `BattleRuntimeUnits` exists.

- [ ] **Step 2: Create `BattleRuntimeUnits.h`**

Create `src/battle/BattleRuntimeUnits.h`:

```cpp
#pragma once

#include "BattleDamageSystem.h"
#include "BattleDeathEffectSystem.h"
#include "BattleStatusSystem.h"
#include "BattleTypes.h"
#include "BattleUnitStore.h"
#include "../ChessBattleEffects.h"
#include "../Find.h"

#include <algorithm>
#include <ranges>
#include <utility>

namespace KysChess::Battle
{

struct BattleRuntimeState;

class BattleRuntimeUnitStatusView
{
public:
    explicit BattleRuntimeUnitStatusView(BattleStatusRuntimeUnit& status)
        : status_(&status)
    {
    }

    BattleStatusEffectState& effects() const { return status_->effects; }
    bool frozen() const { return status_->effects.frozenTimer > 0; }

    void clearFrozen() const
    {
        status_->effects.frozenTimer = 0;
        status_->effects.frozenMaxTimer = 0;
    }

    void setFrozenForTest(int timer, int maxTimer) const
    {
        status_->effects.frozenTimer = timer;
        status_->effects.frozenMaxTimer = maxTimer;
    }

private:
    BattleStatusRuntimeUnit* status_{};
};

class BattleRuntimeUnitComboView
{
public:
    explicit BattleRuntimeUnitComboView(RoleComboState& combo)
        : combo_(&combo)
    {
    }

    void clearPendingSkillHeal() const
    {
        combo_->onSkillTeamHealPending = false;
    }

    int sumAlways(EffectType type) const
    {
        return sumAlwaysEffectValue(*combo_, type);
    }

    int maxAlways(EffectType type) const
    {
        return maxAlwaysEffectValue(*combo_, type);
    }

    bool hasAlways(EffectType type) const
    {
        return firstAlwaysEffect(*combo_, type) != nullptr;
    }

    void applyEffect(const AppliedEffectInstance& effect, int comboId) const
    {
        KysChess::ChessBattleEffects::applyEffect(*combo_, effect, comboId);
    }

    bool hasAppliedEffect(EffectType type, int sourceComboId) const
    {
        return std::any_of(
            combo_->appliedEffects.begin(),
            combo_->appliedEffects.end(),
            [type, sourceComboId](const AppliedEffectInstance& effect)
            {
                return effect.type == type && effect.sourceComboId == sourceComboId;
            });
    }

    const AppliedEffectInstance* firstAlways(EffectType type) const
    {
        return firstAlwaysEffect(*combo_, type);
    }

private:
    RoleComboState* combo_{};
};

class BattleRuntimeUnitDeathEffectsView
{
public:
    explicit BattleRuntimeUnitDeathEffectsView(BattleDeathEffectExtras& extras)
        : extras_(&extras)
    {
    }

    void transferAppliedEffect(const AppliedEffectInstance& effect) const
    {
        extras_->appliedEffects.push_back(effect);
    }

private:
    BattleDeathEffectExtras* extras_{};
};

class BattleRuntimeUnitRescueView
{
public:
    explicit BattleRuntimeUnitRescueView(BattleRuntimeState& state, int unitId)
        : state_(&state),
          unitId_(unitId)
    {
    }

    int forcePullProtectRemaining() const;
    int forcePullExecuteRemaining() const;
    void clearForcePullProtect() const;
    void applyCounterDelta(const BattleRescueCounterDelta& delta) const;

private:
    BattleRuntimeState* state_{};
    int unitId_{};
};

class BattleRuntimeUnitHandle
{
public:
    BattleRuntimeUnitHandle(BattleRuntimeUnit& core,
                            BattleStatusRuntimeUnit* status,
                            RoleComboState* combo,
                            BattleDamageRuntimeUnit* damage,
                            BattleMovementAgentState* movement,
                            BattleDeathEffectExtras* deathEffects)
        : core_(&core),
          status_(status),
          combo_(combo),
          damage_(damage),
          movement_(movement),
          deathEffects_(deathEffects)
    {
    }

    int id() const { return core_->id; }
    bool alive() const { return core_->alive; }

    BattleRuntimeUnit& core() const { return *core_; }
    BattleRuntimeUnitStatusView status() const { return BattleRuntimeUnitStatusView(*status_); }
    BattleRuntimeUnitComboView combo() const { return BattleRuntimeUnitComboView(*combo_); }
    BattleDamageRuntimeUnit& damage() const { return *damage_; }
    BattleMovementAgentState& movement() const { return *movement_; }
    BattleRuntimeUnitDeathEffectsView deathEffects() const { return BattleRuntimeUnitDeathEffectsView(*deathEffects_); }
    BattleRuntimeUnitRescueView rescue() const;

private:
    BattleRuntimeUnit* core_{};
    BattleStatusRuntimeUnit* status_{};
    RoleComboState* combo_{};
    BattleDamageRuntimeUnit* damage_{};
    BattleMovementAgentState* movement_{};
    BattleDeathEffectExtras* deathEffects_{};
};

class BattleRuntimeUnits
{
public:
    explicit BattleRuntimeUnits(BattleRuntimeState& state)
        : state_(&state)
    {
    }

    BattleRuntimeUnitHandle require(int unitId) const;
    auto all() const;
    auto live() const;
    auto dead() const;

private:
    BattleRuntimeState& state() const;
    BattleRuntimeUnitHandle makeHandle(BattleRuntimeUnit& unit) const;

    BattleRuntimeState* state_{};
};

}  // namespace KysChess::Battle
```

If including `BattleCore.h` from this header creates a cycle, keep only declarations in the header and implement methods in `BattleCore.cpp` where `BattleRuntimeState` is complete.

- [ ] **Step 3: Add `BattleRuntimeUnits` to runtime state**

In `src/battle/BattleCore.h`, include `BattleRuntimeUnits.h` and add a view factory near the top of `BattleRuntimeState`:

```cpp
BattleRuntimeUnits units();
BattleUnitStore unitStore;
```

Do not store `BattleRuntimeUnits` as a member. It is a cheap view over `BattleRuntimeState`, and storing it would create self-pointer copy/move hazards because runtime states are returned by value in tests and session setup.

Do not remove `unitStore`, `status`, `combo`, `damage`, `movement`, or `deathEffects` yet. This task creates a facade over existing horizontal stores.

- [ ] **Step 4: Implement facade methods in `BattleCore.cpp`**

Implement `units()` and the view methods where `BattleRuntimeState` is complete. Put the range method definitions inline after the `BattleRuntimeState` definition in `BattleCore.h` because their return type is deduced from the view pipeline. Keep non-range helpers such as `require(...)` in `BattleCore.cpp` only if callers do not need their deduced return type.

```cpp
BattleRuntimeUnits BattleRuntimeState::units()
{
    return BattleRuntimeUnits(*this);
}

BattleRuntimeState& BattleRuntimeUnits::state() const
{
    assert(state_ != nullptr);
    return *state_;
}

BattleRuntimeUnitHandle BattleRuntimeUnits::require(int unitId) const
{
    auto& runtime = state();
    return makeHandle(runtime.unitStore.requireUnit(unitId));
}

BattleRuntimeUnitHandle BattleRuntimeUnits::makeHandle(BattleRuntimeUnit& core) const
{
    auto& runtime = state();
    const int unitId = core.id;
    return {
        core,
        &requireById(runtime.status.units, unitId),
        &requireMappedById(runtime.combo.units, unitId),
        &requireById(runtime.damage.unitExtras, unitId),
        &requireMappedById(runtime.movement.agents, unitId),
        &requireById(runtime.deathEffects.store.units, unitId),
    };
}

auto BattleRuntimeUnits::all() const
{
    auto* runtime = &state();
    return runtime->unitStore.units
        | std::views::transform(
            [runtime](BattleRuntimeUnit& unit)
            {
                return BattleRuntimeUnits(*runtime).makeHandle(unit);
            });
}

auto BattleRuntimeUnits::live() const
{
    auto* runtime = &state();
    return runtime->unitStore.units
        | std::views::filter([](const BattleRuntimeUnit& unit) { return unit.alive; })
        | std::views::transform(
            [runtime](BattleRuntimeUnit& unit)
            {
                return BattleRuntimeUnits(*runtime).makeHandle(unit);
            });
}

auto BattleRuntimeUnits::dead() const
{
    auto* runtime = &state();
    return runtime->unitStore.units
        | std::views::filter([](const BattleRuntimeUnit& unit) { return !unit.alive; })
        | std::views::transform(
            [runtime](BattleRuntimeUnit& unit)
            {
                return BattleRuntimeUnits(*runtime).makeHandle(unit);
            });
}
```

Do not capture `this` in the view pipeline because `state.units()` returns a temporary facade. Capture `BattleRuntimeState*` by value as shown above. Use `assert` rather than defensive fallback. Missing rows mean the horizontal stores are inconsistent.

Add this invariant comment near `BattleRuntimeUnitHandle`:

```cpp
// Runtime unit membership is fixed after clone initialization. Handles and unit
// ranges are frame-local views over stable unit/status/damage/combo/death rows.
// Movement agent access is phase-sensitive because dead-unit cleanup may erase
// movement agents for non-corpse units.
```

If a phase needs a snapshot of "units live at phase start" while it may change `alive` during iteration, materialize ids explicitly:

```cpp
auto liveUnitIds = state.units().live()
    | std::views::transform([](BattleRuntimeUnitHandle unit) { return unit.id(); })
    | std::ranges::to<std::vector>();
```

Do not materialize by default. Normal phase loops should consume the lazy view directly.

- [ ] **Step 5: Verify handle view construction needs no rebuild step**

Do not add a `rebuildIndexes()` call in runtime setup or tests. `BattleRuntimeUnits` is a transient view that reads the current stores directly:

```cpp
auto unit = state.units().require(unitId);
```

If lookup cost becomes a problem later, add an index to `BattleUnitStore` itself or make index maintenance part of the canonical store. Do not add a second index cache to the view.

- [ ] **Step 6: Convert one per-unit phase to handles**

In `src/battle/BattleCore.cpp`, convert `cancelDeadRuntimeActions(...)` from iterating raw units:

```cpp
for (const auto& unit : state.unitStore.units)
```

to:

```cpp
for (auto unit : state.units().dead())
{
    cancelRuntimeAction(state, unit.id());
}
```

This establishes the top-level loop pattern without spreading ad hoc `runtimeUnit(state, id)` calls.

- [ ] **Step 7: Verify unit facade usage is contained**

Run:

```powershell
rg -n "state\\.units\\.(require|all|live|dead)|runtimeUnit\\(state" src\\battle tests
```

Expected:

- `state.units().dead()` appears in `cancelDeadRuntimeActions(...)`.
- `state.units().require(...)` appears only in the new ownership regression or one small converted helper.
- No new free function named `runtimeUnit(state, unitId)` exists.

- [ ] **Step 8: Run tests and commit**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core]"
```

Commit:

```powershell
git add src\battle\BattleRuntimeUnits.h src\battle\BattleCore.h src\battle\BattleCore.cpp tests\BattleCoreUnitTests.cpp
git commit -m "refactor: add battle runtime unit handles"
```

## Task 2: Introduce BattleNextFrameQueues

**Files:**
- Create: `src/battle/BattleRuntimeQueues.h`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleRuntimeScenarioUnitTests.cpp`

- [ ] **Step 1: Write a focused next-frame ownership regression**

In `tests/BattleCoreUnitTests.cpp`, keep or add this test near the existing phase ownership tests:

```cpp
TEST_CASE("BattleFrameRunner_AdvanceFrame_QueuesHitGeneratedProjectilesForNextFrame", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 140, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.combo.units[0].triggeredEffects.push_back(
        triggeredEffect(KysChess::EffectType::NearbyTrackingProjectiles, KysChess::Trigger::OnHit, 80, 100));
    state.combo.units[0].triggeredEffects.back().value2 = 45;

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 480;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.visualEffectId = 44;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    state.attacks.attacks.push_back(projectile);

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    auto nextAttacks = state.nextFrame.queuedAttacksForTest();
    REQUIRE(nextAttacks.size() == 2);
    CHECK(nextAttacks[0].initial.attackerUnitId == 0);
    CHECK(nextAttacks[0].initial.preferredTargetUnitId == 1);
    CHECK(nextAttacks[0].initial.suppressNearbyTrackingProjectileProc);
    CHECK(nextAttacks[1].initial.attackerUnitId == 0);
    CHECK(nextAttacks[1].initial.preferredTargetUnitId == 2);
    CHECK(nextAttacks[1].initial.suppressNearbyTrackingProjectileProc);
}
```

This test intentionally references `state.nextFrame`; it must fail to compile before the owner type exists.

- [ ] **Step 2: Create `BattleRuntimeQueues.h`**

Create `src/battle/BattleRuntimeQueues.h`:

```cpp
#pragma once

#include "BattleAttackSystem.h"
#include "BattleDamageQueue.h"

#include <vector>

namespace KysChess::Battle
{

class BattleNextFrameQueues
{
public:
    void queueAttack(BattleAttackSpawnRequest request)
    {
        attackSpawns_.push_back(std::move(request));
    }

    void queueDamage(BattlePendingDamageIntent damage)
    {
        pendingDamage_.push_back(std::move(damage));
    }

    std::vector<BattleAttackSpawnRequest> drainAttacks()
    {
        return std::exchange(attackSpawns_, {});
    }

    std::vector<BattlePendingDamageIntent> drainDamage()
    {
        return std::exchange(pendingDamage_, {});
    }

    const std::vector<BattleAttackSpawnRequest>& queuedAttacksForTest() const
    {
        return attackSpawns_;
    }

    const std::vector<BattlePendingDamageIntent>& queuedDamageForTest() const
    {
        return pendingDamage_;
    }

private:
    std::vector<BattleAttackSpawnRequest> attackSpawns_;
    std::vector<BattlePendingDamageIntent> pendingDamage_;
};

}  // namespace KysChess::Battle
```

Add `#include <utility>` if the compiler requires it for `std::exchange`.

- [ ] **Step 3: Replace runtime public queues**

In `src/battle/BattleCore.h`, include the new header and replace:

```cpp
std::vector<BattlePendingDamageIntent> pendingDamage;
```

inside `DamageState` with no member. Also replace:

```cpp
std::vector<BattleAttackSpawnRequest> pendingAttackSpawns;
```

with:

```cpp
BattleNextFrameQueues nextFrame;
```

If `nextFrame` belongs better near the bottom of `BattleRuntimeState`, place it near `projectileFollowUps`.

- [ ] **Step 4: Drain next-frame queues into frame context**

In `src/battle/BattleCore.cpp`, update `makeBattleFrameContext(...)`:

```cpp
BattleFrameContext makeBattleFrameContext(BattleRuntimeState& state)
{
    BattleFrameContext frame;
    frame.frameStartMotion = makeUnitMotionSnapshot(state.unitStore);
    frame.attackSpawns = state.nextFrame.drainAttacks();
    frame.pendingDamage = state.nextFrame.drainDamage();
    return frame;
}
```

- [ ] **Step 5: Update next-frame writes**

Replace:

```cpp
state.pendingAttackSpawns.push_back(request);
```

with:

```cpp
state.nextFrame.queueAttack(std::move(request));
```

Replace call sites that pass `state.pendingAttackSpawns` into reducers with a small local adapter in Task 2. For this task, a minimal helper is acceptable:

```cpp
void appendNextFrameAttack(BattleRuntimeState& state, BattleAttackSpawnRequest request)
{
    state.nextFrame.queueAttack(std::move(request));
}
```

For damage carry-over, replace `state.damage.pendingDamage` with `state.nextFrame.queueDamage(...)` or `state.nextFrame.drainDamage()`.

- [ ] **Step 6: Update tests**

Replace test reads of:

```cpp
state.pendingAttackSpawns
state.damage.pendingDamage
```

with:

```cpp
state.nextFrame.queuedAttacksForTest()
state.nextFrame.queuedDamageForTest()
```

Do not expose mutable vectors for tests.

- [ ] **Step 7: Verify no public next-frame queues remain**

Run:

```powershell
rg -n "pendingAttackSpawns|damage\.pendingDamage" src tests
```

Expected: no matches except older plan documents.

- [ ] **Step 8: Run tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core]"
.\x64\Debug\kys_tests.exe "[battle][scenario]"
```

Expected: build succeeds and selected tests pass.

- [ ] **Step 9: Commit**

```powershell
git add src\battle\BattleRuntimeQueues.h src\battle\BattleCore.h src\battle\BattleCore.cpp tests\BattleCoreUnitTests.cpp tests\BattleRuntimeScenarioUnitTests.cpp
git commit -m "refactor: own battle next-frame queues"
```

## Task 3: Add Phase Command Sinks

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Keep the post-hit projectile regression**

Ensure `BattleFrameRunner_AdvanceFrame_QueuesHitGeneratedProjectilesForNextFrame` from Task 1 still exists and asserts queued attacks via `state.nextFrame.queuedAttacksForTest()`.

- [ ] **Step 2: Add sink structs near reducer implementation**

In `src/battle/BattleCore.cpp`, before `reduceFrameGameplayCommand(...)`, add:

```cpp
struct BattleCommandSinks
{
    std::vector<BattleAttackSpawnRequest>& attackSpawns;
    std::vector<BattlePendingDamageIntent>& pendingDamage;
    std::vector<BattleGameplayEvent>& gameplayEvents;
    std::vector<BattleLogEvent>& logEvents;
    std::vector<BattleVisualEvent>& visualEvents;
};
```

Also add a small adapter for next-frame attacks:

```cpp
struct BattleNextFrameAttackSink
{
    BattleNextFrameQueues& nextFrame;

    void push_back(BattleAttackSpawnRequest request)
    {
        nextFrame.queueAttack(std::move(request));
    }
};
```

If a reference member cannot bind cleanly in the existing reducer, make `BattleCommandSinks` store lambdas for queueing attack and damage instead. Prefer the concrete sink first.

- [ ] **Step 3: Replace long reducer parameter list**

Change:

```cpp
bool reduceFrameGameplayCommand(
    BattleRuntimeState& state,
    const BattleGameplayCommand& command,
    std::vector<int>& attackSoundIds,
    std::vector<BattleFrameRumbleEvent>& rumbles,
    std::vector<BattleGameplayCommand>& pending,
    std::vector<BattleAttackSpawnRequest>& attackSpawns,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

to:

```cpp
bool reduceFrameGameplayCommand(
    BattleRuntimeState& state,
    const BattleGameplayCommand& command,
    std::vector<int>& attackSoundIds,
    std::vector<BattleFrameRumbleEvent>& rumbles,
    std::vector<BattleGameplayCommand>& pending,
    BattleCommandSinks sinks)
```

Inside the function, replace direct references:

```cpp
attackSpawns
pendingDamage
gameplayEvents
logEvents
visualEvents
```

with:

```cpp
sinks.attackSpawns
sinks.pendingDamage
sinks.gameplayEvents
sinks.logEvents
sinks.visualEvents
```

- [ ] **Step 4: Replace reducer implementation signature**

Change:

```cpp
void reduceFrameGameplayCommandsImpl(
    BattleRuntimeState& state,
    std::vector<BattleGameplayCommand>& commands,
    std::vector<int>& attackSoundIds,
    std::vector<BattleFrameRumbleEvent>& rumbles,
    std::vector<BattleAttackSpawnRequest>& attackSpawns,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

to:

```cpp
void reduceFrameGameplayCommandsImpl(
    BattleRuntimeState& state,
    std::vector<BattleGameplayCommand>& commands,
    std::vector<int>& attackSoundIds,
    std::vector<BattleFrameRumbleEvent>& rumbles,
    BattleCommandSinks sinks)
```

Pass `sinks` to `reduceFrameGameplayCommand(...)`.

- [ ] **Step 5: Add named sink factories**

Add:

```cpp
BattleCommandSinks currentFrameSinks(BattleFrameContext& frame)
{
    return {
        frame.attackSpawns,
        frame.pendingDamage,
        frame.gameplayEvents,
        frame.logEvents,
        frame.visualEvents,
    };
}

BattleCommandSinks afterAttackHitSinks(BattleRuntimeState& state, BattleFrameContext& frame)
{
    return {
        state.nextFrame.mutableAttacksForReducer(),
        frame.pendingDamage,
        frame.gameplayEvents,
        frame.logEvents,
        frame.visualEvents,
    };
}

BattleCommandSinks afterDamageLifecycleSinks(BattleRuntimeState& state, BattleFrameContext& frame)
{
    return {
        state.nextFrame.mutableAttacksForReducer(),
        state.nextFrame.mutableDamageForReducer(),
        frame.gameplayEvents,
        frame.logEvents,
        frame.visualEvents,
    };
}
```

To support this, add private-by-convention reducer accessors in `BattleNextFrameQueues`:

```cpp
std::vector<BattleAttackSpawnRequest>& mutableAttacksForReducer();
std::vector<BattlePendingDamageIntent>& mutableDamageForReducer();
```

Keep these methods non-public if `BattleRuntimeQueues.h` can declare a friend helper. If friend plumbing is too noisy, make them public with names ending in `ForReducer` so misuse is easy to spot in review.

- [ ] **Step 6: Update phase functions**

The four phase functions should become:

```cpp
void reduceCommandsBeforeMovement(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame.frameCommands,
        frame.attackSoundIds,
        frame.rumbles,
        currentFrameSinks(frame));
}

void reduceCommandsBeforeAttacks(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame.frameCommands,
        frame.attackSoundIds,
        frame.rumbles,
        currentFrameSinks(frame));
}

void reduceCommandsAfterAttackHits(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame.frameCommands,
        frame.attackSoundIds,
        frame.rumbles,
        afterAttackHitSinks(state, frame));
}

void reduceCommandsAfterDamageLifecycle(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame.frameCommands,
        frame.attackSoundIds,
        frame.rumbles,
        afterDamageLifecycleSinks(state, frame));
}
```

- [ ] **Step 7: Verify hidden routing is gone**

Run:

```powershell
rg -n "reduceFrameGameplayCommandsImpl\\(|mutableAttacksForReducer|mutableDamageForReducer|pendingAttackSpawns|damage\\.pendingDamage" src\battle tests
```

Expected:

- `reduceFrameGameplayCommandsImpl(...)` has one implementation and four phase-wrapper calls.
- `mutable*ForReducer` is used only in sink factories.
- no `pendingAttackSpawns` or `damage.pendingDamage` production matches.

- [ ] **Step 8: Run tests and commit**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core]"
```

Then commit:

```powershell
git add src\battle\BattleRuntimeQueues.h src\battle\BattleCore.cpp tests\BattleCoreUnitTests.cpp
git commit -m "refactor: route battle commands through phase sinks"
```

## Task 4: Encapsulate BattleFrameContext Queues Only

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Test: existing core tests

- [ ] **Step 1: Convert only queue fields to private storage**

In `src/battle/BattleCore.cpp`, replace:

```cpp
struct BattleFrameContext
{
    BattlePresentationFrame result;
    std::vector<BattleGameplayCommand> frameCommands;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
    std::vector<int> attackSoundIds;
    std::vector<BattleFrameRumbleEvent> rumbles;
    int blinkSoundCount{};
    std::vector<BattleAttackEvent> attackEvents;
    std::vector<BattleAreaProjectileFollowUp> areaProjectileFollowUps;
    std::vector<BattleFrameMpRestore> lateMpRestores;
    std::vector<BattleAttackSpawnRequest> attackSpawns;
    std::vector<BattlePendingDamageIntent> pendingDamage;
    std::vector<BattleFrameCastScopedComboEffects> castScopedComboEffects;
    UnitMotionSnapshotMap frameStartMotion;
};
```

with:

```cpp
class BattleFrameContext
{
public:
    static BattleFrameContext begin(BattleRuntimeState& state)
    {
        BattleFrameContext frame;
        frame.frameStartMotion_ = makeUnitMotionSnapshot(state.unitStore);
        frame.attackSpawns_ = state.nextFrame.drainAttacks();
        frame.pendingDamage_ = state.nextFrame.drainDamage();
        return frame;
    }

private:
    std::vector<BattleGameplayCommand> frameCommands_;
    std::vector<BattleAreaProjectileFollowUp> areaProjectileFollowUps_;
    std::vector<BattleFrameMpRestore> lateMpRestores_;
    std::vector<BattleAttackSpawnRequest> attackSpawns_;
    std::vector<BattlePendingDamageIntent> pendingDamage_;
    UnitMotionSnapshotMap frameStartMotion_;

public:
    BattlePresentationFrame result;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
    std::vector<int> attackSoundIds;
    std::vector<BattleFrameRumbleEvent> rumbles;
    int blinkSoundCount{};
    std::vector<BattleAttackEvent> attackEvents;
    std::vector<BattleFrameCastScopedComboEffects> castScopedComboEffects;
};
```

This will not compile until the queue/drain methods are added in the following steps.

- [ ] **Step 2: Add methods only for queues with ownership semantics**

Do not add a getter for every field in `BattleFrameContext`. Add methods only for fields whose lifetime or phase ownership is meaningful:

```cpp
std::vector<BattleAttackSpawnRequest>& currentFrameAttacks();
std::vector<BattlePendingDamageIntent>& currentFrameDamage();
void queueCommand(BattleGameplayCommand command);
std::vector<BattleGameplayCommand> drainCommands();
std::vector<BattleAttackSpawnRequest> drainCurrentFrameAttacks();
std::vector<BattleAreaProjectileFollowUp> drainAreaProjectileFollowUps();
std::vector<BattleFrameMpRestore> drainLateMpRestores();
const UnitMotionSnapshotMap& frameStartMotion() const;
```

Leave presentation accumulation such as `gameplayEvents`, `logEvents`, `visualEvents`, `attackSoundIds`, `rumbles`, and `result` as public fields for now. They need a separate presentation/output owner design; wrapping them with same-name getters would not improve ownership.

- [ ] **Step 3: Replace only queue field access**

Examples:

```cpp
frame.frameCommands
```

becomes:

```cpp
frame.queueCommand(command)
```

```cpp
frame.pendingDamage
```

becomes:

```cpp
frame.currentFrameDamage()
```

```cpp
frame.attackSpawns
```

becomes:

```cpp
frame.currentFrameAttacks()
```

Do not replace `frame.gameplayEvents`, `frame.logEvents`, `frame.visualEvents`, `frame.attackSoundIds`, `frame.rumbles`, or `frame.result` in this task.

- [ ] **Step 4: Add consume methods for one-shot queues**

Add:

```cpp
std::vector<BattleAttackSpawnRequest> drainCurrentFrameAttacks()
{
    return std::exchange(attackSpawns_, {});
}

std::vector<BattleGameplayCommand> drainCommands()
{
    return std::exchange(frameCommands_, {});
}

std::vector<BattleAreaProjectileFollowUp> drainAreaProjectileFollowUps()
{
    return std::exchange(areaProjectileFollowUps_, {});
}

std::vector<BattleFrameMpRestore> drainLateMpRestores()
{
    return std::exchange(lateMpRestores_, {});
}
```

Use these in `advanceAttacksAndResolveHits(...)`, `expandFrameDamageFollowUpCommands(...)`, and `applyLateFrameMpRestores(...)`.

- [ ] **Step 5: Update frame creation and consumption**

Remove `makeBattleFrameContext(...)` and use:

```cpp
auto frame = BattleFrameContext::begin(state);
```

Change `consumeBattleFrameContext(...)` to call accessors:

```cpp
BattlePresentationFrame consumeBattleFrameContext(BattleFrameContext&& frame)
{
    assert(frame.drainCommands().empty());
    return std::move(frame.result);
}
```

- [ ] **Step 6: Verify direct baggage access is gone**

Run:

```powershell
rg -n "frame\\.(frameCommands|attackSpawns|pendingDamage|areaProjectileFollowUps|lateMpRestores)" src\\battle\\BattleCore.cpp
```

Expected: no matches.

- [ ] **Step 7: Run tests and commit**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core]"
```

Commit:

```powershell
git add src\battle\BattleCore.cpp
git commit -m "refactor: encapsulate battle frame context"
```

## Task 5: Introduce BattleRuntimeActions And Unit Action View

**Files:**
- Create: `src/battle/BattleRuntimeActions.h`
- Modify: `src/battle/BattleRuntimeUnits.h`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Preserve action cleanup regression**

Ensure `tests/BattleCoreUnitTests.cpp` contains:

```cpp
TEST_CASE("BattleFrameRunner_AdvanceFrame_DeadUnitActionCleanupClearsAllActionOwners", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.unitStore.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
    };
    state.unitStore.requireUnit(1).animation.cooldown = 4;
    state.unitStore.requireUnit(1).animation.actFrame = 2;
    state.unitStore.requireUnit(1).animation.actType = 3;
    state.unitStore.requireUnit(1).operationType = BattleOperationType::Melee;
    state.unitStore.requireUnit(1).haveAction = true;
    state.deathEffects.store.units = { { 0 }, { 1 } };
    auto deadBefore = state.units().require(1);
    deadBefore.action().setPendingCastForTest(BattlePendingCastAction{});
    deadBefore.action().markUltimateCaster();
    queuePendingDamage(state, lethalDamageInput(0, 1));

    runBattleFrame(state);

    const auto& dead = state.unitStore.requireUnit(1);
    CHECK(dead.animation.cooldown == 0);
    CHECK(dead.animation.actFrame == 0);
    CHECK(dead.animation.actType == -1);
    CHECK(dead.operationType == BattleOperationType::None);
    CHECK_FALSE(dead.haveAction);
    auto deadAfter = state.units().require(1);
    CHECK_FALSE(deadAfter.action().hasPendingCast());
    CHECK_FALSE(deadAfter.action().isUltimateCaster());
}
```

This should fail to compile until `BattleRuntimeActions` and `unit.action()` exist.

- [ ] **Step 2: Create `BattleRuntimeActions.h`**

Create:

```cpp
#pragma once

#include "BattleCastSystem.h"

#include <map>
#include <set>
#include <vector>

namespace KysChess::Battle
{

struct BattleActionPlanSeed;
struct BattlePendingCastAction;

class BattleRuntimeActions
{
public:
    std::map<int, BattleActionPlanSeed>& planSeeds() { return planSeeds_; }
    const std::map<int, BattleActionPlanSeed>& planSeeds() const { return planSeeds_; }

private:
    friend class BattleRuntimeUnitActionView;

    BattlePendingCastAction* findPendingCast(int unitId)
    {
        auto it = pendingCasts_.find(unitId);
        return it == pendingCasts_.end() ? nullptr : &it->second;
    }

    void setPendingCast(int unitId, BattlePendingCastAction action)
    {
        pendingCasts_[unitId] = std::move(action);
    }

    void clearPendingCast(int unitId)
    {
        pendingCasts_.erase(unitId);
    }

    bool hasPendingCast(int unitId) const
    {
        return pendingCasts_.count(unitId) != 0;
    }

    void markUltimateCaster(int unitId)
    {
        ultimateCasters_.insert(unitId);
    }

    void clearUltimateCaster(int unitId)
    {
        ultimateCasters_.erase(unitId);
    }

    bool isUltimateCaster(int unitId) const
    {
        return ultimateCasters_.count(unitId) != 0;
    }

    void clearActionOwners(int unitId)
    {
        clearPendingCast(unitId);
        clearUltimateCaster(unitId);
    }

public:
    BattleCastConfig castConfig;
    BattleCastGeometry castGeometry;
    BattleActionRulesConfig actionRules;
    std::vector<int> castFrames;
    int actionRecoveryFrames = 0;
    int dashRecoveryFrames = 0;
    double blinkWeakTargetDefWeight = 0.0;
    int strengthenedMeleeOperationCountThreshold = 0;
    int projectileBounceRange = 0;

private:
    std::map<int, BattleActionPlanSeed> planSeeds_;
    std::map<int, BattlePendingCastAction> pendingCasts_;
    std::set<int> ultimateCasters_;
};

class BattleRuntimeUnitActionView
{
public:
    BattleRuntimeUnitActionView(BattleRuntimeActions& actions, int unitId)
        : actions_(&actions),
          unitId_(unitId)
    {
    }

    BattlePendingCastAction* pendingCast() const
    {
        return actions_->findPendingCast(unitId_);
    }

    void setPendingCast(BattlePendingCastAction action) const
    {
        actions_->setPendingCast(unitId_, std::move(action));
    }

    void clearPendingCast() const
    {
        actions_->clearPendingCast(unitId_);
    }

    bool hasPendingCast() const
    {
        return actions_->hasPendingCast(unitId_);
    }

    void markUltimateCaster() const
    {
        actions_->markUltimateCaster(unitId_);
    }

    void clearUltimateCaster() const
    {
        actions_->clearUltimateCaster(unitId_);
    }

    bool isUltimateCaster() const
    {
        return actions_->isUltimateCaster(unitId_);
    }

    void clearOwners() const
    {
        actions_->clearActionOwners(unitId_);
    }

    const BattleActionPlanSeed* planSeed() const
    {
        auto it = actions_->planSeeds_.find(unitId_);
        return it == actions_->planSeeds_.end() ? nullptr : &it->second;
    }

    void setPendingCastForTest(BattlePendingCastAction action) const
    {
        setPendingCast(std::move(action));
    }

private:
    BattleRuntimeActions* actions_{};
    int unitId_{};
};

}  // namespace KysChess::Battle
```

If forward declaration fails because `BattleActionPlanSeed` and `BattlePendingCastAction` are defined in `BattleCore.h`, keep the first implementation as a nested class in `BattleCore.h` instead. Extracting to a header can happen after those types are moved.

- [ ] **Step 3: Expose action ownership through the unit handle**

In `src/battle/BattleRuntimeUnits.h`, include `BattleRuntimeActions.h`, add a pointer to the action owner, and expose a per-unit view:

```cpp
class BattleRuntimeUnitHandle
{
public:
    BattleRuntimeUnitActionView action() const
    {
        return BattleRuntimeUnitActionView(*action_, id());
    }

private:
    BattleRuntimeActions* action_{};
};
```

Update the handle constructor and `BattleRuntimeUnits::makeHandle(...)` to pass `&runtime.action`. This keeps `BattleRuntimeActions` as the owner while making per-unit action mutations read as unit-owned operations.

- [ ] **Step 4: Replace `ActionState` fields**

In `src/battle/BattleCore.h`, replace:

```cpp
struct ActionState
{
    std::map<int, BattleActionPlanSeed> planSeeds;
    std::map<int, BattlePendingCastAction> pendingCasts;
    BattleCastConfig castConfig;
    BattleCastGeometry castGeometry;
    BattleActionRulesConfig actionRules;
    std::vector<int> castFrames;
    int actionRecoveryFrames = 0;
    int dashRecoveryFrames = 0;
    double blinkWeakTargetDefWeight = 0.0;
    int strengthenedMeleeOperationCountThreshold = 0;
    int projectileBounceRange = 0;
} action;

std::set<int> ultimateCasters;
```

with:

```cpp
BattleRuntimeActions action;
```

Keep config fields inside `BattleRuntimeActions` for this task.

- [ ] **Step 5: Update action call sites**

Replace:

```cpp
state.action.pendingCasts.find(unit.id)
state.action.pendingCasts.erase(unit.id)
state.action.pendingCasts[unit.id] = makePendingCastAction(castInput, cast)
state.ultimateCasters.erase(unit.id)
state.ultimateCasters.insert(unit.id)
```

with:

```cpp
unit.action().pendingCast()
unit.action().clearPendingCast()
unit.action().setPendingCast(makePendingCastAction(castInput, cast))
unit.action().clearUltimateCaster()
unit.action().markUltimateCaster()
unit.action().planSeed()
```

Update `cancelRuntimeAction(...)`:

```cpp
void cancelRuntimeAction(BattleRuntimeState& state, int unitId)
{
    auto unit = state.units().require(unitId);
    auto actionState = makeActionRuntimeState(unit.core());
    resetActionFrameState(actionState);
    commitActionFrameStateToRuntime(unit.core(), actionState);
    unit.action().clearOwners();
}
```

- [ ] **Step 6: Verify no split action owners remain**

Run:

```powershell
rg -n "pendingCasts|ultimateCasters" src\\battle tests
```

Expected:

- `pendingCasts_` and `ultimateCasters_` only appear inside `BattleRuntimeActions`.
- Production per-unit call sites use `unit.action().*` methods.
- Tests use `state.units().require(id).action().*` for seeded runtime units.

- [ ] **Step 7: Run tests and commit**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core]"
```

Commit:

```powershell
git add src\battle\BattleRuntimeActions.h src\battle\BattleRuntimeUnits.h src\battle\BattleCore.h src\battle\BattleCore.cpp src\battle\BattleRuntimeUnitSpawn.cpp tests\BattleCoreUnitTests.cpp
git commit -m "refactor: own battle action runtime state"
```

## Task 6: Move Status Mutations Onto Unit Handles

**Files:**
- Modify: `src/battle/BattleRuntimeUnits.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

This task does not create `BattleRuntimeStatus`. System-wide status ticking still needs `state.status.config` and `state.status.units`, so hiding those behind getters would be fake ownership. Only per-unit status operations move behind the unit handle.

- [ ] **Step 1: Preserve frozen ownership regression**

In `tests/BattleCoreUnitTests.cpp`, update the frozen/death cleanup regression to use the handle:

```cpp
auto deadBefore = state.units().require(1);
deadBefore.status().setFrozenForTest(5, 8);

runBattleFrame(state);

auto dead = state.units().require(1);
CHECK(dead.status().effects().frozenTimer == 0);
CHECK(dead.status().effects().frozenMaxTimer == 0);
```

This should fail to compile until `BattleRuntimeUnitStatusView` exists.

- [ ] **Step 2: Add status verbs to `BattleRuntimeUnitStatusView`**

In `src/battle/BattleRuntimeUnits.h`, ensure the status subview exposes only the per-unit operations used by production code:

```cpp
class BattleRuntimeUnitStatusView
{
public:
    explicit BattleRuntimeUnitStatusView(BattleStatusRuntimeUnit& status);

    BattleStatusEffectState& effects() const;
    bool frozen() const;
    bool mpBlocked() const;
    int mpRecoveryBonusPct(const BattleRuntimeUnitComboView& combo) const;
    void addTempAttackBuff(int attackBonus, int durationFrames) const;
    void writeDamageResult(const BattleStatusUnitState& unit) const;
    void clearFrozen() const;
    void setFrozenForTest(int timer, int maxTimer) const;

private:
    BattleStatusRuntimeUnit* status_{};
};
```

Keep `state.status.units` and `state.status.config` public for now. Do not add `rowsForSystem()`, `configure()`, or `tickAll()` wrappers in this task.

- [ ] **Step 3: Leave system-wide status state exposed**

Do not replace:

```cpp
struct StatusState
{
    BattleStatusSystemConfig config;
    std::vector<BattleStatusRuntimeUnit> units;
} status;
```

with a wrapper in this task. The status system still consumes the config and the full status row vector as a horizontal system input:

```cpp
BattleStatusSystem(state.status.config).tick(state.unitStore, state.status.units);
```

- [ ] **Step 4: Replace per-unit status reads and writes**

In `src/battle/BattleCore.cpp`, replace frozen, MP-block, temp buff, and damage-result status access:

```cpp
auto& status = requireById(state.status.units, unitId);
status.effects.frozenTimer = 0;
status.effects.frozenMaxTimer = 0;
requireById(state.status.units, unit.id).effects.frozenTimer > 0
requireById(state.status.units, command.unitId).effects.tempAttackBuffs.push_back(...)
writeBattleStatusRuntimeUnit(requireById(state.status.units, transaction.defender.id), transaction.defenderStatus)
```

with:

```cpp
unit.status().clearFrozen();
unit.status().frozen();
unit.status().addTempAttackBuff(command.attackBonus, command.durationFrames);
unit.status().writeDamageResult(transaction.defenderStatus);
```

When the caller already has a `BattleRuntimeUnitHandle`, do not re-resolve by id:

```cpp
unit.status().clearFrozen();
```

- [ ] **Step 5: Verify only meaningful status ownership moved**

Run:

```powershell
rg -n "clearFrozen|setFrozenForTest|rowsForSystem|BattleRuntimeStatus|status\\.require\\(|status\\.configure\\(|status\\.tickAll\\(" src\\battle tests
```

Expected:

- `unit.status().clearFrozen()` appears at death cleanup.
- `unit.status().frozen()` replaces direct frozen status reads in unit tick, cast input, and movement physics.
- `unit.status().addTempAttackBuff(...)` replaces direct temp attack buff mutation.
- `unit.status().writeDamageResult(...)` replaces direct damage-result status commit.
- `unit.status().setFrozenForTest(...)` appears only in tests.
- No `BattleRuntimeStatus` or `rowsForSystem()` wrapper exists.
- `state.status.units` and `state.status.config` may still appear where the status system needs the full horizontal store.

- [ ] **Step 6: Run tests and commit**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core]"
.\x64\Debug\kys_tests.exe "[battle][runtime]"
```

Commit:

```powershell
git add src\battle\BattleRuntimeUnits.h src\battle\BattleCore.cpp tests\BattleCoreUnitTests.cpp
git commit -m "refactor: move unit status mutations onto handles"
```

## Task 7: Move Combo And Death-Effect Per-Unit Mutations Onto Unit Handles

**Files:**
- Modify: `src/battle/BattleRuntimeUnits.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: tests around DeathAOE and anti-combo transfer

This task does not create `BattleRuntimeCombos` or `BattleRuntimeDeathEffects`. The broader combo map and death-effect store have lifecycle/setup responsibilities that are not solved by a getter wrapper. Move only the per-unit operations that are already clear.

- [ ] **Step 1: Preserve DeathAOE transfer regression**

In `tests/BattleCoreUnitTests.cpp`, keep broad death-effect setup on the existing store, then assert the per-unit transfer through the target handle:

```cpp
state.deathEffects.store.units = {
    { .id = 0 },
    { .id = 1, .comboIds = { 33 }, .appliedEffects = { antiComboDeathAoe } },
    { .id = 2, .comboIds = { 33 } },
};


auto deadBefore = state.units().require(1);
deadBefore.combo().applyEffect(antiComboDeathAoe, 33);

queuePendingDamage(state, lethalDamageInput(0, 1));
runBattleFrame(state);

auto target = state.units().require(2);
CHECK(target.combo().hasAppliedEffect(EffectType::DeathAOE, 33));
```

- [ ] **Step 2: Add combo and death-effect verbs to unit subviews**

In `src/battle/BattleRuntimeUnits.h`, ensure these methods exist:

```cpp
class BattleRuntimeUnitComboView
{
public:
    explicit BattleRuntimeUnitComboView(RoleComboState& combo);

    void clearPendingSkillHeal() const;
    int sumAlways(EffectType type) const;
    int maxAlways(EffectType type) const;
    bool hasAlways(EffectType type) const;
    void applyEffect(const AppliedEffectInstance& effect, int comboId) const;
    bool hasAppliedEffect(EffectType type, int sourceComboId) const;
    const AppliedEffectInstance* firstAlways(EffectType type) const;

private:
    RoleComboState* combo_{};
};

class BattleRuntimeUnitDeathEffectsView
{
public:
    explicit BattleRuntimeUnitDeathEffectsView(BattleDeathEffectExtras& extras);

    void transferAppliedEffect(const AppliedEffectInstance& effect) const;

private:
    BattleDeathEffectExtras* extras_{};
};
```

Do not add `rowsForSetup()`, `storeForSetup()`, `combo.require(id)`, or `deathEffects.require(id)` wrappers.

- [ ] **Step 3: Update DeathAOE lookup and anti-combo transfer**

Replace per-unit combo queries in frame tick, cast input, runtime skill bonuses, damage modifier assembly, and death consequences:

```cpp
auto comboIt = state.combo.units.find(unit.id);
firstAlwaysEffect(comboIt->second, EffectType::DashAttack)
sumAlwaysEffectValue(comboIt->second, EffectType::CDR)
maxAlwaysEffectValue(actionResult.combo, EffectType::PostSkillInvincFrames)
combo.onSkillTeamHealPending = false;
```

with handle/subview calls when a unit handle is already available or can be resolved once:

```cpp
unit.combo().hasAlways(EffectType::DashAttack)
unit.combo().sumAlways(EffectType::CDR)
unit.combo().maxAlways(EffectType::PostSkillInvincFrames)
unit.combo().clearPendingSkillHeal()
```

When the operation is already about a dead unit, resolve the handle once:

```cpp
auto dead = state.units().require(deadUnitId);
const auto* deathAoe = dead.combo().firstAlways(EffectType::DeathAOE);
```

For anti-combo transfer, resolve the target once and call both per-unit verbs:

```cpp
auto target = state.units().require(targetUnitId);
target.combo().applyEffect(effect, comboId);
target.deathEffects().transferAppliedEffect(effect);
```

If a code path still needs to iterate or configure the whole combo/death-effect store, leave the existing `state.combo.units` or `state.deathEffects.store` access in place. That remaining exposure is honest technical debt, not hidden ownership.

- [ ] **Step 4: Verify no fake combo/death wrappers were introduced**

Run:

```powershell
rg -n "BattleRuntimeCombos|BattleRuntimeDeathEffects|rowsForSetup|storeForSetup|combo\\.require\\(|deathEffects\\.require\\(" src\\battle tests
```

Expected: no matches.

Run:

```powershell
rg -n "unit\\.combo\\(\\)\\.(clearPendingSkillHeal|sumAlways|maxAlways|hasAlways|applyEffect|hasAppliedEffect|firstAlways)|unit\\.deathEffects\\(\\)\\.transferAppliedEffect|target\\.combo\\(\\)\\.applyEffect|target\\.deathEffects\\(\\)\\.transferAppliedEffect" src\\battle tests
```

Expected: matches only where the operation is per-unit.

- [ ] **Step 5: Run tests and commit**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core]"
.\x64\Debug\kys_tests.exe "[battle][initialization]"
```

Commit:

```powershell
git add src\battle\BattleRuntimeUnits.h src\battle\BattleCore.cpp tests
git commit -m "refactor: move unit combo mutations onto handles"
```

## Task 8: Move Rescue Per-Unit Counters Onto Unit Handles

**Files:**
- Modify: `src/battle/BattleRuntimeUnits.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: rescue-related tests

This task does not create `BattleRuntimeRescue`. Rescue cells, puller-team config, and counterattack config are still broader rescue-system state. Only the per-unit force-pull counters move behind `unit.rescue()`.

- [ ] **Step 1: Add rescue subview verbs**

In `src/battle/BattleRuntimeUnits.h`, add:

```cpp
class BattleRuntimeUnitRescueView
{
public:
    explicit BattleRuntimeUnitRescueView(BattleRuntimeState& state, int unitId);

    int forcePullProtectRemaining() const;
    int forcePullExecuteRemaining() const;
    void clearForcePullProtect() const;
    void applyCounterDelta(const BattleRescueCounterDelta& delta) const;

private:
    BattleRuntimeState* state_{};
    int unitId_{};
};
```

Implement these methods by resolving the existing `BattleRuntimeState::RescueState::RescueUnitRuntime` row for `unitId_`. Use `assert` for missing rows; every runtime unit should have a rescue row after spawn initialization.

- [ ] **Step 2: Replace rescue runtime lookups**

Replace per-unit rescue lookups:

```cpp
tryFindBy(state.rescue.units, unit.id, &BattleRuntimeState::RescueState::RescueUnitRuntime::unitId)
requireBy(state.rescue.units, result.counterDelta.unitId, &BattleRuntimeState::RescueState::RescueUnitRuntime::unitId)
```

with:

```cpp
unit.rescue().forcePullProtectRemaining()
unit.rescue().forcePullExecuteRemaining()
unit.rescue().applyCounterDelta(result.counterDelta)
```

If a function only has an id from a rescue result, resolve once:

```cpp
auto unit = state.units().require(result.counterDelta.unitId);
unit.rescue().applyCounterDelta(result.counterDelta);
```

- [ ] **Step 3: Verify rescue ownership moved only for per-unit counters**

Run:

```powershell
rg -n "rescue\\.units|RescueUnitRuntime|forcePullProtectRemaining|forcePullExecuteRemaining|unit\\.rescue\\(\\)" src\\battle tests
```

Expected:

- `unit.rescue().*` appears at per-unit counter read/write sites.
- `state.rescue.units` may remain in setup/spawn code and inside `BattleRuntimeUnitRescueView`.
- Rescue cells/config remain exposed.

- [ ] **Step 4: Run tests and commit**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core]"
.\x64\Debug\kys_tests.exe "[battle][runtime]"
```

Commit:

```powershell
git add src\battle\BattleRuntimeUnits.h src\battle\BattleCore.cpp tests
git commit -m "refactor: move unit rescue counters onto handles"
```

## Task 9: Defer BattleRuntimeMovement Until It Has Real Verbs

**Files:**
- Inspect: `src/battle/BattleCore.cpp`
- Inspect: `src/battle/BattleCore.h`
- Inspect: `src/battle/BattleRuntimeSession.cpp`
- Inspect: movement-related tests

This task is a gate, not an implementation task. Do not create `src/battle/BattleRuntimeMovement.h` while the only API would be `stateForSystem()`, `agentsForSystem()`, `reservationsForSystem()`, or scalar getters/setters over `BattleMovementState`. Revisit movement after deciding whether dead-unit cleanup should stop erasing `movement.agents`; if agents become stable for all runtime units, `unit.movement()` can become a normal unit subview instead of a phase-sensitive one.

- [ ] **Step 1: Audit movement ownership pressure**

Run:

```powershell
rg -n "movement\\.(frame|config|terrainCells|agents|movementReservations)|makeFrameMovementPlanInput|advanceFrameMovementPhysics|commitFrameMovement|requireMappedById\\(state\\.movement\\.agents|movementReservations\\.erase" src\battle tests
```

Expected hotspots to classify:

- Runtime setup: `BattleRuntimeSession.cpp` writes frame/config/terrain once.
- Planner input: `makeFrameMovementPlanInput(...)` reads frame/config/terrain/reservations and builds per-unit movement rows.
- Physics input: `advanceFrameMovementPhysics(...)` reads unit handles plus agent physics.
- Tick commit: `commitFrameMovement(...)` writes frame, reservations, unit motion, and agent physics.
- Cleanup: dead-unit cleanup erases agents and reservations.
- Small physics mutations: knockback, dash spread, pull/teleport, and frozen physics write agent fields by unit id.

- [ ] **Step 2: Keep the top-level unit loop inverted**

Where movement code iterates runtime units, prefer:

```cpp
for (auto unit : state.units().live())
{
    // Use unit.core(), unit.movement(), unit.status(), and unit.combo()
    // instead of resolving each horizontal store again by unit id.
}
```

For projectile, teleport, knockback, and event callbacks that start from a `unitId`, resolve once at the top:

```cpp
auto unit = state.units().require(unitId);
advanceKnockbackForUnit(state, unit, knockback);
```

Do not introduce a helper named `runtimeUnit(state, unitId)`. That would recreate the same hidden dependency as a free function.

- [ ] **Step 3: Name the movement verbs before introducing an owner**

Only create `BattleRuntimeMovement` in a later plan if the call sites can move to verbs shaped like this:

```cpp
class BattleRuntimeMovement
{
public:
    void prepareForFrame(int frame, BattleMovementConfig config, std::vector<BattleTerrainCell> terrainCells);
    BattleMovementPlanInput makePlanInput(std::span<const BattleRuntimeUnitHandle> units,
                                          const PostPhysicsMotionMap& postPhysicsMotion) const;
    void commitTick(BattleRuntimeUnits& units, const BattleTickResult& tick);
    BattleMovementAgentState& requireAgentPhysics(BattleRuntimeUnitHandle unit);
    void clearReservation(BattleRuntimeUnitHandle unit);

private:
    BattleMovementState state_;
};
```

The important boundary is not that `BattleMovementState` is private. The boundary is that planner input creation, physics commit, and reservation cleanup become named transitions with unit handles as parameters.

- [ ] **Step 4: Reject shallow movement wrappers**

Run:

```powershell
rg -n "BattleRuntimeMovement|stateForSystem|agentsForSystem|reservationsForSystem|movement\\.state\\(|movement\\.agents\\(\\)" src\battle tests
```

Expected: no matches in production code. If this search finds a new movement owner that exposes raw storage, stop and replace it with either a real verb API or no owner at all.

## Task 10: Defer BattleRuntimeDamage Until It Has Real Verbs

**Files:**
- Inspect: `src/battle/BattleCore.cpp`
- Inspect: `src/battle/BattleCore.h`
- Inspect: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Inspect: damage-related tests

This task is also a gate. Do not create `src/battle/BattleRuntimeDamage.h` if the result is a private vector plus `unitExtrasForSystem()` and `setSortPendingDamageByDefenderMagnitude(...)`.

- [ ] **Step 1: Audit damage ownership pressure**

Run:

```powershell
rg -n "damage\\.(unitExtras|presentationStylesByDefender|sortPendingDamageByDefenderMagnitude)|makeFrameDamageTransactionInput|writeBattleDamageRuntimeUnit|commitDamageUnitCoreToRuntime|commitDamageCooldownToRuntime|makeFrameDamagePresentation|presentationStyle|tryAppendFrameDamageTransaction" src\battle tests
```

Expected hotspots to classify:

- Runtime spawn: `appendRuntimeUnit(...)` creates damage extras and presentation style.
- Transaction input: `makeFrameDamageTransactionInput(...)` reads attacker/defender core, combo, status, and damage extras.
- Transaction commit: `commitDamageUnitCoreToRuntime(...)` and cooldown helpers write result state back.
- Presentation: `makeFrameDamagePresentation(...)` and `tryApplyDamagePresentationStyle(...)` read style by defender.
- Sorting: `resolveFramePendingDamage(...)` reads the defender-magnitude sort flag.

- [ ] **Step 2: Prefer unit handles for transaction assembly**

For normal damage transactions, resolve attacker and defender handles once:

```cpp
auto defender = state.units().require(request.defenderUnitId);
auto attacker = request.attackerUnitId >= 0
    ? std::optional<BattleRuntimeUnitHandle>{ state.units().require(request.attackerUnitId) }
    : std::nullopt;
auto transaction = makeFrameDamageTransactionInput(state, request, attacker, defender);
```

The transaction builder should then use `defender.core()`, `defender.damage()`, `defender.combo()`, and `defender.status()` instead of re-looking up four horizontal stores by id.

- [ ] **Step 3: Name the damage verbs before introducing an owner**

Only create `BattleRuntimeDamage` in a later plan if the call sites can move to verbs shaped like this:

```cpp
class BattleRuntimeDamage
{
public:
    BattleDamageTransactionInput makeTransactionInput(const BattleDamageRequest& request,
                                                      std::optional<BattleRuntimeUnitHandle> attacker,
                                                      BattleRuntimeUnitHandle defender) const;
    void writeTransactionResult(BattleRuntimeUnits& units, const BattleDamageTransactionResult& transaction);
    const BattleDamagePresentationStyle& presentationStyle(BattleRuntimeUnitHandle defender) const;
    BattleDamageRuntimeUnit& requireUnitExtra(BattleRuntimeUnitHandle unit);
    bool sortsPendingDamageByDefenderMagnitude() const;

private:
    bool sortPendingDamageByDefenderMagnitude_{};
    std::vector<BattleDamageRuntimeUnit> unitExtras_;
    std::map<int, BattleDamagePresentationStyle> presentationStylesByDefender_;
};
```

`requireUnitExtra(...)` is acceptable only as a narrow bridge for code that already has a `BattleRuntimeUnitHandle`. It should not become the standard way to reassemble a unit by id.

- [ ] **Step 4: Reject shallow damage wrappers**

Run:

```powershell
rg -n "BattleRuntimeDamage|unitExtrasForSystem|mutableUnitExtras|damage\\.unitExtras\\(|damage\\.presentationStyles\\(" src\battle tests
```

Expected: no matches in production code. If this search finds a new damage owner that exposes raw mutable vectors or maps, stop and replace it with transaction/presentation verbs or leave the existing horizontal state in place until the verb boundary is ready.

## Task 11: Final Ownership Audit

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `docs/superpowers/plans/2026-05-25-battle-runtime-owner-types.md` only if implementation discovers a needed plan correction.

- [ ] **Step 1: Run ownership grep**

Run:

```powershell
rg -n "pendingAttackSpawns|damage\\.pendingDamage|action\\.pendingCasts|ultimateCasters|stateForSystem|rowsForSystem|rowsForSetup|storeForSetup|agentsForSystem|unitExtrasForSystem|mutable[A-Za-z0-9_]*ForReducer|runtimeUnit\\(state" src\\battle tests
```

Expected:

- No public next-frame queues remain.
- No direct action owner containers remain in production code.
- No `runtimeUnit(state, unitId)` helper exists.
- No `stateForSystem`, `rowsForSystem`, `rowsForSetup`, `storeForSetup`, `agentsForSystem`, or `unitExtrasForSystem` getter-style APIs exist in production code.
- `mutable*ForReducer` appears only in the command sink factory and is phase-confined.
- `state.status.units`, `state.status.config`, `state.combo.units`, `state.deathEffects.store`, `state.movement.*`, and `state.damage.*` may still appear where no real owner verb has been introduced.

- [ ] **Step 2: Check BattleRuntimeState surface**

Open `src/battle/BattleCore.h` and confirm `BattleRuntimeState` has the unit handle facade next to the canonical unit store:

```cpp
BattleUnitStore unitStore;
BattleRuntimeUnits units();
BattleMovementState movement;
BattleAttackState attacks;
BattleRuntimeRandom random;
DamageState damage;
StatusState status;
ComboTriggerState combo;
DeathEffectState deathEffects;
BattleRuntimeActions action;
BattleNextFrameQueues nextFrame;
```

`BattleMovementState`, `DamageState`, `StatusState`, `ComboTriggerState`, and `DeathEffectState` may remain horizontal stores at the end of this plan. Do not replace them with wrappers unless a task has produced real verbs. Top-level per-unit phases should use `state.units().live()`, `state.units().all()`, or `state.units().dead()`.

- [ ] **Step 3: Audit per-unit loop direction**

Run:

```powershell
rg -n "for \\([^\\n]*(state\\.unitStore\\.units|unitStore\\.units)|state\\.unitStore\\.requireUnit\\(|requireById\\(state\\.(status\\.units|damage\\.unitExtras)|requireMappedById\\(state\\.(combo\\.units|movement\\.agents)" src\battle\BattleCore.cpp
```

Expected:

- Top-level phase loops that naturally operate on units use `state.units().live()`, `state.units().all()`, or `state.units().dead()`.
- Event/projectile callbacks may still resolve by id, but the lookup happens once near the operation boundary and the rest of the operation receives a `BattleRuntimeUnitHandle`.
- Any remaining direct horizontal lookups are recorded as candidates for the next small owner/handle conversion, not hidden behind a generic getter.

- [ ] **Step 4: Record future ownership migration constraints**

Before moving from the current view model to actual unit ownership, write down the invariants in `BattleRuntimeUnits.h` or a nearby design note:

```cpp
// Today: BattleRuntimeUnitHandle is a view over horizontal stores.
// Future ownership migration is reasonable only if every per-unit store has the
// same lifetime as BattleRuntimeUnit and system-wide processors can still iterate
// efficiently without rebuilding temporary horizontal arrays each frame.
```

This migration is a good idea if:

- Runtime unit membership remains fixed after clone initialization.
- Movement agents stop being erased, or movement becomes an optional component with explicit absence semantics.
- Damage/status/combo/rescue/action state lifetimes are all one-to-one with runtime units.
- Existing systems can consume ranges/spans of unit components without awkward temporary copies.

It is a bad idea if it produces a giant mutable `BattleRuntimeUnit` object that every system can freely mutate, or if system-wide algorithms start reconstructing horizontal arrays every frame just to call existing systems.

- [ ] **Step 5: Run full battle test groups**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle]"
```

Expected: all battle tests pass.

- [ ] **Step 6: Run final build**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
```

Expected: build succeeds. A final link failure is acceptable only if the game or test executable is running and locking the output.

- [ ] **Step 7: Commit**

```powershell
git add src\battle tests docs\superpowers\plans\2026-05-25-battle-runtime-owner-types.md
git commit -m "refactor: complete battle runtime owner boundaries"
```

## Self-Review

- Spec coverage: The plan covers the unit-inverted view through `BattleRuntimeUnits` / `BattleRuntimeUnitHandle`, next-frame queues, phase sinks, queue-only frame context encapsulation, action ownership, status/combo/death-effect/rescue per-unit handle verbs, explicit movement/damage verb gates, and future ownership migration constraints.
- Placeholder scan: The plan avoids open-ended placeholder instructions and provides concrete file paths, code skeletons, grep checks, commands, and expected outcomes.
- Type consistency: Owner names are consistent across tasks: `BattleRuntimeUnits`, `BattleRuntimeUnitHandle`, `BattleRuntimeUnitStatusView`, `BattleRuntimeUnitComboView`, `BattleRuntimeUnitDeathEffectsView`, `BattleRuntimeUnitRescueView`, `BattleNextFrameQueues`, and `BattleRuntimeActions`. `BattleRuntimeStatus`, `BattleRuntimeCombos`, `BattleRuntimeDeathEffects`, `BattleRuntimeRescue`, `BattleRuntimeMovement`, and `BattleRuntimeDamage` are intentionally not introduced in this plan.
- Scope check: The plan is intentionally staged. Tasks 1-4 are the highest-value path; Tasks 5-8 continue ownership only where the verbs are already clear; Tasks 9-10 prevent shallow movement/damage wrappers until the handle-based verb boundary is obvious.
