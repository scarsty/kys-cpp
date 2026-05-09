# Hades Initialization Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `BattleSceneHades` initialization chess/runtime-only, delete legacy teammate/menu setup, and stop staging runtime/render initialization through inherited legacy `Role` containers.

**Architecture:** Hades requires explicit deployed chess teammates from `DynamicChessMap::createBattle()` and no longer self-selects allies from `BattleInfo`. Runtime setup moves from live `Role*` bindings toward value snapshots: scene may read `Role`/`Magic` once to build setup input, but adapter/session initialization should not require live scene role storage or mirror clone/stat deltas back into `Role`.

**Tech Stack:** C++20, existing Visual Studio solution, Catch2 tests, PowerShell build scripts.

---

## Call Graph Decision

Current chess battle path:

- `src/ChessBattleFlow.cpp:985` builds `DynamicBattleRoles`.
- `src/ChessBattleFlow.cpp:1029` calls `DynamicChessMap::createBattle` with `DynamicBattleRoles`.
- `src/DynamicChessMap.cpp:33` constructs `BattleSceneHades`.
- `src/DynamicChessMap.cpp:60` always calls `setExtendedBattleInfo(teammates)`.
- `src/ChessBattleFlow.cpp:745` rejects empty selected ally lists before battle.

Therefore `BattleSceneHades` can assert `!extended_teammates_.empty()` and delete its `AutoTeamMate` / `TeamMenu` fallback. `Event::tryBattle()` modes `2`, `3`, and `4` still construct Hades without extended teammates; per current direction, ignore those callers for this batch and clean them up later.

## File Responsibilities

- `src/BattleSceneHades.cpp/.h`: Own chess-only setup assembly, render/setup rows, position swap, runtime creation, and initialization result application.
- `src/BattleSceneBattleAdapter.cpp/.h`: Convert setup-only legacy facts into runtime initialization input. After this plan, public setup API should not expose mutable `Role*` bindings or return `rolesByUnitId`.
- `src/BattleSceneAct.h`: Lose Hades-only staging containers once Hades owns setup storage directly.
- `src/DynamicChessMap.cpp/.h`: Remain the only supported Hades battle construction path for chess battles in this batch.
- `tests/BattleInitializationUnitTests.cpp` and `tests/BattleFrameRunnerRuntimeUnitTests.cpp`: Cover runtime/session initialization invariants already in scope; add adapter/unit tests only when a new role-free setup conversion helper is exposed.

---

### Task 1: Delete Legacy Hades Teammate Selection

**Files:**
- Modify: `src/BattleSceneHades.cpp`

- [ ] **Step 1: Remove the `TeamMenu` include from Hades**

Delete this include:

```cpp
#include "TeamMenu.h"
```

- [ ] **Step 2: Add the chess-only setup invariant**

At the start of `BattleSceneHades::onEntrance()`, immediately before setup ids are reset, add:

```cpp
    assert(!extended_teammates_.empty());
```

Keep this as an invariant. Do not add a fallback branch for empty teammates.

- [ ] **Step 3: Delete the old ally selection branches**

Remove the entire `else if (info_->AutoTeamMate[0] >= 0)` block that pushes `info_->AutoTeamMate[i]` into `friends_`.

Remove the entire `if (extended_teammates_.empty() && 1)` block that creates `std::make_shared<TeamMenu>()`, runs it, and pushes `team_menu->getRoles()` into `friends_`.

The ally construction path should only iterate `extended_teammates_`.

- [ ] **Step 4: Simplify ally positioning**

In the ally loop, replace the conditional position branch:

```cpp
            if (!extended_teammates_.empty() && i < extended_teammates_.size())
            {
                r->setPositionOnly(extended_teammates_[i].X, extended_teammates_[i].Y);
            }
            else
            {
                r->setPositionOnly(info_->TeamMateX[i], info_->TeamMateY[i]);
            }
```

with:

```cpp
            assert(static_cast<std::size_t>(i) < extended_teammates_.size());
            r->setPositionOnly(extended_teammates_[static_cast<std::size_t>(i)].X,
                extended_teammates_[static_cast<std::size_t>(i)].Y);
```

- [ ] **Step 5: Run the boundary search**

Run:

```powershell
rg -n "TeamMenu|AutoTeamMate|TeamMateX|extended_teammates_\\.empty\\(\\)" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected:

- No `TeamMenu`, `AutoTeamMate`, or `TeamMateX` matches in Hades.
- `extended_teammates_.empty()` may remain only in the new `assert`.

- [ ] **Step 6: Build**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: compile succeeds, or final link is blocked only if the test executable is running.

---

### Task 2: Replace Inherited Enemy/Ally Queues With Hades-Owned Setup Storage

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneAct.h`

- [ ] **Step 1: Add explicit Hades setup role storage**

In `BattleSceneHades.h`, add Hades-owned storage near the setup/render fields:

```cpp
    std::deque<Role> ally_setup_roles_;
    std::deque<Role> enemy_setup_roles_;
```

- [ ] **Step 2: Rename initialization clone storage in the adapter API**

In `BattleSceneBattleAdapter.h`, rename:

```cpp
    std::deque<Role>* friendsObj = nullptr;
```

to:

```cpp
    std::deque<Role>* cloneRoleStorage = nullptr;
```

Update `applyBattleInitializationResult()` assertions and clone insertion in `BattleSceneBattleAdapter.cpp`:

```cpp
    assert(context.cloneRoleStorage);
    context.cloneRoleStorage->push_back(*source);
    auto* clone = &context.cloneRoleStorage->back();
```

- [ ] **Step 3: Clear Hades-owned setup storage on entrance**

In `BattleSceneHades::onEntrance()`, before adding enemies:

```cpp
    battle_roles_.clear();
    friends_.clear();
    ally_setup_roles_.clear();
    enemy_setup_roles_.clear();
```

This keeps inherited state from retaining old pointers if a scene object is reused.

- [ ] **Step 4: Replace enemy staging with a local vector**

Change enemy setup from `enemies_obj_` + inherited `enemies_` to `enemy_setup_roles_` + local `std::vector<Role*> enemyRoles`.

The shape should be:

```cpp
    std::vector<Role*> enemyRoles;
    for (int i = 0; i < BATTLE_ENEMY_COUNT; i++)
    {
        auto* source = roleSave_.getRole(info_->Enemy[i]);
        if (!source)
        {
            continue;
        }

        enemy_setup_roles_.push_back(*source);
        auto* role = &enemy_setup_roles_.back();
        role->resetBattleInfo();
        role->RealID = source->ID;
        role->setPositionOnly(info_->EnemyX[i], info_->EnemyY[i]);
        role->Team = 1;
        readFightFrame(role);
        role->FaceTowards = rand_.rand_int(4);
        enemyRoles.push_back(role);
        LOG("Adding enemy role {} name {}\n", role->RealID, role->Name);
    }
```

Then sort and drain `enemyRoles`:

```cpp
    assert(!enemyRoles.empty());
    for (auto* role : enemyRoles)
    {
        initializeLegacySetupRole(*role);
    }
    pos_ = enemyRoles.front()->Pos;

    std::sort(enemyRoles.begin(), enemyRoles.end(), [](Role* lhs, Role* rhs)
    {
        return lhs->MaxHP + lhs->Attack < rhs->MaxHP + rhs->Attack;
    });

    for (auto* enemy : enemyRoles)
    {
        assignSetupUnitId(*enemy);
        battle_roles_.push_back(enemy);
    }
```

- [ ] **Step 5: Replace ally staging with a local vector**

Change ally setup from inherited `friends_` + `friends_obj_` to `ally_setup_roles_` + local `std::vector<Role*> allyRoles`.

The shape should be:

```cpp
    std::vector<Role*> allyRoles;
    allyRoles.reserve(extended_teammates_.size());
    for (const auto& teammateInfo : extended_teammates_)
    {
        auto* source = roleSave_.getRole(teammateInfo.ID);
        assert(source);
        ally_setup_roles_.push_back(*source);
        auto* role = &ally_setup_roles_.back();
        role->resetBattleInfo();
        role->RealID = source->ID;
        role->Auto = 2;
        allyRoles.push_back(role);
    }
```

Then loop `allyRoles` to assign ids, positions, team, and setup state:

```cpp
    for (std::size_t i = 0; i < allyRoles.size(); ++i)
    {
        auto* role = allyRoles[i];
        assert(role);
        assignSetupUnitId(*role);
        battle_roles_.push_back(role);
        role->setPositionOnly(extended_teammates_[i].X, extended_teammates_[i].Y);
        role->Team = 0;
        initializeLegacySetupRole(*role);
    }
```

- [ ] **Step 6: Center camera from local allies**

Replace `friends_` camera code with `allyRoles`:

```cpp
    assert(!allyRoles.empty());
    int sx = 0;
    int sy = 0;
    for (auto* role : allyRoles)
    {
        sx += role->X();
        sy += role->Y();
    }
    pos_ = battle_map_.pos45To90(sx / static_cast<int>(allyRoles.size()),
        sy / static_cast<int>(allyRoles.size()));
```

- [ ] **Step 7: Update runtime build context storage references**

In `makeBattleRuntimeBuildContext()`, replace:

```cpp
    assert(enemies_.empty());
    context.setup.allyRoster.reserve(friends_obj_.size());
    for (size_t index = 0; index < friends_obj_.size() && index < extended_teammates_.size(); ++index)
    {
        const auto& role = friends_obj_[index];
```

with:

```cpp
    context.setup.allyRoster.reserve(ally_setup_roles_.size());
    for (size_t index = 0; index < ally_setup_roles_.size(); ++index)
    {
        assert(index < extended_teammates_.size());
        const auto& role = ally_setup_roles_[index];
```

Replace enemy roster references:

```cpp
    context.setup.enemyRoster.reserve(enemies_obj_.size());
    for (size_t index = 0; index < enemies_obj_.size(); ++index)
    {
        const auto& role = enemies_obj_[index];
```

with:

```cpp
    context.setup.enemyRoster.reserve(enemy_setup_roles_.size());
    for (size_t index = 0; index < enemy_setup_roles_.size(); ++index)
    {
        const auto& role = enemy_setup_roles_[index];
```

- [ ] **Step 8: Update initialization apply context**

In `initializeBattleRuntime()`, replace:

```cpp
            &friends_obj_,
```

with:

```cpp
            &ally_setup_roles_,
```

after the adapter context field is renamed to `cloneRoleStorage`.

- [ ] **Step 9: Remove Hades-only inherited staging fields**

Run:

```powershell
rg -n "enemies_|enemies_obj_|friends_obj_" src\BattleSceneAct.h src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
```

Expected after Steps 1-8: only `BattleSceneAct.h` still declares the old fields.

Delete these fields from `BattleSceneAct.h`:

```cpp
    std::deque<Role*> enemies_;
    std::deque<Role> enemies_obj_;
    std::deque<Role> friends_obj_;
```

- [ ] **Step 10: Build and run focused tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleInitialization*"
```

Expected: build succeeds and focused tests pass.

---

### Task 3: Extract Pure Setup Defaults From `initializeLegacySetupRole`

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

- [ ] **Step 1: Add a Hades setup defaults struct**

In `BattleSceneHades.h`, near `BattleSceneSetupUnit`, add:

```cpp
    struct BattleSceneInitialRoleState
    {
        int hp = 0;
        int mp = 0;
        int physicalPower = 0;
        int faceTowards = Towards_None;
        Pointf position;
        Pointf realTowards;
        Pointf acceleration;
        std::array<int, 5> fightFrames{};
    };
```

- [ ] **Step 2: Add pure helper declarations**

In `BattleSceneHades.h`, add:

```cpp
    BattleSceneInitialRoleState makeInitialRoleState(
        const Role& role,
        const std::vector<Role*>& opposingRoles) const;
    static Pointf realTowardsFromFaceTowards(int faceTowards);
```

- [ ] **Step 3: Implement `realTowardsFromFaceTowards`**

In `BattleSceneHades.cpp`, implement:

```cpp
Pointf BattleSceneHades::realTowardsFromFaceTowards(int faceTowards)
{
    switch (faceTowards)
    {
    case Towards_RightDown:
        return { 1, 1 };
    case Towards_RightUp:
        return { 1, -1 };
    case Towards_LeftDown:
        return { -1, 1 };
    case Towards_LeftUp:
        return { -1, -1 };
    default:
        assert(false);
        return {};
    }
}
```

- [ ] **Step 4: Implement `makeInitialRoleState`**

Move the nearest-enemy facing and map-position calculation out of `initializeLegacySetupRole()`:

```cpp
BattleSceneHades::BattleSceneInitialRoleState BattleSceneHades::makeInitialRoleState(
    const Role& role,
    const std::vector<Role*>& opposingRoles) const
{
    BattleSceneInitialRoleState state;
    state.hp = role.MaxHP;
    state.mp = 0;
    state.physicalPower = std::max(role.PhysicalPower, 90);

    int faceTowards = role.FaceTowards;
    int minDistance = COORD_COUNT * COORD_COUNT;
    for (auto* other : opposingRoles)
    {
        assert(other);
        const int distance = calDistance(role.X(), role.Y(), other->X(), other->Y());
        if (distance < minDistance)
        {
            minDistance = distance;
            const int candidate = calTowards(role.X(), role.Y(), other->X(), other->Y());
            if (candidate != Towards_None)
            {
                faceTowards = candidate;
            }
        }
    }

    state.faceTowards = faceTowards;
    state.realTowards = realTowardsFromFaceTowards(faceTowards);
    auto position = battle_map_.pos45To90(role.X(), role.Y());
    state.position = { position.x, position.y, 0 };
    state.acceleration = { 0, 0, gravity_ };
    for (int i = 0; i < 5; ++i)
    {
        state.fightFrames[i] = role.FightFrame[i];
    }
    return state;
}
```

- [ ] **Step 5: Narrow `initializeLegacySetupRole` to storage compatibility**

Change `initializeLegacySetupRole` to receive opposing roles:

```cpp
    void initializeLegacySetupRole(Role& role, const std::vector<Role*>& opposingRoles);
```

Its body should call `readFightFrame(&role)`, then `makeInitialRoleState(role, opposingRoles)`, and only assign storage fields still needed by the current adapter:

```cpp
    role.Acted = 0;
    role.ExpGot = 0;
    role.clearShowStrings();
    role.FightingFrame = 0;
    role.Moved = 0;
    role.AI_Action = -1;
    role.Show.Effect = -1;
    role.Show.BattleHurt = 0;
    role.Show.ProgressChange = 0;
    role.Progress = 0;
    role.ActType = -1;
    role.ActFrame = 0;
    if (role.Auto != 2)
    {
        role.Auto = role.Team;
    }
    role.HP = state.hp;
    role.MP = state.mp;
    role.PhysicalPower = state.physicalPower;
    role.Poison = 0;
    role.HurtThisFrame = 0;
    role.FaceTowards = state.faceTowards;
    role.RealTowards = state.realTowards;
    role.Pos = state.position;
    role.Acceleration = state.acceleration;
```

- [ ] **Step 6: Update callers with explicit opponent lists**

Enemy initialization uses `allyRoles` only after allies exist. To avoid wrong facing, initialize role storage after both local vectors have been built:

```cpp
    for (auto* enemy : enemyRoles)
    {
        initializeLegacySetupRole(*enemy, allyRoles);
    }
    for (auto* ally : allyRoles)
    {
        initializeLegacySetupRole(*ally, enemyRoles);
    }
```

This removes the old dependency on partially populated `battle_roles_`.

- [ ] **Step 7: Run search**

Run:

```powershell
rg -n "initializeLegacySetupRole|for \\(auto\\* other : battle_roles_\\)|role\\.RealTowards|role\\.Acceleration" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected:

- `initializeLegacySetupRole` remains only as the narrow compatibility method.
- No nearest-enemy loop over `battle_roles_` remains inside initialization.

---

### Task 4: Introduce Role-Free Adapter Setup Snapshots

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`

- [ ] **Step 1: Replace public `BattleLegacySetupRole` with a value snapshot**

In `BattleSceneBattleAdapter.h`, replace:

```cpp
struct BattleLegacySetupRole
{
    int id = -1;
    Role* role = nullptr;
};
```

with:

```cpp
struct BattleSetupSkillSnapshot
{
    int id = -1;
    std::string name;
    int soundId = -1;
    int hurtType = 0;
    int attackAreaType = 0;
    int magicType = 0;
    int visualEffectId = -1;
    int selectDistance = 0;
    int actProperty = 0;
    double magicPower = 0.0;
};

struct BattleSetupRoleSnapshot
{
    int unitId = -1;
    int realRoleId = -1;
    std::string name;
    int headId = -1;
    int team = -1;
    bool alive = true;
    int gridX = 0;
    int gridY = 0;
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    Pointf facing;
    int star = 1;
    int cost = 0;
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;
    int attack = 0;
    int defence = 0;
    int speed = 0;
    int fist = 0;
    int sword = 0;
    int knife = 0;
    int unusual = 0;
    int hiddenWeapon = 0;
    int cooldown = 0;
    int cooldownMax = 0;
    bool haveAction = false;
    int actFrame = 0;
    Battle::BattleOperationType operationType = Battle::BattleOperationType::None;
    int actType = -1;
    int operationCount = 0;
    int physicalPower = 0;
    int invincible = 0;
    int hurtFrame = 0;
    int frozen = 0;
    int frozenMax = 0;
    std::array<int, 5> fightFrames{};
    std::array<int, 5> actPropertiesByMagicType{};
    bool hasEquippedSkill = false;
    BattleSetupSkillSnapshot normalSkill;
    BattleSetupSkillSnapshot ultimateSkill;
};
```

Add `#include <array>` because the snapshot uses `std::array`.

- [ ] **Step 2: Change setup input to snapshots**

Replace:

```cpp
    std::vector<BattleLegacySetupRole> roles;
```

with:

```cpp
    std::vector<BattleSetupRoleSnapshot> roles;
```

- [ ] **Step 3: Add a scene-side snapshot builder**

In `BattleSceneHades.cpp`, add a private helper that reads `Role`/`Magic` once:

```cpp
KysChess::BattleSceneBattleAdapter::BattleSetupRoleSnapshot BattleSceneHades::makeSetupRoleSnapshot(
    int unitId,
    const Role& role) const
{
    using KysChess::BattleSceneBattleAdapter::BattleSetupRoleSnapshot;
    BattleSetupRoleSnapshot snapshot;
    snapshot.unitId = unitId;
    snapshot.realRoleId = realRoleIdForRole(role);
    snapshot.name = role.Name;
    snapshot.headId = role.HeadID;
    snapshot.team = role.Team;
    snapshot.alive = role.Dead == 0;
    snapshot.gridX = role.X();
    snapshot.gridY = role.Y();
    snapshot.position = role.Pos;
    snapshot.velocity = role.Velocity;
    snapshot.acceleration = role.Acceleration;
    snapshot.facing = role.RealTowards;
    snapshot.star = role.Star;
    snapshot.cost = role.Cost;
    snapshot.hp = role.HP;
    snapshot.maxHp = role.MaxHP;
    snapshot.mp = role.MP;
    snapshot.maxMp = role.MaxMP;
    snapshot.attack = role.Attack;
    snapshot.defence = role.Defence;
    snapshot.speed = role.Speed;
    snapshot.fist = role.Fist;
    snapshot.sword = role.Sword;
    snapshot.knife = role.Knife;
    snapshot.unusual = role.Unusual;
    snapshot.hiddenWeapon = role.HiddenWeapon;
    snapshot.cooldown = role.CoolDown;
    snapshot.cooldownMax = role.CoolDownMax;
    snapshot.haveAction = role.HaveAction != 0;
    snapshot.actFrame = role.ActFrame;
    snapshot.operationType = KysChess::Battle::battleOperationFromLegacy(role.OperationType);
    snapshot.actType = role.ActType;
    snapshot.operationCount = role.OperationCount;
    snapshot.physicalPower = role.PhysicalPower;
    snapshot.invincible = role.Invincible;
    snapshot.hurtFrame = role.HurtFrame;
    snapshot.frozen = role.Frozen;
    snapshot.frozenMax = role.FrozenMax;
    for (int i = 0; i < 5; ++i)
    {
        snapshot.fightFrames[i] = role.FightFrame[i];
    }
    for (int magicType = 0; magicType <= 4; ++magicType)
    {
        snapshot.actPropertiesByMagicType[magicType] = role.getActProperty(magicType);
    }
    return snapshot;
}
```

Then add skill seed selection in the same helper using the existing adapter logic moved or exposed as a snapshot helper.

- [ ] **Step 4: Convert adapter projection methods from `RoleBattleProjection` to snapshot projection**

Replace `RoleBattleProjection` with `BattleSetupRoleProjection` that holds:

```cpp
class BattleSetupRoleProjection
{
public:
    explicit BattleSetupRoleProjection(const BattleSetupRoleSnapshot& snapshot)
        : snapshot_(snapshot)
    {
        assert(snapshot_.unitId >= 0);
    }

    Battle::BattleUnitState worldUnit() const;
    Battle::BattleUnitState initializedWorldUnit(
        const std::map<int, RoleComboState>& comboStates,
        const Battle::BattleWorldState& world,
        const std::map<int, Battle::BattleMovementPhysicsState>& movementRuntime,
        const Battle::BattleActionRulesConfig& actionRules) const;
    Battle::BattleRuntimeUnit runtimeUnit(
        const RoleComboState* state,
        const Battle::BattleGridTransform& gridTransform) const;
    Battle::BattleStatusUnitState statusUnit(const RoleComboState& state) const;
    Battle::BattleDamageUnitState damageUnit(const RoleComboState* state) const;

private:
    const BattleSetupRoleSnapshot& snapshot_;
};
```

All projection methods should copy from `snapshot_`, not from `Role`.

- [ ] **Step 5: Update action plan seeds from snapshots**

Replace `makeBattleActionPlanSeed(int unitId, Role* role, Magic* normalMagic, Magic* ultimateMagic, bool hasEquippedSkill)` with:

```cpp
Battle::BattleActionPlanSeed makeBattleActionPlanSeed(const BattleSetupRoleSnapshot& role)
{
    Battle::BattleActionPlanSeed seed;
    seed.unitId = role.unitId;
    seed.hasEquippedSkill = role.hasEquippedSkill;
    seed.normalSkill = makeBattleActionSkillSeed(role.normalSkill);
    seed.ultimateSkill = makeBattleActionSkillSeed(role.ultimateSkill);
    return seed;
}
```

`makeBattleActionSkillSeed` should convert `BattleSetupSkillSnapshot` to `Battle::BattleActionSkillSeed`.

- [ ] **Step 6: Update Hades build context**

In `makeBattleRuntimeBuildContext()`, replace:

```cpp
        context.setup.roles.push_back({ unitIdForRole(*role), role });
```

with:

```cpp
        context.setup.roles.push_back(makeSetupRoleSnapshot(unitIdForRole(*role), *role));
```

- [ ] **Step 7: Build**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: compile succeeds.

---

### Task 5: Stop Mirroring Initialization Results Back Into `Role`

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Test: `tests/BattleInitializationUnitTests.cpp`

- [ ] **Step 1: Add a role-free scene initialization result**

In `BattleSceneBattleAdapter.h`, replace `BattleRuntimeCreationResult::rolesByUnitId` and `BattleInitializationApplyContext` with:

```cpp
struct BattleInitializedSceneUnit
{
    BattleUnitIdentity identity;
    int unitId = -1;
    int sourceUnitId = -1;
    int gridX = 0;
    int gridY = 0;
    int faceTowards = Towards_None;
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    Pointf realTowards;
    std::array<int, 5> fightFrames{};
    int star = 1;
    int cost = 0;
    int chessInstanceId = -1;
    int weaponId = -1;
    int armorId = -1;
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;
    int attack = 0;
    int defence = 0;
    int speed = 0;
    std::string skillNames;
};

struct BattleInitializationSceneApplyResult
{
    std::map<int, RoleComboState> comboStates;
    std::vector<BattleInitializedSceneUnit> units;
    std::vector<Battle::BattlePresentationLogEvent> logEvents;
    std::vector<Battle::BattlePresentationVisualEvent> visualEvents;
};
```

- [ ] **Step 2: Make runtime creation return scene apply rows**

Change `BattleRuntimeCreationResult` to:

```cpp
struct BattleRuntimeCreationResult
{
    Battle::BattleRuntimeSession session;
    BattleInitializationSceneApplyResult sceneInitialization;
};
```

`createInitializedBattleRuntimeSession()` should build the session, release the core initialization result, then convert it to role-free scene rows by combining:

- input `BattleSetupRoleSnapshot` rows,
- `result.roleDeltas`,
- `result.cloneIntents`,
- `result.enemyTopDebuffs`,
- `context.setup.allyRoster`,
- `context.setup.enemyRoster`.

- [ ] **Step 3: Apply scene initialization directly in Hades**

Add:

```cpp
    void applyBattleInitializationSceneResult(
        const KysChess::BattleSceneBattleAdapter::BattleInitializationSceneApplyResult& result);
```

Implementation shape:

```cpp
void BattleSceneHades::applyBattleInitializationSceneResult(
    const KysChess::BattleSceneBattleAdapter::BattleInitializationSceneApplyResult& result)
{
    KysChess::ChessCombo::getMutableStates() = result.comboStates;
    setup_units_.clear();
    setup_unit_index_by_id_.clear();
    render_units_.clear();
    render_unit_index_by_id_.clear();
    unit_identities_.clear();
    active_unit_ids_.clear();
    ally_unit_ids_.clear();

    for (const auto& unit : result.units)
    {
        setup_unit_index_by_id_[unit.unitId] = setup_units_.size();
        setup_units_.push_back({
            unit.identity,
            unit.gridX,
            unit.gridY,
            unit.faceTowards,
            unit.star,
            unit.chessInstanceId,
            unit.weaponId,
            unit.armorId,
            unit.cost,
            unit.hp,
            unit.maxHp,
            unit.attack,
            unit.defence,
            unit.speed,
            unit.skillNames,
        });

        BattleSceneRenderUnit render;
        render.unitId = unit.unitId;
        render.realRoleId = unit.identity.realRoleId;
        render.name = unit.identity.name;
        render.headId = unit.identity.headId;
        render.team = unit.identity.team;
        render.alive = true;
        render.position = unit.position;
        render.velocity = unit.velocity;
        render.acceleration = unit.acceleration;
        render.realTowards = unit.realTowards;
        render.fightFrames = unit.fightFrames;
        render.hp = unit.hp;
        render.maxHp = unit.maxHp;
        render.mp = unit.mp;
        render.maxMp = unit.maxMp;
        render.gridX = unit.gridX;
        render.gridY = unit.gridY;
        render_unit_index_by_id_[unit.unitId] = render_units_.size();
        render_units_.push_back(std::move(render));

        unit_identities_[unit.unitId] = unit.identity;
        active_unit_ids_.push_back(unit.unitId);
        if (unit.identity.team == 0)
        {
            ally_unit_ids_.push_back(unit.unitId);
        }
    }
}
```

- [ ] **Step 4: Remove `applyBattleInitializationResult()` from Hades flow**

In `initializeBattleRuntime()`, delete the block that moves `created.rolesByUnitId`, calls `applyBattleInitializationResult`, rebuilds setup ids/render rows from roles, and passes `rolesByUnitId` to `commitInitializedBattleRuntimeConfiguration`.

```cpp
    auto rolesByUnitId = std::move(created.rolesByUnitId);
    rebuildSetupUnitIdsFromRolesByUnitId(rolesByUnitId);
    rebuildRenderUnitsFromBattleRoles();
    KysChess::BattleSceneBattleAdapter::commitInitializedBattleRuntimeConfiguration(
        *battle_session_,
        buildContext,
        rolesByUnitId);
    rebuildSetupTrackingFromLegacyState();
```

Replace with:

```cpp
    applyBattleInitializationSceneResult(created.sceneInitialization);
    KysChess::BattleSceneBattleAdapter::commitInitializedBattleRuntimeConfiguration(
        *battle_session_,
        buildContext);
```

- [ ] **Step 5: Update commit setup configuration signature**

Change:

```cpp
void commitInitializedBattleRuntimeConfiguration(
    Battle::BattleRuntimeSession& session,
    const BattleRuntimeBuildContext& context,
    const std::vector<Role*>& rolesByUnitId);
```

to:

```cpp
void commitInitializedBattleRuntimeConfiguration(
    Battle::BattleRuntimeSession& session,
    const BattleRuntimeBuildContext& context);
```

Use runtime unit ids plus `context.setup.roles` snapshots to order configuration rows.

- [ ] **Step 6: Delete obsolete role mirror API**

Delete:

- `BattleInitializationApplyContext`
- `applyBattleInitializationResult`
- `requireRoleByUnitId`
- `makeOrderedRuntimeRoleBindings`
- `rebuildSetupUnitIdsFromRolesByUnitId`
- `rebuildRenderUnitsFromBattleRoles`
- `rebuildSetupTrackingFromLegacyState`

- [ ] **Step 7: Add clone scene-row test**

In `tests/BattleInitializationUnitTests.cpp`, add coverage that initialization clone rows preserve source identity and use the clone runtime id.

Test shape:

```cpp
TEST_CASE("Battle initialization scene apply result creates clone rows without Role mirror", "[battle][initialization]")
{
    // Build a setup input with one ally source that triggers clone creation.
    // Assert the adapter sceneInitialization.units contains both source unit 0 and clone unit 1.
    // Assert clone identity.realRoleId equals source realRoleId.
    // Assert clone unitId is 1 and clone hp/maxHp/attack/defence/speed match clone intent values.
}
```

Use the existing initialization seed helpers from `tests/BattleInitializationUnitTests.cpp`; do not instantiate `BattleSceneHades` in this unit test.

- [ ] **Step 8: Build and run full tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: all tests pass.

---

### Task 6: Delete Role-Based Hades Setup Helpers

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

- [ ] **Step 1: Delete role-id pointer map**

Remove:

```cpp
    std::unordered_map<const Role*, int> setup_unit_id_by_role_;
```

Replace `assignSetupUnitId(Role&)` with:

```cpp
int BattleSceneHades::assignSetupUnitId()
{
    return next_setup_unit_id_++;
}
```

Delete:

- `unitIdForRole(const Role&)`
- `realRoleIdForRole(const Role&)` only if all callers use snapshot identity or a local `realRoleIdForLegacyRole(const Role&)` inside setup construction.

- [ ] **Step 2: Delete role-based render/setup builders**

Delete:

- `makeRenderUnitFromRole`
- `upsertRenderUnitFromRole`
- `makeSetupUnitFromRole`

Scene setup rows should now come from `BattleInitializationSceneApplyResult`.

- [ ] **Step 3: Delete `initializeLegacySetupRole`**

After role-free snapshots carry all initial defaults, delete:

- declaration in `BattleSceneHades.h`,
- definition in `BattleSceneHades.cpp`,
- all calls.

The setup path should build `BattleSetupRoleSnapshot` directly from source `Role` and `BattleSceneInitialRoleState`.

- [ ] **Step 4: Rewrite `onEntrance()` as three phases**

The target `onEntrance()` shape should be:

```cpp
void BattleSceneHades::onEntrance()
{
    setupRenderingEnvironment();
    buildInitialBattleSetupRows();
    initializeBattleRuntime();
    runPreBattlePositionSwapIfEnabled();
    commitFinalSetupPlacementToRuntime();
    Audio::getInstance()->playMusic(KysChess::getRandomBattleMusic());
}
```

If helper extraction feels too large, keep helper names private and in the same file. Do not introduce a broad setup manager object in this batch.

- [ ] **Step 5: Run deletion searches**

Run:

```powershell
rg -n "initializeLegacySetupRole|makeRenderUnitFromRole|upsertRenderUnitFromRole|makeSetupUnitFromRole|rebuildRenderUnitsFromBattleRoles|rebuildSetupTrackingFromLegacyState|setup_unit_id_by_role_|unitIdForRole\\(|rolesByUnitId|BattleLegacySetupRole|Role\\* role = nullptr" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
```

Expected:

- No matches for deleted setup helpers.
- No public `BattleLegacySetupRole`.
- No `rolesByUnitId`.
- Remaining `Role*` matches in the adapter are limited to local helper declarations used by `makeSetupRoleSnapshot` during the transitional setup-read step.

---

### Task 7: Remove Hades Dependency On `BattleSceneAct` Staging State

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneAct.h`

- [ ] **Step 1: Move required scalar fields into Hades**

Add Hades-owned copies of the `BattleSceneAct` scalar fields that Hades still reads:

```cpp
    Pointf pos_;
    float gravity_ = -4;
    float friction_ = 0.1f;
    int frozen_ = 0;
    int slow_ = 0;
    int shake_ = 0;
    int close_up_ = 0;
```

- [ ] **Step 2: Change inheritance**

Change:

```cpp
class BattleSceneHades : public BattleSceneAct
```

to:

```cpp
class BattleSceneHades : public BattleScene
```

Keep this step only after Hades no longer uses `BattleSceneAct` containers or methods.

- [ ] **Step 3: Remove unused include**

Replace:

```cpp
#include "BattleSceneAct.h"
```

with:

```cpp
#include "BattleScene.h"
```

in `BattleSceneHades.h`.

- [ ] **Step 4: Build**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected: both targets compile, or final game link is blocked only if the executable is running.

---

## Final Verification

Run:

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
rg -n "TeamMenu|AutoTeamMate|TeamMateX|initializeLegacySetupRole|makeRenderUnitFromRole|upsertRenderUnitFromRole|makeSetupUnitFromRole|rebuildRenderUnitsFromBattleRoles|rebuildSetupTrackingFromLegacyState|setup_unit_id_by_role_|rolesByUnitId|BattleLegacySetupRole|enemies_|enemies_obj_|friends_obj_" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h src\BattleSceneAct.h
```

Expected:

- No diff whitespace errors.
- Tests pass.
- `kys` debug target compiles.
- Boundary search has no matches for deleted migration artifacts.

## Known Deferred Cleanup

- `Event::tryBattle()` still routes `battle_mode == 2`, `3`, and `4` to Hades without deployed teammates. This batch intentionally ignores those callers. A later cleanup should either remove those modes or route them to `BattleScene`.
- `BattleScene` still owns legacy `battle_roles_`, `friends_`, cursor, menu, and role layer state. This plan removes Hades initialization dependence first; full `Scene` inheritance cleanup can follow after the initialization boundary is role-free.
