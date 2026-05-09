# Battle Frame Performance Dedup Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove duplicated frame-time work in battle runtime/adapter boundaries without changing the current runtime-vs-scene separation.

**Architecture:** Keep runtime authoritative and scene apply-only. Replace whole-frame mirrors, repeated linear lookup, and repeated copies with explicit active-unit payloads and indexed lookups. Do not reintroduce scene-side runtime access or broad mutable runtime escape hatches.

**Tech Stack:** C++20, MSVC, Catch2 tests, existing `BattleFrameRunner`, `BattleRuntimeState`, `BattleFrameResult`, `BattleSceneBattleAdapter`, and `BattleSceneHades`.

---

## File Structure

- Modify `src/battle/BattleCore.h`
  - Add an explicit active-unit legacy combo mirror DTO carrying only scene-required fields.
  - Replace `BattleFrameStateApplications::comboStates` map with a vector of active-unit combo mirrors.

- Modify `src/battle/BattleCore.cpp`
  - Make movement presentation publishing O(n).
  - Publish combo writes as active runtime-unit payloads instead of copying the full combo map.
  - Rebuild rescue occupancy in O(cells + units), and skip rescue snapshot sync when no damage is pending.
  - Reduce damage application input copies by using const references/spans for read-only queues.

- Modify `src/battle/BattleDamageApplicationSystem.h`
  - Change `BattleDamageApplicationInput` read-only queues/configuration from owning copies to references/pointers.

- Modify `src/battle/BattleDamageApplicationSystem.cpp`
  - Copy pending damage/presentation only when aggregation needs mutable vectors.

- Modify `src/battle/BattleFind.h`
  - Add an associative-map require helper for id-keyed maps so frame adapter role lookup reuses battle lookup conventions without sacrificing O(1) map lookup.

- Modify `src/BattleSceneBattleAdapter.h`
  - Replace vector-only role lookup in frame apply helpers with a binding-map based lookup.

- Modify `src/BattleSceneBattleAdapter.cpp`
  - Remove frame-time uses of linear `findRoleByBattleId()` where a role map is available.
  - Keep `findRoleByBattleId()` only for setup/initialization paths that do not already own a map, or delete it if all callers migrate.

- Modify `src/BattleSceneHades.cpp`
  - Apply active combo mirror fields into the legacy global combo store.
  - Pass `core_role_bindings_.rolesByBattleId` to adapter frame apply helpers.

- Modify tests:
  - `tests/BattleCoreUnitTests.cpp`
  - `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

---

## Task 1: Make Movement Presentation Publishing O(n)

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add/adjust a unit test covering physics overlay without duplicate rows**

In `tests/BattleCoreUnitTests.cpp`, extend the existing movement presentation test so it proves one row is emitted per unit when both movement planner and physics results exist:

```cpp
REQUIRE(result.movementPresentationResults.size() == result.movementPhysicsResults.size());
CHECK(result.movementPresentationResults[0].unitId == result.movementPhysicsResults[0].unitId);
CHECK(result.movementPresentationResults[0].position.x == result.movementPhysicsResults[0].state.position.x);
CHECK(result.movementPresentationResults[0].velocity.x == result.movementPhysicsResults[0].state.velocity.x);
```

- [ ] **Step 2: Replace linear overlay lookup with an index**

In `src/battle/BattleCore.cpp`, update `publishMovementPresentationResults(...)` to build a unit-id index once:

```cpp
void publishMovementPresentationResults(BattleFrameResult& result)
{
    result.movementPresentationResults.clear();
    result.movementPresentationResults.reserve(
        std::max(result.movement.snapshot.units.size(), result.movementPhysicsResults.size()));

    std::unordered_map<int, std::size_t> indexByUnitId;
    indexByUnitId.reserve(result.movement.snapshot.units.size() + result.movementPhysicsResults.size());

    for (const auto& unit : result.movement.snapshot.units)
    {
        BattleFrameMovementPresentationUnitResult presentation;
        presentation.unitId = unit.id;
        presentation.position = unit.position;
        presentation.velocity = unit.velocity;
        if (presentation.velocity.norm() > 0.01)
        {
            presentation.facing = presentation.velocity;
            presentation.facing.normTo(1);
        }
        indexByUnitId.emplace(presentation.unitId, result.movementPresentationResults.size());
        result.movementPresentationResults.push_back(std::move(presentation));
    }

    for (const auto& physics : result.movementPhysicsResults)
    {
        auto indexIt = indexByUnitId.find(physics.unitId);
        if (indexIt == indexByUnitId.end())
        {
            indexIt = indexByUnitId.emplace(physics.unitId, result.movementPresentationResults.size()).first;
            result.movementPresentationResults.push_back({ .unitId = physics.unitId });
        }
        auto& presentation = result.movementPresentationResults[indexIt->second];
        presentation.position = physics.state.position;
        presentation.velocity = physics.state.velocity;
        presentation.acceleration = physics.state.acceleration;
        presentation.frozenFrames = physics.frozenFrames;
        if (presentation.velocity.norm() > 0.01)
        {
            presentation.facing = presentation.velocity;
            presentation.facing.normTo(1);
        }
    }
}
```

- [ ] **Step 3: Verify focused movement tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core][movement]"
```

Expected: all matching movement tests pass.

---

## Task 2: Replace Full Combo Map Mirror With Narrow Active Combo Mirrors

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Replace the frame state application DTO with scene-required fields only**

In `src/battle/BattleCore.h`, add:

```cpp
struct BattleFrameComboLegacyMirror
{
    int unitId = -1;
    int shield = 0;
    int poisonTimer = 0;
    int poisonTickDmg = 0;
    int poisonSourceId = -1;
    int bleedStacks = 0;
    int bleedTimer = 0;
    int bleedSourceId = -1;
    int controlImmunityFrames = 0;
    int mpBlockTimer = 0;
    int damageImmunityTimer = 0;
    std::vector<TimedAttackBuff> tempAttackBuffs;
    std::vector<DamageReduceDebuff> damageReduceDebuffs;
};
```

Change `BattleFrameStateApplications` from:

```cpp
struct BattleFrameStateApplications
{
    std::vector<BattleStatusUnitState> statusUnits;
    std::map<int, RoleComboState> comboStates;
};
```

to:

```cpp
struct BattleFrameStateApplications
{
    std::vector<BattleStatusUnitState> statusUnits;
    std::vector<BattleFrameComboLegacyMirror> comboMirrors;
};
```

- [ ] **Step 2: Add a projection helper for legacy combo mirror fields**

In `src/battle/BattleCore.cpp`, add a helper near `publishFrameApplyOutputs(...)`:

```cpp
BattleFrameComboLegacyMirror makeBattleFrameComboLegacyMirror(
    int unitId,
    const KysChess::RoleComboState& state)
{
    BattleFrameComboLegacyMirror mirror;
    mirror.unitId = unitId;
    mirror.shield = state.shield;
    mirror.poisonTimer = state.poisonTimer;
    mirror.poisonTickDmg = state.poisonTickDmg;
    mirror.poisonSourceId = state.poisonSourceId;
    mirror.bleedStacks = state.bleedStacks;
    mirror.bleedTimer = state.bleedTimer;
    mirror.bleedSourceId = state.bleedSourceId;
    mirror.controlImmunityFrames = state.controlImmunityFrames;
    mirror.mpBlockTimer = state.mpBlockTimer;
    mirror.damageImmunityTimer = state.damageImmunityTimer;
    mirror.tempAttackBuffs.reserve(state.tempAttackBuffs.size());
    for (const auto& buff : state.tempAttackBuffs)
    {
        mirror.tempAttackBuffs.push_back({ buff.attackBonus, buff.remainingFrames });
    }
    mirror.damageReduceDebuffs.reserve(state.dmgReduceDebuffs.size());
    for (const auto& debuff : state.dmgReduceDebuffs)
    {
        mirror.damageReduceDebuffs.push_back({ debuff.remainingFrames, debuff.pct });
    }
    return mirror;
}
```

- [ ] **Step 3: Publish active runtime combo mirrors instead of copying the map**

In `src/battle/BattleCore.cpp`, replace:

```cpp
result.stateApplications.comboStates = state.combo.units;
```

with:

```cpp
result.stateApplications.comboMirrors.clear();
result.stateApplications.comboMirrors.reserve(state.units.units.size());
for (const auto& unit : state.units.units)
{
    const auto comboIt = state.combo.units.find(unit.id);
    if (comboIt == state.combo.units.end())
    {
        continue;
    }
    result.stateApplications.comboMirrors.push_back(
        makeBattleFrameComboLegacyMirror(unit.id, comboIt->second));
}
```

- [ ] **Step 4: Apply combo mirror fields incrementally in the scene**

In `src/BattleSceneHades.cpp`, update `applyCoreStatusState(...)`.

Replace:

```cpp
auto& comboStates = KysChess::ChessCombo::getMutableStates();
comboStates = applications.comboStates;
for (const auto& status : applications.statusUnits)
{
    auto role = requireFrameRole(bindings, status.id);
    assert(role);
    auto stateIt = comboStates.find(status.id);
    assert(stateIt != comboStates.end());
    writeBattleStatusUnit(role, stateIt->second, status);
}
```

with:

```cpp
auto& comboStates = KysChess::ChessCombo::getMutableStates();
for (const auto& mirror : applications.comboMirrors)
{
    auto& state = comboStates[mirror.unitId];
    state.shield = mirror.shield;
    state.poisonTimer = mirror.poisonTimer;
    state.poisonTickDmg = mirror.poisonTickDmg;
    state.poisonSourceId = mirror.poisonSourceId;
    state.bleedStacks = mirror.bleedStacks;
    state.bleedTimer = mirror.bleedTimer;
    state.bleedSourceId = mirror.bleedSourceId;
    state.controlImmunityFrames = mirror.controlImmunityFrames;
    state.mpBlockTimer = mirror.mpBlockTimer;
    state.damageImmunityTimer = mirror.damageImmunityTimer;
    state.tempAttackBuffs.clear();
    for (const auto& buff : mirror.tempAttackBuffs)
    {
        state.tempAttackBuffs.push_back({ buff.attackBonus, buff.remainingFrames });
    }
    state.dmgReduceDebuffs.clear();
    for (const auto& debuff : mirror.damageReduceDebuffs)
    {
        state.dmgReduceDebuffs.push_back({ debuff.remainingFrames, debuff.pct });
    }
}
for (const auto& status : applications.statusUnits)
{
    auto role = requireFrameRole(bindings, status.id);
    assert(role);
    auto stateIt = comboStates.find(status.id);
    assert(stateIt != comboStates.end());
    writeBattleStatusUnit(role, stateIt->second, status);
}
```

- [ ] **Step 5: Update runtime frame test assertions**

In `tests/BattleFrameRunnerRuntimeUnitTests.cpp`, replace map-style assertions:

```cpp
REQUIRE(result.stateApplications.comboStates.size() == 1);
CHECK(result.stateApplications.comboStates.at(1).shield == 12);
```

with vector-style assertions:

```cpp
REQUIRE(result.stateApplications.comboMirrors.size() == 1);
CHECK(result.stateApplications.comboMirrors[0].unitId == 1);
CHECK(result.stateApplications.comboMirrors[0].shield == 12);
```

- [ ] **Step 6: Run combo boundary search**

Run:

```powershell
rg -n "stateApplications\.comboStates|comboStates = applications\.comboStates|comboStates\.at\(|RoleComboState state;" src tests
```

Expected: no matches.

- [ ] **Step 7: Verify runtime frame tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][frame_runner][runtime]"
```

Expected: all matching runtime frame tests pass.

---

## Task 3: Make Rescue Occupancy Sync Linear And Gate It

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add a helper key for rescue cells**

In the anonymous namespace in `src/battle/BattleCore.cpp`, near rescue helpers, add:

```cpp
std::pair<int, int> rescueCellKey(int x, int y)
{
    return { x, y };
}
```

- [ ] **Step 2: Replace nested cell/unit scan with one live-unit occupancy map**

In `syncRescueStateFromRuntimeUnits(...)`, replace the `for (auto& cell : state.rescue.cells)` nested unit scan with:

```cpp
std::map<std::pair<int, int>, int> occupantByCell;
for (const auto& unit : state.units.units)
{
    if (!unit.alive)
    {
        continue;
    }
    occupantByCell[rescueCellKey(unit.grid.x, unit.grid.y)] = unit.id;
}

for (auto& cell : state.rescue.cells)
{
    cell.occupied = false;
    cell.occupantUnitId = -1;
    const auto occupantIt = occupantByCell.find(rescueCellKey(cell.x, cell.y));
    if (occupantIt == occupantByCell.end())
    {
        continue;
    }
    cell.occupied = true;
    cell.occupantUnitId = occupantIt->second;
}
```

- [ ] **Step 3: Gate rescue sync when no damage can trigger rescue**

In `BattleFrameRunner::runFrame(...)`, replace:

```cpp
// Refresh rescue/protect snapshots from current runtime units before damage lifecycle consumes them.
syncRescueStateFromRuntimeUnits(state);
// Apply queued damage and lifecycle effects, e.g. HP loss, death, rescue, death AOE, battle end.
applyDamageAndLifecycle(state, result, frameCommands, gameplayEvents, logEvents, visualEvents);
```

with:

```cpp
// Refresh rescue/protect snapshots only when damage lifecycle can consume them.
if (!state.damage.pendingTransactions.empty())
{
    syncRescueStateFromRuntimeUnits(state);
}
// Apply queued damage and lifecycle effects, e.g. HP loss, death, rescue, death AOE, battle end.
applyDamageAndLifecycle(state, result, frameCommands, gameplayEvents, logEvents, visualEvents);
```

- [ ] **Step 4: Verify rescue behavior still works**

Run:

```powershell
x64\Debug\kys_tests.exe "*Rescue*"
```

Expected: all matching rescue tests pass.

---

## Task 4: Reduce Damage Application Input Copies

**Files:**
- Modify: `src/battle/BattleDamageApplicationSystem.h`
- Modify: `src/battle/BattleDamageApplicationSystem.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleDamageApplicationSystemUnitTests.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Change input fields to read-only pointers**

In `src/battle/BattleDamageApplicationSystem.h`, change `BattleDamageApplicationInput` fields:

```cpp
std::vector<BattleDamageTransactionInput> pendingTransactions;
std::vector<BattleDamagePresentationInput> pendingPresentation;
std::map<int, BattleDamageApplicationUnitEffects> unitEffects;
std::map<int, int> pendingAliveByTeam;
BattleProjectileFollowUpContext projectileFollowUps;
```

to:

```cpp
const std::vector<BattleDamageTransactionInput>* pendingTransactions = nullptr;
const std::vector<BattleDamagePresentationInput>* pendingPresentation = nullptr;
const std::map<int, BattleDamageApplicationUnitEffects>* unitEffects = nullptr;
const std::map<int, int>* pendingAliveByTeam = nullptr;
const BattleProjectileFollowUpContext* projectileFollowUps = nullptr;
```

- [ ] **Step 2: Wire references from runtime state**

In `src/battle/BattleCore.cpp`, update `makeFrameDamageApplicationInput(...)`.

Replace:

```cpp
input.pendingTransactions = state.damage.pendingTransactions;
input.pendingPresentation = state.damage.pendingPresentation;
input.unitEffects = state.damage.unitEffects;
input.pendingAliveByTeam = state.result.pendingAliveByTeam;
input.projectileFollowUps = state.projectileFollowUps;
```

with:

```cpp
input.pendingTransactions = &state.damage.pendingTransactions;
input.pendingPresentation = &state.damage.pendingPresentation;
input.unitEffects = &state.damage.unitEffects;
input.pendingAliveByTeam = &state.result.pendingAliveByTeam;
input.projectileFollowUps = &state.projectileFollowUps;
```

- [ ] **Step 3: Update damage system apply preconditions and local copy policy**

In `src/battle/BattleDamageApplicationSystem.cpp`, at the top of `BattleDamageApplicationSystem::apply(...)`, add invariants:

```cpp
assert(input.pendingTransactions);
assert(input.pendingPresentation);
assert(input.unitEffects);
assert(input.pendingAliveByTeam);
assert(input.projectileFollowUps);
```

Then replace:

```cpp
auto pendingTransactions = input.aggregatePendingTransactionsByDefender
    ? aggregatePendingTransactions(input.pendingTransactions)
    : input.pendingTransactions;
auto pendingPresentation = input.aggregatePendingTransactionsByDefender
    ? aggregatePendingPresentation(input.pendingTransactions, input.pendingPresentation)
    : input.pendingPresentation;
```

with:

```cpp
auto aggregatedTransactions = input.aggregatePendingTransactionsByDefender
    ? aggregatePendingTransactions(*input.pendingTransactions)
    : std::vector<BattleDamageTransactionInput>{};
auto aggregatedPresentation = input.aggregatePendingTransactionsByDefender
    ? aggregatePendingPresentation(*input.pendingTransactions, *input.pendingPresentation)
    : std::vector<BattleDamagePresentationInput>{};

const auto& pendingTransactions = input.aggregatePendingTransactionsByDefender
    ? aggregatedTransactions
    : *input.pendingTransactions;
const auto& pendingPresentation = input.aggregatePendingTransactionsByDefender
    ? aggregatedPresentation
    : *input.pendingPresentation;
```

Update all remaining reads:

```cpp
input.unitEffects
input.pendingAliveByTeam
input.projectileFollowUps
```

to dereference the pointer form:

```cpp
*input.unitEffects
*input.pendingAliveByTeam
*input.projectileFollowUps
```

- [ ] **Step 4: Update direct unit tests that construct `BattleDamageApplicationInput`**

In `tests/BattleDamageApplicationSystemUnitTests.cpp`, for each test-local input with local queues:

```cpp
std::vector<BattleDamageTransactionInput> pendingTransactions{ transaction };
std::vector<BattleDamagePresentationInput> pendingPresentation{ presentation };
std::map<int, BattleDamageApplicationUnitEffects> unitEffects;
std::map<int, int> pendingAliveByTeam;
BattleProjectileFollowUpContext projectileFollowUps;

BattleDamageApplicationInput input;
input.pendingTransactions = &pendingTransactions;
input.pendingPresentation = &pendingPresentation;
input.unitEffects = &unitEffects;
input.pendingAliveByTeam = &pendingAliveByTeam;
input.projectileFollowUps = &projectileFollowUps;
```

Do not allocate these temporaries inline if the input outlives the expression; keep them in the test scope.

- [ ] **Step 5: Verify damage tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][damage]"
x64\Debug\kys_tests.exe "[battle][core]"
```

Expected: all matching tests pass.

---

## Task 5: Use Role Bindings For Adapter Frame Apply Lookups

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/battle/BattleFind.h`
- Test: build and existing frame tests

- [ ] **Step 1: Change frame apply context to use the role map**

In `src/BattleSceneBattleAdapter.h`, change:

```cpp
struct BattleActionFrameApplyContext
{
    const std::vector<Role*>* roles = nullptr;
    int battleFrame = 0;
    float gravity = 0.0f;
};
```

to:

```cpp
struct BattleActionFrameApplyContext
{
    const std::unordered_map<int, Role*>* rolesByBattleId = nullptr;
    int battleFrame = 0;
    float gravity = 0.0f;
};
```

Change movement apply signature:

```cpp
void applyBattleMovementPresentationResults(
    const std::vector<Battle::BattleFrameMovementPresentationUnitResult>& movementResults,
    const std::vector<Role*>& roles);
```

to:

```cpp
void applyBattleMovementPresentationResults(
    const std::vector<Battle::BattleFrameMovementPresentationUnitResult>& movementResults,
    const std::unordered_map<int, Role*>& rolesByBattleId);
```

- [ ] **Step 2: Add an associative-map require helper to battle lookup utilities**

In `src/battle/BattleFind.h`, add this helper after `requireById(...)`:

```cpp
template<typename Map>
decltype(auto) requireMappedById(Map& map, int id)
{
    auto it = map.find(id);
    assert(it != map.end());
    return (it->second);
}
```

This intentionally uses the map's `find()` instead of the range-based `requireById(...)`, because `rolesByBattleId` is already an O(1) id-keyed map and should not be linearly scanned.

- [ ] **Step 3: Migrate movement and action frame apply helpers**

In `src/BattleSceneBattleAdapter.cpp`, include the helper:

```cpp
#include "battle/BattleFind.h"
```

In `applyBattleMovementPresentationResults(...)`, replace:

```cpp
auto* role = findRoleByBattleId(roles, result.unitId);
```

with:

```cpp
auto* role = Battle::requireMappedById(rolesByBattleId, result.unitId);
assert(role);
```

In `applyBattleActionFrameResults(...)`, replace:

```cpp
assert(context.roles);
auto* role = findRoleByBattleId(*context.roles, action.unitId);
```

with:

```cpp
assert(context.rolesByBattleId);
auto* role = Battle::requireMappedById(*context.rolesByBattleId, action.unitId);
assert(role);
```

Also replace other frame-time lookups inside this helper:

```cpp
findRoleByBattleId(*context.roles, event.sourceUnitId)
findRoleByBattleId(*context.roles, event.targetUnitId)
```

with:

```cpp
Battle::requireMappedById(*context.rolesByBattleId, event.sourceUnitId)
Battle::requireMappedById(*context.rolesByBattleId, event.targetUnitId)
```

- [ ] **Step 4: Pass bindings from scene**

In `src/BattleSceneHades.cpp`, update `makeBattleActionFrameApplyContext()`:

```cpp
BattleActionFrameApplyContext BattleSceneHades::makeBattleActionFrameApplyContext() const
{
    BattleActionFrameApplyContext context;
    context.rolesByBattleId = &core_role_bindings_.rolesByBattleId;
    context.battleFrame = battle_frame_;
    context.gravity = gravity_;
    return context;
}
```

Update movement apply call:

```cpp
applyBattleMovementPresentationResults(
    frameResult.movementPresentationResults,
    core_role_bindings_.rolesByBattleId);
```

- [ ] **Step 5: Remove or narrow the old linear helper**

Run:

```powershell
rg -n "findRoleByBattleId\(" src\BattleSceneBattleAdapter.*
```

If remaining uses are setup-only, keep the declaration but rename it to make that clear:

```cpp
Role* findSetupRoleByBattleId(const std::vector<Role*>& roles, int unitId);
```

If no uses remain, delete the declaration and definition.

- [ ] **Step 6: Build test target**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: build exits with code `0`.

---

## Task 6: Full Verification And Boundary Searches

**Files:**
- Verify all changed files.

- [ ] **Step 1: Run whitespace check**

Run:

```powershell
git diff --check
```

Expected: no output and exit code `0`.

- [ ] **Step 2: Run targeted boundary searches**

Run:

```powershell
rg -n "requireBy\(\s*result\.movementPresentationResults|stateApplications\.comboStates =|comboStates = applications\.comboStates|comboStates\.at\(" src tests
rg -n "for \(auto& cell : state\.rescue\.cells\)|for \(const auto& unit : state\.units\.units\)" src\battle\BattleCore.cpp
rg -n "findRoleByBattleId\(\*context\.roles|findRoleByBattleId\(roles, result\.unitId|const std::vector<Role\*>\\* roles|requireRoleByBattleId" src\BattleSceneBattleAdapter.* src\BattleSceneHades.cpp
```

Expected:
- No movement presentation linear overlay.
- No full combo-map assignment across the frame boundary.
- No frame-time adapter role lookup through `context.roles`.
- No adapter-specific map lookup helper when `Battle::requireMappedById(...)` is available.
- Any remaining rescue loops are not the old nested cell-by-unit occupancy scan.

- [ ] **Step 3: Build test target**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: MSBuild exits with code `0`.

- [ ] **Step 4: Run all tests**

Run:

```powershell
x64\Debug\kys_tests.exe
```

Expected: all tests pass.

- [ ] **Step 5: Build game target**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected: MSBuild exits with code `0`. Existing warning noise is acceptable; link failure only counts as acceptable if it is the known running-game executable lock.

---

## Self-Review

- Spec coverage: All five review findings map to one task each, with full verification in Task 6.
- Placeholder scan: No task uses TBD/TODO language; each code-changing step names concrete signatures or replacement snippets.
- Type consistency: `BattleFrameComboLegacyMirror`, `BattleFrameStateApplications::comboMirrors`, `BattleActionFrameApplyContext::rolesByBattleId`, and `Battle::requireMappedById(...)` are consistently named across tasks.
