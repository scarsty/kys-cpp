# Damage Transaction Semantics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Apply gameplay damage rules per projectile hit, while sorting same-frame damage application by larger hit first per defender and keeping logs unaggregated with useful projectile labels.

**Architecture:** Stop aggregating damage inputs before `BattleDamageSystem::resolveTransaction()`, and stop queueing stale full damage snapshots. Queue damage as small immutable hit intents, order those intents deterministically, then build the one-hit `BattleDamageTransactionInput` immediately from live runtime state at application time. Damage number visual ordering can be sorted without changing the per-hit gameplay sequence.

**Tech Stack:** C++20, Catch2, existing `BattleDamageApplicationSystem`, `BattleDamageSystem`, `BattleHitResolver`, `BattleAttackSystem`, and runtime frame runner.

---

## Root Cause

`BattleRuntimeSession` sets `runtime.damage.aggregatePendingTransactionsByDefender = true`, and `BattleDamageApplicationSystem::apply()` currently collapses all pending transactions for the same defender before `BattleDamageSystem::resolveTransaction()`.

That means:

- hurt invincibility is granted after a summed frame transaction instead of after the first projectile hit,
- shield / block-first-hit / death prevention state does not affect later hits in the same frame,
- `單次承傷上限` is correct for normal projectile hits only because `BattleHitResolver` caps before emitting `BattleHpDamageCommand`, but the aggregation layer can still make other paths apply caps to frame-summed damage,
- the last aggregated transaction wins attacker/log/presentation metadata.

The fix is not to add more special cases or “refresh” stale snapshots. The fix is to delete gameplay input aggregation, stop storing full damage snapshots in the pending queue, and make damage application sequential over live runtime state.

## Design Decision: Pending Damage Intent, Not Snapshot

`BattleDamageTransactionInput` is still useful as a local calculation row for one call to `BattleDamageSystem::resolveTransaction()`. It should not be the queued frame data format.

Introduce a smaller queued type:

```cpp
struct BattlePendingDamageIntent
{
    BattleDamageRequest request;
    BattleDamagePresentationInput presentation;
};
```

The queue owns only immutable hit facts: source id, target id, base damage, frozen frames, execute flags, labels, and presentation metadata. Mutable gameplay state such as HP, shield, invincibility, block-first-hit, death prevention, cooldown, and status is read from runtime stores at the moment the intent is applied.

This intentionally allows one small temporary transaction copy per actual hit. It avoids carrying, refreshing, and copying stale attacker/defender/status/cooldown snapshots through the frame queue.

## File Map

- Modify `src/battle/BattleCore.h`
  - Remove or retire `aggregatePendingTransactionsByDefender`.
  - Replace queued `std::vector<BattleDamageTransactionInput> pendingTransactions` and `pendingPresentation` with `std::vector<BattlePendingDamageIntent> pendingDamage`.
  - Add a narrow damage source label field if the label belongs in damage commands rather than attack events.

- Modify `src/battle/BattleRuntimeSession.cpp`
  - Stop enabling input aggregation.

- Modify `src/battle/BattleDamageApplicationSystem.h`
  - Replace `aggregatePendingTransactionsByDefender` with an ordering option only if needed, e.g. `sortPendingDamageByDefenderMagnitude`.
  - Accept pending damage intents plus live runtime stores instead of pending transaction snapshots.

- Modify `src/battle/BattleDamageApplicationSystem.cpp`
  - Delete `aggregatePendingTransactions()`.
  - Delete `aggregatePendingPresentation()`.
  - Build an ordered index of pending intents.
  - Build a temporary `BattleDamageTransactionInput` from live runtime state for each intent immediately before resolving it.
  - Keep logs and transaction outputs unaggregated.
  - Sort damage-number visual events per defender by larger number first if the current event order is not enough.

- Modify `src/battle/BattleCore.cpp`
  - Fix `BattleUnitStore::writeDamageUnit()` so it preserves all damage-state fields needed by later hits, especially `blockFirstHitsRemaining`.
  - Change append helpers to queue `BattlePendingDamageIntent`, not prebuilt transactions.
  - Ensure `makeFrameDamageApplicationInput()` passes current runtime stores into the application system.
  - Add label propagation from hit source into damage presentation/log detail if not already enough.

- Modify `src/battle/BattleHitResolver.h`
  - Add a small label/source kind to `BattleHpDamageCommand` and `BattleMpDamageCommand` only if `detailText` is not sufficient.

- Modify `src/battle/BattleHitResolver.cpp`
  - Populate hit labels from `BattleAttackEvent`: main projectile, tracking projectile, chained/bounce projectile, ultimate projectile, melee/dash.
  - Append labels to log detail, not to gameplay rules.

- Modify `src/battle/BattleAttackSystem.h`
  - If existing fields are insufficient, add a lightweight projectile source enum to `BattleAttackEvent` / `BattleAttackState`; otherwise derive from existing fields (`mainProjectile`, `track`, `ultimate`, `operationType`, `sharedHitGroupId`, bounce event).

- Tests:
  - `tests/BattleDamageApplicationSystemUnitTests.cpp`
  - `tests/BattleDamageSystemUnitTests.cpp`
  - `tests/BattleCoreUnitTests.cpp`
  - `tests/BattleHitResolverUnitTests.cpp`

---

### Task 1: Lock In Per-Hit Hurt Invincibility Behavior

**Files:**
- Modify: `tests/BattleDamageApplicationSystemUnitTests.cpp`

- [ ] **Step 1: Write the failing test**

Add this test near the existing aggregation tests:

```cpp
TEST_CASE("BattleDamageApplication_AppliesHurtInvincibilityBetweenSameFrameHits", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    input.aggregatePendingTransactionsByDefender = true;
    input.deathEffectUnits->units[1].hurtInvincFrames = 5;

    auto first = damageInput(0, 1, 3);
    first.defender.hurtInvincFrames = 5;
    first.request.triggersDefenseEffects = true;

    auto second = damageInput(0, 1, 4);
    second.defender.hurtInvincFrames = 5;
    second.request.triggersDefenseEffects = true;

    frame.pendingTransactions.push_back(first);
    frame.pendingTransactions.push_back(second);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 2);
    CHECK(result.transactions[0].finalHpDamage == 3);
    CHECK(result.transactions[0].hurtInvincGranted);
    CHECK(result.transactions[1].finalHpDamage == 0);
    CHECK(result.transactions[1].blockedByInvincible);
    CHECK(input.deathEffectUnits->requireUnit(1).vitals.hp == 7);
    CHECK(input.deathEffectUnits->requireUnit(1).invincible == 5);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "*AppliesHurtInvincibilityBetweenSameFrameHits*"
```

Expected before implementation: the test fails because there is only one aggregated transaction or the second hit is not blocked by invincibility.

---

### Task 2: Lock In Per-Hit Single-Damage-Cap Behavior

**Files:**
- Modify: `tests/BattleDamageApplicationSystemUnitTests.cpp`

- [ ] **Step 1: Write the failing test**

Add this test near the hurt invincibility test:

```cpp
TEST_CASE("BattleDamageApplication_AppliesSingleHitCapPerPendingHit", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    input.aggregatePendingTransactionsByDefender = true;

    auto first = damageInput(0, 1, 60);
    first.request.preResolvedDamage = false;
    first.defenderModifiers.maxHitPctMaxHp = 25;
    first.defender.vitals.maxHp = 100;

    auto second = damageInput(0, 1, 60);
    second.request.preResolvedDamage = false;
    second.defenderModifiers.maxHitPctMaxHp = 25;
    second.defender.vitals.maxHp = 100;

    frame.pendingTransactions.push_back(first);
    frame.pendingTransactions.push_back(second);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 2);
    CHECK(result.transactions[0].finalHpDamage == 25);
    CHECK(result.transactions[1].finalHpDamage == 25);
    CHECK(input.deathEffectUnits->requireUnit(1).vitals.hp == 50);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
x64\Debug\kys_tests.exe "*AppliesSingleHitCapPerPendingHit*"
```

Expected before implementation: the test fails because aggregation sums both hits before resolving the cap, or only one transaction is emitted.

---

### Task 3: Introduce Pending Damage Intent Queue And Deterministic Ordering

**Files:**
- Modify: `src/battle/BattleDamageApplicationSystem.h`
- Modify: `src/battle/BattleDamageApplicationSystem.cpp`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleRuntimeSession.cpp`

- [ ] **Step 1: Add the queued intent type**

In `BattleDamageApplicationSystem.h`, add:

```cpp
struct BattlePendingDamageIntent
{
    BattleDamageRequest request;
    BattleDamagePresentationInput presentation;
};
```

In `BattleCore.h`, replace:

```cpp
std::vector<BattleDamageTransactionInput> pendingTransactions;
std::vector<BattleDamagePresentationInput> pendingPresentation;
```

with:

```cpp
std::vector<BattlePendingDamageIntent> pendingDamage;
```

- [ ] **Step 2: Rename the input flag**

In `BattleDamageApplicationSystem.h`, replace:

```cpp
bool aggregatePendingTransactionsByDefender = false;
```

with:

```cpp
bool sortPendingDamageByDefenderMagnitude = false;
```

In `BattleCore.h`, replace the runtime field:

```cpp
bool aggregatePendingTransactionsByDefender = false;
```

with:

```cpp
bool sortPendingDamageByDefenderMagnitude = false;
```

- [ ] **Step 3: Update runtime setup**

In `BattleRuntimeSession.cpp`, replace:

```cpp
runtime.damage.aggregatePendingTransactionsByDefender = true;
```

with:

```cpp
runtime.damage.sortPendingDamageByDefenderMagnitude = true;
```

- [ ] **Step 4: Update frame input construction**

In `makeFrameDamageApplicationInput()` in `BattleCore.cpp`, replace:

```cpp
input.aggregatePendingTransactionsByDefender = state.damage.aggregatePendingTransactionsByDefender;
```

with:

```cpp
input.sortPendingDamageByDefenderMagnitude = state.damage.sortPendingDamageByDefenderMagnitude;
```

- [ ] **Step 5: Queue intents from command reducers**

In the `tryAppendFrameDamageTransaction()` overloads in `BattleCore.cpp`, keep building `BattleDamageRequest`, but stop building and queueing a full transaction snapshot.

For HP damage commands, replace the prebuilt transaction append with:

```cpp
state.damage.pendingDamage.push_back({
    request,
    makeFrameDamagePresentation(state, command),
});
```

For MP damage and accepted-hit side effect commands, queue an empty presentation:

```cpp
state.damage.pendingDamage.push_back({
    request,
    {},
});
```

Delete `appendFrameDamageTransaction()` and `appendDamagePendingPresentationPlaceholder()` once all callers use `pendingDamage`.

- [ ] **Step 6: Delete aggregation helpers**

Delete these functions from `BattleDamageApplicationSystem.cpp`:

```cpp
std::vector<BattleDamageTransactionInput> aggregatePendingTransactions(...);
std::vector<BattleDamagePresentationInput> aggregatePendingPresentation(...);
```

- [ ] **Step 7: Add ordered-index helper**

Add this helper in the anonymous namespace of `BattleDamageApplicationSystem.cpp`:

```cpp
std::vector<std::size_t> orderedPendingDamageIndexes(
    const std::vector<BattlePendingDamageIntent>& pendingDamage,
    bool sortByDefenderMagnitude)
{
    std::vector<std::size_t> indexes(pendingDamage.size());
    std::iota(indexes.begin(), indexes.end(), std::size_t{ 0 });
    if (!sortByDefenderMagnitude)
    {
        return indexes;
    }

    std::stable_sort(indexes.begin(), indexes.end(), [&](std::size_t lhs, std::size_t rhs)
    {
        const auto& left = pendingDamage[lhs].request;
        const auto& right = pendingDamage[rhs].request;
        return std::tuple{ left.defenderUnitId, -left.baseDamage }
            < std::tuple{ right.defenderUnitId, -right.baseDamage };
    });
    return indexes;
}
```

Include `<numeric>` if not already present.

- [ ] **Step 8: Iterate indexes instead of aggregated spans**

Replace the current aggregation/span block in `BattleDamageApplicationSystem::apply()` with:

```cpp
const auto orderedIndexes = orderedPendingDamageIndexes(
    *input.pendingDamage,
    input.sortPendingDamageByDefenderMagnitude);

for (const std::size_t pendingIndex : orderedIndexes)
{
    const auto& intent = (*input.pendingDamage)[pendingIndex];
    auto pending = makeDamageTransactionInputFromIntent(intent, input);
    auto transaction = BattleDamageSystem().resolveTransaction(pending);
    ...
    const auto& presentation = intent.presentation;
    ...
}
```

`makeDamageTransactionInputFromIntent()` is added in Task 4.

- [ ] **Step 9: Run focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "*BattleDamageApplication_Applies*SameFrameHits*" "*AppliesSingleHitCapPerPendingHit*"
```

Expected after this task may still fail if intent-to-transaction construction is not implemented yet; continue to Task 4.

---

### Task 4: Build One-Hit Damage Transactions From Live Runtime State

**Files:**
- Modify: `src/battle/BattleDamageApplicationSystem.cpp`
- Modify: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Fix unit-store damage writes**

In `BattleUnitStore::writeDamageUnit()` in `BattleCore.cpp`, add the missing first-hit-block write:

```cpp
unit.blockFirstHitsRemaining = source.blockFirstHitsRemaining;
```

The full damage-state section should include:

```cpp
unit.invincible = source.invincible;
unit.shield = source.shield;
unit.blockFirstHitsRemaining = source.blockFirstHitsRemaining;
unit.mpBlocked = source.mpBlocked;
unit.mpRecoveryBonusPct = source.mpRecoveryBonusPct;
```

- [ ] **Step 2: Pass live runtime stores to damage application**

In `BattleDamageApplicationSystem.h`, make `BattleDamageApplicationInput` point at the live stores needed to construct one-hit calculation rows:

```cpp
const BattleUnitStore* units = nullptr;
std::vector<BattleStatusRuntimeUnit>* statusUnits = nullptr;
std::vector<BattleDamageRuntimeUnit>* unitEffects = nullptr;
const std::vector<BattlePendingDamageIntent>* pendingDamage = nullptr;
```

Remove the old copied `std::vector<BattleDamageApplicationUnitSnapshot> units` input after consumers no longer need it.

- [ ] **Step 3: Add intent-to-transaction helper**

In `BattleDamageApplicationSystem.cpp`, add a helper that constructs the temporary calculation row immediately before the hit resolves:

```cpp
BattleDamageTransactionInput makeDamageTransactionInputFromIntent(
    const BattlePendingDamageIntent& intent,
    const BattleDamageApplicationInput& input)
{
    assert(input.units);
    assert(input.statusUnits);
    assert(input.unitEffects);
    assert(intent.request.defenderUnitId >= 0);

    BattleDamageTransactionInput transaction;
    transaction.request = intent.request;
    transaction.defender = makeBattleDamageUnitState(
        input.units->requireUnit(intent.request.defenderUnitId),
        tryFindById(*input.unitEffects, intent.request.defenderUnitId));
    if (intent.request.attackerUnitId >= 0)
    {
        transaction.attacker = makeBattleDamageUnitState(
            input.units->requireUnit(intent.request.attackerUnitId),
            tryFindById(*input.unitEffects, intent.request.attackerUnitId));
    }
    else
    {
        transaction.attacker.id = intent.request.attackerUnitId;
    }
    transaction.defenderStatus = makeCurrentBattleStatusUnitState(
        *input.statusUnits,
        *input.units,
        intent.request.defenderUnitId);
    transaction.defenderCooldown = makeBattleFrameCooldownState(
        input.units->requireUnit(intent.request.defenderUnitId));
    return transaction;
}
```

If `makeBattleDamageUnitState()`, `makeCurrentBattleStatusUnitState()`, or `makeBattleFrameCooldownState()` are not visible in this file, move small public helper declarations to the relevant battle headers. Do not duplicate field-by-field projection in the application system.

- [ ] **Step 4: Keep mutation local and sequential**

Inside the application loop, after each `BattleDamageSystem().resolveTransaction(pending)`, immediately write the result back into the live stores before processing the next intent:

```cpp
input.units->writeDamageUnit(transaction.defender);
input.units->writeDamageUnit(transaction.attacker);
if (auto* defenderExtra = tryFindById(*input.unitEffects, transaction.defender.id))
{
    writeBattleDamageRuntimeUnit(*defenderExtra, transaction.defender);
}
if (auto* attackerExtra = tryFindById(*input.unitEffects, transaction.attacker.id))
{
    writeBattleDamageRuntimeUnit(*attackerExtra, transaction.attacker);
}
if (auto* status = tryFindById(*input.statusUnits, transaction.defender.id))
{
    writeBattleStatusRuntimeUnit(*status, transaction.defenderStatus);
}
```

This is the important semantic point: later same-frame intents must see the invincibility, shield, death-prevention, HP, and block-first-hit changes made by earlier intents.

- [ ] **Step 5: Run focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "*AppliesHurtInvincibilityBetweenSameFrameHits*" "*AppliesSingleHitCapPerPendingHit*"
```

Expected: both pass.

---

### Task 5: Preserve Larger-Damage Visual Ordering Without Gameplay Aggregation

**Files:**
- Modify: `tests/BattleDamageApplicationSystemUnitTests.cpp`
- Modify: `src/battle/BattleDamageApplicationSystem.cpp`

- [ ] **Step 1: Write the failing visual-order test**

Add this test helper near existing damage application test helpers:

```cpp
BattlePendingDamageIntent pendingDamageIntent(
    BattleDamageTransactionInput transaction,
    BattleDamagePresentationInput presentation = {})
{
    return {
        transaction.request,
        std::move(presentation),
    };
}
```

Add:

```cpp
TEST_CASE("BattleDamageApplication_OrdersDamageNumbersByLargestHitPerDefender", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    input.sortPendingDamageByDefenderMagnitude = true;

    BattleDamagePresentationInput firstPresentation;
    firstPresentation.enabled = true;
    firstPresentation.normalDamageColor = { 10, 20, 30, 255 };
    firstPresentation.normalDamageTextSize = 22;

    BattleDamagePresentationInput secondPresentation = firstPresentation;

    frame.pendingDamage.push_back(pendingDamageIntent(damageInput(0, 1, 3), firstPresentation));
    frame.pendingDamage.push_back(pendingDamageIntent(damageInput(0, 1, 7), secondPresentation));

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 2);
    CHECK(result.transactions[0].finalHpDamage == 7);
    CHECK(result.transactions[1].finalHpDamage == 3);
    REQUIRE(result.visualEvents.size() == 2);
    CHECK(result.visualEvents[0].amount == 7);
    CHECK(result.visualEvents[1].amount == 3);
}
```

- [ ] **Step 2: Run the test**

Run:

```powershell
x64\Debug\kys_tests.exe "*OrdersDamageNumbersByLargestHitPerDefender*"
```

Expected: fail before ordering is wired, pass after Task 3/4.

- [ ] **Step 3: Keep logs unaggregated**

Do not add any log aggregation. Existing `appendDamageOutputEvents()` and `appendDamageTransactionLogEvents()` should run once per transaction.

- [ ] **Step 4: Update old aggregation tests**

Replace assertions expecting one aggregated transaction with assertions expecting ordered per-hit transactions. For example, replace:

```cpp
REQUIRE(result.transactions.size() == 1);
CHECK(result.transactions[0].defender.vitals.hp == 3);
```

with:

```cpp
REQUIRE(result.transactions.size() == 2);
CHECK(result.transactions[0].finalHpDamage == 4);
CHECK(result.transactions[1].finalHpDamage == 3);
CHECK(result.transactions.back().defender.vitals.hp == 3);
```

---

### Task 6: Add Projectile Source Labels To Damage Logs

**Files:**
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `src/battle/BattleHitResolver.h` only if needed

- [ ] **Step 1: Write the failing label test**

Add:

```cpp
TEST_CASE("BattleHitResolver_DamageCommandLabelsProjectileSource", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.id = 101;
    input.skill.name = "降龍";
    input.skill.resolvedBaseDamage = 70;
    input.attackEvent.operationType = BattleOperationType::RangedProjectile;
    input.attackEvent.ultimate = true;
    input.attackEvent.track = true;
    input.attackEvent.mainProjectile = false;

    auto result = resolveHit(input);

    auto* damage = firstHpDamageCommand(result);
    REQUIRE(damage);
    CHECK(damage->detailText.find("絕招追蹤彈") != std::string::npos);
}
```

- [ ] **Step 2: Add source label helper**

In `BattleHitResolver.cpp`, add a helper near other formatting helpers:

```cpp
std::string projectileSourceLabel(const BattleAttackEvent& event)
{
    if (event.operationType == BattleOperationType::Melee)
    {
        return "近戰";
    }
    if (event.operationType == BattleOperationType::Dash)
    {
        return "滑步";
    }
    if (event.ultimate && event.track && !event.mainProjectile)
    {
        return "絕招追蹤彈";
    }
    if (event.ultimate && !event.mainProjectile)
    {
        return "絕招追加彈";
    }
    if (event.track && !event.mainProjectile)
    {
        return "追蹤彈";
    }
    if (event.sharedHitGroupId > 0 && !event.mainProjectile)
    {
        return "連鎖彈";
    }
    if (event.ultimate)
    {
        return "";
    }
    return "";
}
```

- [ ] **Step 3: Append label to damage detail**

Before creating `BattleHpDamageCommand`, append only non-empty labels:

```cpp
if (auto label = projectileSourceLabel(input.attackEvent); !label.empty())
{
    damageDetail = appendDetail(std::move(damageDetail), label);
}
```

Do the same for MP damage command detail if MP damage gets a presentation/log detail field. If it does not, leave MP path untouched in this batch.

- [ ] **Step 4: Run focused label test**

Run:

```powershell
x64\Debug\kys_tests.exe "*DamageCommandLabelsProjectileSource*"
```

Expected: pass.

---

### Task 7: Remove Aggregation Migration Surface

**Files:**
- Modify: `src/battle/BattleDamageApplicationSystem.h`
- Modify: `src/battle/BattleDamageApplicationSystem.cpp`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: tests referencing old aggregation names

- [ ] **Step 1: Search old aggregation names**

Run:

```powershell
rg -n "aggregatePendingTransactionsByDefender|aggregatePendingTransactions|aggregatePendingPresentation|AggregatesPendingDamage|AggregatedPendingDamage" src tests
```

Expected before cleanup: only old tests and old flag references.

- [ ] **Step 2: Delete or rename old tests**

Rename:

```cpp
BattleDamageApplication_AggregatesPendingDamageByDefenderWhenRequested
BattleDamageApplication_AggregatedPendingDamageUsesLastPresentationMetadata
```

to tests that assert ordered per-hit behavior and no log aggregation.

- [ ] **Step 3: Verify search is clean**

Run:

```powershell
rg -n "aggregatePendingTransactionsByDefender|aggregatePendingTransactions|aggregatePendingPresentation|AggregatesPendingDamage|AggregatedPendingDamage" src tests
```

Expected: no matches.

- [ ] **Step 4: Verify the old queued snapshot surface is gone**

Run:

```powershell
rg -n "pendingTransactions|pendingPresentation" src tests
```

Expected: no production queue references. Test helper names should also be migrated unless they refer to local transaction results, not queued pending state.

---

### Task 8: Full Verification

**Files:**
- No code edits.

- [ ] **Step 1: Whitespace check**

Run:

```powershell
git diff --check
```

Expected: no output, exit code 0.

- [ ] **Step 2: Build tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: exit code 0.

- [ ] **Step 3: Run all tests**

Run:

```powershell
x64\Debug\kys_tests.exe
```

Expected: all tests pass.

- [ ] **Step 4: Build game**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected: exit code 0. Existing project warning noise is acceptable.

---

## Expected End State

- No gameplay damage input aggregation remains.
- Hurt invincibility applies after the first resolved projectile hit and can block later same-frame hits.
- `單次承傷上限` applies per hit, not per frame-summed damage.
- Damage transactions/logs remain per hit.
- Damage numbers for the same defender are emitted largest first when same-frame sorting is enabled.
- Logs include a useful projectile source label such as `追蹤彈`, `連鎖彈`, `絕招追加彈`, `絕招追蹤彈`, or `滑步`; ordinary main projectiles, including ultimate main projectiles, do not add a generic `主彈` label.
- The scene receives final runtime state and does not recompute these damage semantics.

## Self-Review

- Spec coverage: per-hit damage rules, larger-first damage number ordering, no log aggregation, and projectile labels are each covered by tasks.
- Placeholder scan: no task uses unresolved TODO/TBD language.
- Type consistency: the plan intentionally renames aggregation flags to ordering flags and removes old aggregation helpers; all follow-up tasks reference the new names.
