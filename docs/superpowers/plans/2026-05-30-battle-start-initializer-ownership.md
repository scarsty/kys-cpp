# Battle Start Initializer Ownership Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the stateless `BattleInitializationSystem().initialize(...)` API with a real one-shot `BattleStartInitializer` that owns draft spawns, consumes them with an rvalue-qualified `initialize() &&`, exposes initialized spawns as output, and removes avoidable combo-state copies inside battle-start initialization.

**Architecture:** Battle-start initialization is a lifecycle transition, not a reusable service object. The creation pipeline should be explicit:

```text
BattleRuntimeSessionCreationInput
  -> draft BattleRuntimeUnitSpawn vector
  -> BattleStartInitializer owns and initializes those draft spawns
  -> initialized BattleRuntimeUnitSpawn vector
  -> BattleRuntimeState records
```

`BattleStartInitializer` should own the spawns it mutates. It should keep setup/context by reference/value, hold per-run resolved setup state and lookup indexes internally, and return both initialized spawns and the initialization report. This avoids fake defensive guards while making double-initialization structurally awkward.

**Tech Stack:** C++23, Catch2 tests through `kys_tests`, PowerShell, Visual Studio/MSBuild via `.github\\build-command.ps1`.

---

## Current Problems

### 1. The public API is a stateless one-shot class

Current code uses:

```cpp
auto initialization = BattleInitializationSystem().initialize(
    spawns,
    input.setup,
    BattleInitializationContext{ input.rules.gridTransform, input.battleFrame });
```

But `BattleInitializationSystem` stores no state and has one public method. This is less clear than either a free function or a real run object.

### 2. Initialization mutates external spawns in place

`initialize()` takes `std::vector<BattleRuntimeUnitSpawn>& spawns`, mutates it, and returns only `BattleInitializationResult`. The caller must know that `spawns` has changed and must pass it onward afterward.

### 3. Mid-initialization combo copies remain

Current initialization still copies `RoleComboState` into temporary maps in at least these places:

- Team flat shield calculation builds `std::map<int, RoleComboState> teamCombos` before calling `computeTeamFlatShield`.
- Clone count calls `resolveCloneCount(setup, comboMapFromSpawns(spawns))`, copying all combos just to scan for `CloneSummon`.
- Final `result.comboStates = comboMapFromSpawns(spawns)` remains as an intentional output bridge for now.

This plan removes the first two. The final output bridge remains intentionally deferred.

### 4. Repeated unit-id lookup is linear

`requireSpawnByUnitId(spawns, unitId)` scans the vector repeatedly. A run object can own a `unitId -> index` map and update it when clones are appended.

---

## Target Public API

Modify `src/battle/BattleInitialization.h` to expose:

```cpp
struct BattleInitializationOutput
{
    std::vector<BattleRuntimeUnitSpawn> spawns;
    BattleInitializationResult result;
};

class BattleStartInitializer
{
public:
    BattleStartInitializer(std::vector<BattleRuntimeUnitSpawn> spawns,
                           const BattleRuntimeSetupSeed& setup,
                           BattleInitializationContext context);

    BattleInitializationOutput initialize() &&;

private:
    std::vector<BattleRuntimeUnitSpawn> spawns_;
    const BattleRuntimeSetupSeed& setup_;
    BattleInitializationContext context_;
};
```

Delete `BattleInitializationSystem`.

The `initialize() &&` qualifier is intentional: initialization consumes the run object and its draft spawns.

---

## Target Session Pipeline

In `src/battle/BattleRuntimeSession.cpp`, replace:

```cpp
auto spawns = buildCanonicalSpawns(input);
auto initialization = BattleInitializationSystem().initialize(
    spawns,
    input.setup,
    BattleInitializationContext{ input.rules.gridTransform, input.battleFrame });
auto runtime = buildRuntimeFromSpawns(input, std::move(spawns));
```

with:

```cpp
auto initialized = BattleStartInitializer(
    buildCanonicalSpawns(input),
    input.setup,
    BattleInitializationContext{ input.rules.gridTransform, input.battleFrame })
    .initialize();

auto runtime = buildRuntimeFromSpawns(input, std::move(initialized.spawns));
```

Return `initialized.result` as the initialization report.

---

## Internal Implementation Shape

Keep the detailed phase orchestration private in `src/battle/BattleInitialization.cpp`. A private run-state class is recommended:

```cpp
class BattleStartInitializationRun
{
public:
    BattleStartInitializationRun(std::vector<BattleRuntimeUnitSpawn> spawns,
                                 const BattleRuntimeSetupSeed& setup,
                                 BattleInitializationContext context);

    BattleInitializationOutput run() &&;

private:
    void initializeSeededUnits();
    void applyTeamFlatShields();
    void summonClones();
    void applyEnemyTopDebuffs();
    void appendSeededRoleDeltas();
    void exportComboStates();

    BattleRuntimeUnitSpawn& spawn(int unitId);
    const BattleRuntimeUnitSpawn& spawn(int unitId) const;
    void appendSpawn(BattleRuntimeUnitSpawn spawn);

    int teamFlatShield(int team) const;
    int cloneCount() const;

    const TeamResolvedSetup& resolvedForTeam(int team) const;
    const std::vector<BattleSetupRosterUnit>& rosterForTeam(int team) const;

    std::vector<BattleRuntimeUnitSpawn> spawns_;
    const BattleRuntimeSetupSeed& setup_;
    BattleInitializationContext context_;
    TeamResolvedSetup allyResolved_;
    TeamResolvedSetup enemyResolved_;
    BattleInitializationResult result_;
    std::unordered_map<int, std::size_t> spawnIndexByUnitId_;
    std::vector<int> seededUnitIds_;
    std::map<int, StarBoostedStats> starStatsByUnitId_;
};
```

`BattleStartInitializer` can be a thin public wrapper around this private run state, or it can own the same fields directly. Prefer the private run state if it keeps the public header small and avoids exposing `TeamResolvedSetup`.

---

## Task 1: Add Consuming Output API

**Files:**
- Modify: `src/battle/BattleInitialization.h`
- Modify: `src/battle/BattleInitialization.cpp`

- [ ] **Step 1: Add `BattleInitializationOutput`**

Add after `BattleInitializationResult`:

```cpp
struct BattleInitializationOutput
{
    std::vector<BattleRuntimeUnitSpawn> spawns;
    BattleInitializationResult result;
};
```

- [ ] **Step 2: Replace `BattleInitializationSystem` with `BattleStartInitializer`**

Remove:

```cpp
class BattleInitializationSystem
{
public:
    BattleInitializationResult initialize(std::vector<BattleRuntimeUnitSpawn>& spawns,
                                          const BattleRuntimeSetupSeed& setup,
                                          const BattleInitializationContext& context) const;
};
```

Add:

```cpp
class BattleStartInitializer
{
public:
    BattleStartInitializer(std::vector<BattleRuntimeUnitSpawn> spawns,
                           const BattleRuntimeSetupSeed& setup,
                           BattleInitializationContext context);

    BattleInitializationOutput initialize() &&;

private:
    std::vector<BattleRuntimeUnitSpawn> spawns_;
    const BattleRuntimeSetupSeed& setup_;
    BattleInitializationContext context_;
};
```

- [ ] **Step 3: Add wrapper implementation**

In `BattleInitialization.cpp`, implement the constructor and rvalue-qualified `initialize() &&`.

At first, it may delegate to a private run object or temporarily to a moved version of the old logic. Keep the public behavior equivalent before splitting phases.

---

## Task 2: Introduce Private Run State

**Files:**
- Modify: `src/battle/BattleInitialization.cpp`

- [ ] **Step 1: Create `BattleStartInitializationRun` in the anonymous namespace**

Move run-owned state into the class:

- `spawns_`
- `setup_`
- `context_`
- `allyResolved_`
- `enemyResolved_`
- `result_`
- `spawnIndexByUnitId_`
- `seededUnitIds_`
- `starStatsByUnitId_`

Constructor responsibilities:

- Move in draft spawns.
- Store setup/context.
- Resolve ally/enemy setup once.
- Build `spawnIndexByUnitId_` from initial spawns.
- Assert unit ids are unique and non-negative.

- [ ] **Step 2: Add indexed spawn accessors**

Replace free `requireSpawnByUnitId(...)` uses inside the initialization flow with:

```cpp
BattleRuntimeUnitSpawn& spawn(int unitId);
const BattleRuntimeUnitSpawn& spawn(int unitId) const;
```

Use `assert` for invariants rather than fallback checks.

- [ ] **Step 3: Add `appendSpawn`**

When clones are created, call a helper that updates both `spawns_` and `spawnIndexByUnitId_`:

```cpp
void appendSpawn(BattleRuntimeUnitSpawn spawn)
{
    const int unitId = spawn.unit.id;
    assert(unitId >= 0);
    assert(!spawnIndexByUnitId_.contains(unitId));
    spawnIndexByUnitId_.emplace(unitId, spawns_.size());
    spawns_.push_back(std::move(spawn));
}
```

---

## Task 3: Split Initialization Phases

**Files:**
- Modify: `src/battle/BattleInitialization.cpp`

Convert the body of `BattleInitializationSystem::initialize()` into phase methods on `BattleStartInitializationRun`.

- [ ] **Step 1: `initializeSeededUnits()`**

Move the loop over `setup.units` into a method that:

- gets `spawn(seed.unitId)`
- uses `auto& combo = spawn.combo`
- applies resolved member effects
- applies teamwide effects
- applies equipment effects
- applies obtained neigong effects
- computes star/fight-win stats
- applies stat bonuses
- initializes shield percentage logs
- initializes auto-ultimate timers
- calls `refreshRuntimeUnitSpawnDerivedState(spawn)`
- records `seededUnitIds_`
- records `starStatsByUnitId_`

- [ ] **Step 2: `applyTeamFlatShields()`**

Move team flat shield logic into a method.

Use direct spawn references, not a copied `std::map<int, RoleComboState>`.

Suggested helper:

```cpp
int BattleStartInitializationRun::teamFlatShield(int team) const
{
    int totalShield = 0;
    std::set<int> seenComboIds;
    for (const auto& seed : setup_.units)
    {
        if (seed.team != team)
        {
            continue;
        }
        const auto& combo = spawn(seed.unitId).combo;
        for (const auto& effect : combo.appliedEffects)
        {
            // same FlatShield/sourceComboId logic as current computeTeamFlatShield
        }
    }
    return totalShield;
}
```

Then apply the shield and logs to each seeded unit.

Delete or refactor `computeTeamFlatShield(const std::map<int, RoleComboState>&)` so no copied combo map is needed.

- [ ] **Step 3: `summonClones()`**

Move clone creation into a method.

Replace:

```cpp
const int cloneCount = resolveCloneCount(setup, comboMapFromSpawns(spawns));
```

with a direct scan over `spawns_`:

```cpp
int BattleStartInitializationRun::cloneCount() const
{
    int count = 0;
    for (const auto& spawn : spawns_)
    {
        count = std::max(count, maxAlwaysEffectValue(spawn.combo, EffectType::CloneSummon));
    }
    return count;
}
```

Delete or refactor `resolveCloneCount(...)` so it no longer requires a copied combo map.

When appending clones, use `appendSpawn(std::move(cloneSpawn))`.

- [ ] **Step 4: `applyEnemyTopDebuffs()`**

`applyEnemyTopDebuff(...)` can stay a free helper taking `spawns_`, or become a method if it needs `result_`/`context_` directly.

Preferred method shape:

```cpp
void BattleStartInitializationRun::applyEnemyTopDebuffs()
{
    result_.enemyTopDebuffs = applyEnemyTopDebuff(spawns_, context_.frame, result_.logEvents);
}
```

- [ ] **Step 5: `appendSeededRoleDeltas()`**

Move the final seeded-unit role delta loop into a method using `seededUnitIds_`, `spawn(unitId)`, and `starStatsByUnitId_`.

- [ ] **Step 6: `exportComboStates()`**

Keep the final bridge:

```cpp
result_.comboStates = comboMapFromSpawns(spawns_);
```

Do not remove this bridge in this plan. The scene/UI still consumes it.

---

## Task 4: Wire the Session Pipeline

**Files:**
- Modify: `src/battle/BattleRuntimeSession.cpp`

- [ ] **Step 1: Consume spawns through `BattleStartInitializer`**

Replace the old two-variable flow with:

```cpp
auto initialized = BattleStartInitializer(
    buildCanonicalSpawns(input),
    input.setup,
    BattleInitializationContext{ input.rules.gridTransform, input.battleFrame })
    .initialize();

auto runtime = buildRuntimeFromSpawns(input, std::move(initialized.spawns));
deriveRuntimeState(runtime, std::move(input));

return {
    std::move(runtime),
    std::move(initialized.result),
};
```

- [ ] **Step 2: Keep `buildCanonicalSpawns(input)` as draft creation**

No behavior change here unless needed for compile. It is okay that it consumes `setup.baseCombo` from `input.units`; `input.units` should not be used for base combo after draft creation.

---

## Task 5: Update Tests

**Files:**
- Modify: `tests/BattleInitializationUnitTests.cpp`
- Modify other battle tests that call `BattleInitializationSystem().initialize(...)`

- [ ] **Step 1: Add a test helper for consuming initialization**

In `BattleInitializationUnitTests.cpp`, replace repeated direct calls with a helper:

```cpp
BattleInitializationOutput initializeBattleStartForTest(
    std::vector<BattleRuntimeUnitSpawn> spawns,
    const BattleRuntimeSetupSeed& setup,
    BattleInitializationContext context = testInitializationContext())
{
    return BattleStartInitializer(std::move(spawns), setup, context).initialize();
}
```

- [ ] **Step 2: Update tests to read output spawns**

Current tests often do:

```cpp
auto result = BattleInitializationSystem().initialize(spawns, setup, context);
CHECK(requireSpawn(spawns, 0).unit.vitals.maxHp == ...);
```

Update to:

```cpp
auto output = initializeBattleStartForTest(std::move(spawns), setup, context);
CHECK(requireSpawn(output.spawns, 0).unit.vitals.maxHp == ...);
CHECK(output.result....);
```

Use names like `output` or `initialized` to avoid confusion between report and spawns.

- [ ] **Step 3: Keep session-level tests using `BattleRuntimeSession::createInitialized` unchanged unless compile requires updates**

Only direct initialization API tests should need major changes.

---

## Task 6: Clean Up Includes and Dead Helpers

**Files:**
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `src/battle/BattleInitialization.h`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify tests as needed

- [ ] **Step 1: Delete stale free helpers**

After phase migration, remove helpers that become unused:

- `computeTeamFlatShield(const std::map<int, RoleComboState>&)` if replaced by the method
- `resolveCloneCount(const BattleRuntimeSetupSeed&, const std::map<int, RoleComboState>&)`
- `requireSpawnByUnitId(...)` if all internal uses move to indexed methods

Keep `comboMapFromSpawns(...)` for `BattleInitializationResult::comboStates`.

- [ ] **Step 2: Remove stale includes**

Re-check includes after deleting map-copy helpers and the old system class.

Potentially affected:

- `src/battle/BattleInitialization.cpp`
- `src/battle/BattleRuntimeSession.cpp`
- `tests/BattleInitializationUnitTests.cpp`

Do not remove includes needed by transitive declarations accidentally; build will catch this.

---

## Task 7: Verify

**Files:**
- All modified files

- [ ] **Step 1: Search for old API**

Ensure these no longer exist:

```text
BattleInitializationSystem
.initialize(spawns,
resolveCloneCount(setup, comboMapFromSpawns
std::map<int, RoleComboState> teamCombos
```

`comboMapFromSpawns(spawns_)` may remain only for final output export.

- [ ] **Step 2: Build tests**

If using CLI:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
```

If integrated with VS Code, use the debug build and VS Code test adapter per project instructions.

- [ ] **Step 3: Run focused tests**

```powershell
.\x64\Debug\kys_tests.exe "[battle][initialization]"
.\x64\Debug\kys_tests.exe "[battle][runtime_session]"
.\x64\Debug\kys_tests.exe "[battle][scene]"
```

Adjust filters to match available Catch2 tags if a filter returns no tests.

---

## Non-Goals

- Do not remove `BattleInitializationResult::comboStates` in this plan.
- Do not migrate `ChessCombo::activeStates_` scene/UI bridge in this plan.
- Do not refactor unrelated battle systems just to enforce a universal API style.
- Do not add defensive runtime guards against double initialization. The consuming API and rvalue-qualified `initialize() &&` are the intended ownership enforcement.

---

## Expected Result

After implementation:

- `BattleInitializationSystem().initialize(...)` is gone.
- `BattleStartInitializer` is a real object because it owns draft spawns and setup-run state.
- Initialization is a one-shot consuming lifecycle transition.
- Team shield and clone count no longer copy every `RoleComboState` into temporary maps.
- Repeated spawn lookups inside initialization use a run-owned index.
- Runtime session setup reads as a clean draft-spawn → initialized-spawn → runtime-state pipeline.
