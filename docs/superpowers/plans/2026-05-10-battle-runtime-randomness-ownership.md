# Battle Runtime Randomness Ownership Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make all gameplay battle randomness runtime-owned, replayable by seed plus call count, and directly consumed through explicit roll APIs instead of `std::function` callbacks or scene RNG lookups.

**Architecture:** `BattleRuntimeRandom` owns its own `RandomDouble rand_`, seed, and call count. Battle setup passes a `BattleRuntimeRandomState` across the scene/runtime boundary. Runtime systems consume randomness through `BattleRuntimeState::random` methods such as `chance()` and `symmetricInt()`. Stateless combo helper wrappers that only hide a chance check are removed.

**Tech Stack:** C++20, MSVC, Catch2, existing `RandomDouble` from `mlcc/Random.h`, existing `.github\build-command.ps1` verification.

---

## Current Problems

- `BattleRuntimeRandom` is a standalone LCG-like state, not aligned with the project’s existing seed/call-count replay model.
- Scene `rand_` still controls one setup gameplay fact: enemy facing fallback in `BattleSceneHades::initializeBattleRuntime()`.
- `BattleHitResolver` pre-generates anonymous `percentRolls`, so tests/callers must know hidden roll count and order.
- `BattleComboTriggerSystem` accepts `std::function<double()>` in many places and is freshly constructed at call sites.
- Several combo methods are tiny chance wrappers and should be deleted once direct roll calls exist.

---

## File Map

- `src/battle/BattleCore.h`
  - Replace current `BattleRuntimeRandom` fields/API with seed + call-count backed `RandomDouble`.
  - Add `BattleRuntimeRandomState`.
  - Add `chance()` and `symmetricInt()` methods.
  - Add random state to runtime init/setup inputs if needed.

- `src/battle/BattleCore.cpp`
  - Implement new random methods.
  - Rename low-level random consumption call sites from raw `nextPercent`/manual differences to direct methods where useful.
  - Stop pre-generating hit roll vectors.

- `src/battle/BattleHitResolver.h`
  - Remove `percentRolls` from `BattleHitResolutionInput`.
  - Change `BattleHitResolver::resolve` to accept `BattleRuntimeRandom& random`.

- `src/battle/BattleHitResolver.cpp`
  - Delete `BattleHitRollStream`.
  - Use `random.chance(...)`, `random.nextPercent()`, and `random.symmetricInt(...)` directly.

- `src/battle/BattleComboTriggerSystem.h`
  - Replace `std::function<double()>` parameters with `BattleRuntimeRandom&` where a collector truly owns multiple chance checks.
  - Delete trivial chance wrappers.

- `src/battle/BattleComboTriggerSystem.cpp`
  - Use `random.chance(...)`.
  - Keep meaningful collectors and reducers.
  - Remove methods whose body is only a chance predicate.

- `src/BattleSceneBattleAdapter.h`
  - Add random state to battle runtime build context.

- `src/BattleSceneBattleAdapter.cpp`
  - Pass runtime random state into `BattleRuntimeInit`.

- `src/BattleSceneHades.cpp`
  - Build and pass `BattleRuntimeRandomState`.
  - Remove gameplay setup use of `rand_` for enemy facing fallback, or route it through runtime random state before session creation.
  - Keep scene `rand_` only for presentation noise unless later visual replay requires otherwise.

- `src/ChessBattleFlow.h`
- `src/ChessBattleFlow.cpp`
- `src/ChessSelector.h`
- `src/ChessSelector.cpp`
- `src/ChessChallengeFlow.cpp`
  - Pass battle random state, not only raw seed.
  - Return/export final battle random state after battle if the battle should advance persistent replay state.

- `src/GameDataStore.h`
- `src/GameState.cpp`
- `src/Save.cpp` if serialization needs explicit fields
  - Add persisted battle seed/call-count fields if battle RNG must survive save/load independently.

- Tests:
  - `tests/BattleCoreUnitTests.cpp`
  - `tests/BattleHitResolverUnitTests.cpp`
  - `tests/BattleComboTriggerSystemUnitTests.cpp`
  - Battle flow/state tests if present.

---

## Design Decisions

- `BattleRuntimeRandom` owns `RandomDouble rand_`; it is not a wrapper around scene `rand_`.
- The constructor takes seed and call count. Public `restore()` is not part of the normal production API.
- `exportState()` is used to save final state.
- `nextInt()` increments call count once. `nextPercent()` increments once. `symmetricInt(n)` increments twice because it calls `nextInt(n)` twice.
- `chance(0)` returns false without consuming a roll. `chance(100)` returns true without consuming a roll. This avoids meaningless RNG consumption for guaranteed outcomes.
- Gameplay randomness must use runtime random. Presentation-only random can stay scene-local.
- Do not add `BattleRollPurpose` unless a later debugging/test-tracing need appears.
- Do not keep wrappers like `shouldApplyKnockback()` when the call site is clearer with `random.chance(combo.knockbackChancePct)`.

---

## Task 1: Add Replayable `BattleRuntimeRandom`

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Write failing seed/call-count test**

Add:

```cpp
TEST_CASE("BattleRuntimeRandom_RestoresFromSeedAndCallCount", "[battle][random]")
{
    KysChess::Battle::BattleRuntimeRandom first({ 1234u, 0 });
    const int firstRoll = first.nextInt(1000);
    const int secondRoll = first.nextInt(1000);
    auto stateAfterTwo = first.exportState();

    KysChess::Battle::BattleRuntimeRandom restored({ 1234u, 1 });
    CHECK(restored.nextInt(1000) == secondRoll);

    KysChess::Battle::BattleRuntimeRandom restoredAfterTwo(stateAfterTwo);
    CHECK(restoredAfterTwo.exportState().callCount == 2);

    KysChess::Battle::BattleRuntimeRandom fresh({ 1234u, 0 });
    CHECK(fresh.nextInt(1000) == firstRoll);
}
```

Expected red: constructor/export state does not exist.

- [ ] **Step 2: Write failing chance/count test**

Add:

```cpp
TEST_CASE("BattleRuntimeRandom_ChanceSkipsGuaranteedOutcomesAndCountsRealRolls", "[battle][random]")
{
    KysChess::Battle::BattleRuntimeRandom random({ 99u, 0 });

    CHECK_FALSE(random.chance(0));
    CHECK(random.exportState().callCount == 0);

    CHECK(random.chance(100));
    CHECK(random.exportState().callCount == 0);

    (void)random.chance(50);
    CHECK(random.exportState().callCount == 1);

    (void)random.symmetricInt(10);
    CHECK(random.exportState().callCount == 3);
}
```

- [ ] **Step 3: Implement state and class**

In `BattleCore.h`:

```cpp
struct BattleRuntimeRandomState
{
    unsigned int seed = 1;
    std::uint64_t callCount = 0;
};

class BattleRuntimeRandom
{
public:
    explicit BattleRuntimeRandom(BattleRuntimeRandomState state = {});

    BattleRuntimeRandomState exportState() const;
    double nextPercent();
    int nextInt(int upperBound);
    bool chance(int chancePct);
    int symmetricInt(int exclusiveBound);

private:
    unsigned int seed_ = 1;
    std::uint64_t callCount_ = 0;
    RandomDouble rand_;
};
```

Include `Random.h` where needed.

In `BattleCore.cpp`, construct by:

```cpp
BattleRuntimeRandom::BattleRuntimeRandom(BattleRuntimeRandomState state)
    : seed_(state.seed),
      callCount_(state.callCount)
{
    rand_.set_seed(seed_);
    rand_.get_generator().discard(callCount_);
}
```

Implement methods using `rand_.rand()` and `rand_.rand_int()`.

- [ ] **Step 4: Remove old LCG internals**

Delete `nextRaw()` and direct public `state` usage from `BattleRuntimeRandom`.

- [ ] **Step 5: Run focused tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleRuntimeRandom_RestoresFromSeedAndCallCount"
x64\Debug\kys_tests.exe "BattleRuntimeRandom_ChanceSkipsGuaranteedOutcomesAndCountsRealRolls"
```

---

## Task 2: Pass Runtime Random State Across Battle Setup Boundary

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/battle/BattleInitialization.h` if `BattleRuntimeInit` owns the random state
- Modify: `src/BattleSceneHades.cpp`
- Test: `tests/BattleInitializationUnitTests.cpp`

- [ ] **Step 1: Add random state to build context**

Add to `BattleRuntimeBuildContext` or `BattleRuntimeSceneSetupInput`:

```cpp
Battle::BattleRuntimeRandomState randomState;
```

Prefer `BattleRuntimeBuildContext` because this is runtime construction state, not unit setup data.

- [ ] **Step 2: Initialize runtime random from context**

In `createInitializedBattleRuntimeSession()` setup:

```cpp
init.runtime.random = Battle::BattleRuntimeRandom(context.randomState);
```

If assignment is awkward because `BattleRuntimeRandom` has no cheap default lifecycle, make `BattleRuntimeState` construction/factory explicit instead of adding restore APIs.

- [ ] **Step 3: Add test proving context random state is consumed**

Add:

```cpp
TEST_CASE("BattleSceneBattleAdapter_InitializesRuntimeRandomFromBuildContext", "[battle][initialization]")
{
    auto context = makeMinimalRuntimeBuildContext();
    context.randomState = { 777u, 2 };

    auto created = Adapter::createInitializedBattleRuntimeSession(context);

    CHECK(created.session.runtime().random.exportState().seed == 777u);
    CHECK(created.session.runtime().random.exportState().callCount == 2);
}
```

Use existing adapter test helpers instead of creating a new broad fixture.

- [ ] **Step 4: Build random state in Hades**

In `BattleSceneHades::makeBattleRuntimeBuildContext()` or adjacent setup:

```cpp
context.randomState = battle_random_state_;
```

Add a Hades member only if needed:

```cpp
KysChess::Battle::BattleRuntimeRandomState battle_random_state_;
```

Do not use scene `rand_` as the runtime random source.

---

## Task 3: Persist Battle Random State Through Chess Battle Flow

**Files:**
- Modify: `src/GameDataStore.h`
- Modify: `src/ChessBattleFlow.h`
- Modify: `src/ChessBattleFlow.cpp`
- Modify: `src/ChessSelector.h`
- Modify: `src/ChessSelector.cpp`
- Modify: `src/ChessChallengeFlow.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

- [ ] **Step 1: Add persisted state fields**

In `GameDataStore.h`, add fields next to existing chess RNG fields:

```cpp
unsigned int battleSeed = 0;
unsigned int battleCallCount = 0;
```

Use `unsigned int` if the existing save format expects that style. Use `std::uint64_t` only if serialization can safely handle it everywhere. If using `unsigned int`, assert before export if runtime call count exceeds max.

- [ ] **Step 2: Extend battle flow API**

Replace raw seed-only battle invocation with a state object:

```cpp
struct ChessBattleRandomState
{
    unsigned int seed = 0;
    unsigned int callCount = 0;
};
```

or use `KysChess::Battle::BattleRuntimeRandomState` directly if include direction is acceptable.

Update:

```cpp
int runBattle(..., int seed = -1, ...);
```

to a form that can carry call count:

```cpp
int runBattle(..., std::optional<Battle::BattleRuntimeRandomState> randomState = std::nullopt, ...);
```

If avoiding `std::optional` in this layer, pass:

```cpp
Battle::BattleRuntimeRandomState randomState
```

and resolve default before calling.

- [ ] **Step 3: Seed new battles consistently**

For brand-new battles, generate or reuse the same battle seed source currently passed as `battleSeed`. Set call count to 0:

```cpp
BattleRuntimeRandomState{ static_cast<unsigned int>(battleSeed), 0 }
```

For retries/load, pass the stored `{ battleSeed, battleCallCount }`.

- [ ] **Step 4: Export final state after battle**

Add to `BattleSceneHades`:

```cpp
KysChess::Battle::BattleRuntimeRandomState battleRuntimeRandomState() const;
```

Implementation:

```cpp
return battle_session_ ? battle_session_->runtime().random.exportState() : battle_random_state_;
```

After `battle->run()`, export the final state back to the owning flow/store:

```cpp
auto finalRandom = battle->battleRuntimeRandomState();
services_.gameState.battleSeed() = finalRandom.seed;
services_.gameState.battleCallCount() = checkedCallCount(finalRandom.callCount);
```

Use the actual service/store access pattern in this codebase; do not invent a global.

- [ ] **Step 5: Remove seed-only scene seeding for gameplay**

Remove or stop relying on:

```cpp
battle->rand_.set_seed(static_cast<unsigned int>(seed));
```

Scene `rand_` can still be seeded for presentation if needed, but it must not be the source of runtime gameplay randomness.

---

## Task 4: Move Setup Gameplay Random Off Scene `rand_`

**Files:**
- Modify: `src/BattleSceneHades.cpp`
- Modify: setup tests if needed

- [ ] **Step 1: Identify gameplay uses of Hades `rand_`**

Run:

```powershell
rg -n "rand_\\.(rand_int|rand)\\(" src\BattleSceneHades.cpp
```

Classify each use:

- `faceTowardsFallback`: setup gameplay/presentation state
- camera shake and blood effect variant: presentation-only

- [ ] **Step 2: Remove random fallback facing or route through runtime random**

Preferred deletion: make setup facing deterministic from nearest opponent and use role/source facing only if no opponent exists. Then enemy fallback no longer needs randomness:

```cpp
request.faceTowardsFallback = source->FaceTowards != Towards_None
    ? source->FaceTowards
    : Towards_RightDown;
```

If random fallback must be preserved, consume from `BattleRuntimeRandom` before building setup units and update `battle_random_state_` with the exported call count.

- [ ] **Step 3: Keep presentation random local**

Leave camera shake and blood visual random on scene `rand_` for now. They do not affect runtime outcome. Document this in final summary.

---

## Task 5: Replace Hit Resolver Percent Roll Vector

**Files:**
- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleHitResolverUnitTests.cpp`

- [ ] **Step 1: Change resolver signature**

Replace:

```cpp
BattleHitResolutionResult resolve(const BattleHitResolutionInput& input) const;
```

with:

```cpp
BattleHitResolutionResult resolve(const BattleHitResolutionInput& input, BattleRuntimeRandom& random) const;
```

Remove:

```cpp
std::vector<double> percentRolls;
```

from `BattleHitResolutionInput`.

- [ ] **Step 2: Delete `BattleHitRollStream`**

Remove the local stream class from `BattleHitResolver.cpp`.

- [ ] **Step 3: Use runtime random directly**

Replace:

```cpp
[&]() { return rolls.next(); }
```

with direct use of `random`:

```cpp
random.nextPercent()
random.chance(chancePct)
```

When a combo collector still needs multiple chance checks, pass `random`.

- [ ] **Step 4: Update caller**

In `BattleCore.cpp`, delete:

```cpp
input.percentRolls = takeHitPercentRolls(state);
```

and call:

```cpp
auto result = BattleHitResolver().resolve(input, state.random);
```

- [ ] **Step 5: Update focused tests**

In `BattleHitResolverUnitTests.cpp`, replace fixed `input.percentRolls` with seeded `BattleRuntimeRandom`:

```cpp
BattleRuntimeRandom random({ 123u, 0 });
auto result = BattleHitResolver().resolve(input, random);
```

Where exact chance pass/fail is needed, choose seed/call-count pairs that produce desired outcomes and document them in helper functions. Do not add a production fixed-roll vector.

---

## Task 6: Replace `std::function<double()>` in Combo Trigger System

**Files:**
- Modify: `src/battle/BattleComboTriggerSystem.h`
- Modify: `src/battle/BattleComboTriggerSystem.cpp`
- Modify: callers in `src/battle/BattleCore.cpp`, `src/battle/BattleHitResolver.cpp`
- Test: `tests/BattleComboTriggerSystemUnitTests.cpp`

- [ ] **Step 1: Replace function parameters**

For meaningful collectors/reducers, change:

```cpp
const std::function<double()>& rollPercent
```

to:

```cpp
BattleRuntimeRandom& random
```

Examples to keep:

- `collectTriggerEvents`
- `collectChanceEffects`
- `collectTriggeredTeamHeal`
- `collectPendingSkillTeamHeal`
- `collectOnHitComboCommands`
- `collectShieldBreakCommands`
- `collectStunCommands`
- `collectExtraProjectileCount`
- `shapeAttackerHitDamage`
- `resolveExecuteCombo`
- `collectDefenderBlockCommands`
- `resolveArmorPenetratedDefense`

- [ ] **Step 2: Use `random.chance()`**

Replace:

```cpp
effect.triggerValue > 0 && rollPercent() >= effect.triggerValue
```

with:

```cpp
effect.triggerValue > 0 && !random.chance(effect.triggerValue)
```

Replace direct comparisons similarly:

```cpp
random.chance(state.critChancePct)
```

- [ ] **Step 3: Remove trivial chance wrapper methods**

Delete these from header and cpp:

```cpp
bool resolveProjectileReflect(...);
int resolveLegacyStunFrames(...);
bool shouldApplyKnockback(...);
int resolveOffensiveCooldownExtendPct(...);
int resolveDefensiveCooldownExtendPct(...);
BattleBleedProc resolveBleedProc(...);
BattleDamageReduceDebuffProc resolveDamageReduceDebuffProc(...);
```

Only delete a method after moving its effect code to the real call site. Keep a method if it does more than a simple chance check and payload construction.

- [ ] **Step 4: Inline deleted helper logic at effect sites**

Examples:

```cpp
if (random.chance(attackerCombo.knockbackChancePct))
{
    commands.push_back(BattleKnockbackCommand{ ... });
}
```

For bleed/debuff, construct the command/event directly where it is applied:

```cpp
if (damagePositive && random.chance(attackerCombo.bleedChancePct))
{
    request.bleedStacks = 1;
    request.bleedMaxStacks = attackerCombo.bleedMaxStacks;
}
```

- [ ] **Step 5: Avoid inline temporary system construction**

Replace repeated:

```cpp
BattleComboTriggerSystem().collectTriggerEvents(...)
```

inside a workflow with one local:

```cpp
BattleComboTriggerSystem combo;
auto events = combo.collectTriggerEvents(...);
```

Do not make the system own `random` as a member.

- [ ] **Step 6: Search for old random callbacks**

Run:

```powershell
rg -n "std::function<double\\(\\)>|rollPercent\\)|\\[&\\]\\(\\) \\{ return .*nextPercent|BattleComboTriggerSystem\\(\\)" src\battle tests
```

Expected:

- no `std::function<double()>` in combo random path
- no lambda random callback pass-through
- no inline `BattleComboTriggerSystem()` in production code

---

## Task 7: Clean Runtime Random Call Sites

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Modify: any touched runtime files
- Test: runtime tests

- [ ] **Step 1: Replace manual symmetric differences**

Replace:

```cpp
state.random.nextInt(10) - state.random.nextInt(10)
```

with:

```cpp
state.random.symmetricInt(10)
```

- [ ] **Step 2: Replace chance comparisons**

Replace:

```cpp
state.random.nextPercent() < chancePct
```

with:

```cpp
state.random.chance(chancePct)
```

Do not force this when the raw roll value is needed for reporting/result data, such as `resolveDodge` returning `chancePct` and `dodged`. In those cases keep explicit:

```cpp
const double roll = state.random.nextPercent();
auto dodge = combo.resolveDodge(..., roll);
```

- [ ] **Step 3: Delete unused helpers**

Delete:

```cpp
takeHitPercentRolls
nextRuntimeUnitRoll
```

if no longer needed after direct runtime random usage.

- [ ] **Step 4: Search**

Run:

```powershell
rg -n "takeHitPercentRolls|nextRuntimeUnitRoll|nextPercent\\(\\).*<|nextInt\\([^\\)]*\\) - .*nextInt" src\battle
```

Expected: no migration leftovers.

---

## Task 8: Tests for Persistence and Replay

**Files:**
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: battle flow tests if available

- [ ] **Step 1: Add frame replay test**

Create a test that runs the same runtime setup two ways:

1. start from `{ seed, 0 }`, run N frames, export state
2. start from `{ seed, exported.callCountBeforeFinalFrame }`, run equivalent remaining frame

Assert the same gameplay outputs for the resumed frame.

Use an existing minimal runtime setup helper.

- [ ] **Step 2: Add battle flow state export test if feasible**

If battle flow is testable without UI, assert:

```cpp
finalBattleRandom.callCount > initialBattleRandom.callCount
finalBattleRandom.seed == initialBattleRandom.seed
```

If not feasible, cover this through `BattleSceneHades`/adapter-level test and note the limitation.

---

## Task 9: Final Verification

**Files:**
- All touched files.

- [ ] **Step 1: Boundary searches**

Run:

```powershell
rg -n "std::function<double\\(\\)>|percentRolls|takeHitPercentRolls|nextRaw|BattleComboTriggerSystem\\(\\)" src\battle src tests
rg -n "rand_\\.(rand_int|rand)\\(" src\BattleSceneHades.cpp
rg -n "state\\.random\\.(nextInt|nextPercent|chance|symmetricInt)|exportState\\(\\)" src\battle src tests
```

Expected:

- no random callback functions in gameplay path
- no anonymous fixed roll vectors in hit resolver
- no old `nextRaw`
- Hades `rand_` uses are presentation-only or documented setup exceptions
- runtime random state is exportable and used for gameplay rolls

- [ ] **Step 2: Diff check**

Run:

```powershell
git diff --check
```

Expected: exit code 0.

- [ ] **Step 3: Build tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: exit code 0.

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

Expected: exit code 0. Existing warnings are acceptable. Link failure is acceptable only if the game executable is locked by a running process.

---

## Recommended Execution Order

1. Replayable `BattleRuntimeRandom`.
2. Runtime random state through adapter/session/Hades.
3. Battle flow save/export wiring.
4. Remove Hades gameplay RNG use.
5. Hit resolver direct runtime random.
6. Combo trigger direct runtime random and trivial helper deletion.
7. Runtime call-site cleanup.
8. Replay/persistence tests and full verification.

---

## Self-Review

- **Spec coverage:** Covers new randomness class, seed/call-count construction, save/export, scene/runtime boundary, `std::function` removal, hit resolver roll vector removal, combo helper deletion, and final verification.
- **No placeholders:** Each task names files, code shapes, commands, and expected outcomes.
- **Boundary clarity:** Scene `rand_` remains only for presentation noise unless explicitly moved later.
- **Risk:** Battle flow persistence depends on actual service/store access. The plan calls this out and requires using existing access patterns rather than adding globals.
