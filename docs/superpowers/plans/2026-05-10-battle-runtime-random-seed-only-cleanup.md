# Battle Runtime Random Seed-Only Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Simplify battle runtime randomness back to seed-only replay semantics while keeping runtime-owned gameplay rolls and removing the unneeded call-count persistence machinery.

**Architecture:** `BattleRuntimeRandom` remains the runtime-owned gameplay RNG facade, but it stores only the battle seed and a direct `std::mt19937` engine. Battle flow passes the battle seed at battle creation. Runtime code consumes rolls through `nextInt`, `nextPercent`, `chance`, and `symmetricInt`. Scene `rand_` remains presentation-only.

**Tech Stack:** C++20, `std::mt19937`, MSVC, Catch2, existing `.github\build-command.ps1` verification.

---

## Current State To Clean Up

The completed implementation introduced:

- `BattleRuntimeRandomState { seed, callCount }`
- `BattleRuntimeRandom` owning `RandomDouble rand_`
- copy/move constructors and assignments that reconstruct by `seed + callCount`
- `exportState()` returning seed and call count
- tests asserting call count preservation

That shape is too heavy if battle retry/load restarts from the beginning of the battle using the stored battle seed. We only need the seed to reproduce the battle from frame 0.

---

## Target Shape

Use a direct standard engine:

```cpp
class BattleRuntimeRandom
{
public:
    explicit BattleRuntimeRandom(unsigned int seed = 1);

    unsigned int seed() const;
    double nextPercent();
    int nextInt(int upperBound);
    bool chance(int chancePct);
    int symmetricInt(int exclusiveBound);

private:
    unsigned int seed_ = 1;
    std::mt19937 rand_;
};
```

Rules:

- No `callCount`.
- No `exportState()`.
- No public restore.
- No `RandomDouble` member.
- No copy/move reconstruction logic. Default compiler behavior is fine if copying is needed in tests; production should pass runtime state by reference.
- `chance(0)` and `chance(100)` may keep the existing no-consume behavior.
- Gameplay rolls stay in `BattleRuntimeState::random`.

---

## File Map

- `src/battle/BattleCore.h`
  - Remove `BattleRuntimeRandomState`.
  - Simplify `BattleRuntimeRandom`.
  - Include `<random>` if not already present.

- `src/battle/BattleCore.cpp`
  - Remove call-count constructor/export/copy/move implementations.
  - Implement seed-only constructor and `seed()`.
  - Keep direct roll methods.

- `src/BattleSceneBattleAdapter.h`
  - Replace `BattleRuntimeRandomState randomState` fields with `unsigned int randomSeed`, if such fields exist.

- `src/BattleSceneBattleAdapter.cpp`
  - Construct `BattleRuntimeRandom(seed)` instead of `BattleRuntimeRandom({ seed, callCount })`.

- `src/BattleSceneHades.h`
- `src/BattleSceneHades.cpp`
  - Store/pass battle seed only.
  - Remove any call-count member/export path added by the previous implementation.

- `src/ChessBattleFlow.h`
- `src/ChessBattleFlow.cpp`
- `src/ChessSelector.h`
- `src/ChessSelector.cpp`
- `src/ChessChallengeFlow.cpp`
  - Keep battle API seed-based.
  - Remove call-count propagation if added.

- `src/GameDataStore.h`
- related game state/save files if touched
  - Remove `battleCallCount` if added.
  - Keep battle seed only if it already belongs to save/retry behavior.

- Tests:
  - `tests/BattleCoreUnitTests.cpp`
  - `tests/BattleInitializationUnitTests.cpp`
  - `tests/BattleComboTriggerSystemUnitTests.cpp`
  - `tests/BattleHitResolverUnitTests.cpp`

---

## Task 1: Replace Call-Count Tests With Seed-Only Replay Tests

**Files:**
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Remove call-count restore test**

Delete or replace:

```cpp
TEST_CASE("BattleRuntimeRandom_RestoresFromSeedAndCallCount", "[battle][random]")
```

with:

```cpp
TEST_CASE("BattleRuntimeRandom_ReplaysFromSeed", "[battle][random]")
{
    KysChess::Battle::BattleRuntimeRandom first(1234u);
    const int firstA = first.nextInt(1000);
    const int firstB = first.nextInt(1000);
    const double firstC = first.nextPercent();

    KysChess::Battle::BattleRuntimeRandom second(1234u);
    CHECK(second.nextInt(1000) == firstA);
    CHECK(second.nextInt(1000) == firstB);
    CHECK(second.nextPercent() == firstC);

    KysChess::Battle::BattleRuntimeRandom different(1235u);
    CHECK(different.nextInt(1000) != firstA);
}
```

- [ ] **Step 2: Replace call-count chance test**

Delete call-count assertions from:

```cpp
BattleRuntimeRandom_ChanceSkipsGuaranteedOutcomesAndCountsRealRolls
```

Replace with behavior-only assertions:

```cpp
TEST_CASE("BattleRuntimeRandom_ChanceHandlesGuaranteedOutcomes", "[battle][random]")
{
    KysChess::Battle::BattleRuntimeRandom random(99u);

    CHECK_FALSE(random.chance(0));
    CHECK(random.chance(100));

    KysChess::Battle::BattleRuntimeRandom first(99u);
    KysChess::Battle::BattleRuntimeRandom second(99u);
    CHECK(first.chance(50) == second.chance(50));
}
```

- [ ] **Step 3: Verify red**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: compile failures because `BattleRuntimeRandom(unsigned int)` does not exist or call-count APIs are still referenced by tests.

---

## Task 2: Simplify `BattleRuntimeRandom`

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Change header type**

In `BattleCore.h`, remove:

```cpp
struct BattleRuntimeRandomState
{
    unsigned int seed = 1;
    std::uint64_t callCount = 0;
};
```

Replace the class with:

```cpp
class BattleRuntimeRandom
{
public:
    explicit BattleRuntimeRandom(unsigned int seed = 1);

    unsigned int seed() const;
    double nextPercent();
    int nextInt(int upperBound);
    bool chance(int chancePct);
    int symmetricInt(int exclusiveBound);

private:
    unsigned int seed_ = 1;
    std::mt19937 rand_;
};
```

Add:

```cpp
#include <random>
```

Remove custom copy/move declarations.

- [ ] **Step 2: Change implementation**

In `BattleCore.cpp`, delete:

```cpp
BattleRuntimeRandom::BattleRuntimeRandom(BattleRuntimeRandomState state)
BattleRuntimeRandom::BattleRuntimeRandom(const BattleRuntimeRandom& other)
BattleRuntimeRandom& BattleRuntimeRandom::operator=(const BattleRuntimeRandom& other)
BattleRuntimeRandom::BattleRuntimeRandom(BattleRuntimeRandom&& other) noexcept
BattleRuntimeRandom& BattleRuntimeRandom::operator=(BattleRuntimeRandom&& other) noexcept
BattleRuntimeRandomState BattleRuntimeRandom::exportState() const
```

Add:

```cpp
BattleRuntimeRandom::BattleRuntimeRandom(unsigned int seed)
    : seed_(seed),
      rand_(seed)
{
}

unsigned int BattleRuntimeRandom::seed() const
{
    return seed_;
}
```

Update roll methods:

```cpp
double BattleRuntimeRandom::nextPercent()
{
    return static_cast<double>(rand_() % 10000u) / 100.0;
}

int BattleRuntimeRandom::nextInt(int upperBound)
{
    assert(upperBound > 0);
    return static_cast<int>(rand_() % static_cast<std::uint32_t>(upperBound));
}
```

Keep `chance()` and `symmetricInt()` behavior.

- [ ] **Step 3: Run focused random tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleRuntimeRandom_ReplaysFromSeed"
x64\Debug\kys_tests.exe "BattleRuntimeRandom_ChanceHandlesGuaranteedOutcomes"
```

---

## Task 3: Remove Call Count From Setup Boundary

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`

- [ ] **Step 1: Replace random state fields**

Search:

```powershell
rg -n "BattleRuntimeRandomState|randomState|callCount|exportState\\(\\)" src\BattleSceneBattleAdapter.* src\BattleSceneHades.* tests\BattleInitializationUnitTests.cpp
```

Where the build/setup context currently stores:

```cpp
Battle::BattleRuntimeRandomState randomState;
```

replace with:

```cpp
unsigned int randomSeed = 1;
```

- [ ] **Step 2: Construct runtime random by seed**

Replace:

```cpp
Battle::BattleRuntimeRandom(context.randomState)
```

or:

```cpp
Battle::BattleRuntimeRandom({ seed, callCount })
```

with:

```cpp
Battle::BattleRuntimeRandom(context.randomSeed)
```

- [ ] **Step 3: Update boundary test**

Replace assertions like:

```cpp
const auto state = created.session.runtime().random.exportState();
CHECK(state.seed == 777u);
CHECK(state.callCount == 2);
```

with:

```cpp
CHECK(created.session.runtime().random.seed() == 777u);
```

- [ ] **Step 4: Run focused initialization test**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleSceneBattleAdapter_InitializesRuntimeRandomFromBuildContext"
```

If that test was named differently during implementation, run the actual random-boundary test name.

---

## Task 4: Remove Call Count From Battle Flow And Save State

**Files:**
- Modify if present: `src/GameDataStore.h`
- Modify if present: `src/GameState.cpp`
- Modify if present: `src/ChessBattleFlow.h`
- Modify if present: `src/ChessBattleFlow.cpp`
- Modify if present: `src/ChessSelector.h`
- Modify if present: `src/ChessSelector.cpp`
- Modify if present: `src/ChessChallengeFlow.cpp`

- [ ] **Step 1: Search flow/save call-count fields**

Run:

```powershell
rg -n "battleCallCount|BattleRuntimeRandomState|callCount|exportState\\(\\)|randomState" src\GameDataStore.* src\GameState.* src\ChessBattleFlow.* src\ChessSelector.* src\ChessChallengeFlow.cpp
```

- [ ] **Step 2: Remove call-count persistence**

Remove any field added only for battle runtime call count:

```cpp
unsigned int battleCallCount = 0;
std::uint64_t battleCallCount = 0;
```

Keep battle seed fields if they are used to restart a battle from frame 0.

- [ ] **Step 3: Keep API seed-based**

If battle flow was changed to accept `BattleRuntimeRandomState`, simplify to:

```cpp
int runBattle(..., int seed = -1, bool countFightsWon = true);
```

or, if unsigned seed is already resolved earlier:

```cpp
int runBattle(..., unsigned int seed, bool countFightsWon = true);
```

Avoid propagating call count through flow layers.

- [ ] **Step 4: Remove final random export**

Delete battle-end code whose only job is:

```cpp
auto finalRandom = battle->battleRuntimeRandomState();
store.battleCallCount = finalRandom.callCount;
```

If the battle seed must be stored when generated, do that at battle start, not by exporting from runtime at the end.

---

## Task 5: Keep Runtime-Owned Gameplay Randomness

**Files:**
- Inspect/modify: `src/BattleSceneHades.cpp`
- Inspect/modify: `src/battle/BattleCore.cpp`
- Inspect/modify: `src/battle/BattleHitResolver.cpp`
- Inspect/modify: `src/battle/BattleComboTriggerSystem.cpp`

- [ ] **Step 1: Keep direct runtime roll APIs**

Do not revert these improvements:

```cpp
state.random.chance(...)
state.random.symmetricInt(...)
BattleHitResolver().resolve(input, state.random)
combo.collectTriggerEvents(..., state.random)
```

- [ ] **Step 2: Verify scene RNG is presentation-only**

Run:

```powershell
rg -n "rand_\\.(rand_int|rand)\\(" src\BattleSceneHades.cpp
```

Expected Hades `rand_` uses are presentation-only, such as:

- camera shake
- blood/effect variant

If setup-facing fallback still uses `rand_`, replace it with deterministic role/source facing or nearest-opponent facing. Do not route it through call-count state.

- [ ] **Step 3: Verify no random callback regression**

Run:

```powershell
rg -n "std::function<double\\(\\)>|percentRolls|takeHitPercentRolls|\\[&\\]\\(\\) \\{ return .*nextPercent" src\battle tests
```

Expected: no gameplay random callback/vector path returns.

---

## Task 6: Remove Call-Count Test Helpers

**Files:**
- Modify: `tests/BattleComboTriggerSystemUnitTests.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: any tests matching search

- [ ] **Step 1: Search test call-count expectations**

Run:

```powershell
rg -n "exportState\\(\\)|callCount|BattleRuntimeRandom\\(\\{[^}]+,[^}]+\\}\\)" tests
```

- [ ] **Step 2: Replace constructors**

Replace:

```cpp
BattleRuntimeRandom({ 1u, 0 })
```

with:

```cpp
BattleRuntimeRandom(1u)
```

- [ ] **Step 3: Delete call-count assertions**

Remove tests that assert exact call count consumption, unless they are directly replaced by observable gameplay behavior.

For random sequence tests, prefer seed helpers:

```cpp
BattleRuntimeRandom randomForChanceSequence(...)
```

can still search seeds by constructing:

```cpp
BattleRuntimeRandom candidate(seed);
```

but should not assert `exportState().callCount`.

---

## Task 7: Final Verification

**Files:**
- All touched files.

- [ ] **Step 1: Run cleanup searches**

Run:

```powershell
rg -n "BattleRuntimeRandomState|callCount|exportState\\(\\)|get_generator\\(\\)\\.discard|RandomDouble rand_" src\battle src tests
rg -n "std::function<double\\(\\)>|percentRolls|takeHitPercentRolls" src\battle tests
rg -n "rand_\\.(rand_int|rand)\\(" src\BattleSceneHades.cpp
```

Expected:

- no runtime call-count machinery
- no random callback/vector gameplay path
- Hades scene random uses are presentation-only

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

1. Replace random tests with seed-only replay tests.
2. Simplify `BattleRuntimeRandom`.
3. Remove call count from adapter/Hades setup boundary.
4. Remove call count from battle flow/save state if it was added.
5. Verify runtime-owned gameplay randomness remains.
6. Remove call-count test helpers.
7. Run cleanup searches and full verification.

---

## Self-Review

- **Spec coverage:** Addresses the revised decision: battle replay is seed-only, not seed plus call count.
- **Preserved improvements:** Keeps runtime-owned gameplay random and direct roll APIs.
- **Subtraction:** Removes `callCount`, `exportState`, custom copy/move reconstruction, `RandomDouble` wrapper, and persistence propagation.
- **Boundary clarity:** Scene `rand_` remains only for presentation noise.
- **No placeholders:** Tasks include concrete file targets, code shapes, commands, and expected outcomes.
