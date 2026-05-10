# Battle Boundary Code Reduction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove dead frame payloads, one-use scene staging, broad setup mutation bridges, duplicated setup projections, and avoidable per-frame lookup work in the battle boundary.

**Architecture:** This is deletion-first. Keep the current runtime/scene separation, but remove unused synchronization formats and replace broad bridge structures with direct setup/runtime data. Runtime remains authoritative; scene setup supplies initial facts only.

**Tech Stack:** C++20, MSBuild solution `kys.sln`, Catch2 unit tests, existing battle runtime/session/adapter modules.

---

## File Map

- `src/battle/BattlePresentation.h/.cpp`: shrink `BattlePresentationFrame` to carry `frame` directly; remove unit snapshot output.
- `src/battle/BattlePresentationPlayback.h/.cpp`: remove copied playback snapshot.
- `src/battle/BattleCore.cpp`: remove runtime-to-presentation snapshot projection and begin presentation frames by frame number only.
- `src/BattleSceneHades.h/.cpp`: remove persistent `setup_units_`; pass setup rows directly into runtime initialization.
- `src/battle/BattleRuntimeSession.h/.cpp`: remove public `commitSetupConfiguration()` bridge; apply post-initialization configuration through construction/factory.
- `src/BattleSceneBattleAdapter.h/.cpp`: create fully configured sessions without exposing broad mutable setup after construction; reduce setup projection duplication.
- `src/battle/BattleMovement.h/.cpp`: replace collision-cell linear scans with dense indexed walkability.
- `src/battle/BattleRuntimeRules.h/.cpp`, `src/BattleSceneHades.cpp`: populate dense movement walkability.
- `src/battle/BattleRescueRepositionSystem.h/.cpp`, `src/battle/BattleCore.h/.cpp`, `src/BattleSceneBattleAdapter.h/.cpp`, `src/BattleSceneHades.cpp`: store rescue cell position on the cell row and remove the parallel position map.
- Tests: update presentation, frame runner, core, initialization, movement, rescue tests.

---

### Task 1: Delete Presentation Unit Snapshots

**Files:**
- Modify: `src/battle/BattlePresentation.h`
- Modify: `src/battle/BattlePresentation.cpp`
- Modify: `src/battle/BattlePresentationPlayback.h`
- Modify: `src/battle/BattlePresentationPlayback.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattlePresentationUnitTests.cpp`
- Modify: `tests/BattlePresentationPlaybackUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Remove snapshot unit types from presentation headers**

In `src/battle/BattlePresentation.h`, delete `BattlePresentationUnitSnapshot` and `BattlePresentationSnapshot`. Change `BattlePresentationFrame` to:

```cpp
struct BattlePresentationFrame
{
    int frame = 0;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
};
```

Change recorder declarations to:

```cpp
class BattlePresentationRecorder
{
public:
    void beginFrame(int frame);
    void recordGameplay(BattleGameplayEvent event);
    void recordLog(BattleLogEvent event);
    void recordVisual(BattleVisualEvent event);

    const BattlePresentationFrame& frame() const;
    BattlePresentationFrame consumeFrame();

private:
    BattlePresentationFrame frame_;
};
```

- [ ] **Step 2: Update presentation recorder implementation**

In `src/battle/BattlePresentation.cpp`, delete `beginFrame(BattlePresentationSnapshot snapshot)`. Keep one `beginFrame(int frame)`:

```cpp
void BattlePresentationRecorder::beginFrame(int frame)
{
    frame_ = {};
    frame_.frame = frame;
}
```

Update stamping calls:

```cpp
stampFrame(event.frame, frame_.frame);
```

- [ ] **Step 3: Remove playback plan snapshot**

In `src/battle/BattlePresentationPlayback.h`, change:

```cpp
struct BattlePresentationPlaybackPlan
{
    std::vector<BattlePresentationCommand> commands;
};
```

In `src/battle/BattlePresentationPlayback.cpp`, delete:

```cpp
plan.snapshot = frame.snapshot;
```

- [ ] **Step 4: Delete runtime presentation snapshot projection**

In `src/battle/BattleCore.cpp`, delete these helpers entirely:

```cpp
BattlePresentationUnitSnapshot toPresentationUnit(const BattleUnitState& unit);
void applyUnitStoreSnapshot(BattlePresentationUnitSnapshot& snapshot, const BattleUnitStore& units);
BattlePresentationSnapshot makePresentationSnapshot(const BattleRuntimeState& state);
```

Change `emitPresentationFrame()` to:

```cpp
BattlePresentationRecorder recorder;
recorder.beginFrame(state.world.frame);
```

- [ ] **Step 5: Update tests**

Replace snapshot assertions with frame assertions:

```cpp
CHECK(frame.frame == 42);
CHECK(result.frame.frame == state.world.frame);
CHECK(plan.commands.size() == expectedCount);
```

Delete tests whose only purpose is asserting `snapshot.units`.

- [ ] **Step 6: Verify task 1**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: build succeeds and tests pass.

---

### Task 2: Remove One-Use `setup_units_` Scene Staging

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

- [ ] **Step 1: Change runtime initialization signatures**

In `src/BattleSceneHades.h`, replace:

```cpp
void initializeBattleRuntime();
KysChess::BattleSceneBattleAdapter::BattleRuntimeBuildContext makeBattleRuntimeBuildContext();
std::vector<KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput> setup_units_;
```

with:

```cpp
void initializeBattleRuntime(std::vector<KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput> setupUnits);
KysChess::BattleSceneBattleAdapter::BattleRuntimeBuildContext makeBattleRuntimeBuildContext(
    std::vector<KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput> setupUnits);
```

- [ ] **Step 2: Move setup rows directly into build context**

In `src/BattleSceneHades.cpp`, change `makeBattleRuntimeBuildContext()` to accept the vector and move it:

```cpp
KysChess::BattleSceneBattleAdapter::BattleRuntimeBuildContext BattleSceneHades::makeBattleRuntimeBuildContext(
    std::vector<KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput> setupUnits)
{
    KysChess::BattleSceneBattleAdapter::BattleRuntimeBuildContext context;
    context.rules = KysChess::Battle::makeHadesBattleRuntimeRules(TILE_W, BATTLE_COORD_COUNT);
    context.randomSeed = battle_random_seed_;
    context.rules.movementPhysicsConfig.gravity = gravity_;
    context.rules.movementPhysicsConfig.friction = friction_;
    context.setup.units = std::move(setupUnits);
    ...
}
```

Change `initializeBattleRuntime()`:

```cpp
void BattleSceneHades::initializeBattleRuntime(
    std::vector<KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput> setupUnits)
{
    KysChess::ChessCombo::clearActiveStates();
    auto buildContext = makeBattleRuntimeBuildContext(std::move(setupUnits));
    ...
}
```

- [ ] **Step 3: Use local setup rows in `onEntrance()`**

Replace:

```cpp
setup_units_ = KysChess::BattleSceneSetupBuilder::buildSetupUnits(setupRequests).units;
...
initializeBattleRuntime();
```

with:

```cpp
auto setupUnits = KysChess::BattleSceneSetupBuilder::buildSetupUnits(setupRequests).units;
...
initializeBattleRuntime(std::move(setupUnits));
```

- [ ] **Step 4: Verify task 2**

Run:

```powershell
rg -n "setup_units_" src\BattleSceneHades.cpp src\BattleSceneHades.h
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: `rg` finds no `setup_units_`; build/tests pass.

---

### Task 3: Remove Public Post-Construction Setup Configuration Bridge

**Files:**
- Modify: `src/battle/BattleRuntimeSession.h`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Make setup configuration factory-owned**

In `src/battle/BattleRuntimeSession.h`, keep the normal constructor, add a static factory, and remove public commit:

```cpp
class BattleRuntimeSession
{
public:
    explicit BattleRuntimeSession(BattleRuntimeInit init);

    template<typename SetupConfigurationFactory>
    static BattleRuntimeSession createConfigured(
        BattleRuntimeInit init,
        SetupConfigurationFactory&& makeSetupConfiguration)
    {
        BattleRuntimeSession session(std::move(init));
        session.applySetupConfiguration(makeSetupConfiguration(session.runtime()));
        return session;
    }

    BattleFrameResult runFrame();
    void commitSetupPlacement(const BattleSetupPlacementInput& input);
    BattleInitializationResult releaseInitializationResult();

    const BattleRuntimeState& runtime() const;

private:
    void applySetupConfiguration(BattleRuntimeSetupConfiguration config);

    BattleRuntimeState runtime_;
    std::optional<BattleInitializationResult> initialization_result_;
    BattleFrameRunner runner_;
    bool setupConfigured_ = false;
    bool setupPlacementCommitted_ = false;
    bool frameStarted_ = false;
};
```

- [ ] **Step 2: Move commit body into private helper**

In `src/battle/BattleRuntimeSession.cpp`, replace `commitSetupConfiguration()` with:

```cpp
void BattleRuntimeSession::applySetupConfiguration(BattleRuntimeSetupConfiguration config)
{
    assert(!setupConfigured_);
    assert(!frameStarted_);
    setupConfigured_ = true;

    runtime_.world = std::move(config.world);
    runtime_.attacks = std::move(config.attacks);
    runtime_.damage = std::move(config.damage);
    runtime_.result = std::move(config.result);
    runtime_.teamEffects = std::move(config.teamEffects);
    runtime_.rescue = std::move(config.rescue);
    runtime_.movementPhysics = std::move(config.movementPhysics);
    runtime_.action = std::move(config.action);
    runtime_.projectileFollowUps = std::move(config.projectileFollowUps);
    runtime_.deathEffects = std::move(config.deathEffects);
}
```

Delete the public `commitSetupConfiguration()` definition.

- [ ] **Step 3: Let adapter construct a configured session**

In `src/BattleSceneBattleAdapter.cpp`, split creation into:

```cpp
Battle::BattleRuntimeInit makeBattleRuntimeInit(const BattleRuntimeBuildContext& context);
Battle::BattleRuntimeSetupConfiguration makeInitializedBattleRuntimeConfiguration(
    const BattleRuntimeBuildContext& context,
    const Battle::BattleRuntimeState& runtime);
```

Construct in `createInitializedBattleRuntimeSession()` as:

```cpp
auto init = makeBattleRuntimeInit(context);
auto session = Battle::BattleRuntimeSession::createConfigured(
    std::move(init),
    [&context](const Battle::BattleRuntimeState& initializedRuntime)
    {
        return makeInitializedBattleRuntimeConfiguration(context, initializedRuntime);
    });
auto initialization = session.releaseInitializationResult();
auto sceneInitialization = makeBattleInitializationSceneApplyResult(context.setup, initialization, session.runtime());
```

The public API must not expose `commitSetupConfiguration()`.

- [ ] **Step 4: Update tests**

In `tests/BattleFrameRunnerRuntimeUnitTests.cpp`, replace direct `session.commitSetupConfiguration(std::move(config));` with constructing the session through the new configured constructor/factory:

```cpp
auto session = BattleRuntimeSession::createConfigured(
    std::move(init),
    [&config](const BattleRuntimeState&)
    {
        return std::move(config);
    });
```

- [ ] **Step 5: Verify task 3**

Run:

```powershell
rg -n "commitSetupConfiguration" src tests
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: no public call sites remain; build/tests pass.

---

### Task 4: Collapse Adapter Projection Duplication

**Files:**
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleStatusSystem.h`
- Modify: `src/battle/BattleStatusSystem.cpp`

- [ ] **Step 1: Add core projection helpers from runtime unit**

Expose or add helpers:

```cpp
BattleUnitState makeBattleWorldUnitState(const BattleRuntimeUnit& unit, double moveSpeedDivisor);
BattleDamageUnitState makeBattleDamageUnitState(const BattleRuntimeUnit& unit, const RoleComboState* state);
BattleStatusUnitState makeBattleStatusUnitState(const BattleRuntimeUnit& unit, const RoleComboState& state);
```

These helpers copy grouped fields directly:

```cpp
BattleUnitState makeBattleWorldUnitState(const BattleRuntimeUnit& runtimeUnit, double moveSpeedDivisor)
{
    BattleUnitState unit;
    unit.id = runtimeUnit.id;
    unit.realRoleId = runtimeUnit.realRoleId;
    unit.name = runtimeUnit.name;
    unit.team = runtimeUnit.team;
    unit.alive = runtimeUnit.alive;
    unit.position = runtimeUnit.motion.position;
    unit.velocity = runtimeUnit.motion.velocity;
    unit.speed = runtimeUnit.stats.speed / moveSpeedDivisor;
    unit.star = runtimeUnit.star;
    unit.canAttack = runtimeUnit.animation.cooldown == 0;
    return unit;
}
```

- [ ] **Step 2: Remove one-line projection accessor class**

In `src/BattleSceneBattleAdapter.cpp`, delete `BattleSetupRoleProjection`.

Keep one direct setup-to-runtime function:

```cpp
Battle::BattleRuntimeUnit makeBattleRuntimeUnit(
    const BattleSetupUnitInput& setup,
    const RoleComboState* state,
    const Battle::BattleGridTransform& gridTransform)
{
    Battle::BattleRuntimeUnit unit;
    unit.id = setup.unitId;
    unit.realRoleId = setup.realRoleId;
    unit.name = setup.name;
    unit.team = setup.team;
    unit.alive = setup.alive;
    unit.vitals = setup.vitals;
    unit.stats = setup.stats;
    unit.motion = setup.motion;
    unit.animation = setup.animation;
    unit.haveAction = setup.haveAction;
    unit.operationType = setup.operationType;
    unit.operationCount = setup.operationCount;
    unit.physicalPower = setup.physicalPower;
    unit.invincible = setup.invincible;
    unit.hurtFrame = setup.hurtFrame;
    for (int magicType = 0; magicType <= 4; ++magicType)
    {
        unit.actPropertiesByMagicType.emplace(magicType, setup.actPropertiesByMagicType[magicType]);
    }
    unit.grid = gridTransform.toGrid(setup.motion.position);
    if (state)
    {
        unit.shield = state->shield;
        unit.mpBlocked = state->mpBlockTimer > 0;
        unit.mpRecoveryBonusPct = state->mpRecoveryBonusPct;
    }
    return unit;
}
```

- [ ] **Step 3: Derive world/status/damage rows from runtime units**

Replace adapter calls:

```cpp
makeBattleWorldUnit(role)
makeBattleStatusUnit(role, combo)
makeBattleDamageUnit(role, combo)
makeBattleDamageUnit(runtimeUnit, combo)
```

with core helpers using `BattleRuntimeUnit`.

- [ ] **Step 4: Verify task 4**

Run:

```powershell
rg -n "BattleSetupRoleProjection|makeBattleStatusUnit\\(|makeBattleDamageUnit\\(const BattleSetupUnitInput|makeBattleWorldUnit\\(const BattleSetupUnitInput" src\BattleSceneBattleAdapter.cpp
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: projection class and setup-row status/damage/world helpers are gone; build/tests pass.

---

### Task 5: Replace Movement Collision Cell Scans With Dense Walkability

**Files:**
- Modify: `src/battle/BattleMovement.h`
- Modify: `src/battle/BattleMovement.cpp`
- Modify: `src/battle/BattleRuntimeRules.h`
- Modify: `src/battle/BattleRuntimeRules.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleMovementRealStatsTests.cpp`

- [ ] **Step 1: Replace collision cell vector**

In `BattleMovementPhysicsCollisionWorld`, replace:

```cpp
std::vector<BattleMovementPhysicsCollisionCellSnapshot> cells;
```

with:

```cpp
std::vector<std::uint8_t> walkableByCell;
```

Add helper declarations:

```cpp
std::size_t movementPhysicsCellIndex(const BattleMovementPhysicsCollisionWorld& world, int x, int y);
bool movementPhysicsCellWalkable(const BattleMovementPhysicsCollisionWorld& world, Point cell);
```

- [ ] **Step 2: Implement dense lookup**

In `src/battle/BattleMovement.cpp`:

```cpp
std::size_t movementPhysicsCellIndex(const BattleMovementPhysicsCollisionWorld& world, int x, int y)
{
    assert(world.coordCount > 0);
    assert(x >= 0);
    assert(y >= 0);
    assert(x < world.coordCount);
    assert(y < world.coordCount);
    return static_cast<std::size_t>(y * world.coordCount + x);
}

bool movementPhysicsCellWalkable(const BattleMovementPhysicsCollisionWorld& world, Point cell)
{
    if (cell.x < 0 || cell.y < 0 || cell.x >= world.coordCount || cell.y >= world.coordCount)
    {
        return false;
    }
    const auto index = movementPhysicsCellIndex(world, cell.x, cell.y);
    assert(index < world.walkableByCell.size());
    return world.walkableByCell[index] != 0;
}
```

The boundary check is a true possible condition because physics probes can leave the map.

- [ ] **Step 3: Populate dense walkability**

In `BattleSceneHades::makeBattleRuntimeBuildContext()`, replace `movementCollisionWorld.cells.push_back(...)` with:

```cpp
context.rules.movementCollisionWorld.walkableByCell.assign(
    BATTLE_COORD_COUNT * BATTLE_COORD_COUNT,
    0);
...
const auto index = static_cast<std::size_t>(y * BATTLE_COORD_COUNT + x);
context.rules.movementCollisionWorld.walkableByCell[index] = walkable ? 1 : 0;
```

- [ ] **Step 4: Update tests**

Replace test setup loops that push cells with:

```cpp
state.movementPhysics.collision.walkableByCell.assign(64 * 64, 1);
```

For blocked cells:

```cpp
state.movementPhysics.collision.walkableByCell[y * 64 + x] = 0;
```

- [ ] **Step 5: Verify task 5**

Run:

```powershell
rg -n "movementPhysics\\.collision\\.cells|BattleMovementPhysicsCollisionCellSnapshot" src tests
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: old collision cell vector is gone; build/tests pass.

---

### Task 6: Merge Rescue Cell Position Into Rescue Cell Snapshot

**Files:**
- Modify: `src/battle/BattleRescueRepositionSystem.h`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleRescueRepositionSystemUnitTests.cpp`

- [ ] **Step 1: Add position to rescue cell**

In `BattleRescueCellSnapshot`, add:

```cpp
Pointf position;
```

Keep fields:

```cpp
int x = 0;
int y = 0;
bool walkable = false;
bool occupied = false;
int occupiedByUnitId = -1;
Pointf position;
```

- [ ] **Step 2: Remove parallel position maps**

Delete:

```cpp
std::map<std::pair<int, int>, Pointf> rescuePositionsByCell;
std::map<std::pair<int, int>, Pointf> positionsByCell;
```

from adapter setup and runtime rescue state.

- [ ] **Step 3: Populate cell position directly**

In `BattleSceneHades::makeBattleRuntimeBuildContext()`, change rescue cell construction to:

```cpp
context.setup.rescueCells.push_back({
    x,
    y,
    walkable,
    false,
    -1,
    position,
});
```

Delete the `rescuePositionsByCell.emplace(...)` call.

- [ ] **Step 4: Use cell position directly in runtime**

In `src/battle/BattleCore.cpp`, replace the lookup helper with:

```cpp
Pointf rescueCellPosition(const BattleRescueCellSnapshot& cell)
{
    return cell.position;
}
```

Update callers that previously looked in `state.rescue.positionsByCell`.

- [ ] **Step 5: Update tests**

Update helper constructors:

```cpp
BattleRescueCellSnapshot rescueCell(int x, int y, bool walkable = true, bool occupied = false)
{
    return {
        x,
        y,
        walkable,
        occupied,
        -1,
        { static_cast<float>(x * 10), static_cast<float>(y * 10), 0 },
    };
}
```

- [ ] **Step 6: Verify task 6**

Run:

```powershell
rg -n "rescuePositionsByCell|positionsByCell" src tests
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: no parallel rescue position map remains; build/tests pass.

---

## Final Verification

Run:

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Run reduction searches:

```powershell
rg -n "BattlePresentationUnitSnapshot|snapshot\.units|makePresentationSnapshot" src tests
rg -n "setup_units_|commitSetupConfiguration|BattleSetupRoleProjection" src tests
rg -n "movementPhysics\.collision\.cells|BattleMovementPhysicsCollisionCellSnapshot" src tests
rg -n "rescuePositionsByCell|positionsByCell" src tests
```

Expected:
- no presentation unit snapshot output remains;
- no persistent scene setup staging remains;
- no public post-construction setup configuration call remains;
- no adapter projection class remains;
- no linear movement collision cell vector remains;
- no parallel rescue position map remains.
