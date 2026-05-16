# Battle Scene Presentation Ownership Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make battle scene logging, tracker updates, and presentation side effects flow through dedicated players/appliers instead of ad hoc `BattleSceneHades` direct writes.

**Architecture:** Core battle runtime emits ordered frame outputs. `BattleSceneTrackerPlayer` is the only component that writes `BattleTracker`; `BattleScenePresentationPlayer` handles visual/text/projectile presentation; `BattleSceneImpactPlayer` handles hit impact presentation such as sound, shake, and rumble; `BattleSceneFrameStateApplier` mutates scene unit state and camera/death-state effects. `BattleSceneHades` becomes orchestration only.

**Tech Stack:** C++20, Catch2, MSBuild via `.github/build-command.ps1`, existing battle runtime/presentation types.

---

## Ownership Rules

- `BattleSceneHades` must not call `tracker_.recordDamage`, `recordHeal`, `recordStatus`, `recordKill`, `recordDeath`, `recordBattleEnd`, or `recordProjectileCancel`.
- `BattleTracker` writes must happen only inside `BattleSceneTrackerPlayer`.
- Ordered visible log events must come from `BattlePresentationFrame::logEvents`.
- `BattleSceneHades` must not directly loop over `frameResult.attackEvents` for presentation-only sound/shake/rumble.
- Scene state mutation stays separate from log/tracker playback. It belongs in `BattleSceneFrameStateApplier`, not in log/player code.
- No compatibility aliases, fallback double paths, or old direct writes left behind.

## File Structure

- Create `src/BattleSceneTrackerPlayer.h`
  - Owns tracker bindings and tracker-only playback APIs.
- Create `src/BattleSceneTrackerPlayer.cpp`
  - Moves ordered log-to-tracker logic out of `BattleScenePresentationLogPlayer.cpp`.
  - Records projectile cancel stats.
- Delete `src/BattleScenePresentationLogPlayer.cpp`
  - Replaced by the properly named tracker player.
- Modify `src/BattleScenePresentationPlayer.h`
  - Remove tracker/log APIs from the visual presentation player.
  - Keep visual playback APIs only.
- Modify `src/BattleScenePresentationPlayer.cpp`
  - Remove log/tracker methods and keep visual/text/projectile presentation.
- Create `src/BattleSceneImpactPlayer.h`
  - Defines hit impact command/planner/player interfaces.
- Create `src/BattleSceneImpactPlayer.cpp`
  - Moves hit sound/shake/rumble decision logic out of `BattleSceneHades`.
- Create `src/BattleSceneFrameStateApplier.h`
  - Defines scene state apply context/result.
- Create `src/BattleSceneFrameStateApplier.cpp`
  - Moves core frame state mutation out of `BattleSceneHades`.
- Modify `src/BattleSceneHades.h`
  - Add `BattleSceneTrackerPlayer`, `BattleSceneImpactPlayer`, and `BattleSceneFrameStateApplier` members.
- Modify `src/BattleSceneHades.cpp`
  - Remove direct tracker writes, raw attack presentation loops, and state apply helper bodies.
- Modify `src/kys.vcxproj`
  - Add new `.cpp` files and remove deleted `BattleScenePresentationLogPlayer.cpp`.
- Modify `tests/kys_tests.vcxproj`
  - Add testable non-rendering `.cpp` files.
- Create `tests/BattleSceneTrackerPlayerUnitTests.cpp`
  - Covers ordered log playback and projectile cancel stats.
- Create `tests/BattleSceneImpactPlayerUnitTests.cpp`
  - Covers hit impact planning without audio/engine dependencies.
- Create `tests/BattleSceneFrameStateApplierUnitTests.cpp`
  - Covers scene unit state mutation and death/battle-end apply results.
- Modify `tests/BattlePresentationUnitTests.cpp`
  - Remove tracker-player tests after moving them to `BattleSceneTrackerPlayerUnitTests.cpp`.

---

### Task 1: Lock Tracker Ordering Tests

**Files:**
- Create: `tests/BattleSceneTrackerPlayerUnitTests.cpp`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Create tracker player tests before implementation**

Add tests that use `BattleSceneTrackerPlayer` as the intended API:

```cpp
#include "BattleSceneTrackerPlayer.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
BattleSceneUnit sceneUnit(int unitId, int battleId, int team, std::string name)
{
    BattleSceneUnit unit;
    unit.unitId = unitId;
    unit.identity = { battleId, battleId, team, 0, std::move(name) };
    return unit;
}
}

TEST_CASE("BattleSceneTrackerPlayer_RecordsDamageBeforeDeathAndBattleEnd", "[battle][scene_tracker]")
{
    BattleSceneUnitStore units;
    units.initialize({
        sceneUnit(0, 101, 0, "段譽"),
        sceneUnit(1, 202, 1, "岳不群"),
    });
    BattleTracker tracker;
    BattleSceneTrackerPlayer player;

    std::vector<KysChess::Battle::BattleLogEvent> logs = {
        { KysChess::Battle::BattleLogEventType::Damage, 221, 0, 1, 152, "", "六脈神劍" },
        { KysChess::Battle::BattleLogEventType::UnitDied, 221, 0, 1 },
        { KysChess::Battle::BattleLogEventType::BattleEnded, 221, -1, -1, 0 },
    };

    player.playLogs(logs, { &tracker, &units });

    const auto& events = tracker.getEvents();
    REQUIRE(events.size() == 4);
    CHECK(events[0].type == ::BattleLogEventType::Damage);
    CHECK(events[0].sourceName == "段譽");
    CHECK(events[0].targetName == "岳不群");
    CHECK(events[0].value == 152);
    CHECK(events[1].type == ::BattleLogEventType::Kill);
    CHECK(events[2].type == ::BattleLogEventType::Death);
    CHECK(events[3].type == ::BattleLogEventType::BattleEnd);
}

TEST_CASE("BattleSceneTrackerPlayer_RecordsProjectileCancelStats", "[battle][scene_tracker]")
{
    BattleSceneUnitStore units;
    units.initialize({
        sceneUnit(0, 101, 0, "段譽"),
        sceneUnit(1, 202, 1, "岳不群"),
    });
    BattleTracker tracker;
    BattleSceneTrackerPlayer player;

    std::vector<KysChess::Battle::BattleProjectileCancelDamageCommand> commands = {
        { 0, 1, 35, 20 },
    };

    player.playProjectileCancelDamageCommands(commands, { &tracker, &units });

    CHECK(tracker.cancelDamageForUnit(0) == 35);
    CHECK(tracker.cancelDamageForUnit(1) == 20);
}
```

- [ ] **Step 2: Add the test file to `tests/kys_tests.vcxproj`**

Add:

```xml
<ClCompile Include="BattleSceneTrackerPlayerUnitTests.cpp" />
```

- [ ] **Step 3: Run tests and verify red**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Configuration Debug -Platform x64 -Verbosity minimal -NoLogo
```

Expected: fail because `BattleSceneTrackerPlayer` does not exist yet or does not yet expose the required API.

---

### Task 2: Create BattleSceneTrackerPlayer

**Files:**
- Create: `src/BattleSceneTrackerPlayer.h`
- Create: `src/BattleSceneTrackerPlayer.cpp`
- Delete: `src/BattleScenePresentationLogPlayer.cpp`
- Modify: `src/BattleScenePresentationPlayer.h`
- Modify: `src/kys.vcxproj`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Add the tracker player header**

Create `src/BattleSceneTrackerPlayer.h`:

```cpp
#pragma once

#include "BattleSceneUnitStore.h"
#include "BattleStatsView.h"
#include "battle/BattleHitResolver.h"
#include "battle/BattlePresentation.h"

#include <vector>

class BattleSceneTrackerPlayer
{
public:
    struct Bindings
    {
        BattleTracker* tracker = nullptr;
        const BattleSceneUnitStore* units = nullptr;
    };

    void playLogs(
        const std::vector<KysChess::Battle::BattleLogEvent>& logEvents,
        const Bindings& bindings) const;

    void playProjectileCancelDamageCommands(
        const std::vector<KysChess::Battle::BattleProjectileCancelDamageCommand>& commands,
        const Bindings& bindings) const;

private:
    void recordLog(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordDamage(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordHeal(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordStatus(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordUnitDied(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordBattleEnded(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
};
```

- [ ] **Step 2: Add the tracker player implementation**

Move the log playback logic currently in `src/BattleScenePresentationLogPlayer.cpp` into `src/BattleSceneTrackerPlayer.cpp`, with class names changed to `BattleSceneTrackerPlayer`.

Add projectile cancel stat playback:

```cpp
void BattleSceneTrackerPlayer::playProjectileCancelDamageCommands(
    const std::vector<KysChess::Battle::BattleProjectileCancelDamageCommand>& commands,
    const Bindings& bindings) const
{
    assert(bindings.tracker);
    for (const auto& command : commands)
    {
        bindings.tracker->recordProjectileCancel(command.sourceUnitId, command.damage);
        bindings.tracker->recordProjectileCancel(command.otherSourceUnitId, command.otherDamage);
    }
}
```

- [ ] **Step 3: Remove log/tracker APIs from visual player**

In `src/BattleScenePresentationPlayer.h`, remove:

```cpp
BattleTracker* tracker = nullptr;
void playLogs(...);
void recordLog(...);
void recordDamage(...);
void recordHeal(...);
void recordStatus(...);
void recordUnitDied(...);
void recordBattleEnded(...);
```

Keep only visual/text/projectile playback concerns.

- [ ] **Step 4: Update projects**

In `src/kys.vcxproj`, replace:

```xml
<ClCompile Include="BattleScenePresentationLogPlayer.cpp" />
```

with:

```xml
<ClCompile Include="BattleSceneTrackerPlayer.cpp" />
```

In `tests/kys_tests.vcxproj`, add:

```xml
<ClCompile Include="..\src\BattleSceneTrackerPlayer.cpp" />
```

Remove any reference to `BattleScenePresentationLogPlayer.cpp`.

- [ ] **Step 5: Run tracker tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Configuration Debug -Platform x64 -Verbosity minimal -NoLogo
.\x64\Debug\kys_tests.exe "[battle][scene_tracker]"
```

Expected: all tracker player tests pass.

---

### Task 3: Route BattleSceneHades Tracker Writes Through Tracker Player

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

- [ ] **Step 1: Add tracker player member**

In `src/BattleSceneHades.h`, include `BattleSceneTrackerPlayer.h` and add:

```cpp
BattleSceneTrackerPlayer tracker_player_;
```

- [ ] **Step 2: Publish logs through tracker player**

In `BattleSceneHades::publishPresentationFrame`, replace tracker/log playback via `presentation_player_` with:

```cpp
last_presentation_frame_ = presentation_recorder_.consumeFrame();
tracker_player_.playLogs(last_presentation_frame_.logEvents, {
    &tracker_,
    &scene_units_,
});
presentation_player_.play(last_presentation_frame_, {
    &text_effects_,
    &attack_effects_,
    &scene_units_,
});
```

Update `BattleScenePresentationPlayer::Bindings` to no longer require `tracker`.

- [ ] **Step 3: Move projectile cancel stats**

In `BattleSceneHades::applyCoreFrameResult`, replace:

```cpp
for (const auto& command : frameResult.projectileCancelDamageCommands)
{
    tracker_.recordProjectileCancel(command.sourceUnitId, command.damage);
    tracker_.recordProjectileCancel(command.otherSourceUnitId, command.otherDamage);
}
```

with:

```cpp
tracker_player_.playProjectileCancelDamageCommands(frameResult.projectileCancelDamageCommands, {
    &tracker_,
    &scene_units_,
});
```

- [ ] **Step 4: Remove direct lifecycle tracker writes**

Ensure the lifecycle helper only collects scene-state information:

```cpp
BattleLifecycleApplyResult collectBattleLifecycleApplyResult(
    int currentBattleResult,
    const std::vector<KysChess::Battle::BattleGameplayEvent>& events);
```

It must not accept `BattleTracker&` and must not call any `tracker.record...` method.

- [ ] **Step 5: Verify no direct tracker writes remain in Hades**

Run:

```powershell
rg "tracker_\\.record" src/BattleSceneHades.cpp
```

Expected: no output.

- [ ] **Step 6: Run tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Configuration Debug -Platform x64 -Verbosity minimal -NoLogo
.\x64\Debug\kys_tests.exe "[battle][scene_tracker]"
.\x64\Debug\kys_tests.exe
```

Expected: all tests pass.

---

### Task 4: Move Hit Impact Presentation Out Of Hades

**Files:**
- Create: `src/BattleSceneImpactPlayer.h`
- Create: `src/BattleSceneImpactPlayer.cpp`
- Create: `tests/BattleSceneImpactPlayerUnitTests.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/kys.vcxproj`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Add pure impact planning tests**

Create `tests/BattleSceneImpactPlayerUnitTests.cpp` with tests for:

```cpp
TEST_CASE("BattleSceneImpactPlayer_PlansNormalHitShake", "[battle][scene_impact]")
{
    KysChess::Battle::BattleAttackEvent hit;
    hit.type = KysChess::Battle::BattleAttackEventType::Hit;
    hit.unitId = 1;
    hit.operationType = KysChess::Battle::BattleOperationType::RangedProjectile;
    hit.totalFrame = 30;
    hit.ultimate = false;

    auto commands = BattleSceneImpactPlanner().plan({ hit });

    REQUIRE(commands.size() == 1);
    CHECK(commands[0].unitId == 1);
    CHECK(commands[0].unitShake == 5);
    CHECK(commands[0].sceneShake == 0);
    CHECK(commands[0].rumble);
}

TEST_CASE("BattleSceneImpactPlayer_PlansUltimateHitShake", "[battle][scene_impact]")
{
    KysChess::Battle::BattleAttackEvent hit;
    hit.type = KysChess::Battle::BattleAttackEventType::Hit;
    hit.unitId = 1;
    hit.operationType = KysChess::Battle::BattleOperationType::TrackingProjectile;
    hit.totalFrame = 30;
    hit.ultimate = true;

    auto commands = BattleSceneImpactPlanner().plan({ hit });

    REQUIRE(commands.size() == 1);
    CHECK(commands[0].unitShake == 10);
    CHECK(commands[0].sceneShake == 10);
    CHECK(commands[0].rumble);
}

TEST_CASE("BattleSceneImpactPlayer_ScriptedImpactOnlyShakesUnit", "[battle][scene_impact]")
{
    KysChess::Battle::BattleAttackEvent hit;
    hit.type = KysChess::Battle::BattleAttackEventType::Hit;
    hit.unitId = 1;
    hit.scriptedDamage = 12;

    auto commands = BattleSceneImpactPlanner().plan({ hit });

    REQUIRE(commands.size() == 1);
    CHECK(commands[0].unitShake == 5);
    CHECK(commands[0].sceneShake == 0);
    CHECK_FALSE(commands[0].rumble);
}
```

- [ ] **Step 2: Implement impact planner/player**

Create `BattleSceneImpactPlayer.h`:

```cpp
#pragma once

#include "BattleSceneUnitStore.h"
#include "battle/BattleAttackSystem.h"

#include <vector>

struct BattleSceneImpactCommand
{
    int unitId = -1;
    int skillId = -1;
    int unitShake = 0;
    int sceneShake = 0;
    bool rumble = false;
};

class BattleSceneImpactPlanner
{
public:
    std::vector<BattleSceneImpactCommand> plan(
        const std::vector<KysChess::Battle::BattleAttackEvent>& events) const;
};

class BattleSceneImpactPlayer
{
public:
    struct Bindings
    {
        BattleSceneUnitStore* units = nullptr;
        int* sceneShake = nullptr;
    };

    void play(
        const std::vector<KysChess::Battle::BattleAttackEvent>& events,
        const Bindings& bindings) const;
};
```

Implementation details:
- Move `hasScriptedImpact` out of `BattleSceneHades.cpp` into this implementation.
- Planner ignores non-hit attack events.
- Player applies unit shake, scene shake, skill sound, and rumble.

- [ ] **Step 3: Replace raw Hades attack-event loop**

In `BattleSceneHades::applyCoreFrameResult`, replace the raw `for (const auto& event : frameResult.attackEvents)` block with:

```cpp
impact_player_.play(frameResult.attackEvents, {
    &scene_units_,
    &shake_,
});
```

- [ ] **Step 4: Verify no raw attack presentation loop remains**

Run:

```powershell
rg "frameResult\\.attackEvents|BattleAttackEventType::Hit|hasScriptedImpact" src/BattleSceneHades.cpp
```

Expected: no output.

- [ ] **Step 5: Run tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Configuration Debug -Platform x64 -Verbosity minimal -NoLogo
.\x64\Debug\kys_tests.exe "[battle][scene_impact]"
.\x64\Debug\kys_tests.exe
```

Expected: all tests pass.

---

### Task 5: Extract Scene Frame State Applier

**Files:**
- Create: `src/BattleSceneFrameStateApplier.h`
- Create: `src/BattleSceneFrameStateApplier.cpp`
- Create: `tests/BattleSceneFrameStateApplierUnitTests.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/kys.vcxproj`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Add state applier tests**

Create tests covering:
- unit applications write cooldown/action/MP;
- damage render applications write HP/alive/invincible/frozen/cooldown;
- death gameplay events produce died-unit ids and battle-end result;
- rescue teleports update scene position/HP/invincibility;
- no tracker methods are involved.

Use `BattleSceneUnitStore` directly and a minimal `BattleFrameResult`.

- [ ] **Step 2: Add applier API**

Create `BattleSceneFrameStateApplier.h`:

```cpp
#pragma once

#include "BattleMap.h"
#include "BattleSceneUnitStore.h"
#include "battle/BattleCore.h"

#include <deque>
#include <map>
#include <set>

struct BattleSceneFrameStateApplyResult
{
    bool battleEnded = false;
    int battleResult = -1;
    bool unitDied = false;
    std::vector<int> diedUnitIds;
    int sceneShake = 0;
    int frozenFrames = 0;
    int slowFrames = 0;
    int closeUpFrames = 0;
    std::optional<Pointf> cameraFocus;
};

struct BattleSceneFrameStateApplyContext
{
    BattleSceneUnitStore* units = nullptr;
    BattleMap* battleMap = nullptr;
    std::map<int, KysChess::RoleComboState>* comboStates = nullptr;
    std::deque<BattleAttackEffect>* attackEffects = nullptr;
    std::map<int, int>* hurtFlashTimers = nullptr;
    Random* random = nullptr;
    double gravity = 0.0;
    bool manualCameraEnabled = false;
};

class BattleSceneFrameStateApplier
{
public:
    BattleSceneFrameStateApplyResult apply(
        const KysChess::Battle::BattleFrameResult& frameResult,
        int currentBattleResult,
        BattleSceneFrameStateApplyContext context) const;
};
```

- [ ] **Step 3: Move state mutation from Hades**

Move these Hades helpers into `BattleSceneFrameStateApplier.cpp`:
- unit applications from `applyCoreFrameResult`;
- `applyCoreStatusState`;
- `applyCoreDamageTransactions`, excluding tracker writes;
- `applyCoreTeamEffectState`;
- `applyCoreFrameApplications`;
- movement/action/rescue scene unit updates.

The applier returns camera/death/battle-end side-effect intent in `BattleSceneFrameStateApplyResult`. `BattleSceneHades` applies only its own private camera fields from that result.

- [ ] **Step 4: Reduce Hades to orchestration**

`BattleSceneHades::applyCoreFrameResult` should become roughly:

```cpp
auto applyResult = frame_state_applier_.apply(frameResult, result_, {
    &scene_units_,
    &battle_map_,
    &KysChess::ChessCombo::getMutableStates(),
    &attack_effects_,
    &hurt_flash_timers_,
    &rand_,
    gravity_,
    isManualCameraEnabled(),
});

tracker_player_.playProjectileCancelDamageCommands(frameResult.projectileCancelDamageCommands, {
    &tracker_,
    &scene_units_,
});

recordPresentationFrame(frameResult.frame);
impact_player_.play(frameResult.attackEvents, { &scene_units_, &shake_ });
applyCameraAndBattleResult(applyResult);
```

No old helper bodies remain in `BattleSceneHades.cpp`.

- [ ] **Step 5: Run scene applier tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Configuration Debug -Platform x64 -Verbosity minimal -NoLogo
.\x64\Debug\kys_tests.exe "[battle][scene_frame_applier]"
.\x64\Debug\kys_tests.exe
```

Expected: all tests pass.

---

### Task 6: Final Cleanup And Verification

**Files:**
- Modify: `src/kys.vcxproj`
- Modify: `tests/kys_tests.vcxproj`
- Delete stale files if any remain.

- [ ] **Step 1: Remove stale file/project entries**

Verify:

```powershell
rg "BattleScenePresentationLogPlayer" src tests
```

Expected: no output.

- [ ] **Step 2: Verify Hades no longer owns tracker writes or raw hit presentation**

Run:

```powershell
rg "tracker_\\.record|frameResult\\.attackEvents|BattleAttackEventType::Hit|hasScriptedImpact" src/BattleSceneHades.cpp
```

Expected: no output.

- [ ] **Step 3: Verify source ownership**

Run:

```powershell
rg "recordDamage|recordHeal|recordStatus|recordKill|recordDeath|recordBattleEnd|recordProjectileCancel" src
```

Expected:
- `BattleTracker.cpp` definitions;
- `BattleSceneTrackerPlayer.cpp` calls;
- tests;
- no `BattleSceneHades.cpp` calls.

- [ ] **Step 4: Run full verification**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Configuration Debug -Platform x64 -Verbosity minimal -NoLogo
.\x64\Debug\kys_tests.exe
.\.github\build-command.ps1 -Configuration Debug -Platform x64 -Verbosity minimal -NoLogo
```

Expected:
- test build succeeds;
- all tests pass;
- full Debug build succeeds. If final link fails only because `x64\Debug\kys.exe` is locked, treat compile as verified per repo instructions and report the lock.

## Self-Review

- Spec coverage: direct tracker writes, projectile cancel stats, hit impact presentation, and scene state mutation boundaries are all covered.
- No migration artifacts: old `BattleScenePresentationLogPlayer.cpp` is deleted, and Hades direct tracker writes are removed rather than bridged.
- Type consistency: tracker player consumes `BattleLogEvent` and `BattleProjectileCancelDamageCommand`; visual player consumes `BattleVisualEvent`; impact player consumes `BattleAttackEvent`; state applier consumes `BattleFrameResult`.
