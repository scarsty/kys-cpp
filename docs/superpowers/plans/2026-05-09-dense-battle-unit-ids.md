# Dense Battle Unit Ids Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace legacy `Role::ID`-derived battle unit ids with dense runtime unit ids (`0..N-1`) so runtime and presentation stores can index by unit id directly.

**Architecture:** Allocate dense battle unit ids during Hades/setup projection before runtime initialization. Preserve `realRoleId` as the only legacy role identity for base stats, combo membership, display metadata, and transitional `Role*` lookup. Migrate runtime stores from id-search/map lookup to direct vector indexing only when the vector is proven dense and append-only; leave non-unit key spaces such as attack ids, combo ids, magic ids, item ids, and team ids alone.

**Tech Stack:** C++20, existing battle runtime systems, Catch2 unit tests, Visual Studio solution build through `.github\build-command.ps1`.

---

## Verified Current Shape

- `BattleSceneHades` still assigns battle ids from `10000` and stores them in `Role::ID`.
- `BattleSceneHades` still has a legacy pending-enemy branch when `extended_teammates_` is empty: it moves only six enemies into `battle_roles_`, leaves the rest in `enemies_`, tracks them in `pending_enemy_unit_ids_`, and later activates only their render rows.
- In the new Hades/chess path, `extended_teammates_` is non-empty and all enemies are drained into `battle_roles_` before runtime initialization. That means pending enemies are already effectively absent there.
- The pending-enemy "dispatch" in `applyLegacyBattleFrameResult()` is not runtime dispatch. It only flips `BattleSceneRenderUnit::active`, bumps `attention`, and appends to `active_unit_ids_`; it does not add units to `BattleRuntimeSession`.
- `pendingAliveByTeam` exists to keep old pending enemies counted for win/loss before they are runtime-owned. Once all enemies are initialized as runtime units, this migration field should be deleted.
- `BattleSceneBattleAdapter` copies `Role::ID` into `BattleRuntimeUnit::id`, `BattleUnitState::id`, status ids, damage ids, and setup seed ids.
- `BattleInitializationSystem` allocates clone ids from `BattleRuntimeSetupSeed::nextCloneUnitId`, currently set from the legacy role-object count.
- `realRoleId` already exists on setup seeds and runtime units, and is the correct identity for base role stats and combo membership.
- Some vectors are unit-domain vectors and can become dense-indexed. Some vectors are not unit-domain vectors and must not be changed.

## Dense Id Rules

- `unitId` is a dense battle/runtime id.
- Initial runtime units are assigned `0..initialUnitCount-1`.
- Clones append at `runtime.units.units.size()` and then push matching rows into every dense runtime store.
- Every enemy that can affect battle result must be a runtime unit at initialization. Do not keep scene-only pending enemies.
- `realRoleId` remains the original `Save`/legacy role id.
- A dense unit vector may be indexed as `rows[unitId]` only if every row satisfies `rows[index].id == index`.
- Setup-only transitional code may keep `Role*` bindings, but keyed/indexed by dense `unitId`, not by `Role::ID`.
- Attack ids are not unit ids. Do not dense-index `attacks.attacks` by unit id.
- Combo definition ids, magic ids, item ids, team ids, battlefield ids, and grid cells are not unit ids.

## File Responsibilities

- `src/BattleSceneHades.h/.cpp`: Allocate dense unit ids, keep setup/render/identity rows dense, stop mutating `Role::ID` for battle identity.
- `src/BattleSceneBattleAdapter.h/.cpp`: Project legacy `Role` into runtime using explicit dense `unitId` bindings instead of reading `Role::ID`.
- `src/battle/BattleInitialization.h/.cpp`: Allocate clone unit ids from dense runtime vector size, remove external clone id offset.
- `src/battle/BattleDamageApplicationSystem.h/.cpp`: Remove pending-alive result accounting after all enemies become runtime-owned.
- `src/battle/BattleFind.h`: Add dense vector helpers with assertions.
- `src/battle/BattleCore.h/.cpp`: Migrate `BattleUnitStore` and other dense runtime stores to direct indexing.
- `src/battle/BattleRuntimeSession.cpp`: Use dense helpers for setup placement.
- `src/ChessCombo.h/.cpp`: Keep `realRoleId`-based combo membership; ensure id/team battle refs use dense `unitId`.
- `tests/BattleCoreUnitTests.cpp`: Cover clone dense ids and dense runtime store invariants.
- `tests/BattleFrameRunnerRuntimeUnitTests.cpp`: Cover frame runner direct-index behavior.
- `tests/BattlePresentationUnitTests.cpp`: Cover role-free dense identity lookup.

---

### Task 0: Delete Legacy Pending Enemy Fallback

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleDamageApplicationSystem.h`
- Modify: `src/battle/BattleDamageApplicationSystem.cpp`

- [ ] **Step 1: Capture the current pending-enemy surface**

Run:

```powershell
rg -n "pending_enemy_unit_ids_|pendingAliveByTeam|人物出場" src\BattleSceneHades.cpp src\BattleSceneHades.h src\battle
```

Expected current matches include:

```text
src\BattleSceneHades.h: pending_enemy_unit_ids_
src\BattleSceneHades.cpp: pending_enemy_unit_ids_
src\BattleSceneHades.cpp: pendingAliveByTeam
src\battle\BattleCore.h: pendingAliveByTeam
src\battle\BattleCore.cpp: pendingAliveByTeam
src\battle\BattleDamageApplicationSystem.h: pendingAliveByTeam
src\battle\BattleDamageApplicationSystem.cpp: pendingAliveByTeam
```

- [ ] **Step 2: Always initialize every enemy as an active battle role**

In `BattleSceneHades::onEntrance()`, replace the split branch:

```cpp
if (!extended_teammates_.empty())
{
    while (!enemies_.empty())
    {
        battle_roles_.push_back(enemies_.front());
        enemies_.pop_front();
    }
}
else
{
    for (int i = 0; i < 6; i++)
    {
        if (!enemies_.empty())
        {
            battle_roles_.push_back(enemies_.front());
            enemies_.pop_front();
        }
    }
}
```

with:

```cpp
while (!enemies_.empty())
{
    battle_roles_.push_back(enemies_.front());
    enemies_.pop_front();
}
```

This removes the old six-enemy staging rule for Hades. Any battle-result-relevant enemy is now initialized into runtime immediately.

- [ ] **Step 3: Remove scene pending-enemy tracking**

In `src/BattleSceneHades.h`, delete:

```cpp
std::deque<int> pending_enemy_unit_ids_;
```

In `BattleSceneHades::rebuildSetupTrackingFromLegacyState()`, remove:

```cpp
pending_enemy_unit_ids_.clear();
```

and replace the append branch:

```cpp
render.active = !pendingEnemy;
if (pendingEnemy)
{
    pending_enemy_unit_ids_.push_back(unitId);
    return;
}
active_unit_ids_.push_back(unitId);
```

with:

```cpp
assert(!pendingEnemy);
render.active = true;
active_unit_ids_.push_back(unitId);
```

Then delete the second loop:

```cpp
for (auto* role : enemies_)
{
    assert(role);
    append(*role, true);
}
```

because `enemies_` must be empty after all enemies are moved to `battle_roles_`.

- [ ] **Step 4: Delete fake pending-enemy render dispatch**

In `BattleSceneHades::applyLegacyBattleFrameResult()`, remove the whole pending-enemy activation block:

```cpp
{
    //人物出場
    while (!pending_enemy_unit_ids_.empty())
    {
        const int unitId = pending_enemy_unit_ids_.front();
        pending_enemy_unit_ids_.pop_front();
        auto& unit = requireRenderUnit(unitId);
        unit.active = true;
        unit.attention = 30;
        active_unit_ids_.push_back(unitId);
    }
    if (slow_ == 0 && (result_ == 0 || result_ == 1))
    {
        calExpGot();
        setExit(true);
    }
}
```

Keep the result-exit code as its own block:

```cpp
if (slow_ == 0 && (result_ == 0 || result_ == 1))
{
    calExpGot();
    setExit(true);
}
```

- [ ] **Step 5: Make scene result checking count only real render units**

In `BattleSceneHades::checkResult()`, replace:

```cpp
const int team1 = aliveRenderUnitsOnTeam(1) + static_cast<int>(pending_enemy_unit_ids_.size());
```

with:

```cpp
const int team1 = aliveRenderUnitsOnTeam(1);
```

- [ ] **Step 6: Remove pending-alive setup input**

In `BattleSceneHades::makeBattleRuntimeBuildContext()`, delete:

```cpp
context.setup.pendingAliveByTeam.emplace(1, static_cast<int>(enemies_.size()));
```

Add this invariant after all enemies have been drained during setup, before runtime creation if there is an existing local setup-check area:

```cpp
assert(enemies_.empty());
```

If there is no clean local setup-check area, place the assertion immediately before `context.setup.roles = battle_roles_;` in `makeBattleRuntimeBuildContext()`.

- [ ] **Step 7: Delete runtime pending-alive plumbing**

In `src/battle/BattleCore.h`, remove:

```cpp
std::map<int, int> pendingAliveByTeam;
```

from `BattleRuntimeState::BattleResultState`.

In `src/battle/BattleCore.cpp`, delete:

```cpp
input.pendingAliveByTeam = &state.result.pendingAliveByTeam;
```

from `makeFrameDamageApplicationInput()`.

Delete this loop from `updateBattleResult()`:

```cpp
for (const auto& [team, count] : state.result.pendingAliveByTeam)
{
    if (count > 0)
    {
        aliveTeams.insert(team);
    }
}
```

In `src/battle/BattleDamageApplicationSystem.h`, remove:

```cpp
const std::map<int, int>* pendingAliveByTeam = nullptr;
```

from `BattleDamageApplicationInput`.

In `src/battle/BattleDamageApplicationSystem.cpp`, delete the matching pending-alive loop in `updateBattleResult()` and delete:

```cpp
assert(input.pendingAliveByTeam);
```

- [ ] **Step 8: Run pending-enemy boundary search**

Run:

```powershell
rg -n "pending_enemy_unit_ids_|pendingAliveByTeam|人物出場" src\BattleSceneHades.cpp src\BattleSceneHades.h src\battle
```

Expected: no matches.

- [ ] **Step 9: Build and run tests**

Run:

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected: tests pass and the game target builds. If tests assert legacy pending-alive behavior, delete those tests because the behavior is intentionally removed.

---

### Task 1: Add Dense Unit Id Helpers And Tests

**Files:**
- Modify: `src/battle/BattleFind.h`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add failing tests for dense helper behavior**

Add tests near existing battle find/core helper tests in `tests/BattleCoreUnitTests.cpp`:

```cpp
TEST_CASE("BattleFind_RequireDenseByIdIndexesVectorByUnitId", "[battle][find][unit]")
{
    std::vector<BattleRuntimeUnit> units;
    BattleRuntimeUnit first;
    first.id = 0;
    first.hp = 10;
    units.push_back(first);
    BattleRuntimeUnit second;
    second.id = 1;
    second.hp = 20;
    units.push_back(second);

    CHECK(requireDenseById(units, 0).hp == 10);
    CHECK(requireDenseById(units, 1).hp == 20);

    requireDenseById(units, 1).hp = 25;
    CHECK(units[1].hp == 25);
}

TEST_CASE("BattleFind_TryDenseByIdReturnsNullForMissingDenseIndex", "[battle][find][unit]")
{
    std::vector<BattleRuntimeUnit> units;
    BattleRuntimeUnit only;
    only.id = 0;
    units.push_back(only);

    CHECK(tryDenseById(units, -1) == nullptr);
    CHECK(tryDenseById(units, 1) == nullptr);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: compile fails because `tryDenseById` and `requireDenseById` do not exist.

- [ ] **Step 3: Add dense helpers**

In `src/battle/BattleFind.h`, add below `requireById`:

```cpp
template<typename Range>
auto tryDenseById(Range& range, int id)
{
    using Element = std::remove_cvref_t<std::ranges::range_reference_t<Range>>;
    if (id < 0 || static_cast<std::size_t>(id) >= std::ranges::size(range))
    {
        return static_cast<Element*>(nullptr);
    }

    auto& item = range[static_cast<std::size_t>(id)];
    assert(item.id == id);
    return std::addressof(item);
}

template<typename Range>
decltype(auto) requireDenseById(Range& range, int id)
{
    auto* item = tryDenseById(range, id);
    assert(item);
    return *item;
}
```

Also add `#include <cstddef>` if needed.

- [ ] **Step 4: Run tests to verify they pass**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleFind*"
```

Expected: dense helper tests pass.

---

### Task 2: Decouple Adapter Projection From `Role::ID`

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add failing adapter projection test**

Add a test showing runtime unit id is explicit and `Role::ID` remains the real legacy id:

```cpp
TEST_CASE("BattleSceneBattleAdapter_UsesExplicitDenseUnitIdInsteadOfRoleId", "[battle][adapter][unit]")
{
    Role role;
    role.ID = 123;
    role.RealID = 123;
    role.Name = "legacy";
    role.Team = 0;
    role.MaxHP = 100;
    role.HP = 100;
    role.MaxMP = 50;
    role.MP = 0;
    role.Attack = 10;
    role.Defence = 5;
    role.Speed = 8;
    role.setPositionOnly(3, 4);

    KysChess::RoleComboState combo;
    std::map<int, KysChess::RoleComboState> combos;
    combos.emplace(0, combo);

    KysChess::BattleSceneBattleAdapter::BattleRuntimeBuildContext context;
    context.rules = KysChess::Battle::makeHadesBattleRuntimeRules(Scene::TILE_W, BATTLEMAP_COORD_COUNT);
    context.setup.roles.push_back({ 0, &role });
    context.setup.comboStates = &combos;
    context.setup.allyRoster.push_back({ 0, 123, 0, 1, 1, -1, -1, -1, 0, 0 });
    context.setup.battleFrame = 0;

    auto created = KysChess::BattleSceneBattleAdapter::createInitializedBattleRuntimeSession(context);

    REQUIRE(created.session.runtime().units.units.size() == 1);
    CHECK(created.session.runtime().units.units[0].id == 0);
    CHECK(created.session.runtime().units.units[0].realRoleId == 123);
    CHECK(role.ID == 123);
}
```

If including `Scene::TILE_W` from the test is inconvenient, use literal `36` and add a comment in the test name only if existing tests already do that.

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: compile fails because `context.setup.roles` still expects `std::vector<Role*>`.

- [ ] **Step 3: Add explicit setup role binding**

In `src/BattleSceneBattleAdapter.h`, add:

```cpp
struct BattleLegacySetupRole
{
    int unitId = -1;
    Role* role = nullptr;
};
```

Change:

```cpp
std::vector<Role*> roles;
```

to:

```cpp
std::vector<BattleLegacySetupRole> roles;
```

Change setup-only role binding returns from maps to dense vectors:

```cpp
struct BattleRuntimeCreationResult
{
    Battle::BattleRuntimeSession session;
    Battle::BattleInitializationResult initializationResult;
    std::vector<Role*> rolesByUnitId;
};

struct BattleInitializationApplyContext
{
    std::vector<BattleLegacySetupRole>* battleRoles = nullptr;
    std::deque<Role>* friendsObj = nullptr;
    std::map<int, RoleComboState>* comboStates = nullptr;
    std::vector<Role*>* rolesByUnitId = nullptr;
};
```

- [ ] **Step 4: Update adapter projection to use explicit unit id**

In `BattleSceneBattleAdapter.cpp`, change `RoleBattleProjection` to store `unitId_`:

```cpp
class RoleBattleProjection
{
public:
    RoleBattleProjection(int unitId, Role* role)
        : unitId_(unitId), role_(role)
    {
        assert(unitId_ >= 0);
        assert(role_);
    }

    int id() const { return unitId_; }
    int realRoleId() const { return role_->RealID >= 0 ? role_->RealID : role_->ID; }
    // keep the existing name/team/stat accessors using role_

private:
    int unitId_ = -1;
    Role* role_ = nullptr;
};
```

Update helper signatures:

```cpp
Battle::BattleUnitState makeBattleWorldUnit(const BattleLegacySetupRole& binding);
Battle::BattleRuntimeUnit makeBattleRuntimeUnit(
    const BattleLegacySetupRole& binding,
    const RoleComboState* state,
    const Battle::BattleGridTransform& gridTransform);
Battle::BattleStatusUnitState makeBattleStatusUnit(const BattleLegacySetupRole& binding, const RoleComboState& state);
Battle::BattleDamageUnitState makeBattleDamageUnit(const BattleLegacySetupRole& binding, const RoleComboState* state);
Battle::BattleDamagePresentationStyle makeBattleDamagePresentationStyle(const BattleLegacySetupRole& binding);
```

Use `binding.unitId` for all runtime ids and `binding.role` for legacy stat/magic projection.

- [ ] **Step 5: Replace adapter setup lookup with dense vectors**

Replace setup role lookup:

```cpp
Role* findSetupRoleByBattleId(const std::vector<BattleLegacySetupRole>& roles, int unitId)
{
    auto* binding = KysChess::Battle::tryDenseById(roles, unitId);
    assert(binding);
    assert(binding->role);
    return binding->role;
}
```

For this helper to work, `BattleLegacySetupRole` must expose `.id`. Either add:

```cpp
int id = -1;
```

or prefer this struct shape:

```cpp
struct BattleLegacySetupRole
{
    int id = -1;
    Role* role = nullptr;
};
```

Use `id` consistently if choosing this shape. Do not keep both `id` and `unitId`.

- [ ] **Step 6: Run adapter test**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleSceneBattleAdapter_UsesExplicitDenseUnitIdInsteadOfRoleId"
```

Expected: test passes.

---

### Task 3: Allocate Dense Unit Ids In Hades Setup

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattlePresentationUnitTests.cpp`

- [ ] **Step 1: Add failing identity/render-row test**

Add a small role-free test for dense identity rows if there is already a Hades-accessible test seam. If Hades helpers are protected, add this as a scene boundary search test instead of a compiled test:

```cpp
TEST_CASE("BattleSceneHades_DenseIdentityRowsUseBattleIdNotRealRoleId", "[battle][scene][unit]")
{
    BattleUnitIdentity first{ 0, 100, 0, 1, "first" };
    BattleUnitIdentity second{ 1, 100, 0, 1, "clone-source-same-real-id" };

    CHECK(first.battleId == 0);
    CHECK(second.battleId == 1);
    CHECK(first.realRoleId == second.realRoleId);
}
```

This test is intentionally simple: the behavior is that same `realRoleId` can appear under multiple dense battle ids.

- [ ] **Step 2: Add Hades dense id allocator fields**

In `BattleSceneHades`, add:

```cpp
int next_setup_unit_id_ = 0;

int allocateSetupUnitId()
{
    return next_setup_unit_id_++;
}
```

If private inline methods are not preferred in the header, declare `int allocateSetupUnitId();` and define it in the cpp.

- [ ] **Step 3: Stop assigning `Role::ID = 10000+` for battle identity**

In `BattleSceneHades::onEntrance()`, replace:

```cpp
int battleUid = 10000;
...
r->ID = battleUid++;
```

with explicit dense ids stored outside `Role`:

```cpp
next_setup_unit_id_ = 0;
```

For every role added to setup, assign:

```cpp
const int unitId = allocateSetupUnitId();
```

Do not write `unitId` into `role.ID`. Set copied roles’ `RealID` from the source role id and leave `Role::ID` as legacy identity.

- [ ] **Step 4: Make setup/render rows accept explicit unit id**

Change:

```cpp
BattleSceneSetupUnit makeSetupUnitFromRole(Role& role, bool pendingEnemy) const;
BattleSceneRenderUnit makeRenderUnitFromRole(const Role& role) const;
void upsertRenderUnitFromRole(const Role& role);
```

to:

```cpp
BattleSceneSetupUnit makeSetupUnitFromRole(int unitId, Role& role, bool pendingEnemy) const;
BattleSceneRenderUnit makeRenderUnitFromRole(int unitId, const Role& role) const;
void upsertRenderUnitFromRole(int unitId, const Role& role);
```

Use:

```cpp
unit.unitId = unitId;
unit.realRoleId = role.RealID >= 0 ? role.RealID : role.ID;
unit.identity.battleId = unitId;
unit.identity.realRoleId = role.RealID >= 0 ? role.RealID : role.ID;
```

- [ ] **Step 5: Store active setup roles as bindings**

Replace Hades setup access to plain `battle_roles_` in runtime context with explicit adapter bindings:

```cpp
std::vector<KysChess::BattleSceneBattleAdapter::BattleLegacySetupRole> legacy_setup_roles_;
```

When a unit is active in runtime setup:

```cpp
legacy_setup_roles_.push_back({ unitId, &role });
```

Do not add a pending-enemy branch. Task 0 removed scene-only pending enemies, so every battle-result-relevant enemy must have a setup binding and become a runtime unit.

- [ ] **Step 6: Update `makeBattleRuntimeBuildContext()`**

Change:

```cpp
context.setup.roles = battle_roles_;
```

to:

```cpp
context.setup.roles = legacy_setup_roles_;
```

Change clone next id:

```cpp
context.setup.nextCloneUnitId = 10000 + static_cast<int>(enemies_obj_.size() + friends_obj_.size());
```

to a temporary dense value:

```cpp
context.setup.nextCloneUnitId = static_cast<int>(legacy_setup_roles_.size());
```

Task 4 removes this field entirely.

Keep the invariant from Task 0:

```cpp
assert(enemies_.empty());
```

- [ ] **Step 7: Run Hades/adapter build**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected: build succeeds.

---

### Task 4: Allocate Clone Ids From Runtime Vector Size

**Files:**
- Modify: `src/battle/BattleInitialization.h`
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add failing clone dense id test**

Add a test in `tests/BattleCoreUnitTests.cpp` near existing clone initialization tests:

```cpp
TEST_CASE("BattleInitializationSystem_CreatesCloneAtNextDenseRuntimeUnitId", "[battle][initialization][unit]")
{
    BattleRuntimeState runtime;
    runtime.units.units.push_back(BattleRuntimeUnit{ .id = 0, .realRoleId = 100, .team = 0, .alive = true, .hp = 100, .maxHp = 100, .attack = 20, .defence = 10, .speed = 5, .star = 1 });
    runtime.world.units.push_back(BattleUnitState{ .id = 0, .realRoleId = 100, .team = 0, .alive = true, .hp = 100, .maxHp = 100, .attack = 20, .defence = 10, .speed = 5, .position = { 0, 0, 0 }, .star = 1 });
    runtime.status.units.push_back(makeBattleStatusRuntimeUnit(BattleStatusUnitState{ .id = 0 }));
    runtime.combo.units[0].summonedCloneCount = 1;
    runtime.units.gridTransform = { 36.0, BATTLEMAP_COORD_COUNT };

    BattleRuntimeSetupSeed setup;
    setup.units.push_back(BattleInitializationUnitSeed{ .unitId = 0, .realRoleId = 100, .team = 0, .star = 1, .cost = 1, .baseMaxHp = 100, .baseAttack = 20, .baseDefence = 10, .baseSpeed = 5 });
    setup.cloneSources.push_back(BattleInitializationCloneSource{ .sourceUnitId = 0, .sourceRealRoleId = 100, .power = 130, .star = 1, .chessInstanceId = 7, .sourceOrder = 0 });
    setup.cloneCells.push_back(BattleInitializationCloneSpawnCell{ .x = 3, .y = 4, .walkable = true, .occupied = false });

    auto result = BattleInitializationSystem().initialize(runtime, setup);

    REQUIRE(result.cloneIntents.size() == 1);
    CHECK(result.cloneIntents[0].cloneUnitId == 1);
    REQUIRE(runtime.units.units.size() == 2);
    CHECK(runtime.units.units[1].id == 1);
    CHECK(runtime.status.units[1].id == 1);
    CHECK(runtime.world.units[1].id == 1);
    CHECK(runtime.combo.units.contains(1));
}
```

If `summonedCloneCount` is not the current combo field name, use the existing clone test field from the current suite and keep the assertion shape above.

- [ ] **Step 2: Run test to verify failure**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleInitializationSystem_CreatesCloneAtNextDenseRuntimeUnitId"
```

Expected: fails because clone id is allocated from `setup.nextCloneUnitId`.

- [ ] **Step 3: Remove external clone id allocation**

In `BattleRuntimeSetupSeed`, delete:

```cpp
int nextCloneUnitId = 100000;
```

In adapter and Hades, delete assignments to `setup.nextCloneUnitId`.

- [ ] **Step 4: Allocate clone id from runtime size**

In `BattleInitializationSystem::initialize()`, replace:

```cpp
int nextCloneUnitId = setup.nextCloneUnitId;
```

with:

```cpp
int nextCloneUnitId = static_cast<int>(runtime.units.units.size());
```

Before pushing a clone, assert all dense runtime stores agree:

```cpp
assert(static_cast<int>(runtime.status.units.size()) == nextCloneUnitId);
assert(static_cast<int>(runtime.world.units.size()) == nextCloneUnitId);
```

After pushing a clone, assert:

```cpp
assert(runtime.units.units.back().id == static_cast<int>(runtime.units.units.size()) - 1);
assert(runtime.status.units.back().id == static_cast<int>(runtime.status.units.size()) - 1);
assert(runtime.world.units.back().id == static_cast<int>(runtime.world.units.size()) - 1);
```

- [ ] **Step 5: Run clone test**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleInitializationSystem_CreatesCloneAtNextDenseRuntimeUnitId"
```

Expected: test passes.

---

### Task 5: Convert Primary Runtime Unit Store To Direct Indexing

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Add failing invariant test for `BattleUnitStore`**

In `tests/BattleFrameRunnerRuntimeUnitTests.cpp`, add:

```cpp
TEST_CASE("BattleUnitStore_RequireUnitUsesDenseIndexInvariant", "[battle][runtime][unit]")
{
    BattleUnitStore store;
    BattleRuntimeUnit first;
    first.id = 0;
    first.hp = 10;
    store.units.push_back(first);
    BattleRuntimeUnit second;
    second.id = 1;
    second.hp = 20;
    store.units.push_back(second);

    CHECK(store.requireUnit(0).hp == 10);
    CHECK(store.requireUnit(1).hp == 20);
}
```

- [ ] **Step 2: Run test**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleUnitStore_RequireUnitUsesDenseIndexInvariant"
```

Expected: existing code passes, because search still works. This test becomes a guard for the next implementation.

- [ ] **Step 3: Replace store lookup implementation**

In `src/battle/BattleCore.cpp`, change:

```cpp
BattleRuntimeUnit* BattleUnitStore::findUnit(int unitId)
{
    return tryFindById(units, unitId);
}

const BattleRuntimeUnit* BattleUnitStore::findUnit(int unitId) const
{
    return tryFindById(units, unitId);
}

BattleRuntimeUnit& BattleUnitStore::requireUnit(int unitId)
{
    return requireById(units, unitId);
}

const BattleRuntimeUnit& BattleUnitStore::requireUnit(int unitId) const
{
    return requireById(units, unitId);
}
```

to:

```cpp
BattleRuntimeUnit* BattleUnitStore::findUnit(int unitId)
{
    return tryDenseById(units, unitId);
}

const BattleRuntimeUnit* BattleUnitStore::findUnit(int unitId) const
{
    return tryDenseById(units, unitId);
}

BattleRuntimeUnit& BattleUnitStore::requireUnit(int unitId)
{
    return requireDenseById(units, unitId);
}

const BattleRuntimeUnit& BattleUnitStore::requireUnit(int unitId) const
{
    return requireDenseById(units, unitId);
}
```

- [ ] **Step 4: Run frame runner tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleFrameRunner*"
```

Expected: tests pass. If any test used non-dense ids such as `70` for units, update the test unit ids to dense ids unless the id is an attack id.

---

### Task 6: Migrate Dense Runtime Unit Vectors

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleStatusSystem.cpp`
- Modify: `src/battle/BattleDamageApplicationSystem.cpp`
- Modify: `src/battle/BattleDeathEffectSystem.cpp`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `src/battle/BattleMovement.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Replace status vector lookups**

Replace unit-domain status vector lookups:

```cpp
requireById(runtime.status.units, unitId)
tryFindById(runtime.status.units, unitId)
```

with:

```cpp
requireDenseById(runtime.status.units, unitId)
tryDenseById(runtime.status.units, unitId)
```

Do this only for `BattleStatusRuntimeUnit` vectors.

- [ ] **Step 2: Replace world unit vector lookups**

Replace unit-domain world vector lookups:

```cpp
requireById(state.world.units, unitId)
tryFindById(state.world.units, unitId)
```

with dense helper calls.

Do not change `BattleAttackWorld::attacks`; attack ids are separate.

- [ ] **Step 3: Replace damage unit extras lookups**

Replace unit-domain `state.damage.unitExtras` lookups with dense helper calls after verifying setup creates one row per runtime unit in unit-id order.

Add this assertion when setup configuration is committed:

```cpp
for (std::size_t index = 0; index < runtime_.damage.unitExtras.size(); ++index)
{
    assert(runtime_.damage.unitExtras[index].id == static_cast<int>(index));
}
```

- [ ] **Step 4: Replace rescue unit snapshot lookups**

Replace unit-domain rescue vector lookups with dense helper calls only if `state.rescue.units` contains a row for every runtime unit. If rescue only snapshots eligible units, keep search lookup and add a comment in this plan execution notes explaining why.

- [ ] **Step 5: Keep sparse/non-unit structures unchanged**

Do not change:

```cpp
state.attacks.attacks
state.effects.activationCounts
state.attacks.sharedHitGroupTargets
setup.comboDefinitions
```

- [ ] **Step 6: Run full tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: all tests pass.

---

### Task 7: Convert Unit-Keyed Runtime Maps To Vectors

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `src/battle/BattleTeamEffectSystem.cpp`
- Modify: `src/battle/BattleProjectileTargetingSystem.cpp`
- Modify: `src/battle/BattleEffectSystem.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Convert combo state storage**

Change:

```cpp
std::map<int, RoleComboState> units;
```

to:

```cpp
std::vector<RoleComboState> units;
```

In initialization, resize before writing:

```cpp
runtime.combo.units.resize(runtime.units.units.size());
runtime.combo.units[seed.unitId] = combo;
```

Replace map lookups:

```cpp
auto comboIt = runtime.combo.units.find(unitId);
assert(comboIt != runtime.combo.units.end());
auto& combo = comboIt->second;
```

with:

```cpp
assert(unitId >= 0);
assert(static_cast<std::size_t>(unitId) < runtime.combo.units.size());
auto& combo = runtime.combo.units[static_cast<std::size_t>(unitId)];
```

For scene-facing mirrors, keep output as explicit `unitId` rows.

- [ ] **Step 2: Convert movement runtime storage**

Change:

```cpp
std::map<int, BattleMovementPhysicsState> movementRuntime;
```

to:

```cpp
std::vector<BattleMovementPhysicsState> movementRuntime;
```

Initialize with:

```cpp
runtime.movementRuntime.resize(runtime.units.units.size());
```

Replace:

```cpp
state.movementRuntime[unit.id]
```

with:

```cpp
state.movementRuntime[static_cast<std::size_t>(unit.id)]
```

after asserting dense size.

- [ ] **Step 3: Convert action plan seeds**

Change:

```cpp
std::map<int, BattleActionPlanSeed> planSeeds;
```

to:

```cpp
std::vector<BattleActionPlanSeed> planSeeds;
```

Initialize with `resize(unitCount)` and write by unit id:

```cpp
action.planSeeds[static_cast<std::size_t>(binding.id)] = makeBattleActionPlanSeed(...);
```

- [ ] **Step 4: Convert sparse unit command maps only if they become dense**

For `pendingCommitInputs`, choose the shape based on actual use:

If there can be at most one pending commit per unit and most frames are sparse, use:

```cpp
std::vector<std::optional<BattleActionCommitInput>> pendingCommitInputs;
```

If the code naturally appends commands and drains them, prefer:

```cpp
std::vector<BattleActionCommitInput> pendingCommitInputs;
```

Do not keep `std::map<int, BattleActionCommitInput>` after dense ids unless there is a documented sparse-order reason.

- [ ] **Step 5: Convert ultimate caster set**

Change:

```cpp
std::set<int> ultimateCasters;
```

to:

```cpp
std::vector<bool> ultimateCasters;
```

Initialize with `runtime.units.units.size()` and replace:

```cpp
state.ultimateCasters.erase(unitId);
state.ultimateCasters.contains(unitId);
```

with direct index bool reads/writes.

- [ ] **Step 6: Run full tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: all tests pass.

---

### Task 8: Make Hades Presentation/Setup Rows Dense-Indexed

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattlePresentationUnitTests.cpp`

- [ ] **Step 1: Replace Hades unit maps with vectors**

Replace:

```cpp
std::unordered_map<int, BattleUnitIdentity> unit_identities_;
std::unordered_map<int, std::size_t> setup_unit_index_by_id_;
std::unordered_map<int, std::size_t> render_unit_index_by_id_;
std::unordered_map<int, int> hurt_flash_timers_;
```

with:

```cpp
std::vector<BattleUnitIdentity> unit_identities_;
std::vector<int> hurt_flash_timers_;
```

Keep:

```cpp
std::vector<BattleSceneSetupUnit> setup_units_;
std::vector<BattleSceneRenderUnit> render_units_;
```

but require `setup_units_[unitId].identity.battleId == unitId` and `render_units_[unitId].unitId == unitId`.

- [ ] **Step 2: Replace Hades require helpers**

Change:

```cpp
auto it = render_unit_index_by_id_.find(unitId);
assert(it != render_unit_index_by_id_.end());
return render_units_[it->second];
```

to:

```cpp
assert(unitId >= 0);
assert(static_cast<std::size_t>(unitId) < render_units_.size());
auto& unit = render_units_[static_cast<std::size_t>(unitId)];
assert(unit.unitId == unitId);
return unit;
```

Apply the same shape for setup rows and identity rows.

- [ ] **Step 3: Resize sidecar vectors during setup**

When a setup row is appended:

```cpp
assert(unitId == static_cast<int>(setup_units_.size()));
setup_units_.push_back(std::move(setup));
render_units_.push_back(std::move(render));
unit_identities_.push_back(identity);
hurt_flash_timers_.push_back(0);
```

- [ ] **Step 4: Update presentation identity resolution**

Change:

```cpp
auto it = unit_identities_.find(unitId);
assert(it != unit_identities_.end());
return &it->second;
```

to:

```cpp
assert(unitId >= 0);
assert(static_cast<std::size_t>(unitId) < unit_identities_.size());
assert(unit_identities_[unitId].battleId == unitId);
return &unit_identities_[unitId];
```

- [ ] **Step 5: Run boundary search**

Run:

```powershell
rg -n "unit_identities_\\.|setup_unit_index_by_id_|render_unit_index_by_id_|hurt_flash_timers_\\.find|unordered_map<int, BattleUnitIdentity>|unordered_map<int, std::size_t>" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: no matches for old map-indexed Hades unit stores.

---

### Task 9: Remove Legacy Battle Id Reassignment And Cleanup

**Files:**
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/battle/BattleInitialization.h`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Delete `battleUid = 10000` path**

Remove all setup code that treats `Role::ID` as battle identity:

```cpp
int battleUid = 10000;
r->ID = battleUid++;
```

Copied legacy roles keep their original `ID`; if needed, set:

```cpp
r->RealID = r_ptr->ID;
```

but never overwrite `r->ID` with a battle unit id.

- [ ] **Step 2: Rename transitional adapter fields**

Rename:

```cpp
rolesByUnitId
```

to:

```cpp
legacyRolesByUnitId
```

This makes the remaining `Role*` escape clearly setup-only.

- [ ] **Step 3: Delete `nextCloneUnitId` searches**

Run:

```powershell
rg -n "nextCloneUnitId|10000 \\+|battleUid|rolesByBattleId" src tests
```

Expected:

- no `nextCloneUnitId`
- no `battleUid`
- no `10000 +` battle id offset
- no `rolesByBattleId`

- [ ] **Step 4: Run full tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected: all tests pass and game target builds.

---

## Final Boundary Searches

Run:

```powershell
rg -n "battleUid|nextCloneUnitId|rolesByBattleId|10000 \\+" src tests
rg -n "pending_enemy_unit_ids_|pendingAliveByTeam|人物出場" src tests
rg -n "requireById\\(state\\.units\\.units|tryFindById\\(state\\.units\\.units|requireById\\(runtime\\.units\\.units|tryFindById\\(runtime\\.units\\.units" src\battle
rg -n "std::map<int, RoleComboState>|std::map<int, BattleActionPlanSeed>|std::map<int, BattleMovementPhysicsState>|std::set<int> ultimateCasters" src\battle
rg -n "unit_identities_\\.find|setup_unit_index_by_id_|render_unit_index_by_id_|unordered_map<int, BattleUnitIdentity>" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected:

- no legacy `10000` battle-unit id allocation
- no scene-only pending enemy fallback
- no runtime unit lookup by linear vector search for dense unit stores
- no unit-keyed maps for combo/action/movement runtime state after Task 7
- Hades unit rows index by dense id directly

## Verification

Run after each task that changes compiled code:

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

## Assumptions And Caveats

- Dense direct indexing is safe only for vectors whose row identity is `row.id == index`.
- Hades no longer supports scene-only pending enemies. All enemies that count toward battle result are runtime units from initialization.
- `realRoleId` must remain stable and must not be overwritten by dense unit id.
- `Role::ID` should return to meaning legacy/base role id only.
- Do not convert attack ids, combo ids, magic ids, item ids, team ids, or grid cells into dense unit-indexed storage.
- Do not add defensive missing-id checks. Use assertions where a dense row must exist.
