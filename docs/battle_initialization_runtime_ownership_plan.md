# Battle Initialization Runtime Ownership Plan

## Goal
Move battle setup gameplay rules out of `BattleSceneHades` by constructing `Battle::BattleRuntimeState` from legacy setup once, then running a core-owned initialization pass that mutates runtime state and emits explicit setup output for the scene to mirror.

## Non-Negotiable Direction
The initialization order must become:

1. `BattleSceneHades` creates and positions base legacy `Role` battle copies.
2. Adapter takes the complete legacy setup boundary and creates a self-initialized `BattleRuntimeSession` once.
3. During session creation, `BattleInitializationSystem` consumes `BattleRuntimeInit::setup` and mutates `BattleRuntimeState` directly.
4. During session creation, `BattleInitializationSystem` creates runtime-owned clone units and returns `BattleInitializationResult`.
5. `BattleSceneHades` mirrors result values to legacy `Role*`, creates legacy clone render objects from clone intents, and plays returned log/visual output.
6. The pre-battle position swap runs against the fully initialized legacy battle list, including clones.
7. Adapter commits final setup placement back into `BattleRuntimeState` once, after the swap and before the first frame.
8. `BattleFrameRunner` starts from the already-initialized, finally placed runtime state.

The position swap is setup input, not frame input. It is the only allowed post-initialization scene-to-runtime write in this plan, and it may only update position/facing fields for units that already exist in runtime.

The scene must not build a separate core initialization input or call `BattleInitializationSystem`. If a value is needed by initialization, the adapter stores it in `BattleRuntimeInit::setup` while constructing the runtime session. `BattleRuntimeState` must not contain setup seed/input members.

The runtime setup boundary passes facts, not derived battle state. Do not pass values that can be computed from translated roster/combo/effect state:

- do not pass `allyTeamFlatShield` or `enemyTeamFlatShield`; compute team flat shield inside backend initialization from `RoleComboState::appliedEffects`;
- do not pass pre-collected `allyGlobalEffects` or `enemyGlobalEffects` if active combos are available; collect team-wide combo effects inside backend setup initialization;
- do not pass equipment-derived `ComboEffect`s if equipment item IDs are available; resolve and apply equipment effects inside backend setup initialization;
- do not pass `allyStates` / `enemyStates` as scene-owned derived state if the runtime setup can own combo detection. Prefer raw setup roster facts plus combo/equipment/effect catalogs, then let runtime initialization build combo state.

Do not implement:

- scene applies initialization to `Role*`, then runtime imports the already-mutated roles;
- per-unit callbacks from core into scene;
- `std::function` lookups for role, equipment, terrain, or logs;
- moving current setup lambdas wholesale into `BattleSceneBattleAdapter`;
- creating an empty `BattleRuntimeSession` and then filling it through multiple import calls;
- importing all role values again after the position swap;
- passing `BattleRuntimeSetupSeed` from scene orchestration into `BattleInitializationSystem`;
- passing precomputed team shield, global effects, equipment effects, or scene-built combo state as initialization shortcuts;
- a compatibility path that keeps the old scene setup logic alive.

## Current Ordering Fact To Preserve
Today clones are created before the player can swap positions:

1. Allies are copied and added to `battle_roles_`.
2. The scene creates clone `Role` objects, pushes them into `friends_obj_` and `battle_roles_`, writes clone combo state, and records the clone log.
3. The player chooses map/list/no swap.
4. `runPositionSwapLoop()` and `runListBasedSwap()` operate over friendly roles in `battle_roles_`, so cloned allies are eligible for the same setup placement flow.
5. `initializeBattleRuntimeSession()` imports runtime after the swap.

The runtime-owned design must therefore not move clone creation after player swap. The corrected direction is runtime clone first, legacy clone mirror second, player swap third, one-time final placement commit fourth.

## Current Scene-Owned Setup To Cut
These are the scene-owned gameplay/setup paths this plan targets:

- `BattleSceneHades.cpp:1679-1761`
  - combo stat mutation on battle copies;
  - `shieldPctMaxHP`;
  - team flat shield;
  - equipment/global effect application;
  - timed counter initialization;
  - block-first-hit initialization;
  - setup shield logs.
- `BattleSceneHades.cpp:1762`
  - `updateEnemyTopDebuffState()` is called before runtime exists.
- `BattleSceneHades.cpp:1883-1955`
  - clone spawning decides legal cells, creates clone `Role`, writes clone combo state, and records clone logs.
- `BattleSceneHades.cpp:2511-2613`
  - `updateEnemyTopDebuffState()` computes target order, mutates enemy attack/defence, updates combo state, and records logs.
- `BattleSceneHades.cpp:697-708`
  - `recordBattleStatusLog()` lets setup write battle facts directly from scene.
- `BattleSceneHades.cpp:744-817`
  - `initializeBattleRuntimeSession()` currently imports runtime after setup has already mutated scene-owned copies.

## New Files And Responsibilities

### Create `src/battle/BattleInitialization.h`
Owns pure initialization API and output types. It must not include or reference `Role`, `Magic`, `Item`, scene, engine, font, texture, save, or database types.

Required types:

```cpp
namespace KysChess::Battle
{

struct BattleInitializationUnitSeed
{
    int unitId = -1;
    int realRoleId = -1;
    int team = 0;
    int star = 0;
    int cost = 0;
    int baseMaxHp = 0;
    int baseAttack = 0;
    int baseDefence = 0;
    int baseSpeed = 0;
    RoleComboState baseCombo;
};

struct BattleInitializationCloneSpawnCell
{
    int x = 0;
    int y = 0;
    bool walkable = false;
    bool occupied = false;
};

struct BattleInitializationCloneSource
{
    int sourceUnitId = -1;
    int sourceRealRoleId = -1;
    int power = 0;
};

struct BattleRuntimeSetupSeed
{
    std::vector<BattleInitializationUnitSeed> units;
    std::vector<BattleInitializationCloneSource> cloneSources;
    std::vector<BattleInitializationCloneSpawnCell> cloneCells;
    int cloneSummonCount = 0;
    int nextCloneUnitId = 100000;
};

struct BattleRuntimeInit
{
    BattleRuntimeState runtime;
    BattleRuntimeSetupSeed setup;
};

struct BattleInitializationRoleDelta
{
    int unitId = -1;
    int maxHp = 0;
    int hp = 0;
    int attack = 0;
    int defence = 0;
    int speed = 0;
};

struct BattleInitializationCloneIntent
{
    int sourceUnitId = -1;
    int cloneUnitId = -1;
    int gridX = 0;
    int gridY = 0;
    BattleInitializationRoleDelta roleValues;
    RoleComboState combo;
};

struct BattleInitializationEnemyTopDebuffDelta
{
    int unitId = -1;
    int attackDelta = 0;
    int defenceDelta = 0;
    int appliedValue = 0;
};

struct BattleInitializationResult
{
    std::vector<BattleInitializationRoleDelta> roleDeltas;
    std::vector<BattleInitializationCloneIntent> cloneIntents;
    std::vector<BattleInitializationEnemyTopDebuffDelta> enemyTopDebuffs;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
};

class BattleInitializationSystem
{
public:
    BattleInitializationResult initialize(BattleRuntimeState& runtime,
                                          const BattleRuntimeSetupSeed& setup) const;
};

}  // namespace KysChess::Battle
```

### Create `src/battle/BattleInitialization.cpp`
Owns initialization rules:

- apply resolved `ComboEffect`s to `RoleComboState`;
- read setup data from the `BattleRuntimeSetupSeed` parameter;
- mutate `runtime.combo.units`;
- mutate `runtime.units.units`;
- mutate `runtime.status.units`;
- set shield values;
- initialize timers;
- initialize block-first-hit counters;
- compute clone intents and append clone runtime state before scene creates legacy clone objects;
- compute enemy-top debuff deltas against runtime units and combo state;
- emit `BattleLogEvent`s for setup facts.

### Modify `src/BattleSceneBattleAdapter.h/.cpp`
Owns legacy-to-core import and core-to-legacy apply only:

- build `BattleRuntimeInit` from the complete legacy setup boundary;
- store setup seed data in `BattleRuntimeInit::setup` during that construction;
- apply `BattleInitializationResult` to `Role*` and `ChessCombo::getMutableStates()`;
- create legacy clone `Role` objects from `BattleInitializationCloneIntent`.
- commit final setup placement into `BattleRuntimeState` once after the pre-battle swap.

It may touch `Role*`, `Magic*`, `Item*`, save/database, and scene-adjacent helpers because it is a boundary object. It must not decide gameplay rules that the new core initialization system owns.

### Modify `src/BattleSceneHades.cpp/.h`
Becomes the orchestrator:

- creates base `Role` battle objects and positions them;
- asks adapter to construct `BattleRuntimeInit` from legacy setup;
- asks adapter to create a self-initialized `BattleRuntimeSession`;
- applies returned result through adapter;
- records returned logs/visuals;
- runs pre-battle position swap after clone mirror;
- asks adapter to commit final setup placement to runtime exactly once;
- deletes direct setup logs and setup gameplay mutation.

`BattleSceneHades` may assemble a boundary-only `BattleRuntimeBuildContext` because legacy setup facts still live in save/database/combo/equipment containers. That context must not cross into `src/battle`; it exists only long enough for the adapter to build `BattleRuntimeInit`.

### Modify tests

- Add `tests/BattleInitializationUnitTests.cpp`.
- Update `tests/kys_tests.vcxproj` if this project does not auto-include new test files.
- Add scene/adapter compile coverage through existing `kys` and `kys_tests` builds.

## Dependency-Ordered Tasks

### Task 1: Add Core Initialization Type Skeleton

**Files**
- Create: `src/battle/BattleInitialization.h`
- Create: `src/battle/BattleInitialization.cpp`
- Test: `tests/BattleInitializationUnitTests.cpp`

**Steps**

1. Add `BattleInitialization.h` with the exact public types listed above.
2. Add `BattleInitialization.cpp` with `BattleInitializationSystem::initialize()` returning an empty `BattleInitializationResult`.
3. Add a compile-only test:

```cpp
BattleRuntimeUnit runtimeUnit(int id, int team, int maxHp, int attack, int defence, int speed)
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = maxHp > 0;
    unit.hp = maxHp;
    unit.maxHp = maxHp;
    unit.attack = attack;
    unit.defence = defence;
    unit.speed = speed;
    return unit;
}

BattleStatusRuntimeUnit statusRuntime(int id, int hp, int attack)
{
    BattleStatusRuntimeUnit unit;
    unit.id = id;
    unit.alive = hp > 0;
    unit.hp = hp;
    unit.maxHp = hp;
    unit.attack = attack;
    return unit;
}

TEST_CASE("BattleInitializationSystem_CompilesPureRuntimeInitializationApi", "[battle][initialization]")
{
    BattleRuntimeState runtime;
    BattleRuntimeSetupSeed setup;

    auto result = BattleInitializationSystem().initialize(runtime, setup);

    CHECK(result.roleDeltas.empty());
    CHECK(result.cloneIntents.empty());
    CHECK(result.logEvents.empty());
}
```

4. Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

5. Commit:

```powershell
git add src\battle\BattleInitialization.h src\battle\BattleInitialization.cpp tests\BattleInitializationUnitTests.cpp tests\kys_tests.vcxproj
git commit -m "refactor: add battle initialization boundary"
```

### Task 2: Move Combo Stat, Shield, And Timer Initialization Into Core

**Files**
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`

**Core behavior**

For each `BattleInitializationUnitSeed`:

- start from `seed.baseCombo`;
- collect and apply team-wide combo effects from `setup` active combo data;
- resolve and apply equipment effects from `setup` equipment data;
- resolve and apply obtained neigong effects from `setup` neigong data;
- mutate runtime unit values:
  - `maxHp += flatHP`;
  - `attack += flatATK`;
  - `defence += flatDEF`;
  - `speed += flatSPD`;
  - percent values apply after flat values;
  - `hp = maxHp`;
- apply `shieldPctMaxHP`;
- compute team flat shield from initialized `RoleComboState::appliedEffects` once per team and add it to each same-team unit;
- initialize:
  - `damageImmunityTimer = damageImmunityAfterFrames`;
  - `autoUltimateTimer = autoUltimateAfterFrames`;
  - `blockFirstHitsRemaining = blockFirstHitsCount`;
- write initialized combo into `runtime.combo.units[unitId]`;
- update `runtime.status.units` for `hp`, `maxHp`, `attack`, and status timers;
- emit setup shield logs as `BattleLogEvent`.

**Required tests**

Add tests with concrete assertions:

```cpp
TEST_CASE("BattleInitializationSystem_AppliesComboStatsToImportedRuntimeUnit", "[battle][initialization]")
{
    BattleRuntimeState runtime;
    runtime.units.units.push_back(runtimeUnit(10, 0, 100, 20, 30, 40));
    runtime.status.units.push_back(statusRuntime(10, 100, 20));

    RoleComboState combo;
    combo.flatHP = 25;
    combo.flatATK = 5;
    combo.flatDEF = 3;
    combo.flatSPD = 2;
    combo.pctHP = 20;
    combo.pctATK = 10;
    combo.pctDEF = 50;

    BattleRuntimeSetupSeed setup;
    BattleInitializationUnitSeed seed;
    seed.unitId = 10;
    seed.realRoleId = 1001;
    seed.team = 0;
    seed.baseMaxHp = 100;
    seed.baseAttack = 20;
    seed.baseDefence = 30;
    seed.baseSpeed = 40;
    seed.baseCombo = combo;
    setup.units.push_back(seed);

    auto result = BattleInitializationSystem().initialize(runtime, setup);

    const auto& unit = runtime.units.requireUnit(10);
    CHECK(unit.maxHp == 150);
    CHECK(unit.hp == 150);
    CHECK(unit.attack == 27);
    CHECK(unit.defence == 49);
    CHECK(unit.speed == 42);
    REQUIRE(result.roleDeltas.size() == 1);
    CHECK(result.roleDeltas[0].unitId == 10);
    CHECK(result.roleDeltas[0].maxHp == 150);
}
```

```cpp
TEST_CASE("BattleInitializationSystem_InitializesShieldTimersAndBlockCounters", "[battle][initialization]")
{
    BattleRuntimeState runtime;
    runtime.units.units.push_back(runtimeUnit(10, 0, 200, 20, 30, 40));
    runtime.status.units.push_back(statusRuntime(10, 200, 20));

    RoleComboState combo;
    combo.shieldPctMaxHP = 25;
    combo.damageImmunityAfterFrames = 12;
    combo.autoUltimateAfterFrames = 30;
    combo.blockFirstHitsCount = 2;
    AppliedEffectInstance teamShield;
    teamShield.type = EffectType::FlatShield;
    teamShield.trigger = Trigger::Always;
    teamShield.value = 15;
    teamShield.sourceComboId = 77;
    combo.appliedEffects.push_back(teamShield);

    BattleRuntimeSetupSeed setup;
    BattleInitializationUnitSeed seed;
    seed.unitId = 10;
    seed.realRoleId = 1001;
    seed.team = 0;
    seed.baseMaxHp = 200;
    seed.baseAttack = 20;
    seed.baseDefence = 30;
    seed.baseSpeed = 40;
    seed.baseCombo = combo;
    setup.units.push_back(seed);

    auto result = BattleInitializationSystem().initialize(runtime, setup);

    const auto& initialized = runtime.combo.units.at(10);
    CHECK(initialized.shield == 65);
    CHECK(initialized.damageImmunityTimer == 12);
    CHECK(initialized.autoUltimateTimer == 30);
    CHECK(initialized.blockFirstHitsRemaining == 2);
    REQUIRE(result.logEvents.size() == 2);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 10);
    CHECK(result.logEvents[0].text == "獲取65護盾");
    CHECK(result.logEvents[1].text == "全隊獲取15護盾");
}
```

**Deletion gate**
Do not touch scene setup yet in this task. This task only proves the core can own the rules.

**Commit**

```powershell
git add src\battle\BattleInitialization.cpp tests\BattleInitializationUnitTests.cpp
git commit -m "refactor: initialize combo setup in runtime"
```

### Task 3: Construct Runtime Before Initialization

**Files**
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`

**New adapter APIs**

Add:

```cpp
struct BattleRuntimeBuildContext
{
    std::vector<Role*> roles;
    std::map<int, RoleComboState>* comboStates = nullptr;
    Battle::BattleGridTransform gridTransform;
    Battle::BattleMovementConfig movementConfig;
    std::vector<BattleTerrainCell> terrainCells;
    std::vector<BattleSetupRosterUnit> allyRoster;
    std::vector<BattleSetupRosterUnit> enemyRoster;
    std::vector<BattleSetupComboDefinition> comboDefinitions;
    std::vector<BattleSetupEquipmentDefinition> equipmentDefinitions;
    std::vector<BattleSetupNeigongDefinition> neigongDefinitions;
    std::vector<int> obtainedNeigongMagicIds;
    std::vector<std::pair<int, int>> cloneSpawnCells;
    int nextCloneUnitId = 100000;
};

struct BattleRuntimeCreationResult
{
    Battle::BattleRuntimeSession session;
    Battle::BattleInitializationResult initializationResult;
    std::unordered_map<int, Role*> rolesByBattleId;
};

BattleRuntimeCreationResult createInitializedBattleRuntimeSession(
    const BattleRuntimeBuildContext& context);
```

Behavior:

- create a local `BattleRuntimeState runtime`;
- fill `runtime.units.units`, `runtime.status.units`, `runtime.combo.units`, and `runtime.world.units` from `context.roles`;
- create a local `BattleRuntimeSetupSeed setup` from legacy setup seed data in `context`;
- set static terrain/config fields available before setup initialization;
- construct `BattleRuntimeSession` with `BattleRuntimeInit{ std::move(runtime), std::move(setup) }`;
- `BattleRuntimeSession` runs setup initialization internally before it can run frames;
- after initialization succeeds, `BattleRuntimeSession` discards the consumed setup seed; frame code cannot read setup input because it is not stored in `BattleRuntimeState`;
- return the self-initialized session, its `BattleInitializationResult`, and `rolesByBattleId`;
- do not mutate an already-created `BattleRuntimeSession`.

**Scene order change**

Replace empty-session-plus-imports with one construction call:

```cpp
KysChess::BattleSceneBattleAdapter::BattleRuntimeBuildContext BattleSceneHades::makeBattleRuntimeBuildContext() const
{
    KysChess::BattleSceneBattleAdapter::BattleRuntimeBuildContext context;
    context.roles = battle_roles_;
    context.comboStates = &KysChess::ChessCombo::getMutableStates();
    context.gridTransform = { TILE_W, BATTLE_COORD_COUNT };
    context.movementConfig = makeCoreMovementConfig();
    context.terrainCells = makeBattleTerrainCellsFromScene();
    context.allyRoster = makeAllyBattleSetupRoster();
    context.enemyRoster = makeEnemyBattleSetupRoster();
    context.comboDefinitions = KysChess::BattleSceneBattleAdapter::makeBattleSetupComboDefinitions();
    context.equipmentDefinitions = KysChess::BattleSceneBattleAdapter::makeBattleSetupEquipmentDefinitions();
    context.neigongDefinitions = KysChess::BattleSceneBattleAdapter::makeBattleSetupNeigongDefinitions();
    context.obtainedNeigongMagicIds = progress_.getObtainedNeigong();
    context.cloneSpawnCells = clone_spawn_positions_;
    context.nextCloneUnitId = battleUid;
    return context;
}
```

Then create the initialized session once:

```cpp
auto created = KysChess::BattleSceneBattleAdapter::createInitializedBattleRuntimeSession(
    makeBattleRuntimeBuildContext());
battle_session_.emplace(std::move(created.session));
core_role_bindings_.rolesByBattleId = std::move(created.rolesByBattleId);
KysChess::BattleSceneBattleAdapter::applyBattleInitializationResult(
    created.initializationResult,
    {
        &battle_roles_,
        &friends_obj_,
        &KysChess::ChessCombo::getMutableStates(),
        &core_role_bindings_.rolesByBattleId,
    });
```

Do not keep `startBattleRuntimeSession()`, `importBaseBattleRolesIntoRuntime()`, or `importBattleInitializationSeedIntoRuntime()`.

`commitFinalSetupPlacementToRuntime()` remains a separate adapter method because it represents real player setup input after runtime construction and clone mirror:

```cpp
void BattleSceneHades::commitFinalSetupPlacementToRuntime()
{
    KysChess::BattleSceneBattleAdapter::BattleSetupPlacementInput input;
    input.gridTransform = battleRuntime().units.gridTransform;
    input.roles.reserve(battle_roles_.size());
    for (auto* role : battle_roles_)
    {
        assert(role);
        input.roles.push_back(
            {
                role->ID,
                role->X(),
                role->Y(),
                role->FaceTowards,
            });
    }
    KysChess::BattleSceneBattleAdapter::commitFinalSetupPlacementToRuntime(battleRuntime(), input);
}
```

`runPreBattlePositionSwapIfEnabled()` is only the existing prompt and `runPositionSwapLoop()` / `runListBasedSwap()` behavior moved behind a named setup orchestration call. It must run after clone mirror so clones remain swappable, matching today's behavior.

**Required deletion in old `initializeBattleRuntimeSession()`**

Remove this role import block from the old method:

```cpp
battleRuntime().combo.units = comboStates;
battleRuntime().units.units.clear();
battleRuntime().status.units.clear();
for (auto* role : battle_roles_)
{
    auto stateIt = comboStates.find(role->ID);
    battleRuntime().units.units.push_back(makeBattleRuntimeUnit(
        role,
        stateIt != comboStates.end() ? &stateIt->second : nullptr,
        battleRuntime().units.gridTransform));
    if (stateIt != comboStates.end())
    {
        battleRuntime().status.units.push_back(KysChess::Battle::makeBattleStatusRuntimeUnit(
            makeBattleStatusUnit(role, stateIt->second)));
    }
}
```

**Commit**

```powershell
git add src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
git commit -m "refactor: import battle runtime before initialization"
```

### Task 4: Populate Initialization Seed During Runtime Build

**Files**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`

**Adapter API**

Extend `createInitializedBattleRuntimeSession()` from Task 3. Do not add a second import API.

Adapter responsibilities:

- translate legacy combo/equipment/neigong definitions into pure setup catalogs;
- translate selected ally/enemy roster facts into local `BattleRuntimeSetupSeed`;
- build unit seeds by battle unit ID without pre-applying setup effects;
- build clone source candidates by battle unit ID and power and store them in `BattleRuntimeSetupSeed::cloneSources`;
- build clone cells as data, not callbacks, and store them in `BattleRuntimeSetupSeed::cloneCells`;
- move `BattleRuntimeSetupSeed` into `BattleRuntimeInit`.

Scene responsibilities after this task:

- collect legacy arrays already available in `init()`;
- pass them once in `BattleRuntimeBuildContext`;
- no local `applyEquipmentEffects` lambda;
- no local `initializeTimedEffects` lambda;
- no local `applyOnCopies` lambda.

**Deletion gate**

After this task, this search should be empty:

```powershell
rg -n "applyEquipmentEffects|initializeTimedEffects|applyOnCopies|shieldPctMaxHP|teamFlatShield|blockFirstHitsRemaining" src\BattleSceneHades.cpp
```

If `shieldPctMaxHP` remains only in non-setup death-effect-store code, narrow the search to setup lines and document the remaining non-setup match in the commit message.

**Commit**

```powershell
git add src\BattleSceneHades.cpp src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
git commit -m "refactor: populate runtime initialization seed at boundary"
```

### Task 5: Apply Initialization Result To Legacy Roles

**Files**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`

**New adapter API**

```cpp
struct BattleInitializationApplyContext
{
    std::vector<Role*>* battleRoles = nullptr;
    std::deque<Role>* friendsObj = nullptr;
    std::map<int, RoleComboState>* comboStates = nullptr;
    std::unordered_map<int, Role*>* rolesByBattleId = nullptr;
};

void applyBattleInitializationResult(
    const Battle::BattleInitializationResult& result,
    const BattleInitializationApplyContext& context);
```

Apply behavior:

- for every `roleDelta`, mutate matching legacy `Role`:
  - `MaxHP`, `HP`, `Attack`, `Defence`, `Speed`;
- write initialized combo state from runtime/result to `ChessCombo::getMutableStates()`;
- record returned `BattleLogEvent`s by pushing into `presentation_recorder_`;
- do not recompute log text in scene.

Scene call:

```cpp
KysChess::BattleSceneBattleAdapter::applyBattleInitializationResult(
    created.initializationResult,
    {
        &battle_roles_,
        &friends_obj_,
        &KysChess::ChessCombo::getMutableStates(),
        &core_role_bindings_.rolesByBattleId,
    });
for (const auto& event : initResult.logEvents)
{
    presentation_recorder_.recordLog(event);
}
for (const auto& event : initResult.visualEvents)
{
    presentation_recorder_.recordVisual(event);
}
```

**Deletion gate**

After this task, this search should be empty:

```powershell
rg -n "recordBattleStatusLog|formatStatusValue\\(\"獲取\"|formatStatusValue\\(\"全隊獲取\"" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Delete `BattleSceneHades::recordBattleStatusLog()` and its declaration.

**Commit**

```powershell
git add src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
git commit -m "refactor: apply initialization output to legacy roles"
```

### Task 6: Move Clone Runtime State Creation Into Core

**Files**
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`

**Core behavior**

During initialization:

- choose clone sources from `input.cloneSources`, sorted by existing scene-equivalent power rules;
- choose legal cells from `input.cloneCells` where `walkable && !occupied`;
- assign clone IDs in core using `BattleRuntimeSetupSeed::nextCloneUnitId`;
- append cloned `BattleRuntimeUnit` to `runtime.units.units`;
- append cloned `BattleStatusRuntimeUnit` to `runtime.status.units`;
- append cloned `BattleUnitState` to `runtime.world.units`;
- append clone combo state to `runtime.combo.units`;
- return `BattleInitializationCloneIntent`.

Scene apply behavior:

- create legacy `Role` clone from `BattleInitializationCloneIntent`;
- set ID/team/position/render fields from intent;
- push clone into `friends_obj_` and `battle_roles_`;
- add `core_role_bindings_.rolesByBattleId[cloneUnitId] = clone`;
- do not call `makeSummonedCloneState()` in scene;
- do not write `cs[clone->ID]` in scene.

**Required tests**

```cpp
TEST_CASE("BattleInitializationSystem_CreatesRuntimeCloneBeforeSceneMirror", "[battle][initialization]")
{
    BattleRuntimeState runtime;
    runtime.units.units.push_back(runtimeUnit(10, 0, 100, 20, 30, 40));
    runtime.status.units.push_back(statusRuntime(10, 100, 20));
    runtime.combo.units[10].cloneSummonCount = 1;

    BattleRuntimeSetupSeed setup;
    setup.cloneSummonCount = 1;
    setup.nextCloneUnitId = 1000;
    setup.cloneSources.push_back({ 10, 1001, 999 });
    setup.cloneCells.push_back({ 3, 4, true, false });

    auto result = BattleInitializationSystem().initialize(runtime, setup);

    REQUIRE(result.cloneIntents.size() == 1);
    CHECK(result.cloneIntents[0].sourceUnitId == 10);
    CHECK(result.cloneIntents[0].cloneUnitId == 1000);
    CHECK(runtime.units.findUnit(1000) != nullptr);
    CHECK(runtime.combo.units.find(1000) != runtime.combo.units.end());
}
```

**Deletion gate**

After this task, this search should be empty:

```powershell
rg -n "makeSummonedCloneState|七截分身|cloneSourceIndices|cloneCandidates|cloneSummonCount" src\BattleSceneHades.cpp
```

`clone_spawn_positions_` may remain only as legacy input that adapter converts into `BattleInitializationCloneSpawnCell`.

**Commit**

```powershell
git add src\battle\BattleInitialization.cpp tests\BattleInitializationUnitTests.cpp src\BattleSceneHades.cpp src\BattleSceneBattleAdapter.cpp
git commit -m "refactor: create clone runtime state during initialization"
```

### Task 7: Move Enemy Top Debuff Into Core Initialization And Frame Output

**Files**
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`

**Core behavior**

Initialization computes enemy-top debuff from runtime:

- count live allies with `enemyTopDebuffCount > 0`;
- determine `topTargets` and `perMemberValue`;
- sort live enemies by existing score:
  - `Cost ^ star`, descending;
  - `MaxHP`, descending;
- set `combo.enemyTopDebuffApplied`;
- mutate runtime enemy attack/defence values;
- return `BattleInitializationEnemyTopDebuffDelta`;
- emit `BattleLogEvent` text:

```cpp
std::format("陰險：前{}名攻防{}{}（{}名存活）",
            topTargets,
            delta > 0 ? "-" : "+",
            std::abs(delta),
            liveAllies)
```

Frame-time death update must also be core-owned. Add a frame command/result path:

- death lifecycle emits a `BattleRecomputeEnemyTopDebuffCommand`;
- runner reduces it using runtime units/combo;
- result exposes `enemyTopDebuffDeltas` in `BattleFrameApplications`;
- scene applies deltas to legacy enemies.

**Deletion gate**

After this task, this search should be empty:

```powershell
rg -n "updateEnemyTopDebuffState|enemyTopDebuffApplied|陰險" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

`enemyTopDebuffApplied` may remain in `src/battle` and combo state definitions.

**Commit**

```powershell
git add src\battle\BattleInitialization.cpp src\battle\BattleCore.cpp src\battle\BattleCore.h tests\BattleInitializationUnitTests.cpp src\BattleSceneHades.cpp src\BattleSceneHades.h
git commit -m "refactor: compute enemy top debuff in core"
```

### Task 8: Collapse Runtime Initialization Into One Scene Call

**Files**
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`

Replace the scattered sequence with a single orchestration method:

```cpp
void BattleSceneHades::initializeCoreBattleStateFromLegacySetup()
{
    auto created = KysChess::BattleSceneBattleAdapter::createInitializedBattleRuntimeSession(
        makeBattleRuntimeBuildContext());
    battle_session_.emplace(std::move(created.session));
    core_role_bindings_.rolesByBattleId = std::move(created.rolesByBattleId);

    KysChess::BattleSceneBattleAdapter::applyBattleInitializationResult(
        created.initializationResult,
        {
            &battle_roles_,
            &friends_obj_,
            &KysChess::ChessCombo::getMutableStates(),
            &core_role_bindings_.rolesByBattleId,
        });

    runPreBattlePositionSwapIfEnabled();
    commitFinalSetupPlacementToRuntime();
    finishBattleRuntimeSessionConfiguration();
}
```

`finishBattleRuntimeSessionConfiguration()` contains static/config-only setup:

- movement config;
- attack world configuration;
- death effect store from initialized runtime/combo;
- rescue config;
- movement physics config;
- cast/action config;
- projectile follow-up config;
- action plan input initialization.

The position swap is intentionally inside this orchestration method, after initialization output is mirrored and before final runtime configuration. Clones exist in both runtime and `battle_roles_` before swap starts, and final placement is committed to runtime exactly once after swap finishes.

**No role import after this method**

After this task, `initializeBattleRuntimeSession()`, `startBattleRuntimeSession()`, `importBaseBattleRolesIntoRuntime()`, and `importBattleInitializationSeedIntoRuntime()` should not exist. The only full runtime/session construction must be `createInitializedBattleRuntimeSession()`. `BattleSceneHades` must not call `BattleInitializationSystem` directly. The only post-initialization scene-to-runtime write is `commitFinalSetupPlacementToRuntime()`, and it may only write placement fields.

**Deletion gate**

```powershell
rg -n "initializeBattleRuntimeSession|startBattleRuntimeSession|importBaseBattleRolesIntoRuntime|importBattleInitializationSeedIntoRuntime|BattleInitializationSystem|battleRuntime\\(\\)\\.units\\.units\\.push_back\\(makeBattleRuntimeUnit|battleRuntime\\(\\)\\.status\\.units\\.push_back" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: no matches.

This search should only find setup placement writes and frame-result legacy apply, not backend state prep:

```powershell
rg -n "setPositionOnly\\(" src\BattleSceneHades.cpp
```

**Commit**

```powershell
git add src\BattleSceneHades.cpp src\BattleSceneHades.h
git commit -m "refactor: collapse runtime-first battle initialization"
```

## Final Verification

Run all searches:

```powershell
rg -n "recordBattleStatusLog|applyStateToCopy|initializeTimedEffects|applyOnCopies|makeSummonedCloneState|updateEnemyTopDebuffState|initializeBattleRuntimeSession|startBattleRuntimeSession|importBaseBattleRolesIntoRuntime|importBattleInitializationSeedIntoRuntime" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: no matches.

```powershell
rg -n "Role\\*|Magic\\*|Item\\*|Engine|TextureManager|Font|Scene" src\battle
```

Expected: no matches.

```powershell
rg -n "std::function|callback|resolveRole|findRoleByBattleId" src\battle\BattleInitialization.*
```

Expected: no matches.

Run:

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Commit final cleanup:

```powershell
git add src docs tests
git commit -m "refactor: finish runtime-owned battle initialization"
```

## Review Checklist

- Runtime is constructed once before initialization gameplay rules.
- `BattleRuntimeSession` self-initializes before it can run frames.
- Consumed setup seed data is not a frame-time dependency.
- There is no scene-side setup shield/stat/timer/clone/enemy-debuff gameplay logic.
- Scene does not create battle facts during setup; it plays returned `BattleLogEvent`s.
- Core initialization has no `Role*`, `Magic*`, `Item*`, save/database, scene, engine, font, or texture dependency.
- Adapter is the only place that translates legacy equipment/neigong/save data into pure runtime initialization state.
- Clone runtime state exists before scene creates the legacy clone render object.
- Clone legacy render objects exist before player position swap starts.
- Final setup placement is committed to runtime once after player swap and before the first frame.
- No full runtime construction or role import runs after initialization output is applied.
- No `std::function` or callback lookup is introduced.
