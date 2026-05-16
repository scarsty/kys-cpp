# Runtime State Ownership Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove frame-time runtime state mirrors from `RoleComboState` so damage/status/shield state has one runtime owner and damage defense is applied exactly once.

**Architecture:** `RoleComboState` remains the source for static combo rules and combo-owned counters only. Live battle state is owned by `BattleRuntimeUnit`, `BattleStatusRuntimeUnit`, `BattleDamageRuntimeUnit`, and `BattleDeathEffectStore`; scene rendering consumes explicit frame outputs built from those stores. Damage hit resolution should emit damage intents and presentation facts, while `BattleDamageApplicationSystem` owns live defense application.

**Tech Stack:** C++20, Catch2, MSBuild via `.github\build-command.ps1`, existing battle runtime systems under `src/battle`.

---

## File Map

- `src/battle/BattleCore.cpp`
  - Stop syncing shield/status/damage runtime state back into `state.combo.units`.
  - Publish render shield/block state from `state.units` and `state.damage.unitExtras`.
  - Keep combo-owned counters only in combo.
- `src/battle/BattleCore.h`
  - Keep `BattleFrameComboRenderApplication` if the name remains acceptable, but make its data come from runtime stores.
  - Remove declarations for deleted combo mirror helpers when no longer used.
- `src/battle/BattleDamageApplicationSystem.cpp`
  - Remove live damage/status-to-combo writes.
  - Require runtime status/damage stores as the authoritative state for transaction input.
- `src/battle/BattleDamageSystem.cpp`
  - Stop reading mutable damage/status facts from `RoleComboState` once runtime stores are authoritative.
- `src/battle/BattleHitResolver.cpp`
  - Remove early shield/block-first-hit defense application and combo mutation.
  - Keep dodge/reflect/crit/raw damage shaping and output labels.
- `src/battle/BattleStatusSystem.cpp`
  - Keep setup conversion from legacy combo to runtime status only if still needed at initialization.
  - Remove runtime status fields from combo conversion once callers migrate.
- `src/battle/BattleRuntimeSession.cpp`
  - During setup, seed runtime stores from initial combo rules, then treat runtime stores as authoritative.
- `src/BattleSceneHades.cpp`
  - Stop writing runtime death-effect tracker state into global combo during frame apply.
- Tests:
  - `tests/BattleDamageApplicationSystemUnitTests.cpp`
  - `tests/BattleCoreUnitTests.cpp`
  - `tests/BattleHitResolverUnitTests.cpp`
  - `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

---

### Task 1: Publish Shield And Block Render State From Runtime Stores

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Write failing render output test**

Add a test near existing frame apply output tests:

```cpp
TEST_CASE("BattleFrameRunner_PublishesRenderComboFromRuntimeStores", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 210, 100, 0 }),
    };
    state.units.requireUnit(1).shield = 33;
    state.combo.units[1].shield = 999;

    BattleDamageRuntimeUnit damage;
    damage.id = 1;
    damage.blockFirstHitsRemaining = 2;
    state.damage.unitExtras = { { 0 }, damage };
    state.combo.units[1].blockFirstHitsRemaining = 9;

    auto result = runBattleFrame(state);

    auto it = std::find_if(
        result.stateApplications.comboRenderUnits.begin(),
        result.stateApplications.comboRenderUnits.end(),
        [](const BattleFrameComboRenderApplication& unit)
        {
            return unit.unitId == 1;
        });
    REQUIRE(it != result.stateApplications.comboRenderUnits.end());
    CHECK(it->shield == 33);
    CHECK(it->blockFirstHitsRemaining == 2);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleFrameRunner_PublishesRenderComboFromRuntimeStores"
```

Expected: FAIL because render output reads `state.combo.units`.

- [ ] **Step 3: Change render payload builder**

Replace `makeBattleFrameComboRenderApplication(int unitId, const RoleComboState& state)` with:

```cpp
BattleFrameComboRenderApplication makeBattleFrameComboRenderApplication(
    const BattleRuntimeUnit& unit,
    const BattleDamageRuntimeUnit* damage)
{
    BattleFrameComboRenderApplication application;
    application.unitId = unit.id;
    application.shield = unit.shield;
    application.blockFirstHitsRemaining = damage ? damage->blockFirstHitsRemaining : 0;
    return application;
}
```

Update `publishFrameApplyOutputs()` combo loop to iterate `state.units.units` and use `tryFindById(state.damage.unitExtras, unit.id)`:

```cpp
for (const auto& unit : state.units.units)
{
    result.stateApplications.comboRenderUnits.push_back(
        makeBattleFrameComboRenderApplication(
            unit,
            tryFindById(state.damage.unitExtras, unit.id)));
}
```

- [ ] **Step 4: Run focused test**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleFrameRunner_PublishesRenderComboFromRuntimeStores"
```

Expected: PASS.

---

### Task 2: Remove Shield Writes To Combo

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Write failing shield ownership tests**

Add:

```cpp
TEST_CASE("BattleFrameRunner_TeamShieldDoesNotMirrorIntoCombo", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 0, { 150, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.units.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 0, 100, { 150, 100, 0 }),
    };
    state.combo.units[0] = {};
    state.combo.units[1] = {};
    state.teamEffects.pendingCommands.push_back(BattleTeamShieldCommand{ 0, 12, false, "測試護盾" });

    runBattleFrame(state);

    CHECK(state.units.requireUnit(0).shield == 12);
    CHECK(state.units.requireUnit(1).shield == 12);
    CHECK(state.combo.units.at(0).shield == 0);
    CHECK(state.combo.units.at(1).shield == 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleFrameRunner_TeamShieldDoesNotMirrorIntoCombo"
```

Expected: FAIL because `applyTeamEffectEventsToFrameState()` mirrors shield into combo.

- [ ] **Step 3: Remove combo shield writes**

In `applyTeamEffectEventsToFrameState()`, delete:

```cpp
const auto& unit = state.units.requireUnit(event.targetUnitId);
if (auto comboIt = state.combo.units.find(event.targetUnitId);
    comboIt != state.combo.units.end())
{
    comboIt->second.shield = unit.shield;
}
```

Either delete the helper if it becomes empty, or leave only non-combo behavior if still needed.

In `applyFrameUnitShieldCommand()`, replace the body with runtime-only mutation:

```cpp
auto& runtimeUnit = state.units.requireUnit(command.targetUnitId);
runtimeUnit.shield += command.amount;
appendStatusEventLog(
    logEvents,
    command.sourceUnitId,
    command.targetUnitId,
    formatStatusValue(command.reason, command.amount, "護盾"));
return true;
```

- [ ] **Step 4: Run focused tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleFrameRunner_TeamShieldDoesNotMirrorIntoCombo"
x64\Debug\kys_tests.exe "BattleFrameRunner_PublishesRenderComboFromRuntimeStores"
```

Expected: PASS.

---

### Task 3: Remove Status-To-Combo Runtime Mirror

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleDamageApplicationSystem.cpp`
- Test: `tests/BattleDamageApplicationSystemUnitTests.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Write failing damage-reduce status ownership test**

Add to `tests/BattleDamageApplicationSystemUnitTests.cpp`:

```cpp
TEST_CASE("BattleDamageApplication_UsesStatusDamageReduceDebuffsWithoutComboMirror", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;

    auto first = damageInput(0, 1, 10);
    queuePendingDamage(frame, first);

    auto& attackerStatus = requireById(frame.statusUnits, 0);
    attackerStatus.effects.damageReduceDebuffs.push_back({ 3, 50 });
    frame.comboUnits.at(0).dmgReduceDebuffs.clear();

    auto second = damageInput(0, 1, 10);
    second.request.preResolvedDamage = false;
    queuePendingDamage(frame, second);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 2);
    CHECK(result.transactions[1].finalHpDamage == 5);
    CHECK(frame.comboUnits.at(0).dmgReduceDebuffs.empty());
}
```

- [ ] **Step 2: Run test**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleDamageApplication_UsesStatusDamageReduceDebuffsWithoutComboMirror"
```

Expected: PASS or FAIL depending current ordering. If PASS, it still documents the desired owner.

- [ ] **Step 3: Remove status mirror writes from damage application**

In `applyResolvedTransactionToLiveState()` remove:

```cpp
writeBattleStatusComboState(defenderCombo, transaction.defenderStatus);
```

Leave damage combo writes temporarily if tests still depend on them; they are removed in Task 4.

- [ ] **Step 4: Delete `writeBattleStatusComboState()` frame usage**

Search:

```powershell
rg -n "writeBattleStatusComboState" src\battle tests
```

For production code, remove all frame-time calls. Keep setup/test helper usage only until Task 6 if needed.

- [ ] **Step 5: Remove live status fields from `writeBattleStatusComboState()` or delete helper**

If no production caller remains, delete the declaration in `src/battle/BattleCore.h` and the function in `src/battle/BattleCore.cpp`.

If tests still need setup conversion, replace test helper calls with direct status seeding:

```cpp
writeBattleStatusRuntimeUnit(
    ensureById(state.status.units, transaction.defenderStatus.id),
    transaction.defenderStatus);
```

- [ ] **Step 6: Run status/damage tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][damage_application][unit]"
x64\Debug\kys_tests.exe "[battle][status]"
```

Expected: PASS.

---

### Task 4: Remove Damage-To-Combo Runtime Mirror

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleDamageApplicationSystem.cpp`
- Modify: `src/battle/BattleDamageSystem.cpp`
- Test: `tests/BattleDamageApplicationSystemUnitTests.cpp`

- [ ] **Step 1: Write failing block-first-hit ownership test**

Add:

```cpp
TEST_CASE("BattleDamageApplication_UsesDamageRuntimeBlockFirstHitWithoutComboMirror", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;

    auto damage = damageInput(0, 1, 9);
    queuePendingDamage(frame, damage);
    auto& defenderExtras = requireById(frame.damageUnitExtras, 1);
    defenderExtras.blockFirstHitsRemaining = 1;
    frame.comboUnits.at(1).blockFirstHitsRemaining = 0;

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 1);
    CHECK(result.transactions[0].blockedByFirstHit);
    CHECK(result.transactions[0].finalHpDamage == 0);
    CHECK(requireById(frame.damageUnitExtras, 1).blockFirstHitsRemaining == 0);
    CHECK(frame.comboUnits.at(1).blockFirstHitsRemaining == 0);
}
```

- [ ] **Step 2: Run test**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleDamageApplication_UsesDamageRuntimeBlockFirstHitWithoutComboMirror"
```

Expected: PASS or FAIL. If FAIL, fix in Step 3.

- [ ] **Step 3: Remove damage mirror writes from application**

In `applyResolvedTransactionToLiveState()` remove:

```cpp
writeBattleDamageComboState(defenderCombo, transaction.defender);
writeBattleDamageComboState(attackerCombo, transaction.attacker);
```

Keep combo lookup only for static modifier state:

```cpp
auto& defenderCombo = requireMappedById(*input.comboUnits, transaction.defender.id);
```

If the variable becomes unused, remove it.

- [ ] **Step 4: Split static combo rules from live damage extras**

In `BattleDamageSystem.cpp`, keep `makeBattleDamageModifierState(const RoleComboState*)` for static damage rules, but stop copying live mutable fields into `BattleDamageUnitState` from combo in runtime paths.

Use this shape for runtime:

```cpp
BattleDamageUnitState makeBattleDamageUnitState(const BattleRuntimeUnit& unit, const BattleDamageRuntimeUnit* runtime)
{
    BattleDamageUnitState damage;
    damage.id = unit.id;
    damage.alive = unit.alive;
    damage.vitals = unit.vitals;
    damage.attack = unit.stats.attack;
    damage.invincible = unit.invincible;
    damage.shield = unit.shield;
    damage.mpBlocked = unit.mpBlocked;
    damage.mpRecoveryBonusPct = unit.mpRecoveryBonusPct;
    if (runtime)
    {
        damage.hurtInvincFrames = runtime->hurtInvincFrames;
        damage.blockFirstHitsRemaining = runtime->blockFirstHitsRemaining;
        damage.deathPrevention = runtime->deathPrevention;
        damage.deathPreventionUsed = runtime->deathPreventionUsed;
        damage.deathPreventionFrames = runtime->deathPreventionFrames;
        damage.killHealPct = runtime->killHealPct;
        damage.killInvincFrames = runtime->killInvincFrames;
        damage.bloodlustAttackPerKill = runtime->bloodlustAttackPerKill;
    }
    return damage;
}
```

- [ ] **Step 5: Delete `writeBattleDamageComboState()` production usage**

Search:

```powershell
rg -n "writeBattleDamageComboState" src\battle tests
```

Remove production calls. Keep or replace tests with direct runtime seeding.

- [ ] **Step 6: Run damage tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][damage_application][unit]"
x64\Debug\kys_tests.exe "[battle][damage][unit]"
```

Expected: PASS.

---

### Task 5: Move Defense Application Out Of Hit Resolver

**Files:**
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleHitResolverUnitTests.cpp`
- Test: `tests/BattleDamageApplicationSystemUnitTests.cpp`

- [ ] **Step 1: Write failing no-early-defense test**

Add to `tests/BattleHitResolverUnitTests.cpp`:

```cpp
TEST_CASE("BattleHitResolver_QueuesRawDamageWithoutConsumingShield", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.defenderCombo.shield = 50;
    input.defenderCombo.blockFirstHitsRemaining = 1;
    input.skill.hurtType = 0;
    input.skill.resolvedBaseDamage = 20;

    BattleRuntimeRandom random(1234);
    auto result = BattleHitResolver().resolve(input, random);

    auto hp = std::find_if(result.commands.begin(), result.commands.end(), [](const BattleGameplayCommand& command)
    {
        return std::holds_alternative<BattleHpDamageCommand>(command);
    });
    REQUIRE(hp != result.commands.end());
    const auto& damage = std::get<BattleHpDamageCommand>(*hp);
    CHECK(damage.damage > 0);
    CHECK(result.defenderCombo.shield == 50);
    CHECK(result.defenderCombo.blockFirstHitsRemaining == 1);
}
```

- [ ] **Step 2: Run test**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleHitResolver_QueuesRawDamageWithoutConsumingShield"
```

Expected: FAIL because resolver currently consumes shield/block.

- [ ] **Step 3: Remove resolver defense block**

Delete the `resolveDefense()` call and `writeDefenseState()` usage in `BattleHitResolver::resolve()`:

```cpp
auto defense = BattleDamageSystem().resolveDefense(...);
result.shapedHpDamage = defense.damage;
writeDefenseState(defenderCombo, defense.defender);
```

Do not emit shield absorbed or block-first-hit presentation from hit resolver anymore.

- [ ] **Step 4: Preserve presentation from damage application**

In `BattleDamageApplicationSystem.cpp`, when transaction events include:

- `BattleDamageEventType::ShieldAbsorbed`
- `BattleDamageEventType::BlockedByFirstHit`

emit the corresponding log/visual events there.

Use existing text:

```cpp
// Shield absorbed detail belongs in damage detail/log.
// Block-first-hit should emit EFT_BLOCK and "格擋了首輪傷害".
```

If `BattleDamageEventType::BlockedByFirstHit` is missing, add it to `BattleDamageSystem` when defense blocks by first hit.

- [ ] **Step 5: Run resolver and damage tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][hit_resolver][unit]"
x64\Debug\kys_tests.exe "[battle][damage_application][unit]"
```

Expected: PASS.

---

### Task 6: Keep Setup Import One-Way

**Files:**
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `src/battle/BattleStatusSystem.cpp`
- Modify: `src/battle/BattleDamageSystem.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Add setup import test**

Add:

```cpp
TEST_CASE("BattleRuntimeSession_ImportsInitialComboRuntimeStateOnce", "[battle][runtime][unit]")
{
    BattleRuntimeSessionInput input = minimalRuntimeSessionInput();
    KysChess::RoleComboState combo;
    combo.shield = 20;
    combo.blockFirstHitsRemaining = 1;
    combo.poisonTimer = 6;
    combo.mpBlockTimer = 3;
    input.comboStates[0] = combo;

    auto session = BattleRuntimeSession::create(input);
    const auto& runtime = session.runtime();

    CHECK(runtime.units.requireUnit(0).shield == 20);
    CHECK(requireById(runtime.damage.unitExtras, 0).blockFirstHitsRemaining == 1);
    CHECK(requireById(runtime.status.units, 0).effects.poisonTimer == 6);
    CHECK(requireById(runtime.status.units, 0).effects.mpBlockTimer == 3);
}
```

- [ ] **Step 2: Run test**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleRuntimeSession_ImportsInitialComboRuntimeStateOnce"
```

Expected: PASS after adapting helper names to existing test fixtures.

- [ ] **Step 3: Restrict combo-to-runtime conversion to session setup**

Keep `makeBattleStatusUnitState(const BattleRuntimeUnit&, const RoleComboState&)` only in setup if still needed.

Add a comment above it:

```cpp
// Setup-only legacy import: RoleComboState may carry saved/initial status fields before runtime construction.
// Frame-time code must use BattleStatusRuntimeUnit directly.
```

- [ ] **Step 4: Boundary search**

Run:

```powershell
rg -n "makeBattleStatusUnitState\\(.*RoleComboState|makeBattleDamageUnitState\\(.*RoleComboState|writeBattleStatusComboState|writeBattleDamageComboState" src\battle
```

Expected:

- `makeBattleStatusUnitState(const BattleRuntimeUnit&, const RoleComboState&)` appears only in setup/session import.
- No `writeBattleStatusComboState`.
- No `writeBattleDamageComboState`.
- No frame-time `makeBattleDamageUnitState(... RoleComboState*)`.

---

### Task 7: Remove Scene Frame Writes Back Into Global Combo

**Files:**
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp` or `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Decide tracker ownership**

Treat `shieldOnAllyDeathTracker` as runtime-only during battle. Do not write it into `ChessCombo::getMutableStates()` every frame.

- [ ] **Step 2: Remove scene writer**

Delete:

```cpp
void writeBattleDeathEffectTrackers(...)
```

And remove its call around frame apply:

```cpp
writeBattleDeathEffectTrackers(frameResult.deathEffectTrackers, cs);
```

- [ ] **Step 3: Remove scene-facing tracker payload if unused**

Search:

```powershell
rg -n "deathEffectTrackers|BattleFrameDeathEffectTrackerResult" src tests
```

If only used for this mirror, delete:

- `BattleFrameDeathEffectTrackerResult`
- `BattleFrameResult::deathEffectTrackers`
- population in `publishFrameApplyOutputs()`

- [ ] **Step 4: Add regression search**

Run:

```powershell
rg -n "ChessCombo::getMutableStates\\(\\).*shieldOnAllyDeathTracker|writeBattleDeathEffectTrackers|deathEffectTrackers" src tests
```

Expected: no frame-time scene writes to global combo.

---

### Task 8: Final Boundary Verification

**Files:**
- No planned source edits unless searches expose leftovers.

- [ ] **Step 1: Run mirror searches**

Run:

```powershell
rg -n "writeBattleStatusComboState|writeBattleDamageComboState|combo\\.shield|combo\\.blockFirstHitsRemaining|combo\\.poisonTimer|combo\\.bleedStacks|combo\\.mpBlockTimer|combo\\.dmgReduceDebuffs|combo\\.tempAttackBuffs" src\battle src\BattleSceneHades.cpp
```

Expected:

- No `writeBattleStatusComboState`.
- No `writeBattleDamageComboState`.
- `combo.shield` / `combo.blockFirstHitsRemaining` only in setup/import or test code, not frame application.
- `combo.poisonTimer`, `combo.bleedStacks`, `combo.mpBlockTimer`, `combo.dmgReduceDebuffs`, `combo.tempAttackBuffs` not used in frame-time runtime application.

- [ ] **Step 2: Run formatting check**

Run:

```powershell
git diff --check
```

Expected: no output.

- [ ] **Step 3: Build tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: MSBuild exits `0`.

- [ ] **Step 4: Run full test suite**

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

Expected: MSBuild exits `0`. Existing warnings are acceptable unless new warnings point at touched battle files.

---

## Notes For Implementation

- Do not add compatibility fallbacks from combo when a runtime row is missing. Missing runtime status/damage rows are invariants and should assert through `requireById`.
- Keep setup import separate from frame runtime. It is acceptable for session construction to read legacy combo fields once.
- Do not introduce a new broad DTO just to move copies around. Prefer removing mirrors and using existing runtime stores directly.
- If a test currently requires combo mirrors, update the test to assert the runtime owner instead.
- Use Traditional Chinese for new source strings.

## Self-Review

- Findings 1 and 2 are covered by Tasks 4 and 5.
- Findings 3 through 5 are covered by Tasks 1 through 4 and Task 8.
- Finding 6 is covered by Task 7.
- The plan keeps setup import one-way in Task 6 so old initial combo data can still seed runtime state without frame-time mirrors.
