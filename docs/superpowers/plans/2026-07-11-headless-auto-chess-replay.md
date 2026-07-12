# Headless Auto-Chess and Verifiable Replay Implementation Plan

> **Execution gate:** Task 0 is complete and the architecture contract is approved. Production implementation begins at Task 1 and proceeds one task at a time, with focused tests and a build checkpoint after every non-trivial increment.

**Goal:** Make auto-chess fully playable without graphical systems, route GUI and headless play through one deterministic session, and produce replays that a fresh explicit verifier can reproduce from exact-version immutable content, the original root seed, and the complete accepted action history from action 1.

**Architecture:** An injected immutable `ChessGameContent` supplies all gameplay definitions. An instance-owned `ChessGameSession` owns run state, named RNG streams, phase/pending-decision state, prepared battles, and the active replay journal. GUI, CLI, verifier, save/checkpoint, and MCP adapters submit the same typed actions. A shared battle planner, setup factory, runtime termination policy, semantic report collector, and battle runner prevent GUI/headless rule duplication.

**Tech stack:** C++23, Catch2, Glaze JSON, yaml-cpp, SQLite, picosha2, libzip, MSBuild solution projects, CMake, and a thin Python MCP adapter only after the JSONL protocol is stable.

---

## Contract Source of Truth

The authoritative architecture and behavioral contract is the [Headless Auto-Chess and Verifiable Replay Design](../specs/2026-07-11-headless-auto-chess-replay-design.md). Task 0 incorporated the reviewed corrections into that specification on 2026-07-11.

This document is the single mutable execution ledger:

- behavior, architecture, determinism, replay semantics, or compatibility changes update the design first and then affected tasks here;
- file ownership, sequencing, tests, build commands, and completion status update this plan only;
- no additional per-task plans are needed unless a genuinely independent follow-on feature is split from this scope.

---

### Task 0: Amend and approve the design contract

**Files:**
- Modify: `docs/superpowers/specs/2026-07-11-headless-auto-chess-replay-design.md`
- Reference: this plan

- [x] Replace the original `Complete`, `apply/runAutomatic`, RNG retry, timeout, compatibility, observation, and action-vocabulary sections with the reviewed contract.
- [x] Add the exact canonical byte layout, replay partial-footer behavior, shared timeout ownership, exact `game_version` compatibility, and trusted snapshot-only normal-load contract.
- [x] Align JSONL and MCP operation lists, including `export_save` and `import_save`.
- [x] Confirm that post-clear challenges remain available until `finish_run`.
- [x] Confirm the revised design and this task sequence agree before production code begins.

### Task 1: Deterministic RNG, canonical writer, and hash primitives

**Files:**
- Create: `src/ChessRunRandom.h`
- Create: `src/ChessRunRandom.cpp`
- Create: `src/ChessCanonicalEncoding.h`
- Create: `src/ChessCanonicalEncoding.cpp`
- Create: `src/ChessReplayHash.h`
- Create: `src/ChessReplayHash.cpp`
- Modify: `src/ChessRandom.h`, `src/ChessRandom.cpp` and then delete them after all callers migrate
- Modify: `src/battle/BattleRuntimeRandom.h`
- Modify: `src/battle/BattleRuntimeRandom.cpp`
- Create: `tests/ChessRunRandomUnitTests.cpp`
- Create: `tests/ChessCanonicalEncodingUnitTests.cpp`
- Create: `tests/BattleRuntimeRandomUnitTests.cpp`
- Modify project/source lists used by MSBuild and CMake

- [x] Write fixed SplitMix64, xoshiro256**, stream-derivation, bounded-sampling, raw-counter, checkpoint/restore, reroll-family reset, and stream-isolation golden tests.
- [x] Write fixed battle-runtime RNG vectors for seed, `nextInt`, `chance`, and `symmetricInt`.
- [x] Implement the versioned generators without standard-library distributions.
- [x] Implement the canonical writer and SHA-256 helpers with exact byte-vector goldens.
- [x] Add chain-hash golden tests and malformed canonical-value assertions.
- [x] Build `kys_tests`, run focused determinism tests in Debug, then run the same goldens in Release.

### Task 2: Immutable injected content and game-version foundation

**Files:**
- Create: `src/ChessGameContent.h`
- Create: `src/ChessGameContent.cpp`
- Create: `src/ChessContentLoader.h`
- Create: `src/ChessContentLoader.cpp`
- Create: `src/ChessDiagnostics.h`
- Create: `src/GameVersion.h`
- Create: `src/GameVersion.cpp`
- Modify: `src/ChessBalance.h`, `src/ChessBalance.cpp`
- Modify: `src/ChessCombo.h`, `src/ChessCombo.cpp`
- Modify: `src/ChessEquipment.h`, `src/ChessEquipment.cpp`
- Modify: `src/ChessNeigong.h`, `src/ChessNeigong.cpp`
- Modify: `src/ChessBattleEffects.h`, `src/ChessBattleEffects.cpp`
- Modify: `src/ChessPool.h`, `src/ChessPool.cpp`
- Modify: `src/ChessRoleSave.h`, `src/ChessRoleSave.cpp`
- Modify: `src/BattleMap.h`, `src/BattleMap.cpp`
- Modify: `src/battle/BattleRuntimeSession.h`, `src/battle/BattleRuntimeSession.cpp`
- Create: `tests/ChessGameContentUnitTests.cpp`

- [x] Load top-level chess configs, role/magic/item records, dynamic map definitions, and battlefield terrain into one immutable ID-keyed snapshot from an explicit data root.
- [x] Replace `Font` conversion, `GameUtil::PATH()`, `Save::getInstance()`, and mutable static lazy caches in headless gameplay code with injected content and diagnostics.
- [x] Move tier colors, item display lookup, and other presentation helpers out of core definitions.
- [x] Make `BattleRuntimeSessionCreationInput` carry combo, equipment, synergy, internal-skill, and magic-effect definitions; remove runtime global lookups.
- [x] Load the exact game version from `config/release.ini`, with `dev` as the development fallback, and carry it in immutable content.
- [x] Test two different difficulty/content snapshots in one process, exact game-version loading/fallback, shuffled logical input ordering, and diagnostics routed away from stdout.

### Task 3: Establish structural headless build targets

**Files:**
- Create: `src/kys_battle_core.vcxproj`
- Create: `src/kys_battle_core.vcxproj.filters`
- Create: `src/kys_chess_core.vcxproj`
- Create: `src/kys_chess_core.vcxproj.filters`
- Modify: `kys.sln`
- Modify: `src/kys.vcxproj`
- Modify: `tests/kys_tests.vcxproj`
- Modify: `src/CMakeLists.txt`
- Modify: `cmake/KysSources.cmake`
- Create: `tests/ChessCoreDependencyUnitTests.cpp`

- [x] Move battle runtime sources into `kys_battle_core`.
- [x] Move content, deterministic RNG, canonical encoding, run-state, and later session-domain sources into `kys_chess_core`, which references `kys_battle_core`.
- [x] Remove the same source files from the GUI executable and direct test compilation lists; GUI and tests link the libraries instead.
- [x] Compile both core libraries with strict floating-point options; no separate persisted determinism profile is used.
- [x] Add a dependency scan/test proving core headers and sources do not include or link `Engine`, `Audio`, `Font`, `RunNode`, `SystemSettings`, ImGui, SDL renderer, or SDL mixer presentation code.
- [x] Build `kys` and `kys_tests` through `.github/build-command.ps1`.

### Task 4: Instance-owned state, options, and stable configured IDs

**Files:**
- Modify: `src/GameDataStore.h`, `src/GameDataStore.cpp`
- Modify: `src/GameState.h`, `src/GameState.cpp`
- Modify: `src/ChessProgress.h`, `src/ChessProgress.cpp`
- Modify: `src/Chess.h`
- Modify: `src/ChessRoster.h`, `src/ChessRoster.cpp`
- Modify: `src/ChessEquipmentInventory.h`, `src/ChessEquipmentInventory.cpp`
- Modify: `src/ChessShop.h`, `src/ChessShop.cpp`
- Modify: `src/ChessPool.h`, `src/ChessPool.cpp`
- Modify: `src/ChessBalance.h`, `src/ChessBalance.cpp`
- Modify: `config/chess_challenge.yaml`
- Create: `src/ChessApplicationSessionHost.h`
- Create: `src/ChessApplicationSessionHost.cpp`
- Modify the 22 current `GameState::get()` call sites in GUI/bootstrap code
- Create: `tests/GameStateInstanceUnitTests.cpp`

- [x] Make `GameState` publicly constructible from immutable content, session options, root seed/RNG state, and an optional stored state; remove `GameState::get()`.
- [x] Store gameplay-affecting position-swap policy in session state/options, not `SystemSettings`.
- [x] Replace old shop/enemy seed fields with versioned root/stream state and counters.
- [x] Add challenge/reward IDs and store completed challenge IDs rather than vector indexes.
- [x] Keep any GUI-global access in the application adapter host only; session/domain code receives explicit references.
- [x] Test two independent sessions with different difficulties and seeds in one process without cross-mutation.
- [x] Use exact `game_version` compatibility and do not add save migration or backward-compatibility branches.

### Task 5: Session types, first management vertical slice, and journal-from-action-one

**Files:**
- Create: `src/ChessSessionTypes.h`
- Create: `src/ChessGameSession.h`
- Create: `src/ChessGameSession.cpp`
- Create: `src/ChessManagementRules.h`
- Create: `src/ChessManagementRules.cpp`
- Create: `src/ChessReplayTypes.h`
- Create: `src/ChessReplayJournal.h`
- Create: `src/ChessReplayJournal.cpp`
- Modify: `src/ChessManager.h`, `src/ChessManager.cpp`
- Modify: `src/ChessEconomy.h`, `src/ChessEconomy.cpp`
- Modify: `src/ChessShop.h`, `src/ChessShop.cpp`
- Modify: `src/ChessPool.h`, `src/ChessPool.cpp`
- Modify: `src/ChessRoster.h`, `src/ChessRoster.cpp`
- Create: `tests/ChessGameSessionUnitTests.cpp`
- Create: `tests/ChessManagementRulesUnitTests.cpp`
- Create: `tests/ChessReplayJournalUnitTests.cpp`

- [x] Define phase, typed action, legal descriptor, stable error code, gameplay observation, semantic event, transition, and journal record types.
- [x] Implement seeded new game plus pure observation/legal-action enumeration.
- [x] Implement shop refresh/lock/purchase, sell, buy experience, and atomic deployment through the session.
- [x] Move seen-role mutation into shop generation; observing/opening a menu must be pure.
- [x] Finalize every accepted non-battle action at the next boundary and record pre/post/event/RNG/chain hashes from the first accepted action.
- [x] Test valid mutations, all boundary errors, merge/bench/deploy constraints, stale identifiers, no-mutation rejection, observation purity, and same-seed/action parity.

### Task 6: Migrate GUI management adapters and remaining management actions

**Files:**
- Modify: `src/ChessSelector.h`, `src/ChessSelector.cpp`
- Modify: `src/ChessShopFlow.h`, `src/ChessShopFlow.cpp`
- Modify: `src/ChessEquipmentFlow.h`, `src/ChessEquipmentFlow.cpp`
- Modify: `src/ChessInfoFlow.h`, `src/ChessInfoFlow.cpp`
- Modify: `src/ChessUiCommon.h`, `src/ChessUiCommon.cpp`
- Modify: `src/ChessUIStatus.cpp`
- Modify: `src/ChessDetailPanels.cpp`
- Modify: `src/ChessModHook.h`, `src/ChessModHook.cpp`
- Modify: `src/MainScene.cpp`, `src/SubScene.cpp`, `src/Event.cpp`, `src/Console.cpp`
- Modify: `tests/ChessShopFlowUnitTests.cpp`
- Create: `tests/ChessGuiSessionParityUnitTests.cpp`

- [x] Make GUI menus render session observations/legal descriptors and submit typed actions.
- [x] Add ban, forced-ban skip, legendary purchase, equipment assignment, position-swap option, and deterministic enemy-reroll actions.
- [x] Preserve current monotonic ban behavior and stable equipment/chess instance identities.
- [x] Add parity tests that compare the GUI adapter’s submitted actions and resulting session hashes to direct session calls.
- [x] Delete each old UI-owned mutation path immediately after its parity test passes.
- [x] Verify no gameplay flow calls `GameState::get()`, `SystemSettings` gameplay options, or `std::random_device`.

### Task 7: Immutable battle planner and preparation RNG checkpoint

**Files:**
- Create: `src/PreparedChessBattle.h`
- Create: `src/ChessBattlePlanner.h`
- Create: `src/ChessBattlePlanner.cpp`
- Create: `src/ChessBattleMapCatalog.h`
- Create: `src/ChessBattleMapCatalog.cpp`
- Modify: `src/ChessBattleFlow.h`, `src/ChessBattleFlow.cpp`
- Modify: `src/DynamicChessMap.h`, `src/DynamicChessMap.cpp`
- Modify: `src/ChessGameSession.h`, `src/ChessGameSession.cpp`
- Create: `tests/ChessBattlePlannerUnitTests.cpp`

- [x] Move enemy-lineup candidates, synergy scoring, enemy equipment, map candidates, battle seed, and challenge preparation out of `ChessBattleFlow`.
- [x] Split the pure map catalog from graphical scene creation; never mutate shared `BattleInfo`.
- [x] Capture the complete preparation-stream checkpoint before any preparation draw.
- [x] Make prepared battle data immutable except for chosen map and a separate formation overlay.
- [x] Include all prepared data and the RNG checkpoint in canonical state/checkpoints.
- [x] Add total tie-breakers to enemy equipment and every planner sort.
- [x] Test one-time consumption, observation purity, deterministic reroll rekeying, map choice legality, and every rollback-matrix row.

### Task 8: Pure battlefield data and shared setup factory

**Files:**
- Create: `src/BattlefieldData.h`
- Create: `src/BattlefieldData.cpp`
- Rename/replace: `src/BattleSceneSetupBuilder.h` -> `src/BattleSetupFactory.h`
- Rename/replace: `src/BattleSceneSetupBuilder.cpp` -> `src/BattleSetupFactory.cpp`
- Modify: `src/BattleSceneMapState.h`, `src/BattleSceneMapState.cpp`
- Modify: `src/BattleSceneHades.h`, `src/BattleSceneHades.cpp`
- Modify: `src/battle/BattleRuntimeSession.h`, `src/battle/BattleRuntimeSession.cpp`
- Rename/extend: `tests/BattleSceneSetupBuilderUnitTests.cpp` -> `tests/BattleSetupFactoryUnitTests.cpp`
- Create: `tests/BattlefieldDataUnitTests.cpp`

- [x] Split terrain cells, walkability, grid conversion, and board bounds from texture construction.
- [x] Extend the existing setup builder rather than copying it: copy all role, skill-ID, equipment, synergy, internal-skill, terrain, and runtime-rule facts into the creation input.
- [x] Assign deterministic unit IDs with complete tie-breakers.
- [x] Apply formation swaps to prepared input before `BattleRuntimeSession::createInitialized()`; do not create clones and then move only base units.
- [x] Ensure setup never mutates shared `Role`, `Magic`, or content objects.
- [x] Test byte-identical creation input from identical prepared data and valid swaps only before frame 0.

### Task 9: Semantic events, shared termination, report collector, and headless runner

**Files:**
- Create: `src/battle/BattleOutcome.h`
- Create: `src/battle/BattleDigestEvent.h`
- Modify: `src/battle/BattlePresentation.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleRuntimeRules.h`, `src/battle/BattleRuntimeRules.cpp`
- Modify: `src/battle/BattleRuntimeSession.h`, `src/battle/BattleRuntimeSession.cpp`
- Modify: `src/battle/BattleCastSystem.cpp`
- Create: `src/BattleReportCollector.h`
- Create: `src/BattleReportCollector.cpp`
- Create: `src/BattleSummaryBuilder.h`
- Create: `src/BattleSummaryBuilder.cpp`
- Create: `src/HeadlessBattleRunner.h`
- Create: `src/HeadlessBattleRunner.cpp`
- Modify: `src/BattleReport.h`, `src/BattleReport.cpp`
- Modify: `src/BattleSceneFrameApplier.h`, `src/BattleSceneFrameApplier.cpp`
- Create: `tests/BattleReportCollectorUnitTests.cpp`
- Create: `tests/HeadlessBattleRunnerUnitTests.cpp`
- Create: `tests/BattleDeterminismGoldenUnitTests.cpp`

- [x] Add structured skill/status/resource IDs so report and digest code never depends on localized names.
- [x] Add total target tie-breakers, including the equal-angle/equal-distance case in `BattleCastSystem`.
- [x] Enforce defeat, simultaneous wipe, and the 36,000-frame timeout inside the shared runtime policy used by all callers.
- [x] Extract `BattleSceneFrameApplier::recordLog()` into a pure collector; make the frame applier presentation-only.
- [x] Move post-battle survivor/stat summary projection beside the collector.
- [x] Implement headless drain with initialization events, semantic events, report, summary, final runtime state, and digest.
- [x] Test one timeout event, exact off-by-one frame, visual/text exclusion, semantic digest goldens, and Debug/Release equality.

### Task 10: Make the graphical battle consume the shared planner/setup/collector path

**Files:**
- Modify: `src/BattleSceneHades.h`, `src/BattleSceneHades.cpp`
- Modify: `src/DynamicChessMap.h`, `src/DynamicChessMap.cpp`
- Modify: `src/ChessBattleFlow.h`, `src/ChessBattleFlow.cpp`
- Modify: `src/BattleSceneUnitStore.h`, `src/BattleSceneUnitStore.cpp`
- Modify: `tests/BattleSceneFrameApplierUnitTests.cpp`
- Create: `tests/ChessBattleGuiHeadlessParityUnitTests.cpp`

- [x] Construct the GUI scene from `PreparedChessBattle` plus `BattleSetupFactory`.
- [x] Feed initialization and runtime frames to the shared report collector before presentation effects.
- [x] Keep GUI battle progression incremental through the pending session transition.
- [x] Compare GUI and headless result, end frame, semantic events, report, survivor state, and digest for identical prepared input and swaps.
- [x] Delete `BattleSceneHades::onEntrance()` rule/setup duplication and old `DynamicChessMap::createBattle()` mutation paths after parity passes.

### Task 11: Progression, rewards, challenges, and post-clear management

**Files:**
- Create: `src/ChessProgressionRules.h`
- Create: `src/ChessProgressionRules.cpp`
- Create: `src/ChessRewardRules.h`
- Create: `src/ChessRewardRules.cpp`
- Modify: `src/ChessGameSession.h`, `src/ChessGameSession.cpp`
- Modify: `src/ChessBattleFlow.h`, `src/ChessBattleFlow.cpp`
- Modify: `src/ChessRewardFlow.h`, `src/ChessRewardFlow.cpp`
- Modify: `src/ChessChallengeFlow.h`, `src/ChessChallengeFlow.cpp`
- Modify: `src/ChessChallengeMenu.h`, `src/ChessChallengeMenu.cpp`
- Modify: `src/ChessShopFlow.h`, `src/ChessShopFlow.cpp`
- Create: `tests/ChessProgressionRulesUnitTests.cpp`
- Create: `tests/ChessRewardRulesUnitTests.cpp`
- Create: `tests/ChessScriptedRunUnitTests.cpp`

- [x] Move victory economy, interest, experience, shop refresh, fight advance, fights-won, boss rewards, equipment rewards, internal-skill rewards, forced bans, challenge rewards, and repeat-challenge behavior into session rules.
- [x] Represent reward rerolls and nested reward targets as successive explicit `RewardChoice` boundaries.
- [x] Derive survivors and fights-won from the runtime summary; do not reset mutable `Role::HP/MP` before survivor-dependent rules.
- [x] Apply the frozen timeout/defeat/repeat rollback matrix centrally.
- [x] Keep `Management` available with `campaignComplete=true`; implement `finish_run`.
- [x] Add a small synthetic-content complete run and an actual-config smoke run with final roster/equipment/hash assertions.
- [x] Prove no GUI flow performs a remaining rule mutation.

### Task 12: Structured and Traditional-Chinese text presentation

**Files:**
- Create: `src/ChessObservationText.h`
- Create: `src/ChessObservationText.cpp`
- Create: `src/ChessAsciiBoard.h`
- Create: `src/ChessAsciiBoard.cpp`
- Create: `src/ChessBattleText.h`
- Create: `src/ChessBattleText.cpp`
- Modify/reuse: `src/BattleLogPresenter.h`, `src/BattleLogPresenter.cpp`
- Create: `tests/ChessObservationTextUnitTests.cpp`
- Create: `tests/ChessAsciiBoardUnitTests.cpp`
- Create: `tests/ChessBattleTextUnitTests.cpp`

- [x] Format management state, parameterized legal actions, pending rewards, post-clear status, and session-operation consequences in Traditional Chinese.
- [x] Render the initial board from the same battlefield data and prepared formation, cropped with preserved coordinates.
- [x] Assign stable A/E tokens for the battle duration and include the semantic token legend.
- [x] Format compact logs from structured semantic events; keep trace-only movement separate.
- [x] Add golden text tests without using display strings in replay hashes.

### Task 13: CLI, JSONL protocol, and build integration

**Files:**
- Create: `cli/ChessCliMain.cpp`
- Create: `src/ChessJsonProtocol.h`
- Create: `src/ChessJsonProtocol.cpp`
- Create: `src/ChessCliController.h`
- Create: `src/ChessCliController.cpp`
- Create: `src/kys_chess_cli.vcxproj`
- Create: `src/kys_chess_cli.vcxproj.filters`
- Modify: `kys.sln`
- Modify: `src/CMakeLists.txt`
- Modify: `.github/build-command.ps1`
- Modify: `tests/kys_tests.vcxproj`
- Create: `tests/ChessJsonProtocolUnitTests.cpp`
- Create: `tests/test_kys_chess_cli.py`

- [x] Add interactive, compact, JSONL, and trace modes with explicit seed and data-root options.
- [x] Implement all gameplay and session operations, caller request IDs, stale/illegal-action errors, and timeline-replacement responses.
- [x] Keep stdout protocol-clean under successful loads, warnings, malformed config, battle execution, and verification; route diagnostics to stderr.
- [x] Resolve default data paths relative to the executable, never the process working directory.
- [x] Link only `kys_chess_core`, `kys_battle_core`, and headless dependencies; do not link GUI/audio/render projects.
- [x] Add the CLI project to the solution and the default build targets.
- [x] Run subprocess tests proving no window/audio initialization, request-ID preservation, REPL/JSONL action parity, and clean shutdown.

### Task 14: Replay JSONL format and fresh-session verifier

**Files:**
- Create: `src/ChessReplayJson.h`
- Create: `src/ChessReplayJson.cpp`
- Create: `src/ChessReplayVerifier.h`
- Create: `src/ChessReplayVerifier.cpp`
- Create: `tests/ChessReplayJsonUnitTests.cpp`
- Create: `tests/ChessReplayVerifierUnitTests.cpp`
- Create: `tests/data/replays/short_v1.jsonl`
- Create: `tests/data/replays/complete_v1.jsonl`

- [x] Serialize the exact `game_version`, difficulty, root seed, options, decision records, and in-progress/complete footer with fixed-width seed/hash strings.
- [x] Rebuild a fresh session from exact-version immutable content, difficulty, options, and the original root seed.
- [x] Verify pre-state, legal descriptor, action, automatic transition, event/RNG/post/chain hashes, and footer in order.
- [x] Report exact sequence and mismatch category.
- [x] Test modified, removed, inserted, duplicated, reordered, truncated, malformed, wrong-game-version, wrong-runtime-option, RNG-divergent, event-divergent, state-divergent, and chain-divergent replays.
- [x] Require the same committed golden replays to pass Debug and Release.

### Task 15: Replay archive packaging

**Files:**
- Create: `src/ChessReplayArchive.h`
- Create: `src/ChessReplayArchive.cpp`
- Create: `tests/ChessReplayArchiveUnitTests.cpp`
- Modify CLI export/verify commands

- [x] Package authoritative `replay.jsonl`, optional `summary.txt`, and optional diagnostics with libzip.
- [x] Ignore non-authoritative files during verification.
- [x] Test archive round-trip, missing/duplicate authoritative entry, corrupt zip, and diagnostic tampering.
- [x] Do not change canonical hashes when switching between plain JSONL and archive transport.

### Task 16: Self-contained checkpoints, save slots, and timeline replacement

**Files:**
- Create: `src/ChessSessionCheckpoint.h`
- Create: `src/ChessSessionCheckpoint.cpp`
- Create: `src/ChessSaveStore.h`
- Create: `src/ChessSaveStore.cpp`
- Modify: `src/Save.h`, `src/Save.cpp`
- Modify: `src/UISave.h`, `src/UISave.cpp`
- Modify: `src/ChessModHook.h`, `src/ChessModHook.cpp`
- Modify: `src/ChessGameSession.h`, `src/ChessGameSession.cpp`
- Create: `tests/ChessSessionCheckpointUnitTests.cpp`
- Create: `tests/ChessSaveStoreUnitTests.cpp`

- [x] Move the private `SavePersistence::SlotData` concept into a public headless checkpoint/store abstraction.
- [x] Save only at stable boundaries and embed exact `game_version`, the complete active journal from action 1, state snapshot/hash, complete RNG state, phase/pending data, and monotonic save revision.
- [x] Restore the trusted snapshot directly during normal load with no historical replay or load-time state/event/RNG/footer/chain validation.
- [x] Keep the journal complete from action 1 with no compaction, truncation, or snapshot-origin branch.
- [x] Make inspect/save consume no RNG and make malformed, incompatible-version, unrepresentable, or content-load failure transactional.
- [x] On load, replace state, RNG, phase, pending data, and journal prefix while preserving the external slot catalog; return discarded-action count.
- [x] Keep save metadata outside gameplay hashes and expose `SessionObservation{gameplay, saveSlots, operations}`.
- [x] Import portable saves using syntax/readability plus exact game-version checks only, without activation or replay verification.
- [x] Test direct snapshot restoration, accepted snapshot edits, structurally readable journal corruption without implicit verification, explicit audit divergence, every supported boundary, copied self-contained checkpoints, rollback/new suffix, and catalog preservation.

### Task 17: Thin MCP adapter after protocol stability

**Files:**
- Create: `tools/kys_chess_mcp/pyproject.toml`
- Create: `tools/kys_chess_mcp/kys_chess_mcp/server.py`
- Create: `tests/test_kys_chess_mcp.py`
- Create: `docs/headless-auto-chess-agent-workflow.md`

- [x] Spawn and own one CLI JSONL session.
- [x] Expose new/observe/legal/take-action, list/inspect/save/load/export/import save, and export/verify replay tools.
- [x] Explain rollback and discarded suffixes in tool descriptions.
- [x] Compare every MCP response to the equivalent direct JSONL response.
- [x] Keep all gameplay rules in the C++ session; the adapter performs transport and process lifecycle only.

### Task 18: Final verification and deletion audit

**Files:**
- Modify only if verification exposes a defect.

- [x] Run every new focused Catch2 group.
- [x] Run the complete Debug `kys_tests` suite.
- [x] Run the complete Release `kys_tests` suite against the final source state.
- [x] Run `tests/test_kys_chess_cli.py` and `tests/test_kys_chess_mcp.py`.
- [x] Run `powershell -ExecutionPolicy Bypass -File ./.github/build-command.ps1`; only the documented `kys.exe` file-lock final-link failure is acceptable. `kys_tests` and `kys_chess_cli` must link.
- [x] Re-run source and dependency scans against the final source state, proving there is no `GameState::get()`, replay-sensitive `std::random_device`, headless `GameUtil::PATH()`, GUI-owned gameplay mutation, core presentation dependency, or duplicate old rule path.
- [x] Verify the CLI opens no window/audio device and produces protocol-clean stdout.
- [x] Rebuild and test the headless and default CMake presets against the final source state.
- [x] Verify normal save load directly restores a trusted snapshot, retains the full journal from action 1, and performs no historical replay or load-time hash validation.
- [x] Re-read every acceptance criterion after the Release/CMake gates and record automated coverage plus any platform-specific smoke tests.

### Verification ledger (as of 2026-07-12)

- Debug `.github/build-command.ps1` linked `kys`, `kys_tests`, and `kys_chess_cli`; the complete Debug suite passed **8,875 assertions in 699 test cases**.
- Release `.github/build-command.ps1 -Configuration Release` linked `kys`, `kys_tests`, and `kys_chess_cli`; the complete Release suite passed **8,875 assertions in 699 test cases**.
- The final acceptance-gap tests passed: GUI/JSONL replay parity, 49 assertions; post-clear challenge legality, 15 assertions; JSON observation rollback visibility, 22 assertions.
- The follow-up graphical lifecycle regressions passed focused coverage: automatic prebattle-summary preload, 16 assertions; post-battle completion/abort handling, 24 assertions; enclosing run-owner exit propagation, 2 assertions. The combined lifecycle group passed **42 assertions in 3 test cases**.
- The final GUI parity follow-up passed **33 assertions in 5 test cases**, covering legacy menu presentations and reward overrides, prefix/` [已裝]` spacing, challenge browsing independent of deployment legality, session-derived star-up presentation without the extra combo panel, and integer-grid-average initial camera conversion.
- The final trusted-load boundary audit proves a syntactically valid nonstandard `battle_frame_limit` parses, imports, and loads normally, while explicit verification alone rejects it as a header mismatch at sequence 0.
- The final dependency/source-scan group passed **820 assertions in 8 test cases**.
- The default graphical Debug CMake preset built the final GUI sources, and default CTest passed **699/699** tests.
- A fresh headless CMake configure passed; the clean headless build linked `kys_battle_core`, `kys_chess_core`, and `kys_chess_cli`. Headless CTest reported zero tests as expected because `BUILD_TESTING=OFF`.
- Python coverage passed CLI **5/5** and MCP **3/3** tests. The previously verified battle-map SMAP packer coverage remains **2/2**.
- MSBuild and CMake source manifests are equivalent: chess core **41/41** exact source entries and battle core **22/22** `src/battle/*.cpp` sources.
- `git diff --check` is clean.
- The graphical game was not launched. Static code-path comparison and automated tests are complete, but live graphical presentation remains a user-owned smoke test.

### Final source and dependency audit

- Graphical adapters submit `ChessAction` values and consume session transitions/observations. `graphical adapters do not call gameplay mutation owners` found no direct GUI-owned rule mutation.
- `kys_chess_core` and `kys_battle_core` have strict floating-point settings and no active presentation includes or links. The CLI links only those headless boundaries, and subprocess coverage confirms protocol-clean execution without window or audio initialization.
- No active production source calls `GameState::get()`. Headless core contains neither `GameUtil::PATH()` nor `std::random_device`; the only production `std::random_device` use is the non-headless `src/BattleNetwork.cpp`, and `GameUtil::PATH()` remains confined to graphical/legacy application code.
- Retired auto-chess mutation/setup sources remain deleted, and GUI/tests consume the shared core libraries rather than compiling copied rule sources. No tool references the retired `DynamicChessMap.cpp` path.
- The obsolete `tools/simulate_enemy_run.py` duplicate rules path was deleted. `tools/pack_chess_smap_zip.py` now derives curated map IDs from `ChessBattleMapCatalog.cpp` and resolves battlefield records from `war.sta`; its focused Python tests pass.
- MSBuild/CMake source ownership remains aligned: all 41 chess-core entries and all 22 battle-core entries match exactly.
- Obsolete ruleset/profile/schema and RNG-version identity symbols have zero active hits. The headless dependency closure contains no presentation include, GUI/global-state access, or copied core source.
- Normal restore/import/parsing contains no replay, runtime-policy, or hash validation. `battle_frame_limit` is accepted structurally there and enforced only by explicit replay verification.
- Journal mutation is limited to restoring the complete persisted decision vector and appending accepted actions; there is no `erase`, `resize`, `clear`, truncation, or compaction path.

### Acceptance criteria traceability

All 22 design acceptance criteria are mapped below. These mappings establish domain, adapter, protocol, and static-path behavior; they do not claim that the graphical executable was launched. Live presentation items are explicitly retained as user-owned smoke tests after the numbered criteria.

1. Explicit-seed CLI startup — `test_protocol_stdout_is_clean_and_ids_are_preserved` starts `kys_chess_cli --jsonl` with a fixed 64-bit seed; `JSON protocol preserves request identifiers and session state` covers the same contract in-process.
2. All gameplay decisions are available without graphical UI — the JSONL `act` path feeds the shared typed action schema; `interactive commands submit the same typed actions as direct play`, the management/reward/challenge action suites, and `synthetic scripted run completes through a real headless battle` cover the full headless decision flow.
3. Structured state and legal actions — `JSON protocol preserves request identifiers and session state`, `JSONL controller writes exactly one protocol response per line`, and MCP/direct-JSONL parity cover machine-consumable observations and legal descriptors.
4. Headless battles use `BattleRuntimeSession` without graphical initialization — `test_headless_battle_keeps_jsonl_stdout_protocol_clean`, the `HeadlessBattleRunner` tests, and `CLI project links only headless project boundaries` cover runtime execution and dependency isolation.
5. GUI/headless share timeout and simultaneous-wipe policy — `session timeout policy is frozen at 36000 frames`, `shared runtime emits one exact timeout event`, `simultaneous wipe is player defeat`, and `incremental GUI drain and headless runner consume identical shared input` cover the shared policy path.
6. Initial ASCII board, meaningful logs, and final summary — `ASCII board preserves coordinates and stable unit tokens`, `battle text uses semantic IDs and omits movement traces`, and `test_headless_battle_keeps_jsonl_stdout_protocol_clean` cover all three outputs.
7. Graphical decisions route through `ChessGameSession` — `GUI adapter and direct session calls produce identical hashes` plus `graphical adapters do not call gameplay mutation owners` cover routing and prohibit direct mutation.
8. Equivalent GUI and CLI playthroughs produce identical replay hashes — `GUI adapter and JSONL playthrough export identical replays` compares every action's state/chain hashes and the final JSONL byte-for-byte.
9. Every accepted decision is journaled automatically — `accepted management actions journal from sequence one`, replay round-trip tests, and the scripted-run verifier coverage prove automatic records from the first accepted action.
10. Post-clear management and expedition challenges remain legal until `finish_run` — `post-clear management keeps challenges legal until finish run` covers the management, challenge, finish, and post-finish rejection boundaries.
11. Stable-boundary export verifies as `in_progress`, while `finish_run` yields `campaign_complete` — `committed short and complete replay goldens verify` checks the exact committed footer forms in Debug and Release.
12. Save/inspect/rollback capabilities are obvious to an LLM — `JSON protocol preserves request identifiers and session state` asserts operation names and rollback consequences; `test_mcp_tool_surface_matches_the_protocol_contract` asserts the MCP rollback/discard wording.
13. Ordinary load directly restores the trusted snapshot/RNG/journal without replay, runtime-option, or hash validation — `self-contained checkpoint JSON directly restores its snapshot and full journal`, `snapshot cheats load immediately and explicit replay audit finds first divergence`, `structurally valid journal corruption can load without implicit verification`, and `nonstandard replay runtime options load normally and fail only explicit verification` cover the deliberate trust boundary.
14. Every checkpoint retains the selected journal from action 1 without compaction, truncation, or snapshot-origin history — the direct-restore, snapshot-cheat continuation, rollback replacement, replay round-trip, and map-decision checkpoint tests assert the original prefix remains present and new actions append after it.
15. Save, close, and resume preserve the saved journal prefix — `self-contained checkpoint JSON directly restores its snapshot and full journal`, `loading replaces the active suffix while preserving save slots`, and `map decision point and selected map persist in checkpoint replay` cover restoration and continued journaling.
16. Exact-version saves export/import without activation, while malformed or different-version saves reject — `portable save import does not activate the checkpoint`, `JSON protocol exports and imports a portable save without activation`, and `direct restore only rejects incompatible or unrepresentable snapshots` cover syntax/version/representability and non-activation without replay validation.
17. Loading an earlier checkpoint restores its prefix and removes the active suffix — `loading replaces the active suffix while preserving save slots`, `timeline replacement counts a divergent equal-length branch`, and the JSONL save/load subprocess test assert restored/discarded counts.
18. The verifier accepts only the selected final timeline, without discarded rollback attempts — the checkpoint replacement test continues from the restored prefix, exports the replacement suffix, and verifies that replay successfully.
19. A fresh verifier reconstructs the run and validates final pieces/equipment — `actual configuration gameplay smoke has stable roster equipment and hash`, `synthetic scripted run completes through a real headless battle`, and `fresh session verifier accepts an unmodified replay` cover final-state reconstruction.
20. Windows x64 MSVC v145 Debug and Release accept the same committed goldens — both complete suites passed **8,875 assertions in 699 cases**, including `committed short and complete replay goldens verify`.
21. Altered, illegal, corrupted, truncated, and wrong-game-version replays reject precisely — the `ChessReplayVerifierUnitTests` mismatch sections and replay-archive corruption tests cover action, sequence, RNG, event, state, chain, footer, game-version, runtime-option, malformed, truncated, and archive failures.
22. No duplicated gameplay implementation exists across GUI, CLI, verifier, or MCP — the eight dependency/source-scan cases, exact MSBuild/CMake manifest comparison, retired-path deletion audit, GUI/JSONL replay parity, and MCP/direct-JSONL parity cover shared ownership.

### GUI regression remediation status (code/test audit; no graphical launch)

- The static legacy-versus-current UI comparison is complete. Subsequent live reports exposed lifecycle, navigation, typography, reward-presentation, and camera regressions; each identified code path is fixed with focused automated coverage, and adjacent external-exit paths were hardened.
- The post-battle log now calls `showBattleLogOverlay(...)` with its setting-aware default; the manual in-battle log intentionally retains the explicit `false` argument.
- Pre-chrome visibility filtering suppresses empty refresh/placeholder role, equipment, and internal-skill panels. Role detail opacity is restored to alpha 128, while the non-empty roster panel uses alpha 160 and hides when empty.
- The centered prebattle summary now renders its loading prompt immediately, automatically preloads assets exactly once after the first presented frame, remains visible after loading, and accepts dismissal only after loading completes.
- Save loading now renders `讀取中...` before checkpoint restoration, legacy map/database preparation, and scene-texture preload. The checkpoint is parsed once, the trusted snapshot is restored without replay verification, and immutable chess content is cached per difficulty so a same-difficulty load performs no content reload and a cold load performs at most one.
- Terminal graphical battle completion is captured even after the presentation frame sets its win/loss result, preserving the completed `StartBattle` action for postbattle rewards and feedback. Genuine scene exits now stop before standalone-vitals restoration, battle start from formation/team-selection modals, postbattle statistics, and mandatory reward loops, and propagate back to the enclosing context flow without an assertion, null dereference, or retry loop.
- The GUI adapter again uses the established first-level, overview, and equipment menu definitions and ordering, including conditional ban management and the legendary equipment shop. Challenge panels browse configured rows without requiring a deployment and reopen after attempts/rewards; leaving `神兵商店` returns to `裝備管理`.
- The GUI adapter includes `SuperMenuText`, role previews, roster/combo summaries, detailed chess, combo, internal-skill, equipment, challenge, experience, formation, reroll, guide, and system-settings panels, plus graphical pre/post-battle lineup and statistics screens; gameplay mutations remain routed through `ChessGameSession` actions.
- Structured auto-chess menus use shared `Font::getTextDrawSize`-based column formatting for names, state prefixes, stars/equipment state, prices, and combo progress. Legacy presentations are restored at 36/10 for browsing, 32/12 for overview, 36/9 for challenges, and 36/2 for position toggles, including prefix separators and ` [已裝]` spacing.
- Star-up rewards display `選擇升星 X★→Y★`, derive both stars from current session state, and do not add the unrelated combo panel.
- GUI battle presentation state is indexed explicitly by runtime unit ID; regression coverage includes non-zero and non-contiguous IDs `{1, 2}` and `{2, 7}`.
- The GUI battle-scene flow exposes map/list formation choices, visible battlefield coordinates, and authoritative `SwapPositions` actions before frame zero.
- The post-battle flow includes avatar- and semantic-color-based statistics over the final battlefield frame and continues to the existing ImGui battle log.
- Initial unit facing is derived from the nearest opposing unit through one shared direction-conversion path, with focused coverage for both teams.
- Initial battle camera placement integer-averages allied grid coordinates before converting that grid point to world coordinates, matching the legacy centering sequence.
- Code-path audit and automated coverage exercise selected-map terrain, cached-ground rendering, per-tile and whole-scene texture-limit fallbacks, cursor/range floor overlays, and unit movement.
- User-owned graphical smoke checks remain: menu alignment/colors and empty-panel behavior; challenge browse/reopen behavior, `神兵商店` return, exact menu geometry/spacing, and star-up title/panel presentation; selected-map floor, cached-floor, and texture-limit fallback rendering; animated piece movement, initial facing, and initial camera center; the loading prompt/preload starting automatically within the centered prebattle summary before dismissal; save-load `讀取中...` appearing before slow work with no duplicate immutable-content load block on a same-difficulty load; and postbattle avatars, semantic colors, statistics, battle-log setting behavior, and clean return to progression without an assertion.

## Per-task verification gate

For every non-trivial task:

1. Add the focused test first and confirm it fails for the intended missing behavior.
2. Implement the smallest complete slice without copied rule logic.
3. Run:

   `powershell -ExecutionPolicy Bypass -File ./.github/build-command.ps1 -Target kys_tests`

4. Run the focused Catch2 tag/executable and inspect the scoped diff.
5. At phase boundaries, run the complete Debug suite.
6. At Tasks 1, 9, 14, and final verification, run the required Release golden tests.
