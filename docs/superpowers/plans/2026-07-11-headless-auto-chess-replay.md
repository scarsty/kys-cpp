# Headless Auto-Chess and Verifiable Replay Implementation Plan

> **Execution gate:** Task 0 is complete and the architecture contract is approved. Production implementation begins at Task 1 and proceeds one task at a time, with focused tests and a build checkpoint after every non-trivial increment.

**Goal:** Make auto-chess fully playable without graphical systems, route GUI and headless play through one deterministic session, and produce replays that a fresh verifier can reproduce from immutable ruleset content, a root seed, and accepted player decisions.

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
- [x] Add the determinism profile, exact canonical byte layout, replay partial-footer behavior, and shared timeout ownership.
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

- [ ] Write fixed SplitMix64, xoshiro256**, stream-derivation, bounded-sampling, raw-counter, checkpoint/restore, reroll-family reset, and stream-isolation golden tests.
- [ ] Write fixed battle-runtime RNG vectors for seed, `nextInt`, `chance`, and `symmetricInt`.
- [ ] Implement the versioned generators without standard-library distributions.
- [ ] Implement the canonical writer and SHA-256 helpers with exact byte-vector goldens.
- [ ] Add chain-hash golden tests and malformed canonical-value assertions.
- [ ] Build `kys_tests`, run focused determinism tests in Debug, then run the same goldens in Release.

### Task 2: Immutable injected content and ruleset identity foundation

**Files:**
- Create: `src/ChessGameContent.h`
- Create: `src/ChessGameContent.cpp`
- Create: `src/ChessContentLoader.h`
- Create: `src/ChessContentLoader.cpp`
- Create: `src/ChessDiagnostics.h`
- Create: `src/ChessRulesetIdentity.h`
- Create: `src/ChessRulesetIdentity.cpp`
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
- Create: `tests/ChessRulesetIdentityUnitTests.cpp`

- [ ] Load top-level chess configs, role/magic/item records, dynamic map definitions, and battlefield terrain into one immutable ID-keyed snapshot from an explicit data root.
- [ ] Replace `Font` conversion, `GameUtil::PATH()`, `Save::getInstance()`, and mutable static lazy caches in headless gameplay code with injected content and diagnostics.
- [ ] Move tier colors, item display lookup, and other presentation helpers out of core definitions.
- [ ] Make `BattleRuntimeSessionCreationInput` carry combo, equipment, synergy, internal-skill, and magic-effect definitions; remove runtime global lookups.
- [ ] Compute the ruleset hash from canonical logical records, selected top-level configs, compiled gameplay version, RNG versions, timeout policy, and determinism profile.
- [ ] Test two different difficulty/content snapshots in one process, deterministic content hashes, shuffled logical input ordering, and diagnostics routed away from stdout.

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

- [ ] Move battle runtime sources into `kys_battle_core`.
- [ ] Move content, deterministic RNG, canonical encoding, run-state, and later session-domain sources into `kys_chess_core`, which references `kys_battle_core`.
- [ ] Remove the same source files from the GUI executable and direct test compilation lists; GUI and tests link the libraries instead.
- [ ] Compile both core libraries with the determinism-profile floating-point options.
- [ ] Add a dependency scan/test proving core headers and sources do not include or link `Engine`, `Audio`, `Font`, `RunNode`, `SystemSettings`, ImGui, SDL renderer, or SDL mixer presentation code.
- [ ] Build `kys` and `kys_tests` through `.github/build-command.ps1`.

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

- [ ] Make `GameState` publicly constructible from immutable content, session options, root seed/RNG state, and an optional stored state; remove `GameState::get()`.
- [ ] Store gameplay-affecting position-swap policy in session state/options, not `SystemSettings`.
- [ ] Replace old shop/enemy seed fields with versioned root/stream state and counters.
- [ ] Add challenge/reward IDs and store completed challenge IDs rather than vector indexes.
- [ ] Keep any GUI-global access in the application adapter host only; session/domain code receives explicit references.
- [ ] Test two independent sessions with different difficulties and seeds in one process without cross-mutation.
- [ ] Reject old slot-state shapes explicitly; do not add compatibility branches.

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

- [ ] Define phase, typed action, legal descriptor, stable error code, gameplay observation, semantic event, transition, and journal record types.
- [ ] Implement seeded new game plus pure observation/legal-action enumeration.
- [ ] Implement shop refresh/lock/purchase, sell, buy experience, and atomic deployment through the session.
- [ ] Move seen-role mutation into shop generation; observing/opening a menu must be pure.
- [ ] Finalize every accepted non-battle action at the next boundary and record pre/post/event/RNG/chain hashes from the first accepted action.
- [ ] Test valid mutations, all boundary errors, merge/bench/deploy constraints, stale identifiers, no-mutation rejection, observation purity, and same-seed/action parity.

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

- [ ] Make GUI menus render session observations/legal descriptors and submit typed actions.
- [ ] Add ban, forced-ban skip, legendary purchase, equipment assignment, position-swap option, and deterministic enemy-reroll actions.
- [ ] Preserve current monotonic ban behavior and stable equipment/chess instance identities.
- [ ] Add parity tests that compare the GUI adapter’s submitted actions and resulting session hashes to direct session calls.
- [ ] Delete each old UI-owned mutation path immediately after its parity test passes.
- [ ] Verify no gameplay flow calls `GameState::get()`, `SystemSettings` gameplay options, or `std::random_device`.

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

- [ ] Move enemy-lineup candidates, synergy scoring, enemy equipment, map candidates, battle seed, and challenge preparation out of `ChessBattleFlow`.
- [ ] Split the pure map catalog from graphical scene creation; never mutate shared `BattleInfo`.
- [ ] Capture the complete preparation-stream checkpoint before any preparation draw.
- [ ] Make prepared battle data immutable except for chosen map and a separate formation overlay.
- [ ] Include all prepared data and the RNG checkpoint in canonical state/checkpoints.
- [ ] Add total tie-breakers to enemy equipment and every planner sort.
- [ ] Test one-time consumption, observation purity, deterministic reroll rekeying, map choice legality, and every rollback-matrix row.

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

- [ ] Split terrain cells, walkability, grid conversion, and board bounds from texture construction.
- [ ] Extend the existing setup builder rather than copying it: copy all role, skill-ID, equipment, synergy, internal-skill, terrain, and runtime-rule facts into the creation input.
- [ ] Assign deterministic unit IDs with complete tie-breakers.
- [ ] Apply formation swaps to prepared input before `BattleRuntimeSession::createInitialized()`; do not create clones and then move only base units.
- [ ] Ensure setup never mutates shared `Role`, `Magic`, or content objects.
- [ ] Test byte-identical creation input from identical prepared data and valid swaps only before frame 0.

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

- [ ] Add structured skill/status/resource IDs so report and digest code never depends on localized names.
- [ ] Add total target tie-breakers, including the equal-angle/equal-distance case in `BattleCastSystem`.
- [ ] Enforce defeat, simultaneous wipe, and the 36,000-frame timeout inside the shared runtime policy used by all callers.
- [ ] Extract `BattleSceneFrameApplier::recordLog()` into a pure collector; make the frame applier presentation-only.
- [ ] Move post-battle survivor/stat summary projection beside the collector.
- [ ] Implement headless drain with initialization events, semantic events, report, summary, final runtime state, and digest.
- [ ] Test one timeout event, exact off-by-one frame, visual/text exclusion, semantic digest goldens, and Debug/Release equality.

### Task 10: Make the graphical battle consume the shared planner/setup/collector path

**Files:**
- Modify: `src/BattleSceneHades.h`, `src/BattleSceneHades.cpp`
- Modify: `src/DynamicChessMap.h`, `src/DynamicChessMap.cpp`
- Modify: `src/ChessBattleFlow.h`, `src/ChessBattleFlow.cpp`
- Modify: `src/BattleSceneUnitStore.h`, `src/BattleSceneUnitStore.cpp`
- Modify: `tests/BattleSceneFrameApplierUnitTests.cpp`
- Create: `tests/ChessBattleGuiHeadlessParityUnitTests.cpp`

- [ ] Construct the GUI scene from `PreparedChessBattle` plus `BattleSetupFactory`.
- [ ] Feed initialization and runtime frames to the shared report collector before presentation effects.
- [ ] Keep GUI battle progression incremental through the pending session transition.
- [ ] Compare GUI and headless result, end frame, semantic events, report, survivor state, and digest for identical prepared input and swaps.
- [ ] Delete `BattleSceneHades::onEntrance()` rule/setup duplication and old `DynamicChessMap::createBattle()` mutation paths after parity passes.

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

- [ ] Move victory economy, interest, experience, shop refresh, fight advance, fights-won, boss rewards, equipment rewards, internal-skill rewards, forced bans, challenge rewards, and repeat-challenge behavior into session rules.
- [ ] Represent reward rerolls and nested reward targets as successive explicit `RewardChoice` boundaries.
- [ ] Derive survivors and fights-won from the runtime summary; do not reset mutable `Role::HP/MP` before survivor-dependent rules.
- [ ] Apply the frozen timeout/defeat/repeat rollback matrix centrally.
- [ ] Keep `Management` available with `campaignComplete=true`; implement `finish_run`.
- [ ] Add a small synthetic-ruleset complete run and an actual-config smoke run with final roster/equipment/hash assertions.
- [ ] Prove no GUI flow performs a remaining rule mutation.

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

- [ ] Format management state, parameterized legal actions, pending rewards, post-clear status, and session-operation consequences in Traditional Chinese.
- [ ] Render the initial board from the same battlefield data and prepared formation, cropped with preserved coordinates.
- [ ] Assign stable A/E tokens for the battle duration and include the semantic token legend.
- [ ] Format compact logs from structured semantic events; keep trace-only movement separate.
- [ ] Add golden text tests without using display strings in replay hashes.

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

- [ ] Add interactive, compact, JSONL, and trace modes with explicit seed and data-root options.
- [ ] Implement all gameplay and session operations, caller request IDs, stale/illegal-action errors, and timeline-replacement responses.
- [ ] Keep stdout protocol-clean under successful loads, warnings, malformed config, battle execution, and verification; route diagnostics to stderr.
- [ ] Resolve default data paths relative to the executable, never the process working directory.
- [ ] Link only `kys_chess_core`, `kys_battle_core`, and headless dependencies; do not link GUI/audio/render projects.
- [ ] Add the CLI project to the solution and the default build targets.
- [ ] Run subprocess tests proving no window/audio initialization, request-ID preservation, REPL/JSONL action parity, and clean shutdown.

### Task 14: Replay JSONL schema and fresh-session verifier

**Files:**
- Create: `src/ChessReplayJson.h`
- Create: `src/ChessReplayJson.cpp`
- Create: `src/ChessReplayVerifier.h`
- Create: `src/ChessReplayVerifier.cpp`
- Create: `tests/ChessReplayJsonUnitTests.cpp`
- Create: `tests/ChessReplayVerifierUnitTests.cpp`
- Create: `tests/data/replays/short_v1.jsonl`
- Create: `tests/data/replays/complete_v1.jsonl`

- [ ] Serialize header, decision records, and in-progress/complete footer with the frozen schema and fixed-width seed/hash strings.
- [ ] Rebuild a fresh session from immutable content, profile, difficulty, options, and root seed.
- [ ] Verify pre-state, legal descriptor, action, automatic transition, event/RNG/post/chain hashes, and footer in order.
- [ ] Report exact sequence and mismatch category.
- [ ] Test modified, removed, inserted, duplicated, reordered, truncated, malformed, wrong-profile, wrong-ruleset, RNG-divergent, event-divergent, state-divergent, and chain-divergent replays.
- [ ] Require the same committed golden replays to pass Debug and Release.

### Task 15: Replay archive packaging

**Files:**
- Create: `src/ChessReplayArchive.h`
- Create: `src/ChessReplayArchive.cpp`
- Create: `tests/ChessReplayArchiveUnitTests.cpp`
- Modify CLI export/verify commands

- [ ] Package authoritative `replay.jsonl`, optional `summary.txt`, and optional diagnostics with libzip.
- [ ] Ignore non-authoritative files during verification.
- [ ] Test archive round-trip, missing/duplicate authoritative entry, corrupt zip, and diagnostic tampering.
- [ ] Do not change canonical hashes when switching between plain JSONL and archive transport.

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

- [ ] Move the private `SavePersistence::SlotData` concept into a public headless checkpoint/store abstraction.
- [ ] Save only at stable boundaries and embed the complete active journal prefix, canonical state snapshot/hash, RNG state, phase/pending data, ruleset/profile, and monotonic save revision.
- [ ] Validate the journal prefix before accepting the snapshot cache.
- [ ] Make inspect/save consume no RNG and make failed/tampered load transactional.
- [ ] On load, replace state, RNG, phase, pending data, and journal prefix while preserving the external slot catalog; return discarded-action count.
- [ ] Keep save metadata outside gameplay hashes and expose `SessionObservation{gameplay, saveSlots, operations}`.
- [ ] Reject old slot schema versions; do not create snapshot-origin replay branches.
- [ ] Test every supported boundary, copied self-contained checkpoints, rollback/new suffix, catalog preservation, tampering, and full verification with snapshots ignored.

### Task 17: Thin MCP adapter after protocol stability

**Files:**
- Create: `tools/kys_chess_mcp/pyproject.toml`
- Create: `tools/kys_chess_mcp/kys_chess_mcp/server.py`
- Create: `tests/test_kys_chess_mcp.py`
- Create: `docs/headless-auto-chess-agent-workflow.md`

- [ ] Spawn and own one CLI JSONL session.
- [ ] Expose new/observe/legal/take-action, list/inspect/save/load/export/import save, and export/verify replay tools.
- [ ] Explain rollback and discarded suffixes in tool descriptions.
- [ ] Compare every MCP response to the equivalent direct JSONL response.
- [ ] Keep all gameplay rules in the C++ session; the adapter performs transport and process lifecycle only.

### Task 18: Final verification and deletion audit

**Files:**
- Modify only if verification exposes a defect.

- [ ] Run every new focused Catch2 group.
- [ ] Run the complete Debug `kys_tests` suite.
- [ ] Run Release RNG, canonical, battle-digest, and replay-golden suites.
- [ ] Run `tests/test_kys_chess_cli.py` and `tests/test_kys_chess_mcp.py`.
- [ ] Run `powershell -ExecutionPolicy Bypass -File ./.github/build-command.ps1`; only the documented `kys.exe` file-lock final-link failure is acceptable. `kys_tests` and `kys_chess_cli` must link.
- [ ] Run source scans proving there is no `GameState::get()`, replay-sensitive `std::random_device`, headless `GameUtil::PATH()`, GUI-owned gameplay mutation, core presentation dependency, or duplicate old rule path.
- [ ] Verify the CLI opens no window/audio device and produces protocol-clean stdout.
- [ ] Re-read every acceptance criterion and record automated coverage plus any platform-specific smoke tests.

## Per-task verification gate

For every non-trivial task:

1. Add the focused test first and confirm it fails for the intended missing behavior.
2. Implement the smallest complete slice without copied rule logic.
3. Run:

   `powershell -ExecutionPolicy Bypass -File ./.github/build-command.ps1 -Target kys_tests`

4. Run the focused Catch2 tag/executable and inspect the scoped diff.
5. At phase boundaries, run the complete Debug suite.
6. At Tasks 1, 9, 14, and final verification, run the required Release golden tests.
