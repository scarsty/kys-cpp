# Battle Scene Role Removal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove Hades frame-time and rendering dependence on `Role`, leaving `Role`/`Magic` as setup-only legacy inputs inside clearly named setup projection code.

**Architecture:** Hades should render from `BattleSceneRenderUnit`, play presentation from runtime events, and build post-battle stats from explicit unit summaries. The legacy `BattleScene`/`BattleSceneAct` inheritance should be reduced to map/coordinate helpers first, then replaced with small Hades-owned map and presentation-effect state.

**Tech Stack:** C++20, existing battle runtime systems, Catch2 unit tests, Visual Studio solution build through `.github\build-command.ps1`.

---

## Current Dependency Map

Hades still needs these inherited/base capabilities:

- `BattleInfo` loading through `setID()` / `info_`.
- Map layer data, `makeEarthTexture()`, `isOutLine()`, `isBuilding()`, `isWater()`, `pos45To90()`, `pos90To45()`.
- `RandomDouble rand_`, battle music restore, and normal `Scene` / `RunNode` lifecycle.
- `AttackEffect` and `TextEffect` containers, but only as role-free presentation carriers.

Hades should stop using these inherited legacy combat members:

- `battle_roles_`, `friends_`, `role_layer_`, `acting_role_`.
- `battle_cursor_`, `battle_menu_`, `head_self_`, `head_boss_`.
- `setFaceTowardsNearest(Role*)`, `inEffect(Role*, Role*)`, `calRolePic(Role*)`, `BattleScene::calExpGot()`.
- Legacy action/menu/cursor systems in `BattleCursor.*` and `BattleMenu.*`.

Remaining `Role` usage categories:

- Setup roster construction and runtime seeding: allowed for this batch.
- Pre-battle placement swap: must move to Hades setup/render rows.
- Frame application: must be removed.
- Presentation logging/tracker: must use unit identity rows, not `Role*`.
- Post-battle stats/rewards: must consume explicit summary rows, not `friends_obj_` / `enemies_obj_`.

## File Responsibilities

- `src/BattleSceneHades.h/.cpp`: Own Hades setup rows, render rows, presentation application, swap state, and post-battle summaries. Stop using inherited `Role` state outside setup seeding.
- `src/BattleSceneAct.h`: Stop being a dependency of Hades after `AttackEffect` / `TextEffect` move to a role-free header.
- `src/BattleSceneBattleAdapter.h/.cpp`: Keep setup-only projection from `Role`/`Magic`; delete frame apply helpers after Hades consumes runtime output directly.
- `src/BattleScenePresentationPlayer.h/.cpp`: Play logs/visuals through role-free unit identity and render unit views.
- `src/BattleStatsView.h/.cpp`: Accept post-battle summary rows instead of `std::deque<Role>`.
- `src/BattlePostBattleSummary.h`: Define role-free battle identity and post-battle row data shared by Hades and stats view.
- `src/battle/BattleCore.h/.cpp`: Add only missing runtime frame output fields needed to remove scene role writes.
- `src/ChessCombo.h/.cpp`: Add an id/team based anti-combo transfer path, then remove the Hades call that passes `std::vector<Role*>`.
- `tests/BattleCoreUnitTests.cpp` and `tests/BattlePresentationUnitTests.cpp`: Cover new runtime outputs and role-free tracker/presentation behavior.

---

### Task 1: Add Role-Free Unit Identity And Post-Battle Summary Types

**Files:**
- Create: `src/BattlePostBattleSummary.h`
- Modify: `src/kys.vcxproj`
- Modify: `src/kys.vcxproj.filters`
- Test: `tests/BattlePresentationUnitTests.cpp`

- [ ] **Step 1: Add shared role-free summary types**

Create `src/BattlePostBattleSummary.h`:

```cpp
#pragma once

#include <map>
#include <string>
#include <vector>

struct BattleUnitIdentity
{
    int battleId = -1;
    int realRoleId = -1;
    int team = -1;
    int headId = -1;
    std::string name;
};

struct BattlePostBattleUnitSummary
{
    BattleUnitIdentity identity;
    int star = 1;
    int chessInstanceId = -1;
    int hp = 0;
    int maxHp = 0;
    int attack = 0;
    int defence = 0;
    int speed = 0;
    int weaponId = -1;
    int armorId = -1;
    std::string skillNames;
    int hpRemaining = 0;
    int maxHpRemaining = 0;
    bool dead = false;
    int cancelDmg = 0;
};

struct BattlePostBattleSummary
{
    std::vector<BattlePostBattleUnitSummary> allies;
    std::vector<BattlePostBattleUnitSummary> enemies;
    int battleResult = -1;
};
```

- [ ] **Step 2: Register the header in project files**

Add `BattlePostBattleSummary.h` to `src/kys.vcxproj` near other battle UI headers and to `src/kys.vcxproj.filters` in the same filter group as `BattleStatsView.h`.

- [ ] **Step 3: Build check for the new header**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: compilation succeeds or reaches an unrelated final link lock if the game executable is running.

---

### Task 2: Make BattleTracker Role-Free

**Files:**
- Modify: `src/BattleStatsView.h`
- Modify: `src/BattleStatsView.cpp`
- Modify: `src/BattleScenePresentationPlayer.h`
- Modify: `src/BattleScenePresentationPlayer.cpp`
- Test: `tests/BattlePresentationUnitTests.cpp`

- [ ] **Step 1: Change tracker APIs to accept identities**

In `src/BattleStatsView.h`, replace the `Role*` tracker methods with identity-based methods:

```cpp
class BattleTracker
{
public:
    void recordDamage(
        const BattleUnitIdentity* attacker,
        const BattleUnitIdentity* defender,
        int damage,
        const std::string& skillName,
        int frame,
        const std::string& detailText = "");
    void recordHeal(
        const BattleUnitIdentity* source,
        const BattleUnitIdentity* target,
        int amount,
        const std::string& reason,
        int frame);
    void recordStatus(
        const BattleUnitIdentity* source,
        const BattleUnitIdentity* target,
        const std::string& text,
        int frame);
    void recordKill(const BattleUnitIdentity* killer, const BattleUnitIdentity* victim, int frame);
    void recordDeath(const BattleUnitIdentity* unit, int frame);
    void recordProjectileCancel(int unitId, int damage);
    int cancelDamageForUnit(int unitId) const;
```

Add `std::map<int, int> cancelDamageByUnit_;` to the private section.

- [ ] **Step 2: Implement tracker identity writes**

In `src/BattleStatsView.cpp`, replace every `role->ID`, `role->Team`, and `role->Name` read inside `BattleTracker` with `identity->battleId`, `identity->team`, and `identity->name`.

Use this shape for null-safe assignment inside each method:

```cpp
if (attacker)
{
    auto& s = stats_[attacker->battleId];
    s.damageDealt += damage;
    if (s.firstDamageFrame == 0)
    {
        s.firstDamageFrame = frame;
    }
    s.lastActiveFrame = frame;
    if (!skillName.empty())
    {
        s.damagePerSkill[skillName] += damage;
    }
    event.sourceId = attacker->battleId;
    event.sourceTeam = attacker->team;
    event.sourceName = attacker->name;
}
```

Implement projectile cancel accumulation:

```cpp
void BattleTracker::recordProjectileCancel(int unitId, int damage)
{
    assert(unitId >= 0);
    assert(damage >= 0);
    cancelDamageByUnit_[unitId] += damage;
}

int BattleTracker::cancelDamageForUnit(int unitId) const
{
    const auto it = cancelDamageByUnit_.find(unitId);
    return it == cancelDamageByUnit_.end() ? 0 : it->second;
}
```

- [ ] **Step 3: Update presentation player bindings**

In `src/BattleScenePresentationPlayer.h`, replace `resolveRole` with:

```cpp
std::function<const BattleUnitIdentity*(int)> resolveIdentity;
```

In `src/BattleScenePresentationPlayer.cpp`, replace `resolveRole()` with `resolveIdentity()` and pass identities into `BattleTracker`.

- [ ] **Step 4: Add tracker tests**

Add tests that construct two `BattleUnitIdentity` values and verify `recordDamage()`, `recordKill()`, `recordDeath()`, and `recordProjectileCancel()` fill ids, teams, names, stats, and cancel damage without a `Role`.

- [ ] **Step 5: Run focused tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattlePresentation*"
```

Expected: tracker and presentation tests pass.

---

### Task 3: Build Hades Setup Rows And Stop Using Role For Swap/Identity

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Test: scene boundary searches

- [ ] **Step 1: Add Hades setup and identity storage**

In `BattleSceneHades`, add:

```cpp
struct BattleSceneSetupUnit
{
    BattleUnitIdentity identity;
    int gridX = 0;
    int gridY = 0;
    int star = 1;
    int chessInstanceId = -1;
    int weaponId = -1;
    int armorId = -1;
    bool pendingEnemy = false;
};

std::vector<BattleSceneSetupUnit> setup_units_;
std::unordered_map<int, std::size_t> setup_unit_index_by_id_;
std::vector<int> active_unit_ids_;
std::deque<int> pending_enemy_unit_ids_;
std::vector<int> ally_unit_ids_;
int swapSelectedUnitId_ = -1;
```

Delete `swapSelected_`.

- [ ] **Step 2: Populate setup rows during `onEntrance()`**

When Hades currently pushes into `enemies_obj_`, `friends_obj_`, `enemies_`, `friends_`, and `battle_roles_`, also call a helper:

```cpp
void BattleSceneHades::addSetupUnitFromRole(
    const Role& role,
    int team,
    int gridX,
    int gridY,
    int star,
    int chessInstanceId,
    int weaponId,
    int armorId,
    bool pendingEnemy)
{
    BattleSceneSetupUnit unit;
    unit.identity.battleId = role.ID;
    unit.identity.realRoleId = role.RealID;
    unit.identity.team = team;
    unit.identity.headId = role.HeadID;
    unit.identity.name = role.Name;
    unit.gridX = gridX;
    unit.gridY = gridY;
    unit.star = star;
    unit.chessInstanceId = chessInstanceId;
    unit.weaponId = weaponId;
    unit.armorId = armorId;
    unit.pendingEnemy = pendingEnemy;
    setup_unit_index_by_id_[unit.identity.battleId] = setup_units_.size();
    setup_units_.push_back(std::move(unit));
}
```

Use `active_unit_ids_`, `pending_enemy_unit_ids_`, and `ally_unit_ids_` for Hades state. Restrict `battle_roles_` to setup projection code and remove every frame-time Hades reference before Task 8.

- [ ] **Step 3: Replace tile-click swap role lookup**

In `PositionSwapNode`, iterate `ally_unit_ids_` and use `requireRenderUnit(unitId)` / `requireSetupUnit(unitId)` instead of `getBattleRoles()`.

Swap by updating setup rows and render rows:

```cpp
auto& aSetup = battle_->requireSetupUnit(aId);
auto& bSetup = battle_->requireSetupUnit(bId);
std::swap(aSetup.gridX, bSetup.gridX);
std::swap(aSetup.gridY, bSetup.gridY);

auto& aRender = battle_->requireRenderUnit(aId);
auto& bRender = battle_->requireRenderUnit(bId);
std::swap(aRender.gridX, bRender.gridX);
std::swap(aRender.gridY, bRender.gridY);
aRender.position = battle_->pos45To90(aRender.gridX, aRender.gridY);
bRender.position = battle_->pos45To90(bRender.gridX, bRender.gridY);
```

- [ ] **Step 4: Replace list-based swap role lookup**

Build the menu from `ally_unit_ids_` and `BattleSceneSetupUnit::identity.name`, `gridX`, and `gridY`. Do not read `Role::Name`, `Role::X()`, or `Role::Y()` in the swap loop.

- [ ] **Step 5: Update final setup placement input**

Change `makeBattleSetupPlacementInput(battle_roles_)` usage to build from `setup_units_` or `render_units_`:

```cpp
auto placement = makeBattleSetupPlacementInputFromSetupUnits(setup_units_);
battle_session_->commitSetupPlacement(placement);
```

This helper should produce unit id, grid cell, and facing only.

---

### Task 4: Remove Frame-Time Role Writes From Hades

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/ChessCombo.h`
- Modify: `src/ChessCombo.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Remove `applyBattleFrameUnitApplication()` usage**

In `BattleSceneHades::applyCoreFrameResult()`, keep the existing `BattleSceneRenderUnit` writes and delete:

```cpp
auto* role = requireFrameRole(bindings, application.unitId);
applyBattleFrameUnitApplication(role, application);
```

After all call sites are removed, delete `applyBattleFrameUnitApplication()` from adapter header and cpp.

- [ ] **Step 2: Remove status role mirrors**

In `applyCoreStatusState()`, delete `Role::Invincible`, `Role::Frozen`, and `Role::FrozenMax` writes. Keep only `BattleSceneRenderUnit` writes.

- [ ] **Step 3: Remove movement role mirrors**

In `applyCoreFrameResult()`, delete `Role::Frozen`, `Role::Pos`, `Role::Velocity`, `Role::Acceleration`, and `Role::RealTowards` writes in the movement presentation loop.

- [ ] **Step 4: Replace action apply adapter with direct render behavior**

Delete `makeBattleActionFrameApplyContext()` and `applyBattleActionFrameResults()` from Hades call sites.

For blink teleports, play the blink sound directly while applying render teleports:

```cpp
for (const auto& teleport : action.actionResult.blinkTeleports)
{
    unit.gridX = teleport.gridX;
    unit.gridY = teleport.gridY;
    unit.position = { teleport.position.x, teleport.position.y, 0 };
    unit.velocity = { 0, 0, 0 };
    unit.acceleration = { 0, 0, gravity_ };
    unit.realTowards = teleport.facing;
    Audio::getInstance()->playESound(BLINK_SOUND_EFFECT_ID);
}
```

After this, run `rg -n "BattleActionFrameApplyContext|BattleActionFrameApplyResult|applyBattleActionFrameResults|applyActionFrameUnitState|applyBlinkTeleportDelta|applyBattleCastStart|applyBattleCastCommit" src tests`. Delete every matched declaration/definition that has no producer or consumer outside the old frame apply adapter.

- [ ] **Step 5: Move projectile cancel stats to tracker**

In Hades, replace `applyBattleProjectileCancelDamage(requireFrameRole(...))` with:

```cpp
tracker_.recordProjectileCancel(command.sourceUnitId, command.damage);
tracker_.recordProjectileCancel(command.otherSourceUnitId, command.otherDamage);
```

Then delete `applyBattleProjectileCancelDamage()` from adapter header and cpp.

- [ ] **Step 6: Remove `LastAttacker` writes**

Delete Hades writes to `Role::LastAttacker` in damage and frame applications. Run `rg -n "lastAttackers|BattleFrameLastAttacker" src tests`; if Hades was the only consumer, remove the scene-facing application field in the same task.

- [ ] **Step 7: Replace anti-combo transfer role list**

Add an id/team based overload in `ChessCombo`:

```cpp
struct ChessComboBattleUnitRef
{
    int battleId = -1;
    int realRoleId = -1;
    int team = -1;
    bool alive = true;
};

static void transferAntiCombo(int deadRoleId, const std::vector<ChessComboBattleUnitRef>& units);
```

Build the vector from `render_units_` plus `setup_units_`, not `battle_roles_`. Then remove the Hades call to `ChessCombo::transferAntiCombo(r->ID, battle_roles_)`.

- [ ] **Step 8: Remove rescue role writes**

In `applyCoreDamageTransactions()`, update pulled/puller render rows directly. Replace `setFaceTowardsNearest(pulled)` and `setFaceTowardsNearest(puller)` with a Hades helper:

```cpp
Pointf BattleSceneHades::facingTowardNearestEnemy(int unitId) const;
```

The helper iterates `render_units_`, ignores dead units and same-team units, and returns the normalized direction from the source render unit to the nearest enemy.

- [ ] **Step 9: Remove team effect role writes**

In `applyCoreTeamEffectState()` and `applyCoreFrameApplications()`, keep render writes, sound playback, and rumble playback. Delete writes to `Role::HP`, `Role::MP`, `Role::CoolDown`, `Role::Velocity`, and `Role::HurtFrame`.

- [ ] **Step 10: Add runtime/output tests**

Add tests that verify:

- `projectileCancelDamageCommands` can be consumed without a `Role`.
- Blink teleport output contains enough grid/position/facing data for render application.
- Damage/death output contains enough final hp/mp/alive/frozen/cooldown data for render application.

- [ ] **Step 11: Run boundary search**

Run:

```powershell
rg -n "applyBattleFrameUnitApplication|applyBattleProjectileCancelDamage|applyBattleActionFrameResults|BattleActionFrameApplyContext|LastAttacker|CancelDmg|setFaceTowardsNearest\\(|transferAntiCombo\\([^\\n]*battle_roles_|requireFrameRole" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
```

Expected: no Hades frame-time role application helpers remain. Setup-only role helpers may still exist in adapter.

---

### Task 5: Remove Cursor/Menu/Head Usage From Hades

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneAct.h`
- Test: boundary searches

- [ ] **Step 1: Delete unused cursor targeting highlight**

In Hades `draw()`, delete:

```cpp
if (battle_cursor_->isRunning() && !acting_role_->isAuto())
{
    info.color = { 128, 128, 128, 255 };
    auto* legacyRole = requireFrameRole(core_role_bindings_, unit.unitId);
    if (inEffect(acting_role_, legacyRole))
    {
        info.color = { 255, 255, 255, 255 };
    }
}
```

No replacement is needed because Hades does not use `BattleCursor` or legacy action targeting.

- [ ] **Step 2: Remove `head_self_` usage**

Delete `head_self_->setPosition(10, 10);` from Hades `onEntrance()`. Hades draw already renders in-world bars from `BattleSceneRenderUnit`.

- [ ] **Step 3: Replace boss head visibility with render HUD or delete it**

Delete `head_boss_` construction and `setRole()` calls from Hades. If boss HP must remain visible outside the world view, add a small Hades render method that draws enemy `BattleSceneRenderUnit` rows directly using `headId`, `hp`, and `maxHp`.

The first implementation should delete `head_boss_` in Hades and rely on in-world bars unless product review says a boss HUD is still required.

- [ ] **Step 4: Remove Hades dependency on legacy menus**

Confirm Hades only uses `MenuText` for setup swap confirmation and list selection. Keep `MenuText`; do not use `BattleActionMenu`, `BattleMagicMenu`, `BattleCursor`, or `BattleMenu`.

- [ ] **Step 5: Run boundary search**

Run:

```powershell
rg -n "battle_cursor_|battle_menu_|head_self_|head_boss_|acting_role_|inEffect\\(|BattleCursor|BattleActionMenu" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: no matches in Hades.

---

### Task 6: Make Post-Battle Stats Consume Summary Rows

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleStatsView.h`
- Modify: `src/BattleStatsView.cpp`
- Modify: `src/ChessBattleFlow.cpp`
- Test: manual build and battle stats view smoke path

- [ ] **Step 1: Add Hades post-battle summary API**

Replace:

```cpp
const std::deque<Role>& getFriendsObj() const;
const std::deque<Role>& getEnemiesObj() const;
```

with:

```cpp
BattlePostBattleSummary makePostBattleSummary() const;
```

Build summary rows from `setup_units_`, `render_units_`, tracker cancel damage, and runtime final state already published into render rows.

- [ ] **Step 2: Fill stats from setup identity and render state**

For each setup unit, look up its render row by `battleId` and fill:

```cpp
summaryUnit.identity = setup.identity;
summaryUnit.star = setup.star;
summaryUnit.chessInstanceId = setup.chessInstanceId;
summaryUnit.weaponId = setup.weaponId;
summaryUnit.armorId = setup.armorId;
summaryUnit.hpRemaining = render.hp;
summaryUnit.maxHpRemaining = render.maxHp;
summaryUnit.dead = !render.alive;
summaryUnit.cancelDmg = tracker_.cancelDamageForUnit(setup.identity.battleId);
```

Use `BattleRoleManager::computeStarStats()` during setup while the original `Role*` is available, then store hp/attack/defence/speed on the setup row. Do not call `roleSave_.getRole()` from `makePostBattleSummary()` except for immutable metadata that was not captured during setup.

- [ ] **Step 3: Update `BattleStatsView::setupPostBattle()`**

Change the signature to:

```cpp
void setupPostBattle(
    const BattlePostBattleSummary& summary,
    const BattleTracker& tracker);
```

Remove loops over `std::deque<Role>`. Build `RoleEntry` from `BattlePostBattleUnitSummary`.

- [ ] **Step 4: Update battle flow**

In `src/ChessBattleFlow.cpp`, replace:

```cpp
view->setupPostBattle(battle->getFriendsObj(), battle->getEnemiesObj(), battle->getTracker(), battle->getResult());
```

with:

```cpp
view->setupPostBattle(battle->makePostBattleSummary(), battle->getTracker());
```

- [ ] **Step 5: Remove `RoleEntry::role`**

In `BattleStatsView::RoleEntry`, replace `Role* role` with role-free identity fields:

```cpp
BattleUnitIdentity identity;
std::string displayName;
```

Update table drawing and log label building to use `entry.displayName` and `entry.identity.battleId`.

---

### Task 7: Replace Hades Result And Reward Paths

**Files:**
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Test: `tests/BattleCoreUnitTests.cpp`, boundary search, and a manual battle stats smoke run

- [ ] **Step 1: Replace `checkResult()` team counts**

Stop calling inherited `getTeamMateCount()`. Count alive active render units:

```cpp
int BattleSceneHades::aliveRenderUnitsOnTeam(int team) const
{
    return static_cast<int>(std::ranges::count_if(render_units_, [team](const auto& unit)
        {
            return unit.team == team && unit.alive;
        }));
}
```

Use `pending_enemy_unit_ids_.size()` plus alive active enemy count if queued enemies still exist.

- [ ] **Step 2: Replace `BattleScene::calExpGot()`**

Delete `BattleScene::calExpGot()` call from `BattleSceneHades::calExpGot()`.

Implement Hades reward handling directly:

- Increment `fightsWon` for each unique `extended_teammates_.chessInstanceId` when `result_ == 0`.
- Preserve `count_fights_won_` behavior.
- Do not run `ShowExp` / `ShowRoleDifference` from the legacy base path in Hades.

- [ ] **Step 3: Replace enemy entrance queue**

In `applyLegacyBattleFrameResult()`, replace `while (!enemies_.empty())` with `pending_enemy_unit_ids_`. Add the unit id to `active_unit_ids_`, set `render.attention = 30`, and keep runtime pending alive counts owned by runtime.

- [ ] **Step 4: Delete `applyLegacyBattleFrameResult()` role loop**

Remove the loop over `battle_roles_` that clamps `Role::HP`, `Role::MP`, `Role::Shake`, `Role::HurtFrame`, and `Role::Attention`. Keep the render-unit clamp/decrement loop.

---

### Task 8: Shrink The Adapter To Setup-Only Role Projection

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Test: boundary searches

- [ ] **Step 1: Delete public frame apply API**

Remove these declarations and definitions after Tasks 2-7 remove call sites:

- `BattleActionFrameApplyContext`
- `BattleActionFrameApplyResult`
- `BattleLifecycleApplicationContext`
- `BattleLifecycleApplicationResult`
- `applyBattleFrameUnitApplication()`
- `applyBattleProjectileCancelDamage()`
- `applyBattleActionFrameResults()`
- `applyBattleLifecycleEvents()`

- [ ] **Step 2: Keep setup projection explicitly named**

Rename setup-only functions to make the remaining adapter ownership clear:

```cpp
BattleRuntimeCreationResult createRuntimeSessionFromLegacyRoleSetup(const BattleRuntimeBuildContext& context);
void applyRuntimeInitializationToLegacySetupCopies(
    const Battle::BattleInitializationResult& result,
    const BattleInitializationApplyContext& context);
```

If `applyRuntimeInitializationToLegacySetupCopies()` only exists for clone Role copies after Task 6, delete it instead of renaming it.

- [ ] **Step 3: Remove `BattleSceneRoleBindings` from Hades frame path**

Replace `BattleSceneRoleBindings core_role_bindings_` with:

```cpp
std::unordered_map<int, BattleUnitIdentity> unit_identities_;
```

Use it for tracker/presentation identity resolution.

- [ ] **Step 4: Run adapter boundary search**

Run:

```powershell
rg -n "BattleActionFrameApply|BattleLifecycleApplication|applyBattleFrameUnitApplication|applyBattleProjectileCancelDamage|applyBattleActionFrameResults|applyBattleLifecycleEvents|rolesByBattleId" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
```

Expected: no frame apply adapter types remain. Any `Role*` usage in adapter should be setup projection only.

---

### Task 9: Move Hades Off `BattleSceneAct` Legacy State

**Files:**
- Create: `src/BattlePresentationEffects.h`
- Modify: `src/BattleSceneAct.h`
- Modify: `src/BattleScenePresentationPlayer.h`
- Modify: `src/BattleScenePresentationPlayer.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

- [ ] **Step 1: Extract role-free effect types**

Create `src/BattlePresentationEffects.h` with `BattleAttackEffect` and `BattleTextEffect` copied from `BattleSceneAct` after removing role pointers:

```cpp
struct BattleAttackEffect
{
    Pointf Pos;
    Pointf Velocity;
    Pointf Acceleration;
    int Frame = 0;
    int TotalFrame = 1;
    int TotalEffectFrame = 1;
    std::string Path;
    int FollowUnitId = -1;
    int VisualAttackId = -1;
    int VisualEffectId = -1;
    int VisualOnly = 0;
    int VisualTeam = -1;
    int SpiralMotion = 0;
    Pointf SpiralCenter;
    float SpiralRadius = 0.0f;
    float SpiralRadiusGrowth = 0.0f;
    float SpiralAngle = 0.0f;
    float SpiralAngularVelocity = 0.0f;

    void setEft(int num);
    void setPath(const std::string& path);
    int renderTeam() const;
};

struct BattleTextEffect
{
    Pointf Pos;
    std::string Text;
    int Size = 15;
    int Frame = 0;
    Color color;
    int Type = 0;
};
```

Keep non-visual gameplay-only fields out of the extracted type.

- [ ] **Step 2: Update Hades and presentation player containers**

Change Hades to:

```cpp
std::deque<BattleAttackEffect> attack_effects_;
std::deque<BattleTextEffect> text_effects_;
```

Update `BattleScenePresentationPlayer::Bindings` to use the new types.

- [ ] **Step 3: Delete `FollowRole` fallback**

In Hades draw, remove:

```cpp
else if (ae.FollowRole)
{
    effect_pos = ae.FollowRole->Pos + ae.Pos;
}
```

Use only `FollowUnitId`.

- [ ] **Step 4: Move `advanceVisualOnlyEffects()`**

Add a free helper beside the new effect types:

```cpp
void advanceVisualOnlyEffects(std::deque<BattleAttackEffect>& effects);
```

Call it from Hades instead of `BattleSceneAct::advanceVisualOnlyEffects()`.

---

### Task 10: Replace Inherited BattleScene Map Usage With A Hades Map Base

**Files:**
- Create: `src/BattleSceneMapState.h`
- Create: `src/BattleSceneMapState.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/kys.vcxproj`
- Modify: `src/kys.vcxproj.filters`

- [ ] **Step 1: Extract only the map helpers Hades actually uses**

Create `BattleSceneMapState` with:

```cpp
class BattleSceneMapState
{
public:
    void initialize(int coordCount);
    void loadBattlefield(int battlefieldId);
    void makeEarthTexture();
    bool isOutLine(int x, int y) const;
    bool isBuilding(int x, int y) const;
    bool isWater(int x, int y) const;
    bool canWalk45(int x, int y) const;
    Pointf pos45To90(int x, int y) const;
    Point pos90To45(double x, double y) const;

    MapSquareInt& earthLayer();
    MapSquareInt& buildingLayer();
    const MapSquareInt& earthLayer() const;
    const MapSquareInt& buildingLayer() const;

private:
    int coordCount_ = 0;
    MapSquareInt earthLayer_;
    MapSquareInt buildingLayer_;
};
```

- [ ] **Step 2: Move Hades draw/build context calls to map state**

Replace Hades direct references to `earth_layer_`, `building_layer_`, `isOutLine()`, `canWalk45()`, `pos45To90()`, and `pos90To45()` with `battle_map_`.

- [ ] **Step 3: Keep legacy BattleScene untouched for old battles**

Do not remove `BattleScene` yet. This task only makes Hades independent of its map fields so the final inheritance switch is small.

---

### Task 11: Make Hades Inherit `Scene` Directly

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneAct.h` if no longer needed by Hades
- Test: full build

- [ ] **Step 1: Change base class**

Change:

```cpp
class BattleSceneHades : public BattleSceneAct
```

to:

```cpp
class BattleSceneHades : public Scene
```

Include only the headers Hades actually needs.

- [ ] **Step 2: Add Hades-owned lifecycle fields**

Move the fields Hades still needs out of inherited bases:

```cpp
RandomDouble rand_;
BattleInfo* info_ = nullptr;
int battle_id_ = 0;
Pointf pos_;
float gravity_ = -4;
float friction_ = 0.1f;
UIKeyConfig::Keys keys_;
int frozen_ = 0;
int slow_ = 0;
int shake_ = 0;
int close_up_ = 0;
int result_ = -1;
int prev_music_ = 0;
std::vector<ExtendedTeammateInfo> extended_teammates_;
BattleSceneMapState battle_map_;
```

Only add fields that Hades references after Tasks 1-10.

- [ ] **Step 3: Move `setID()` and `setExtendedBattleInfo()` behavior**

Implement Hades-owned versions:

```cpp
void BattleSceneHades::setID(int id)
{
    battle_id_ = id;
    info_ = &BattleMap::getInstance()->modifiableBattleInfo(id);
}

void BattleSceneHades::setExtendedBattleInfo(const std::vector<ExtendedTeammateInfo>& teammates)
{
    extended_teammates_ = teammates;
}
```

Copy the current `BattleScene::setID()` behavior for battle info lookup and layer loading into Hades, then delete Hades calls that depend on the inherited implementation.

- [ ] **Step 4: Remove inherited UI allocation side effects**

After direct `Scene` inheritance, Hades no longer constructs `BattleActionMenu`, `BattleCursor`, `Head`, `role_layer_`, `select_layer_`, or `effect_layer_`.

- [ ] **Step 5: Full boundary search**

Run:

```powershell
rg -n "Role\\*|Role&|battle_roles_|friends_|enemies_|friends_obj_|enemies_obj_|role_layer_|BattleSceneAct|BattleScene::|battle_cursor_|battle_menu_|head_self_|head_boss_|acting_role_|FollowRole|getBattleRoles|getRoleLayer|calRolePic\\(|setRoleInitState\\(|setFaceTowardsNearest\\(|inEffect\\(" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleScenePresentationPlayer.cpp src\BattleScenePresentationPlayer.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
```

Expected:

- Hades has no frame-time or render-time `Role` references.
- Adapter has `Role` references only in setup projection functions.
- Presentation player has no `Role` references.
- `BattleSceneAct` is not included by Hades.

---

## Verification

Run after each task that changes compiled code:

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Run after the final task:

```powershell
rg -n "BattleCursor|BattleActionMenu|BattleMenu|head_self_|head_boss_|battle_cursor_|battle_menu_|acting_role_" src\BattleSceneHades.cpp src\BattleSceneHades.h
rg -n "Role\\*|Role&|requireFrameRole|rolesByBattleId|FollowRole|CancelDmg|LastAttacker|battle_roles_|friends_obj_|enemies_obj_" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleScenePresentationPlayer.cpp src\BattleScenePresentationPlayer.h
rg -n "applyBattleFrameUnitApplication|applyBattleProjectileCancelDamage|applyBattleActionFrameResults|BattleActionFrameApplyContext" src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
```

Expected:

- No Hades cursor/menu/head usage.
- No Hades frame-time `Role` dependency.
- No presentation-player `Role` dependency.
- Adapter frame apply surface removed.
- Remaining adapter `Role` usage is setup-only projection from legacy `Role`/`Magic`.

## Execution Notes

- Do not preserve backwards-compatible Hades adapter APIs. Remove callers and delete the old function in the same task.
- Use `assert` for invariants such as missing unit ids. Do not add boundary `if` checks that hide corrupt state.
- Keep source comments and user-facing Chinese text in Traditional Chinese.
- Prefer id-based helpers and `std::ranges` for lookups. Do not add one-line wrappers that only hide `requireById`.
- Avoid a broad render DTO. Add fields only when a render path, presentation path, or stats path actually consumes them.
