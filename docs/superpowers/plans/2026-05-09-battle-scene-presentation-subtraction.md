# Battle Scene Presentation Subtraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stop using legacy `Role` as the battle draw carrier by moving sprite, bar, hurt flash, and runtime visual attachment reads to a narrow scene-owned render unit store.

**Architecture:** Runtime remains authoritative and publishes explicit frame outputs. `BattleSceneHades` owns a small render state keyed by battle unit id; frame application updates that render state, and draw reads it directly. `Role` remains available for setup, position swap, tracker logging, and legacy targeting helpers that have not been migrated, but draw and runtime visual playback should not depend on `Role` fields.

**Tech Stack:** C++20, MSVC, Catch2 tests, existing `BattleFrameRunner`, `BattleFrameResult`, `BattlePresentationRecorder`, `BattleSceneHades`, `BattleScenePresentationPlayer`, and `BattleSceneBattleAdapter`.

---

## File Structure

- Modify `src/battle/BattlePresentation.h`
  - Add `BattlePresentationRecorder::beginFrame(int frame)` so the scene can stamp presentation events without copying all units into a snapshot.

- Modify `src/battle/BattlePresentation.cpp`
  - Implement the frame-only recorder overload.

- Modify `src/BattleSceneAct.h`
  - Add a render-unit follow id to `AttackEffect` so runtime visual effects can follow scene render state instead of `Role*`.

- Modify `src/BattleScenePresentationPlayer.h`
  - Add a tiny presentation unit view containing only `position`, `team`, and `maxHp`.
  - Add a resolver for that view while keeping `resolveRole` only for tracker logging.

- Modify `src/BattleScenePresentationPlayer.cpp`
  - Use the unit view for floating text placement, role effect attachment, projectile team lookup, and damage number sizing.
  - Keep `Role*` resolution only in tracker log methods.

- Modify `src/BattleSceneHades.h`
  - Add `BattleSceneRenderUnit`.
  - Add ordered render unit storage plus id lookup.
  - Change draw helpers to accept render units instead of `Role*`.
  - Remove `makePresentationSnapshot()`.

- Modify `src/BattleSceneHades.cpp`
  - Seed render units from setup roles once initialization has created clones and role bindings.
  - Upsert render units when delayed enemy roles enter `battle_roles_`.
  - Apply runtime frame outputs to render units before drawing.
  - Draw sprites, bars, shield/block overlays, hurt flash, and runtime follow effects from render units.
  - Keep narrowly scoped legacy `Role` writes only where current non-render systems still require them.

- Modify `src/BattleSceneBattleAdapter.h`
  - Remove or narrow scene-facing render apply helpers that only existed to write draw fields back into `Role`.

- Modify `src/BattleSceneBattleAdapter.cpp`
  - Delete combo render writes from damage role application.
  - Leave setup/projection code intact.

- Modify `src/battle/BattleCore.h`
  - Add narrow damage render application payload if it is not already present when this plan is executed.

- Modify `src/battle/BattleCore.cpp`
  - Populate narrow damage render applications from damage transaction results.

- Modify tests:
  - `tests/BattlePresentationUnitTests.cpp`
  - `tests/BattleCoreUnitTests.cpp`
  - `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

---

## Task 1: Remove Scene-Side Full Presentation Snapshot Copies

**Files:**
- Modify: `src/battle/BattlePresentation.h`
- Modify: `src/battle/BattlePresentation.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Test: `tests/BattlePresentationUnitTests.cpp`

- [ ] **Step 1: Add a recorder test for frame-only presentation begin**

In `tests/BattlePresentationUnitTests.cpp`, add:

```cpp
TEST_CASE("BattlePresentationRecorder_BeginFrameWithFrameOnlyStampsEvents", "[battle][presentation]")
{
    BattlePresentationRecorder recorder;

    recorder.beginFrame(17);
    recorder.recordLog({ BattleLogEventType::Status });
    recorder.recordVisual({ BattleVisualEventType::FloatingText });
    recorder.recordGameplay({ BattleGameplayEventType::StatusApplied });

    const auto& frame = recorder.frame();
    CHECK(frame.snapshot.frame == 17);
    CHECK(frame.snapshot.units.empty());
    REQUIRE(frame.logEvents.size() == 1);
    CHECK(frame.logEvents[0].frame == 17);
    REQUIRE(frame.visualEvents.size() == 1);
    CHECK(frame.visualEvents[0].frame == 17);
    REQUIRE(frame.gameplayEvents.size() == 1);
    CHECK(frame.gameplayEvents[0].frame == 17);
}
```

- [ ] **Step 2: Run the targeted test and verify it fails**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattlePresentationRecorder_BeginFrameWithFrameOnlyStampsEvents"
```

Expected before implementation: compile failure because `BattlePresentationRecorder::beginFrame(int)` does not exist.

- [ ] **Step 3: Add the frame-only recorder API**

In `src/battle/BattlePresentation.h`, add the overload:

```cpp
void beginFrame(int frame);
```

In `src/battle/BattlePresentation.cpp`, implement it next to the existing overload:

```cpp
void BattlePresentationRecorder::beginFrame(int frame)
{
    frame_ = {};
    frame_.snapshot.frame = frame;
}
```

- [ ] **Step 4: Delete scene full snapshot construction**

In `src/BattleSceneHades.h`, remove:

```cpp
KysChess::Battle::BattlePresentationSnapshot makePresentationSnapshot() const;
```

In `src/BattleSceneHades.cpp`, delete `BattleSceneHades::makePresentationSnapshot()` and update `beginPresentationFrame()`:

```cpp
void BattleSceneHades::beginPresentationFrame()
{
    presentation_recorder_.beginFrame(battle_frame_);
}
```

- [ ] **Step 5: Verify the scene no longer builds presentation snapshots**

Run:

```powershell
rg -n "BattleSceneHades::makePresentationSnapshot|makePresentationSnapshot\(\)" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: no matches in `BattleSceneHades.*`.

---

## Task 2: Add A Scene Render Unit Store

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

- [ ] **Step 1: Add render unit types to `BattleSceneHades.h`**

Add `<array>` to the includes if it is not already present:

```cpp
#include <array>
```

Add these protected structs before the private member fields:

```cpp
struct BattleSceneRenderComboState
{
    int shield = 0;
    int blockFirstHitsRemaining = 0;
};

struct BattleSceneRenderUnit
{
    int unitId = -1;
    int realRoleId = -1;
    std::string name;
    int headId = -1;
    int team = 0;
    bool alive = true;
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    Pointf realTowards;
    int actType = -1;
    int actFrame = 0;
    std::array<int, 5> fightFrames{};
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;
    int cooldown = 0;
    int cooldownMax = 0;
    int frozen = 0;
    int frozenMax = 0;
    int invincible = 0;
    int shake = 0;
    int attention = 0;
    int gridX = 0;
    int gridY = 0;
    BattleSceneRenderComboState combo;
};
```

Add render store members:

```cpp
std::vector<BattleSceneRenderUnit> render_units_;
std::unordered_map<int, std::size_t> render_unit_index_by_id_;
```

- [ ] **Step 2: Add render unit helper declarations**

In `BattleSceneHades.h`, replace:

```cpp
void renderExtraRoleInfo(Role* r, double x, double y);
Color calculateHurtFlashColor(const Role* r, const Color& base_color) const;
```

with:

```cpp
void renderExtraRoleInfo(const BattleSceneRenderUnit& unit, double x, double y);
BattleSceneRenderUnit& requireRenderUnit(int unitId);
const BattleSceneRenderUnit& requireRenderUnit(int unitId) const;
void rebuildRenderUnitsFromBattleRoles();
void upsertRenderUnitFromRole(const Role& role);
int calRenderUnitPic(const BattleSceneRenderUnit& unit, int style, int frame) const;
Color calculateHurtFlashColor(int unitId, const Color& baseColor) const;
```

- [ ] **Step 3: Implement render unit seeding**

In `src/BattleSceneHades.cpp`, add:

```cpp
BattleSceneHades::BattleSceneRenderUnit makeBattleSceneRenderUnitFromRole(const Role& role)
{
    BattleSceneHades::BattleSceneRenderUnit unit;
    unit.unitId = role.ID;
    unit.realRoleId = role.RealID;
    unit.name = role.Name;
    unit.headId = role.HeadID;
    unit.team = role.Team;
    unit.alive = role.Dead == 0;
    unit.position = role.Pos;
    unit.velocity = role.Velocity;
    unit.acceleration = role.Acceleration;
    unit.realTowards = role.RealTowards;
    unit.actType = role.ActType;
    unit.actFrame = role.ActFrame;
    for (int i = 0; i < 5; ++i)
    {
        unit.fightFrames[i] = role.FightFrame[i];
    }
    unit.hp = role.HP;
    unit.maxHp = role.MaxHP;
    unit.mp = role.MP;
    unit.maxMp = role.MaxMP;
    unit.cooldown = role.CoolDown;
    unit.cooldownMax = role.CoolDownMax;
    unit.frozen = role.Frozen;
    unit.frozenMax = role.FrozenMax;
    unit.invincible = role.Invincible;
    unit.shake = role.Shake;
    unit.attention = role.Attention;
    unit.gridX = role.X();
    unit.gridY = role.Y();
    return unit;
}
```

Implement the member helpers:

```cpp
BattleSceneHades::BattleSceneRenderUnit& BattleSceneHades::requireRenderUnit(int unitId)
{
    auto it = render_unit_index_by_id_.find(unitId);
    assert(it != render_unit_index_by_id_.end());
    return render_units_[it->second];
}

const BattleSceneHades::BattleSceneRenderUnit& BattleSceneHades::requireRenderUnit(int unitId) const
{
    auto it = render_unit_index_by_id_.find(unitId);
    assert(it != render_unit_index_by_id_.end());
    return render_units_[it->second];
}

void BattleSceneHades::upsertRenderUnitFromRole(const Role& role)
{
    auto unit = makeBattleSceneRenderUnitFromRole(role);
    auto it = render_unit_index_by_id_.find(unit.unitId);
    if (it == render_unit_index_by_id_.end())
    {
        render_unit_index_by_id_[unit.unitId] = render_units_.size();
        render_units_.push_back(std::move(unit));
        return;
    }
    render_units_[it->second] = std::move(unit);
}

void BattleSceneHades::rebuildRenderUnitsFromBattleRoles()
{
    render_units_.clear();
    render_unit_index_by_id_.clear();
    render_units_.reserve(battle_roles_.size());
    for (auto* role : battle_roles_)
    {
        assert(role);
        upsertRenderUnitFromRole(*role);
    }
}
```

- [ ] **Step 4: Seed render units after runtime initialization**

In `BattleSceneHades::initializeBattleRuntime()`, after `applyBattleInitializationResult(...)`, call:

```cpp
rebuildRenderUnitsFromBattleRoles();
```

- [ ] **Step 5: Upsert delayed enemy entries**

In `BattleSceneHades::applyLegacyBattleFrameResult(...)`, immediately after each `battle_roles_.push_back(enemies_.front())`, add:

```cpp
upsertRenderUnitFromRole(*battle_roles_.back());
```

This keeps render ordering aligned with `battle_roles_` while avoiding full per-frame role copies.

---

## Task 3: Move Sprite And Bar Draw Reads Off `Role`

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

- [ ] **Step 1: Add render-unit fight-frame helpers**

In `src/BattleSceneHades.cpp`, add anonymous namespace helpers:

```cpp
constexpr int kRenderFightStyleCount = 5;
constexpr int kRenderFightStyleFallbacks[kRenderFightStyleCount][kRenderFightStyleCount - 1] = {
    { 1, 2, 3, 4 },
    { 4, 2, 3, 0 },
    { 3, 1, 4, 0 },
    { 2, 1, 4, 0 },
    { 1, 2, 3, 0 },
};

int firstAvailableRenderFightStyle(const BattleSceneHades::BattleSceneRenderUnit& unit)
{
    for (int style = 0; style < kRenderFightStyleCount; ++style)
    {
        if (unit.fightFrames[style] > 0)
        {
            return style;
        }
    }
    return -1;
}

int resolveRenderFightStyle(const BattleSceneHades::BattleSceneRenderUnit& unit, int style)
{
    if (style >= 0 && style < kRenderFightStyleCount && unit.fightFrames[style] > 0)
    {
        return style;
    }
    if (style >= 0 && style < kRenderFightStyleCount)
    {
        for (int fallback : kRenderFightStyleFallbacks[style])
        {
            if (unit.fightFrames[fallback] > 0)
            {
                return fallback;
            }
        }
    }
    return firstAvailableRenderFightStyle(unit);
}
```

Implement:

```cpp
int BattleSceneHades::calRenderUnitPic(const BattleSceneRenderUnit& unit, int style, int frame) const
{
    const int faceTowards = realTowardsToFaceTowards(unit.realTowards);
    if (style < 0 || style >= kRenderFightStyleCount)
    {
        style = resolveRenderFightStyle(unit, -1);
        return style >= 0 ? unit.fightFrames[style] * faceTowards : faceTowards;
    }

    style = resolveRenderFightStyle(unit, style);
    if (style < 0)
    {
        return faceTowards;
    }

    int total = 0;
    for (int i = 0; i < kRenderFightStyleCount; ++i)
    {
        if (i == style)
        {
            const int frameCount = unit.fightFrames[style];
            const int clampedFrame = frame < frameCount - 2 ? frame : frameCount - 2;
            return total + frameCount * faceTowards + clampedFrame;
        }
        total += unit.fightFrames[i] * 4;
    }
    return faceTowards;
}
```

- [ ] **Step 2: Move hurt flash lookup to unit id**

Replace `calculateHurtFlashColor(const Role* r, ...)` with:

```cpp
Color BattleSceneHades::calculateHurtFlashColor(int unitId, const Color& baseColor) const
{
    auto it = hurt_flash_timers_.find(unitId);
    if (it == hurt_flash_timers_.end())
    {
        return baseColor;
    }

    int timer = it->second;
    int phase = timer % HURT_FLASH_PERIOD;
    if (phase < 2)
    {
        return { 255, 100, 100, baseColor.a };
    }
    return baseColor;
}
```

- [ ] **Step 3: Draw role sprites from render units**

In `BattleSceneHades::draw()`, replace the `for (auto r : battle_roles_)` sprite loop with:

```cpp
for (const auto& unit : render_units_)
{
    if (!is_visible_world(unit.position.x, unit.position.y / 2.0))
    {
        continue;
    }

    DrawInfo info;
    auto path = std::format("fight/fight{:03}", unit.headId);
    info.color = { 255, 255, 255, 255 };
    info.alpha = 255;
    info.white = 0;
    if (battle_cursor_->isRunning() && !acting_role_->isAuto())
    {
        info.color = { 128, 128, 128, 255 };
        auto* legacyRole = requireFrameRole(core_role_bindings_, unit.unitId);
        if (inEffect(acting_role_, legacyRole))
        {
            info.color = { 255, 255, 255, 255 };
        }
    }

    info.p = unit.position;
    if (result_ == -1 && unit.shake)
    {
        info.p.x += shakeJitter(battle_frame_, unit.unitId);
    }

    info.tex = TextureManager::getInstance()->getTexture(path, calRenderUnitPic(unit, unit.actType, unit.actFrame));
    if (!info.tex)
    {
        continue;
    }

    if (!unit.alive)
    {
        const int faceTowards = realTowardsToFaceTowards(unit.realTowards);
        info.rot = faceTowards >= 2 ? 90 : 270;
    }
    if (unit.attention)
    {
        info.alpha = 255 - unit.attention * 4;
    }
    info.color = calculateHurtFlashColor(unit.unitId, info.color);
    info.shadow = 1;
    draw_infos.emplace_back(std::move(info));
}
```

The `legacyRole` lookup is only for the existing targeting highlight calculation. It must not supply sprite position, animation, bars, or damage state.

- [ ] **Step 4: Render bars from render units**

Change the helper signature:

```cpp
void BattleSceneHades::renderExtraRoleInfo(const BattleSceneRenderUnit& unit, double x, double y)
```

Replace field reads:

```cpp
r->Dead
r->Team
r->HP
r->MaxHP
r->MP
r->MaxMP
r->Invincible
r->Frozen
r->FrozenMax
r->CoolDown
r->CoolDownMax
```

with:

```cpp
!unit.alive
unit.team
unit.hp
unit.maxHp
unit.mp
unit.maxMp
unit.invincible
unit.frozen
unit.frozenMax
unit.cooldown
unit.cooldownMax
```

Replace combo lookup with:

```cpp
const auto& comboState = unit.combo;
```

Use `comboState.shield` and `comboState.blockFirstHitsRemaining` directly.

- [ ] **Step 5: Draw status bars from render units**

Replace the second `for (auto r : battle_roles_)` loop with:

```cpp
for (const auto& unit : render_units_)
{
    if (is_visible_world(unit.position.x, unit.position.y / 2.0))
    {
        renderExtraRoleInfo(unit, renderWorldX(unit.position.x), renderWorldY(unit.position.y / 2.0));
    }
}
```

- [ ] **Step 6: Verify render no longer reads combo globals**

Run:

```powershell
rg -n "ChessCombo::getActiveStates\(\)|comboStates\[.*\]\.shield|comboStates\[.*\]\.blockFirstHitsRemaining" src\BattleSceneHades.cpp
```

Expected:
- No `ChessCombo::getActiveStates()` in `renderExtraRoleInfo`.
- No scene-side shield/block writes for render.

---

## Task 4: Apply Runtime Frame Output To Render Units

**Files:**
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`

- [ ] **Step 1: Apply unit applications to render units**

In `BattleSceneHades::applyCoreFrameResult(...)`, before the legacy role apply call, update the render unit:

```cpp
for (const auto& application : frameResult.unitApplications)
{
    auto& unit = requireRenderUnit(application.unitId);
    unit.cooldown = application.cooldown;
    unit.actFrame = application.actFrame;
    unit.actType = application.actType;
    unit.mp = application.finalMp;
    if (application.resetDashVelocity)
    {
        unit.velocity = { 0, 0, 0 };
    }

    auto* role = requireFrameRole(bindings, application.unitId);
    applyBattleFrameUnitApplication(role, application);
}
```

- [ ] **Step 2: Apply status and combo output to render units**

Replace `BattleSceneHades::applyCoreStatusState(...)` body with:

```cpp
for (const auto& mirror : applications.comboMirrors)
{
    auto& unit = requireRenderUnit(mirror.unitId);
    unit.combo.shield = mirror.shield;
    unit.combo.blockFirstHitsRemaining = mirror.blockFirstHitsRemaining;
}
for (const auto& status : applications.statusRenderUnits)
{
    auto& unit = requireRenderUnit(status.unitId);
    unit.invincible = status.invincible;
    unit.frozen = status.frozenFrames;
    unit.frozenMax = status.frozenMaxFrames;

    auto* role = requireFrameRole(bindings, status.unitId);
    role->Invincible = status.invincible;
    role->Frozen = status.frozenFrames;
    role->FrozenMax = status.frozenMaxFrames;
}
```

The legacy role writes remain only because current non-render code still reads these fields.

- [ ] **Step 3: Rename and narrow damage role helper**

In `src/BattleSceneBattleAdapter.h`, replace:

```cpp
void writeBattleDamageRenderUnit(Role* role, RoleComboState* state, const Battle::BattleDamageUnitState& unit);
```

with:

```cpp
void applyBattleDamageLegacyRoleState(Role* role, const Battle::BattleDamageUnitState& unit);
```

In `src/BattleSceneBattleAdapter.cpp`, replace the implementation with:

```cpp
void applyBattleDamageLegacyRoleState(Role* role, const Battle::BattleDamageUnitState& unit)
{
    assert(role);

    role->HP = unit.hp;
    role->MP = unit.mp;
    role->Invincible = unit.invincible;
    role->Dead = unit.alive ? 0 : 1;
}
```

- [ ] **Step 4: Apply damage transactions to render units first**

In `BattleSceneHades::applyCoreDamageTransactions(...)`, replace the damage role mirror section with:

```cpp
auto& defenderUnit = requireRenderUnit(damageTaken.defender.id);
defenderUnit.hp = damageTaken.defender.hp;
defenderUnit.mp = damageTaken.defender.mp;
defenderUnit.invincible = damageTaken.defender.invincible;
defenderUnit.alive = damageTaken.defender.alive;
defenderUnit.frozen = damageTaken.defenderStatus.frozenTimer;
defenderUnit.frozenMax = damageTaken.defenderStatus.frozenMaxTimer;
defenderUnit.cooldown = damageTaken.defenderCooldown.cooldown;

if (damageTaken.attacker.id >= 0)
{
    auto& attackerUnit = requireRenderUnit(damageTaken.attacker.id);
    attackerUnit.hp = damageTaken.attacker.hp;
    attackerUnit.mp = damageTaken.attacker.mp;
    attackerUnit.invincible = damageTaken.attacker.invincible;
    attackerUnit.alive = damageTaken.attacker.alive;
}

auto* r = requireFrameRole(bindings, damageTaken.defender.id);
Role* attacker = damageTaken.attacker.id >= 0
    ? requireFrameRole(bindings, damageTaken.attacker.id)
    : nullptr;
r->LastAttacker = attacker;
if (attacker)
{
    applyBattleDamageLegacyRoleState(attacker, damageTaken.attacker);
}
applyBattleDamageLegacyRoleState(r, damageTaken.defender);
r->Frozen = defenderUnit.frozen;
r->FrozenMax = defenderUnit.frozenMax;
r->CoolDown = defenderUnit.cooldown;
```

- [ ] **Step 5: Apply team effects to render units**

In `BattleSceneHades::applyCoreTeamEffectState(...)`, remove:

```cpp
auto& comboStates = KysChess::ChessCombo::getMutableStates();
```

Update each case:

```cpp
auto& unit = requireRenderUnit(event.targetUnitId);
auto* target = requireFrameRole(bindings, event.targetUnitId);
switch (event.type)
{
case KysChess::Battle::BattleTeamEffectEventType::Heal:
    unit.hp = event.after;
    target->HP = event.after;
    break;
case KysChess::Battle::BattleTeamEffectEventType::MpRestore:
    unit.mp = event.after;
    target->MP = event.after;
    break;
case KysChess::Battle::BattleTeamEffectEventType::ShieldGain:
    unit.combo.shield = event.after;
    break;
case KysChess::Battle::BattleTeamEffectEventType::CooldownReduced:
    unit.cooldown = event.after;
    target->CoolDown = event.after;
    break;
}
```

- [ ] **Step 6: Apply frame application deltas to render units**

In `BattleSceneHades::applyCoreFrameApplications(...)`, update render units before legacy role fields:

```cpp
auto& targetUnit = requireRenderUnit(knockback.targetUnitId);
targetUnit.velocity += knockback.velocityDelta;
if (knockback.velocityCap > 0.0 && targetUnit.velocity.norm() > knockback.velocityCap)
{
    targetUnit.velocity.normTo(static_cast<float>(knockback.velocityCap));
}
```

For MP restore:

```cpp
auto& unit = requireRenderUnit(restore.unitId);
unit.mp = std::min(unit.maxMp, unit.mp + restore.amount);
```

For heal:

```cpp
auto& unit = requireRenderUnit(heal.targetUnitId);
unit.hp = std::min(unit.maxHp, unit.hp + heal.amount);
```

Keep existing tracker and sound behavior.

- [ ] **Step 7: Apply movement presentation to render units**

Replace the scene call to `applyBattleMovementPresentationResults(...)` with an inline loop in `BattleSceneHades::applyCoreFrameResult(...)`:

```cpp
for (const auto& movement : frameResult.movementPresentationResults)
{
    auto& unit = requireRenderUnit(movement.unitId);
    unit.frozen = movement.frozenFrames;
    unit.position = movement.position;
    unit.velocity = movement.velocity;
    unit.acceleration = movement.acceleration;
    if (movement.facing.norm() > 0.01)
    {
        unit.realTowards = movement.facing;
    }

    auto* role = requireFrameRole(bindings, movement.unitId);
    role->Frozen = movement.frozenFrames;
    role->Pos = movement.position;
    role->Velocity = movement.velocity;
    role->Acceleration = movement.acceleration;
    if (movement.facing.norm() > 0.01)
    {
        role->RealTowards = movement.facing;
    }
}
```

Then delete `applyBattleMovementPresentationResults(...)` from the adapter header and implementation if no callers remain.

- [ ] **Step 8: Apply action animation output to render units**

Before calling `applyBattleActionFrameResults(...)`, update render units:

```cpp
for (const auto& action : frameResult.actionResults)
{
    auto& unit = requireRenderUnit(action.unitId);
    unit.actFrame = action.state.actFrame;
    unit.actType = action.state.actType;
    if (action.castStarted)
    {
        unit.actFrame = action.castResult.state.actFrame;
        unit.actType = action.castResult.state.actType;
    }
    for (const auto& teleport : action.actionResult.blinkTeleports)
    {
        unit.gridX = teleport.gridX;
        unit.gridY = teleport.gridY;
        unit.position = { teleport.position.x, teleport.position.y, 0 };
        unit.velocity = { 0, 0, 0 };
        unit.acceleration = { 0, 0, gravity_ };
        unit.realTowards = teleport.facing;
    }
}
```

Keep `applyBattleActionFrameResults(...)` for legacy role compatibility and sound/face-towards side effects in this task.

---

## Task 5: Move Runtime Visual Playback Off `Role`

**Files:**
- Modify: `src/BattleSceneAct.h`
- Modify: `src/BattleScenePresentationPlayer.h`
- Modify: `src/BattleScenePresentationPlayer.cpp`
- Modify: `src/BattleSceneHades.cpp`

- [ ] **Step 1: Add a render follow id to attack effects**

In `BattleSceneAct::AttackEffect`, add:

```cpp
int FollowUnitId = -1;
```

Do not remove `FollowRole`; legacy effects may still use it.

- [ ] **Step 2: Add a presentation unit view**

In `BattleScenePresentationPlayer.h`, add inside `BattleScenePresentationPlayer`:

```cpp
struct UnitView
{
    Pointf position;
    int team = -1;
    int maxHp = 0;
};
```

Extend `Bindings`:

```cpp
std::function<std::optional<UnitView>(int)> resolveUnitView;
```

Add `<optional>` to the header includes.

- [ ] **Step 3: Wire the render unit resolver in Hades**

In `BattleSceneHades::publishPresentationFrame()`, pass:

```cpp
[this](int unitId) -> std::optional<BattleScenePresentationPlayer::UnitView>
{
    auto it = render_unit_index_by_id_.find(unitId);
    if (it == render_unit_index_by_id_.end())
    {
        return std::nullopt;
    }
    const auto& unit = render_units_[it->second];
    return BattleScenePresentationPlayer::UnitView{
        unit.position,
        unit.team,
        unit.maxHp,
    };
},
```

Keep the existing `resolveRole` binding for tracker logs.

- [ ] **Step 4: Use unit views for visual positions and teams**

In `BattleScenePresentationPlayer.cpp`, add:

```cpp
std::optional<BattleScenePresentationPlayer::UnitView> resolveUnitView(
    const BattleScenePresentationPlayer::Bindings& bindings,
    int unitId)
{
    assert(bindings.resolveUnitView);
    if (unitId < 0)
    {
        return std::nullopt;
    }
    return bindings.resolveUnitView(unitId);
}

Pointf floatingTextPositionFor(const BattleScenePresentationPlayer::UnitView& unit, const std::string& text)
{
    Pointf position = unit.position;
    position.x -= 7.5 * Font::getTextDrawSize(text);
    position.y -= 50;
    return position;
}
```

Change `resolveVisualTeam(...)` to use `resolveUnitView(...)`:

```cpp
int resolveVisualTeam(const BattleScenePresentationPlayer::Bindings& bindings, int unitId)
{
    auto unit = resolveUnitView(bindings, unitId);
    return unit ? unit->team : -1;
}
```

- [ ] **Step 5: Spawn text and role effects from render views**

In `spawnFloatingText(...)`, replace `effect.set(..., role)` with:

```cpp
effect.Text = command.text;
effect.color = toSceneColor(command.color);
if (auto unit = resolveUnitView(bindings, command.targetUnitId))
{
    effect.Pos = floatingTextPositionFor(*unit, command.text);
}
else
{
    effect.Pos = command.position;
}
effect.Size = command.textSize;
effect.Type = command.textMotionType;
```

In `spawnRoleEffect(...)`, replace `FollowRole` and `role->Team` usage with:

```cpp
auto unit = resolveUnitView(bindings, command.targetUnitId);
assert(unit);

BattleSceneAct::AttackEffect effect;
effect.FollowUnitId = command.targetUnitId;
effect.Pos = { 0.0f, 0.0f, ROLE_STATUS_EFT_Z_OFFSET };
effect.setEft(command.effectId);
effect.TotalFrame = command.durationFrames > 0
    ? std::max(command.durationFrames, effect.TotalEffectFrame)
    : std::max(1, effect.TotalEffectFrame);
effect.Frame = 0;
effect.VisualOnly = 1;
effect.VisualTeam = unit->team;
bindings.attackEffects->push_back(std::move(effect));
```

In `spawnDamageNumber(...)`, replace role lookup with:

```cpp
auto unit = resolveUnitView(bindings, command.targetUnitId);
assert(unit);
assert(command.amount > 0);

BattleSceneAct::TextEffect effect;
effect.Text = std::to_string(-command.amount);
effect.color = toSceneColor(command.color);
effect.Pos = floatingTextPositionFor(*unit, effect.Text);
effect.Size = damageTextSize(command.textSize, command.amount, unit->maxHp);
effect.color.a = damageTextAlpha(command.amount, unit->maxHp);
bindings.textEffects->push_back(std::move(effect));
```

- [ ] **Step 6: Draw follow effects from render units**

In `BattleSceneHades::draw()`, replace:

```cpp
auto effect_pos = ae.FollowRole ? ae.FollowRole->Pos + ae.Pos : ae.Pos;
```

with:

```cpp
Pointf effect_pos = ae.Pos;
if (ae.FollowUnitId >= 0)
{
    effect_pos = requireRenderUnit(ae.FollowUnitId).position + ae.Pos;
}
else if (ae.FollowRole)
{
    effect_pos = ae.FollowRole->Pos + ae.Pos;
}
```

- [ ] **Step 7: Spawn scene blood effects against render units**

In `BattleSceneHades::applyCoreDamageTransactions(...)`, replace:

```cpp
ae1.FollowRole = r;
```

with:

```cpp
ae1.FollowUnitId = damageTaken.defender.id;
```

---

## Task 6: Narrow Scene Damage Application Away From Full Runtime Transactions

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add a scene-facing damage render payload**

In `src/battle/BattleCore.h`, add:

```cpp
struct BattleFrameDamageRenderUnit
{
    int unitId{};
    int hp{};
    int mp{};
    int invincible{};
    bool alive{};
};

struct BattleFrameDamageRenderApplication
{
    BattleFrameDamageRenderUnit defender;
    BattleFrameDamageRenderUnit attacker;
    int frozenFrames{};
    int frozenMaxFrames{};
    int cooldown{};
    int committedHpDamage{};
    bool killed{};
    bool critical{};
};
```

Then add this vector to `BattleFrameResult`:

```cpp
std::vector<BattleFrameDamageRenderApplication> damageRenderApplications;
```

Keep `damageTransactions` until tests and non-render logic no longer consume transaction internals.

- [ ] **Step 2: Populate the payload in runtime**

In `src/battle/BattleCore.cpp`, add:

```cpp
BattleFrameDamageRenderUnit makeBattleFrameDamageRenderUnit(const BattleDamageUnitState& unit)
{
    return {
        unit.id,
        unit.hp,
        unit.mp,
        unit.invincible,
        unit.alive,
    };
}

BattleFrameDamageRenderApplication makeBattleFrameDamageRenderApplication(
    const BattleDamageTransactionResult& transaction,
    bool critical)
{
    return {
        makeBattleFrameDamageRenderUnit(transaction.defender),
        makeBattleFrameDamageRenderUnit(transaction.attacker),
        transaction.defenderStatus.frozenTimer,
        transaction.defenderStatus.frozenMaxTimer,
        transaction.defenderCooldown.cooldown,
        transaction.finalHpDamage,
        transaction.killed,
        critical,
    };
}
```

When damage transactions are appended to `result.damageTransactions`, also append `result.damageRenderApplications`. Build critical defender ids from `result.hitResults`:

```cpp
std::set<int> criticalDefenderIds;
for (const auto& hit : result.hitResults)
{
    if (hit.critical)
    {
        criticalDefenderIds.insert(hit.defenderUnitId);
    }
}
```

Then append:

```cpp
result.damageRenderApplications.push_back(
    makeBattleFrameDamageRenderApplication(
        transaction,
        criticalDefenderIds.contains(transaction.defender.id)));
```

- [ ] **Step 3: Add a runtime test for the narrow payload**

In an existing damage frame test in `tests/BattleCoreUnitTests.cpp`, add:

```cpp
REQUIRE(result.damageRenderApplications.size() == 1);
const auto& render = result.damageRenderApplications[0];
const auto& transaction = result.damageTransactions[0];
CHECK(render.defender.unitId == transaction.defender.id);
CHECK(render.defender.hp == transaction.defender.hp);
CHECK(render.defender.mp == transaction.defender.mp);
CHECK(render.defender.alive == transaction.defender.alive);
CHECK(render.frozenFrames == transaction.defenderStatus.frozenTimer);
CHECK(render.frozenMaxFrames == transaction.defenderStatus.frozenMaxTimer);
CHECK(render.cooldown == transaction.defenderCooldown.cooldown);
CHECK(render.committedHpDamage == transaction.finalHpDamage);
```

- [ ] **Step 4: Consume the narrow payload for render writes**

In `BattleSceneHades::applyCoreDamageTransactions(...)`, use `frameResult.damageRenderApplications` to update render units, hurt flash, blood effects, critical hit ids, and death camera. Keep `damageTransactions` only for any remaining legacy combo transfer input that cannot be removed in the same pass.

The render update block should be:

```cpp
for (const auto& damage : frameResult.damageRenderApplications)
{
    auto& defenderUnit = requireRenderUnit(damage.defender.unitId);
    defenderUnit.hp = damage.defender.hp;
    defenderUnit.mp = damage.defender.mp;
    defenderUnit.invincible = damage.defender.invincible;
    defenderUnit.alive = damage.defender.alive;
    defenderUnit.frozen = damage.frozenFrames;
    defenderUnit.frozenMax = damage.frozenMaxFrames;
    defenderUnit.cooldown = damage.cooldown;

    if (damage.attacker.unitId >= 0)
    {
        auto& attackerUnit = requireRenderUnit(damage.attacker.unitId);
        attackerUnit.hp = damage.attacker.hp;
        attackerUnit.mp = damage.attacker.mp;
        attackerUnit.invincible = damage.attacker.invincible;
        attackerUnit.alive = damage.attacker.alive;
    }
}
```

- [ ] **Step 5: Remove critical scan over full hit results from scene**

Delete the scene scan:

```cpp
for (const auto& hitResolution : frameResult.hitResults)
{
    if (hitResolution.critical)
    {
        criticalHitRoles_.insert(requireFrameRole(bindings, hitResolution.defenderUnitId));
    }
}
```

If `criticalHitRoles_` has no remaining reads, delete the member and all writes.

---

## Task 7: Remove Draw-Only Legacy Role Writes That Became Redundant

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`

- [ ] **Step 1: Search for draw-path `Role` reads**

Run:

```powershell
rg -n "r->|role->|FollowRole|renderExtraRoleInfo\\(|calRolePic\\(" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleScenePresentationPlayer.cpp src\BattleSceneAct.h
```

Classify remaining references as one of:

```text
setup
position-swap
tracker-log
legacy-targeting-highlight
legacy-effect-fallback
delete
```

- [ ] **Step 2: Delete dead hit role sets**

If the search confirms `ultHitRoles_` and `criticalHitRoles_` are only written and never read, remove these declarations:

```cpp
std::set<Role*> ultHitRoles_;
std::set<Role*> criticalHitRoles_;
```

Remove their `clear()` calls and insertions.

- [ ] **Step 3: Delete unused adapter render apply helpers**

Run:

```powershell
rg -n "applyBattleMovementPresentationResults|writeBattleDamageRenderUnit|applyBattleDamageLegacyRoleState" src
```

Delete helper declarations and definitions with no callers. Keep projection/setup helpers that still build runtime initialization input from `Role` and `Magic`.

- [ ] **Step 4: Keep an explicit compatibility list**

Add `docs/battle_scene_render_read_map.md`:

```markdown
# Battle Scene Render Read Map

## Draw Source

Draw reads `BattleSceneHades::BattleSceneRenderUnit`, not `Role`.

## Render Unit Fields

- Identity/assets: `unitId`, `realRoleId`, `name`, `headId`, `fightFrames`
- Sprite pose: `position`, `velocity`, `acceleration`, `realTowards`, `actType`, `actFrame`
- State: `alive`, `team`, `shake`, `attention`
- Bars/text: `hp`, `maxHp`, `mp`, `maxMp`, `frozen`, `frozenMax`, `cooldown`, `cooldownMax`, `invincible`
- Combo render: `combo.shield`, `combo.blockFirstHitsRemaining`

## Remaining `Role` Uses

- Setup and initialization seed `Role` and `Magic` facts into runtime.
- Position swap still mutates pre-battle setup roles.
- Tracker logging still accepts `Role*`.
- Targeting highlight still calls legacy `inEffect(acting_role_, targetRole)`.
- Legacy non-runtime effects may still use `AttackEffect::FollowRole`; runtime visual effects use `FollowUnitId`.
```

---

## Task 8: Full Verification

**Files:**
- No code file changes expected in this task.

- [ ] **Step 1: Run whitespace/diff validation**

Run:

```powershell
git diff --check
```

Expected: no output, exit code 0.

- [ ] **Step 2: Run boundary searches**

Run:

```powershell
rg -n "BattleSceneHades::makePresentationSnapshot|ChessCombo::getActiveStates\\(\\)|writeBattleDamageRenderUnit|frameResult\\.damageTransactions|defenderStatus|defenderCooldown" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
```

Expected:
- No scene `makePresentationSnapshot`.
- No render path reading `ChessCombo::getActiveStates()`.
- No old `writeBattleDamageRenderUnit`.
- No scene frame-time render writes from full damage transaction internals.

Run:

```powershell
rg -n "for \\(auto r : battle_roles_\\)|renderExtraRoleInfo\\(Role\\*|calculateHurtFlashColor\\(const Role\\*|ae\\.FollowRole \\? ae\\.FollowRole->Pos" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected:
- No sprite or bar draw loop over `battle_roles_`.
- No draw helper taking `Role*`.
- No runtime visual effect positioning through `FollowRole`.

- [ ] **Step 3: Build test target**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: MSBuild exits with code 0. Existing conversion/NOMINMAX warnings are acceptable unless new errors appear.

- [ ] **Step 4: Run tests**

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

Expected: MSBuild exits with code 0. Existing warning noise is acceptable; linking can be considered successful if the only failure is an already-running game binary lock.

---

## Execution Notes

- This plan intentionally makes `BattleSceneRenderUnit` the draw carrier.
- Do not make draw read `BattleRuntimeState` or hold runtime references.
- Do not mirror broad runtime structs back into `Role`.
- Keep `Role` only for setup, position swap, tracker logging, legacy targeting highlight, and legacy effect fallback.
- Prefer deleting adapter apply helpers once render state owns the field updates.
- Use `assert` for required unit ids; do not add fallback defaults that hide missing render state.
