# Scoped Combo Trigger Timers Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `AllyLowHPBurst` trigger timers respect combo-effect ownership, then remove the stale `RoleComboState` copy-back path from frame advance.

**Architecture:** `RoleComboState` should store frame-trigger timers by trigger plus effect source, not by trigger alone. Broadcasted low-HP timers should only reach units that own a matching triggered effect from the same combo source; unrelated allies and unrelated combo effects must not observe or suppress the timer. Once cross-unit timer mutation is scoped and explicit, `advanceRuntimeUnits()` can mutate the record-owned combo state directly without returning a copied `RoleComboState` for later merge.

**Tech Stack:** C++23, Catch2 tests through `kys_tests`, PowerShell, Visual Studio/MSBuild via `.github\build-command.ps1`.

---

## File Structure

- Modify `src/ChessBattleEffects.h`
  - Add a small key type for combo trigger timers.
  - Change `RoleComboState::triggerTimers` from `std::map<Trigger, int>` to `std::map<ComboTriggerTimerKey, int>`.
- Modify `src/battle/BattleComboTriggerSystem.h`
  - Add the timer key to `BattleComboTriggerAction` and `BattleComboFrameRuntimeEvent`.
- Modify `src/battle/BattleComboTriggerSystem.cpp`
  - Build timer keys from triggered effect source metadata.
  - Read, write, and decrement scoped timers.
  - Keep frame-trigger effect queries matching only active timers from the same source.
- Modify `src/battle/BattleCore.cpp`
  - Broadcast scoped timers only to matching combo-effect owners.
  - Remove `BattleRuntimeUnitFrameCommit::comboState` and the copy-back merge.
- Modify `tests/BattleComboTriggerSystemUnitTests.cpp`
  - Add scoped timer characterization tests.
  - Migrate direct timer setup to `ComboTriggerTimerKey`.
- Modify `tests/BattleCoreUnitTests.cpp`
  - Add a runtime-level test proving one combo's low-HP burst does not activate or suppress another combo's matching trigger.
  - Add a runtime-level test proving the copied combo state is no longer needed for broadcast timer preservation.
- Modify `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
  - Migrate direct timer setup if present.

---

## Task 1: Characterize Scoped Timer Semantics

**Files:**
- Modify: `tests/BattleComboTriggerSystemUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Extend the local triggered-effect helper with source combo id**

In `tests/BattleComboTriggerSystemUnitTests.cpp`, replace the helper near the top of the file with:

```cpp
AppliedEffectInstance triggeredEffect(EffectType type,
                                      Trigger trigger,
                                      int value,
                                      int triggerValue = 0,
                                      int duration = 0,
                                      int maxCount = 0,
                                      int sourceComboId = -1)
{
    AppliedEffectInstance effect;
    effect.type = type;
    effect.trigger = trigger;
    effect.value = value;
    effect.triggerValue = triggerValue;
    effect.duration = duration;
    effect.maxCount = maxCount;
    effect.sourceComboId = sourceComboId;
    return effect;
}
```

- [ ] **Step 2: Add failing unit tests for scoped low-HP timers**

Add these tests after `BattleComboTriggerSystem_FrameTriggers_BroadcastsAllyLowHpTimer` in `tests/BattleComboTriggerSystemUnitTests.cpp`:

```cpp
TEST_CASE("BattleComboTriggerSystem_FrameTriggers_UsesSourceScopedAllyLowHpTimer", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::PctATK, Trigger::AllyLowHPBurst, 40, 35, 90, 2, 17));

    BattleComboTriggerSystem system;
    auto actions = system.updateFrameTriggers(state, { 30, 100, false });

    REQUIRE(actions.size() == 1);
    CHECK(actions[0].type == BattleComboTriggerActionType::BroadcastTriggerTimer);
    CHECK(actions[0].timerKey.trigger == Trigger::AllyLowHPBurst);
    CHECK(actions[0].timerKey.sourceComboId == 17);

    state.triggerTimers[{ Trigger::AllyLowHPBurst, 18 }] = 10;
    auto differentComboTimer = system.updateFrameTriggers(state, { 20, 100, false });
    REQUIRE(differentComboTimer.size() == 1);
    CHECK(differentComboTimer[0].timerKey.sourceComboId == 17);

    state.triggerTimers[{ Trigger::AllyLowHPBurst, 17 }] = 10;
    auto matchingComboTimer = system.updateFrameTriggers(state, { 20, 100, false });
    CHECK(matchingComboTimer.empty());
}

TEST_CASE("BattleComboTriggerSystem_FrameTriggerEffects_RequireMatchingTimerSource", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggerTimers[{ Trigger::AllyLowHPBurst, 17 }] = 3;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::PctATK, Trigger::AllyLowHPBurst, 10, 0, 0, 0, 17));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::DmgReductionPct, Trigger::AllyLowHPBurst, 20, 0, 0, 0, 18));

    auto active = BattleComboTriggerSystem().activeFrameTriggerEffects(
        state,
        { 90, 100, false },
        { EffectType::PctATK, EffectType::DmgReductionPct });

    REQUIRE(active.size() == 1);
    CHECK(active[0].effect.type == EffectType::PctATK);
    CHECK(active[0].effect.sourceComboId == 17);
}
```

- [ ] **Step 3: Add failing runtime test for cross-combo isolation**

In `tests/BattleCoreUnitTests.cpp`, add this helper near the existing local `triggeredEffect(...)` helper:

```cpp
KysChess::AppliedEffectInstance triggeredEffectFromCombo(KysChess::EffectType type,
                                                         KysChess::Trigger trigger,
                                                         int value,
                                                         int triggerValue,
                                                         int duration,
                                                         int maxCount,
                                                         int sourceComboId)
{
    KysChess::AppliedEffectInstance effect;
    effect.type = type;
    effect.trigger = trigger;
    effect.value = value;
    effect.triggerValue = triggerValue;
    effect.duration = duration;
    effect.maxCount = maxCount;
    effect.sourceComboId = sourceComboId;
    return effect;
}
```

Then add this test near the frame-runner combo tests in `tests/BattleCoreUnitTests.cpp`:

```cpp
TEST_CASE("BattleFrameRunner_AdvanceFrame_AllyLowHpBurstOnlyScopesToMatchingComboMembers", "[battle][core][combo]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 0, { 130, 100, 0 }),
        unit(2, 0, { 160, 100, 0 }),
        unit(3, 1, { 260, 100, 0 }),
    }));
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 20, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 0, 100, { 130, 100, 0 }),
        runtimeUnitSnapshot(2, 0, 100, { 160, 100, 0 }),
        runtimeUnitSnapshot(3, 1, 100, { 260, 100, 0 }),
    });

    state.units.require(0).combo.triggeredEffects.push_back(
        triggeredEffectFromCombo(KysChess::EffectType::PctATK, KysChess::Trigger::AllyLowHPBurst, 40, 35, 90, 2, 17));
    state.units.require(1).combo.triggeredEffects.push_back(
        triggeredEffectFromCombo(KysChess::EffectType::PctATK, KysChess::Trigger::AllyLowHPBurst, 40, 35, 90, 2, 17));
    state.units.require(2).combo.triggeredEffects.push_back(
        triggeredEffectFromCombo(KysChess::EffectType::PctATK, KysChess::Trigger::AllyLowHPBurst, 80, 35, 90, 2, 18));

    auto frame = runBattleFrame(state);

    (void)frame;
    CHECK(state.units.require(0).combo.triggerTimers[{ KysChess::Trigger::AllyLowHPBurst, 17 }] > 0);
    CHECK(state.units.require(1).combo.triggerTimers[{ KysChess::Trigger::AllyLowHPBurst, 17 }] > 0);
    CHECK_FALSE(state.units.require(2).combo.triggerTimers.contains({ KysChess::Trigger::AllyLowHPBurst, 17 }));
    CHECK_FALSE(state.units.require(2).combo.triggerTimers.contains({ KysChess::Trigger::AllyLowHPBurst, 18 }));
}
```

- [ ] **Step 4: Run the focused tests and verify they fail**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "BattleComboTriggerSystem_FrameTriggers_UsesSourceScopedAllyLowHpTimer"
.\x64\Debug\kys_tests.exe "BattleComboTriggerSystem_FrameTriggerEffects_RequireMatchingTimerSource"
.\x64\Debug\kys_tests.exe "BattleFrameRunner_AdvanceFrame_AllyLowHpBurstOnlyScopesToMatchingComboMembers"
```

Expected: build fails because `timerKey` and `ComboTriggerTimerKey` do not exist, or tests fail because timers are still keyed only by `Trigger`.

- [ ] **Step 5: Commit the failing characterization tests**

```powershell
git add tests\BattleComboTriggerSystemUnitTests.cpp tests\BattleCoreUnitTests.cpp
git commit -m "test: characterize scoped combo trigger timers"
```

---

## Task 2: Add `ComboTriggerTimerKey` And Migrate Unit-Level Combo Logic

**Files:**
- Modify: `src/ChessBattleEffects.h`
- Modify: `src/battle/BattleComboTriggerSystem.h`
- Modify: `src/battle/BattleComboTriggerSystem.cpp`
- Modify: `tests/BattleComboTriggerSystemUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add the timer key type**

In `src/ChessBattleEffects.h`, add this after `struct AppliedEffectInstance`:

```cpp
struct ComboTriggerTimerKey
{
    Trigger trigger = Trigger::Always;
    int sourceComboId = -1;

    friend bool operator<(const ComboTriggerTimerKey& lhs, const ComboTriggerTimerKey& rhs)
    {
        if (lhs.trigger != rhs.trigger)
        {
            return lhs.trigger < rhs.trigger;
        }
        return lhs.sourceComboId < rhs.sourceComboId;
    }
};
```

Then change `RoleComboState`:

```cpp
std::map<ComboTriggerTimerKey, int> triggerTimers;
```

- [ ] **Step 2: Add scoped timer data to combo action/event structs**

In `src/battle/BattleComboTriggerSystem.h`, change `BattleComboFrameRuntimeEvent` and `BattleComboTriggerAction` to:

```cpp
struct BattleComboFrameRuntimeEvent
{
    BattleComboFrameRuntimeEventType type = BattleComboFrameRuntimeEventType::SelfHpRegen;
    Trigger trigger = Trigger::Always;
    ComboTriggerTimerKey timerKey;
    int effectIndex = -1;
    int value = 0;
    int value2 = 0;
    int durationFrames = 0;
};

struct BattleComboTriggerAction
{
    BattleComboTriggerActionType type{};
    Trigger trigger{};
    ComboTriggerTimerKey timerKey;
    int effectIndex{};
    int value{};
    int durationFrames{};
};
```

- [ ] **Step 3: Add local helper functions for scoped timers**

In `src/battle/BattleComboTriggerSystem.cpp`, inside the anonymous namespace after `hookMatchesConfiguredTrigger(...)`, add:

```cpp
ComboTriggerTimerKey triggerTimerKeyFor(const AppliedEffectInstance& effect)
{
    assert(effect.trigger == Trigger::AllyLowHPBurst);
    return { effect.trigger, effect.sourceComboId };
}

bool hasActiveTriggerTimer(const RoleComboState& state, ComboTriggerTimerKey key)
{
    const auto timer = state.triggerTimers.find(key);
    return timer != state.triggerTimers.end() && timer->second > 0;
}
```

- [ ] **Step 4: Read scoped timers in active frame checks**

In `isActiveFrameTrigger(...)`, replace:

```cpp
auto timer = state.triggerTimers.find(effect.trigger);
return timer != state.triggerTimers.end() && timer->second > 0;
```

with:

```cpp
return hasActiveTriggerTimer(state, triggerTimerKeyFor(effect));
```

- [ ] **Step 5: Write scoped timers in frame trigger actions**

In `BattleComboTriggerSystem::updateFrameTriggers(...)`, replace the `AllyLowHPBurst` branch with:

```cpp
else if (effect.trigger == Trigger::AllyLowHPBurst)
{
    const auto timerKey = triggerTimerKeyFor(effect);
    if (unit.hp * 100 < unit.maxHp * effect.triggerValue
        && !hasActiveTriggerTimer(state, timerKey))
    {
        recordActivation(state, i);
        state.triggerTimers[timerKey] = effect.duration;
        actions.push_back({
            BattleComboTriggerActionType::BroadcastTriggerTimer,
            effect.trigger,
            timerKey,
            static_cast<int>(i),
            effect.value,
            effect.duration,
        });
    }
    continue;
}
```

This intentionally starts the source unit's timer too. It prevents repeated broadcasts during the duration and lets the triggering combo member share the same active-buff rule as the other combo members.

- [ ] **Step 6: Update non-broadcast runtime event aggregate initializers**

In `BattleComboTriggerSystem::advanceFrameRuntime(...)`, change the `SelfHpRegen` event construction to:

```cpp
events.push_back({
    BattleComboFrameRuntimeEventType::SelfHpRegen,
    effect.trigger,
    {},
    effectIndex,
    effect.value,
});
```

Change the `HealAura` event construction to:

```cpp
events.push_back({
    BattleComboFrameRuntimeEventType::HealAura,
    Trigger::Always,
    {},
    -1,
    healAuraFlat,
    healAuraPct,
    healedBoostPct,
});
```

Change the `HealPercentSelf` event construction to:

```cpp
events.push_back({
    BattleComboFrameRuntimeEventType::HealPercentSelf,
    action.trigger,
    {},
    action.effectIndex,
    action.value,
    0,
    action.durationFrames,
});
```

The `AutoUltimateReady` initializer can remain:

```cpp
events.push_back({ BattleComboFrameRuntimeEventType::AutoUltimateReady });
```

- [ ] **Step 7: Carry scoped timer keys through broadcast runtime events**

In `BattleComboTriggerSystem::advanceFrameRuntime(...)`, replace the `BroadcastTriggerTimer` event construction with:

```cpp
events.push_back({
    BattleComboFrameRuntimeEventType::BroadcastTriggerTimer,
    action.trigger,
    action.timerKey,
    action.effectIndex,
    action.value,
    0,
    action.durationFrames,
});
```

- [ ] **Step 8: Migrate direct test timer setup**

Replace direct timer writes in tests:

```cpp
state.triggerTimers[Trigger::AllyLowHPBurst] = 3;
```

with:

```cpp
state.triggerTimers[{ Trigger::AllyLowHPBurst, -1 }] = 3;
```

For tests that use a specific source combo id, use that id:

```cpp
state.triggerTimers[{ Trigger::AllyLowHPBurst, 17 }] = 3;
```

Search command:

```powershell
rg -n "triggerTimers\\[Trigger::AllyLowHPBurst\\]|triggerTimers\\[KysChess::Trigger::AllyLowHPBurst\\]" tests src
```

Expected after migration: no matches.

- [ ] **Step 9: Run focused combo tests**

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][combo]"
```

Expected: build succeeds and combo tests pass.

- [ ] **Step 10: Commit scoped timer unit logic**

```powershell
git add src\ChessBattleEffects.h src\battle\BattleComboTriggerSystem.h src\battle\BattleComboTriggerSystem.cpp tests
git commit -m "refactor: scope combo trigger timers by source"
```

---

## Task 3: Scope Runtime Broadcast To Matching Effect Owners

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add helper functions for matching timer recipients**

In `src/battle/BattleCore.cpp`, immediately before `applyBroadcastTriggerTimer(...)`, add:

```cpp
bool ownsComboTriggerTimer(const RoleComboState& combo, ComboTriggerTimerKey key)
{
    return std::any_of(
        combo.triggeredEffects.begin(),
        combo.triggeredEffects.end(),
        [key](const AppliedEffectInstance& effect)
        {
            return effect.trigger == key.trigger
                && effect.sourceComboId == key.sourceComboId;
        });
}

void setComboTriggerTimer(RoleComboState& combo, ComboTriggerTimerKey key, int durationFrames)
{
    assert(durationFrames > 0);
    combo.triggerTimers[key] = std::max(combo.triggerTimers[key], durationFrames);
}
```

- [ ] **Step 2: Scope broadcast application**

Replace `applyBroadcastTriggerTimer(...)` in `src/battle/BattleCore.cpp` with:

```cpp
void applyBroadcastTriggerTimer(BattleRuntimeState& state, int sourceUnitId, const BattleComboFrameRuntimeEvent& event)
{
    assert(event.durationFrames > 0);
    assert(event.type == BattleComboFrameRuntimeEventType::BroadcastTriggerTimer);
    assert(event.timerKey.trigger == event.trigger);

    const auto& source = state.units.requireCore(sourceUnitId);
    if (!source.alive)
    {
        return;
    }

    for (auto& record : state.units.live())
    {
        const auto& unit = record.core;
        if (unit.team != source.team)
        {
            continue;
        }
        if (!ownsComboTriggerTimer(record.combo, event.timerKey))
        {
            continue;
        }
        setComboTriggerTimer(record.combo, event.timerKey, event.durationFrames);
    }
}
```

This includes the source unit when it owns the matching effect. That matches the timer started in `updateFrameTriggers(...)` and avoids a separate cooldown-only store.

- [ ] **Step 3: Run runtime scoped broadcast test**

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "BattleFrameRunner_AdvanceFrame_AllyLowHpBurstOnlyScopesToMatchingComboMembers"
```

Expected: test passes.

- [ ] **Step 4: Run broader battle core combo tests**

```powershell
.\x64\Debug\kys_tests.exe "[battle][core][combo],[battle][frame_runner]"
```

Expected: tests pass. If existing tests expected the source unit to be excluded from the broadcast, update those tests only if the asserted behavior contradicts the new explicit "all matching combo members" rule.

- [ ] **Step 5: Commit runtime broadcast scoping**

```powershell
git add src\battle\BattleCore.cpp tests\BattleCoreUnitTests.cpp
git commit -m "fix: restrict ally low hp timers to matching combo members"
```

---

## Task 4: Remove `RoleComboState` Copy-Back From Runtime Advance

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add a preservation test for direct mutation**

Add this test near the scoped broadcast test in `tests/BattleCoreUnitTests.cpp`:

```cpp
TEST_CASE("BattleFrameRunner_AdvanceFrame_DirectComboMutationPreservesScopedBroadcastTimers", "[battle][core][combo][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 0, { 130, 100, 0 }),
        unit(2, 1, { 260, 100, 0 }),
    }));
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 20, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 0, 100, { 130, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 100, { 260, 100, 0 }),
    });

    state.units.require(0).combo.triggeredEffects.push_back(
        triggeredEffectFromCombo(KysChess::EffectType::PctATK, KysChess::Trigger::AllyLowHPBurst, 40, 35, 90, 2, 17));
    state.units.require(1).combo.triggeredEffects.push_back(
        triggeredEffectFromCombo(KysChess::EffectType::PctATK, KysChess::Trigger::AllyLowHPBurst, 40, 35, 90, 2, 17));

    auto frame = runBattleFrame(state);

    (void)frame;
    const auto key = KysChess::ComboTriggerTimerKey{ KysChess::Trigger::AllyLowHPBurst, 17 };
    CHECK(state.units.require(0).combo.triggerTimers.at(key) > 0);
    CHECK(state.units.require(1).combo.triggerTimers.at(key) > 0);
    CHECK(state.units.require(0).combo.effectActivationCounts.at(0) == 1);
}
```

- [ ] **Step 2: Remove `comboState` from the commit struct**

In `src/battle/BattleCore.cpp`, change:

```cpp
struct BattleRuntimeUnitFrameCommit
{
    int unitId = -1;
    std::vector<BattleComboFrameRuntimeEvent> comboEvents;
    RoleComboState comboState;
};
```

to:

```cpp
struct BattleRuntimeUnitFrameCommit
{
    int unitId = -1;
    std::vector<BattleComboFrameRuntimeEvent> comboEvents;
};
```

- [ ] **Step 3: Stop copying combo state during unit advance**

In `advanceRuntimeUnits(...)`, delete:

```cpp
committed.comboState = combo;
```

Leave `comboSystem.advanceFrameRuntime(...)` and `collectPendingSkillTeamHeal(...)` mutating the record-owned combo directly.

- [ ] **Step 4: Delete copy-back merge in `applyRuntimeComboEvents(...)`**

In `applyRuntimeComboEvents(...)`, delete:

```cpp
auto& combo = state.units.require(result.unitId).combo;
auto committed = result.comboState;
committed.triggerTimers = combo.triggerTimers;
combo = std::move(committed);
```

The function should only apply emitted events and append deferred auto-ultimate commands.

- [ ] **Step 5: Verify no stale combo copy remains**

Run:

```powershell
rg -n "comboState|committed\\.triggerTimers|BattleRuntimeUnitFrameCommit" src\battle\BattleCore.cpp
```

Expected: `BattleRuntimeUnitFrameCommit` remains, `comboState` and `committed.triggerTimers` do not.

- [ ] **Step 6: Run focused ownership tests**

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "BattleFrameRunner_AdvanceFrame_DirectComboMutationPreservesScopedBroadcastTimers"
.\x64\Debug\kys_tests.exe "[battle][core][ownership],[battle][runtime_session][death]"
```

Expected: tests pass.

- [ ] **Step 7: Commit copy-back removal**

```powershell
git add src\battle\BattleCore.cpp tests\BattleCoreUnitTests.cpp
git commit -m "refactor: remove battle combo state copy-back"
```

---

## Task 5: Cleanup And Verification

**Files:**
- Modify: any file touched by Tasks 1-4 only if needed for compile/test fallout.

- [ ] **Step 1: Search for unscoped timer usage**

Run:

```powershell
rg -n "triggerTimers\\[Trigger::|triggerTimers\\[KysChess::Trigger::|std::map<Trigger, int> triggerTimers|comboState|committed\\.triggerTimers" src tests
```

Expected: no matches.

- [ ] **Step 2: Search for broad ally broadcast without ownership check**

Run:

```powershell
rg -n "applyBroadcastTriggerTimer|ownsComboTriggerTimer|BroadcastTriggerTimer" src\battle tests
```

Expected:
- `applyBroadcastTriggerTimer(...)` uses `ownsComboTriggerTimer(...)`.
- `BattleComboFrameRuntimeEventType::BroadcastTriggerTimer` carries `timerKey`.
- Tests assert scoped behavior.

- [ ] **Step 3: Run focused battle tests**

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][combo]"
.\x64\Debug\kys_tests.exe "[battle][core]"
.\x64\Debug\kys_tests.exe "[battle][runtime_session]"
```

Expected: build succeeds and focused tests pass.

- [ ] **Step 4: Run full unit test binary**

```powershell
.\x64\Debug\kys_tests.exe
```

Expected: all tests pass.

- [ ] **Step 5: Check diff hygiene**

```powershell
git diff --check
git status --short
```

Expected: no whitespace errors. Only planned files are modified.

- [ ] **Step 6: Commit final cleanup if needed**

Only commit if Task 5 required additional source/test edits:

```powershell
git add src tests
git commit -m "test: verify scoped combo trigger timers"
```

---

## Self-Review Notes

- Scope coverage: The plan tests and implements source-scoped timers, runtime broadcast filtering, and removal of stale combo copy-back.
- No global applicability flag is added because the current config model does not expose one for runtime triggered combo effects. Existing explicit teamwide handling remains the `Team*` effect path in battle initialization.
- The plan intentionally includes the triggering unit in the scoped timer. This makes `triggerTimers` one concept: an active scoped timer for every owning combo member, instead of a mixed active-timer/cooldown side channel.
- Reduction target: delete `BattleRuntimeUnitFrameCommit::comboState` and the manual `triggerTimers` merge, while adding only a small scoped key and ownership check.
