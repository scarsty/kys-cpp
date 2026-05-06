# BattleRuntimeSession Ownership Plan

## Goal

Make one long-lived core object own battle runtime and frame advancement. `BattleSceneHades` must not rebuild gameplay state every frame, must not call `BattleFrameRunner` directly, and must not infer battle facts from legacy `Role*` deltas. The scene initializes a core battle session once, ticks it, applies explicit outputs to legacy/render objects, and renders.

This replaces the previous "import into `battle_runtime_` every frame" plan. A per-frame import method is still too close to the snapshot model and would allow the same failure under a new name.

## Target Boundary

### Core owner

Add a dedicated core class:

```cpp
namespace KysChess::Battle
{

class BattleRuntimeSession
{
public:
    explicit BattleRuntimeSession(BattleRuntimeInit init);

    BattleFrameResult runFrame();

    const BattleRuntimeState& runtime() const;
    BattleRuntimeState& runtimeForTests();

    void enqueueCommand(BattleRuntimeCommand command);

private:
    BattleRuntimeState runtime_;
    BattleFrameRunner runner_;
};

}  // namespace KysChess::Battle
```

Rules:

- `BattleRuntimeSession` owns the only authoritative `BattleRuntimeState`.
- `BattleFrameRunner::runFrame(BattleRuntimeState&)` becomes private core machinery, not a scene API.
- `BattleSceneHades` stores `std::optional<KysChess::Battle::BattleRuntimeSession> battle_session_`.
- `BattleSceneHades` never stores `Battle::BattleRuntimeState battle_runtime_`.
- `BattleSceneHades::backRun1()` never builds gameplay input.

### Scene frame shape

Target `backRun1()`:

```cpp
void BattleSceneHades::backRun1()
{
    tickVisualOnlyTimers();

    if (shouldSkipCoreFrameForPacing())
    {
        playCorePresentationFrame();
        return;
    }

    auto frameResult = battle_session_->runFrame();
    applyCoreFrameResult(frameResult);
    playCorePresentationFrame();
}
```

This is still acceptable during the transition:

```cpp
void BattleSceneHades::backRun1()
{
    tickVisualOnlyTimers();

    auto frameResult = battle_session_->runFrame();
    applyCoreFrameResult(frameResult);
    applyCorePacingEvents(frameResult.pacingEvents);
    playCorePresentationFrame();
}
```

The important point is that `backRun1()` takes no gameplay input and constructs no gameplay snapshot.

## What Gets Imported, And When

Runtime is built once after `battle_roles_` is finalized:

```cpp
void BattleSceneHades::initializeBattleRuntimeSession()
{
    auto init = KysChess::BattleSceneBattleAdapter::makeBattleRuntimeInit(
        battle_roles_,
        KysChess::ChessCombo::getMutableStates(),
        makeBattleRuntimeInitConfig());
    battle_session_.emplace(std::move(init));
}
```

Allowed setup imports:

- `Role*` values converted into unit value state.
- `Magic*`, `Item*`, save/database records converted into skill/item/config tables.
- combo state copied or moved into core-owned combo runtime state.
- static terrain/cells built from `canWalk45()` / `pos45To90()`.
- initial action/cast/movement/status/damage/team/effect/death/result worlds.
- deterministic RNG source/roll stream configuration.

Not allowed after setup:

- rebuilding role snapshots every frame;
- refreshing HP/MP/status/action/movement from `Role*` every frame;
- rebuilding nearest/farthest enemy action imports every frame in the scene;
- rebuilding team/effect/damage/status worlds every frame from `battle_roles_`;
- moving persistent runtime fields out of the session, running a copied runtime, then copying selected fields back.

Explicit post-setup runtime inputs must be commands, not snapshots:

```cpp
enum class BattleRuntimeCommandType
{
    AddUnit,
    RemoveUnit,
    QueueAttackSpawn,
    ForceCast,
    SetPaused,
};

struct BattleRuntimeCommand
{
    BattleRuntimeCommandType type;
    int unitId = -1;
    BattleUnitInit unit;
    BattleAttackSpawnRequest attackSpawn;
    BattleCastRequest cast;
};
```

These commands are for true external boundary events only: delayed enemy admission, scripted spawn, forced cast from an external script, pause/resume. They must not become a per-frame data refresh path.

## Output Boundary

`BattleFrameResult` must carry everything the scene needs to mutate legacy `Role*`, trigger pacing, and enqueue presentation:

```cpp
struct BattleFrameResult
{
    BattlePresentationFrame frame;
    BattleTickResult movement;
    std::vector<BattleAttackEvent> attackEvents;
    std::vector<BattleLegacyUnitWrite> unitWrites;
    std::vector<BattlePacingEvent> pacingEvents;
    std::vector<BattleRuntimeLifecycleEvent> lifecycleEvents;
};
```

`BattlePacingEvent` is core-owned and engine-free:

```cpp
enum class BattlePacingEventType
{
    HitStop,
    DeathPause,
    CameraShake,
    BattleEnded,
};

struct BattlePacingEvent
{
    BattlePacingEventType type;
    int frame = 0;
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int durationFrames = 0;
    int magnitude = 0;
};
```

The scene may translate `BattlePacingEvent` into `slow_`, `Shake`, camera focus, or sound. The scene must not decide that a death means slow frames by inspecting HP/death deltas; the core output says so.

Legacy role apply is output-only:

```cpp
void BattleSceneHades::applyCoreFrameResult(const KysChess::Battle::BattleFrameResult& result)
{
    for (const auto& write : result.unitWrites)
    {
        auto* role = requireBattleRole(write.unitId);
        KysChess::BattleSceneBattleAdapter::applyBattleLegacyUnitWrite(*role, write);
    }
    applyCorePacingEvents(result.pacingEvents);
    presentation_player_.play(result.frame, makePresentationBindings());
}
```

`rolesByBattleId` may exist inside scene/apply helpers, but that is a render/apply lookup. It is not an input to the core runtime.

## Required Deletions

Delete or fully replace:

- `BattleSceneHades::buildBattleFrameInput()` if it contains core pacing decisions; keep only a renamed visual timer helper if needed.
- `BattleSceneHades::buildBattleFrameSceneImport()`.
- `BattleSceneHades::buildCoreRuntimeContext()`.
- `BattleSceneHades::makeCoreFrameState()`.
- `BattleSceneBattleAdapter::BattleFrameSceneImport`.
- `BattleSceneBattleAdapter::BattleFrameApplyContext::state`.
- `BattleSceneHades::battle_runtime_`.
- direct scene calls to `BattleFrameRunner`.
- direct scene reads of runtime internals for applying committed damage/status/team/effect/action/movement results.

## File Plan

### Core files

- Create `src/battle/BattleRuntimeSession.h`.
  - Owns `BattleRuntimeSession`, `BattleRuntimeInit`, `BattleRuntimeCommand`.
- Create `src/battle/BattleRuntimeSession.cpp`.
  - Converts init into `BattleRuntimeState`.
  - Applies queued commands.
  - Calls `BattleFrameRunner::runFrame(runtime_)`.
- Modify `src/battle/BattleCore.h`.
  - Keep `BattleRuntimeState`, `BattleFrameResult`, `BattleFrameRunner`.
  - Add `BattleLegacyUnitWrite`, `BattlePacingEvent`, lifecycle output types.
  - Stop exposing `BattleFrameRunner` as the scene boundary once session is in place.
- Modify `src/battle/BattleCore.cpp`.
  - Move per-frame cleared vectors into `BattleFrameScratch`.
  - Populate `BattleFrameResult` with all scene apply outputs.

### Adapter files

- Modify `src/BattleSceneBattleAdapter.h`.
  - Delete `BattleFrameSceneImport`.
  - Replace `BattleFrameApplyContext` with scene-only lookup if needed:

```cpp
struct BattleFrameApplyContext
{
    std::unordered_map<int, Role*> rolesByBattleId;
};
```

  - Add `makeBattleRuntimeInit(...)`.
  - Add `applyBattleLegacyUnitWrite(Role&, const Battle::BattleLegacyUnitWrite&)`.
- Modify `src/BattleSceneBattleAdapter.cpp`.
  - Move current setup conversion into `makeBattleRuntimeInit`.
  - Move current apply writes into `applyBattleLegacyUnitWrite`.
  - Keep `Role*`, `Magic*`, `Item*` here only as import/apply boundary objects.

### Scene files

- Modify `src/BattleSceneHades.h`.
  - Replace `Battle::BattleRuntimeState battle_runtime_` with `std::optional<Battle::BattleRuntimeSession> battle_session_`.
  - Declare `initializeBattleRuntimeSession()`.
  - Delete frame import/build declarations.
- Modify `src/BattleSceneHades.cpp`.
  - Call `initializeBattleRuntimeSession()` after `battle_roles_` is finalized.
  - Change `backRun1()` to call `battle_session_->runFrame()` with no gameplay input.
  - Apply only `BattleFrameResult` outputs.

## Task Sequence

### Task 1: Introduce Session Without Scene Usage

- Add `BattleRuntimeSession`.
- Add tests that prove the session owns state across two `runFrame()` calls:
  - pending attack spawns are consumed into active attacks;
  - active attacks continue advancing on the second frame;
  - battle end emits once.
- No scene files should call the session yet.

Acceptance:

```powershell
rg -n "class BattleRuntimeSession|BattleRuntimeSession::runFrame" src\battle
x64\Debug\kys_tests.exe "[battle][runtime_session]"
```

### Task 2: Build Runtime Once From Scene Setup

- Add `BattleSceneHades::initializeBattleRuntimeSession()`.
- Move initial static terrain, role, combo, team, effect, status, damage, movement, action, hit, rescue config imports into `makeBattleRuntimeInit`.
- Call initialization after setup, not inside `backRun1()`.

Acceptance:

```powershell
rg -n "initializeBattleRuntimeSession" src\BattleSceneHades.cpp src\BattleSceneHades.h
rg -n "makeBattleRuntimeInit" src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
```

### Task 3: Route Frame Advance Through Session

- Replace `BattleSceneHades::battle_runtime_` with `battle_session_`.
- Replace direct scene runner call with `battle_session_->runFrame()`.
- Delete copy-run-copy-back patterns.

Acceptance:

```powershell
rg -n "BattleFrameRunner\\(\\)\\.runFrame|BattleFrameRunner\\(\\)\\.advanceFrame" src\BattleSceneHades.cpp src\BattleSceneHades.h
rg -n "battle_runtime_" src\BattleSceneHades.cpp src\BattleSceneHades.h
rg -n "std::move\\(battle_runtime_|Battle::BattleRuntimeState state" src\BattleSceneHades.cpp src\BattleSceneBattleAdapter.h
```

Expected: no matches.

### Task 4: Delete Per-Frame Scene Import

- Delete `BattleFrameSceneImport`.
- Delete `buildBattleFrameSceneImport()`.
- Delete `buildCoreRuntimeContext()`.
- Delete `makeCoreFrameState()`.
- Move any remaining real external input into explicit `BattleRuntimeCommand`.

Acceptance:

```powershell
rg -n "BattleFrameSceneImport|buildBattleFrameSceneImport|buildCoreRuntimeContext|makeCoreFrameState|context\\.snapshot|requireLegacyRoleSnapshot" src
```

Expected: no matches.

### Task 5: Make Pacing And Apply Output-Driven

- Add `BattlePacingEvent`.
- Emit death pause, hit stop, camera shake, and battle-ended pacing from core phases.
- Add `BattleLegacyUnitWrite`.
- Make scene apply only `BattleFrameResult::unitWrites`, `pacingEvents`, and presentation events.
- Delete scene-side log/death/slow/shake reconstruction.

Acceptance:

```powershell
rg -n "slow_|Shake|checkResult|Dead|HP" src\BattleSceneHades.cpp
```

Review expected matches manually. Remaining matches must be rendering, setup, or legacy output apply only.

### Task 6: Split Runtime State From Frame Scratch

- Add private `BattleFrameScratch`.
- Move per-frame input/output vectors out of `BattleRuntimeState`.
- Keep persistent world/value/runtime state in `BattleRuntimeState`.
- `BattleRuntimeSession::runFrame()` should produce `BattleFrameResult` without exposing scratch.

Acceptance:

```powershell
rg -n "committedResults|pendingPresentation|percentRolls|nextPercentRoll|applications" src\battle\BattleCore.h
```

Expected: names appear in `BattleFrameScratch` or `BattleFrameResult`, not as persistent runtime owner fields unless there is a documented persistent reason.

## Final Verification

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
rg -n "BattleFrameSceneImport|buildBattleFrameSceneImport|buildCoreRuntimeContext|makeCoreFrameState|context\\.snapshot|requireLegacyRoleSnapshot" src
rg -n "BattleFrameRunner\\(\\)\\.(runFrame|advanceFrame)|battle_runtime_" src\BattleSceneHades.cpp src\BattleSceneHades.h
rg -n "Battle::BattleRuntimeState state|std::move\\(battle_runtime_" src
rg -n "Role\\*|Magic\\*|Item\\*|Engine|TextureManager|Font|Scene" src\battle
```

Expected:

- no old scene snapshot/build path;
- no scene-owned `battle_runtime_`;
- no direct scene `BattleFrameRunner` call;
- no runtime copy-run-copy-back path;
- no legacy scene/render dependencies inside `src\battle`;
- tests and both builds pass.

## Definition Of Done

The refactor is not complete because class names changed. It is complete only when there is exactly one battle runtime owner, `BattleRuntimeSession`, and `BattleSceneHades::backRun1()` cannot reconstruct a frame even accidentally. Any data entering runtime after setup must be an explicit command with a narrow reason. Any data leaving runtime must be an explicit frame output.
