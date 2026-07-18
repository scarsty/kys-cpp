# Offline Save PvP Design

Date: 2026-07-18  
Status: Draft for review

## Overview

This feature lets a player use the composition from their current Hard-mode run to fight a composition imported from another player's save file.

The released game binary verifies the imported save, reconstructs its final state from the embedded replay, extracts the opponent's combat composition, and launches a standalone battle on a fixed symmetric map.

Counter-positioning is intentionally allowed in this first version. After importing an opponent, the player can inspect the opponent's saved formation and adjust their own formation before starting the battle.

## Goals

- Support Windows, browser, and Android.
- Import an opponent save through the platform's normal file-selection UI.
- Require an exact game-version match.
- Accept only Hard-mode saves.
- Verify every replayed action and the final saved snapshot.
- Preserve role, star level, equipment, individual fights won, inner powers, deployment, and formation.
- Let the player adjust their own formation after viewing the opponent formation.
- Run the fight through the existing standalone battle and visual battle flow.
- Keep offline battles separate from campaign progression.
- Use one fixed symmetric battlefield with capacity for ten initial units and three additional spawn cells per side.

## Non-goals for V1

- Servers or hosted services.
- Organizer signatures or certificates.
- Ruleset or policy identifiers beyond the existing game version.
- Rankings, brackets, tournament scheduling, or automated series.
- Preventing players from counter-positioning.
- An opponent collection or online sharing interface.

## Player Flow

1. The player opens `Offline Battle` from the chess context menu.
2. The current Hard-mode session becomes the local composition.
3. The player selects `Import Opponent Save`.
4. The game opens the platform file picker.
5. The game verifies the imported save without loading it into the active campaign session.
6. The screen shows both formations.
7. The player may swap their own formation slots directly on the offline battle screen.
8. The player starts the battle.
9. The game creates an ephemeral standalone battle using both compositions.
10. After the battle, the player may return to the offline battle screen, adjust formation, and fight again.

The offline battle itself must not change campaign money, fight progression, rewards, shops, equipment ownership, fights won, or any other campaign state. A formation swap made on the offline battle screen is a normal management action and is immediately written to the active run's replay.

## Save Acceptance Rules

An imported save is accepted when all of the following are true:

- Its game version exactly matches the running game version.
- Its difficulty is Hard.
- Its embedded replay parses successfully.
- Every recorded action can be replayed legally.
- Every recorded evidence hash matches the reconstructed action result.
- The reconstructed replay footer matches the stored replay footer.
- The reconstructed final session state exactly matches the checkpoint state.
- The reconstructed full random state exactly matches the checkpoint random state.
- The checkpoint snapshot hash, replay final-state hash, and freshly calculated state hash agree.
- The snapshot is at a representable stable decision boundary.
- At least one piece is deployed.

There is no restriction on campaign fight number, completed challenges, money, level, bans, shop contents, or other progression. Those values only need to be the verified result of the recorded run.

The current GUI only creates normal saves during the Management phase, so the V1 GUI export is also available only during Management.

## Save Verification Architecture

Normal save loading remains a fast restore path. Offline PvP uses a separate strict audit path.

The existing replay verification loop should be refactored into a reusable audit that can return the fully reconstructed session:

```cpp
struct ChessReplayAuditResult
{
    ChessReplayVerificationResult verification;
    std::unique_ptr<ChessGameSession> reconstructedSession;
};
```

The existing replay verifier and the new PvP save verifier must use the same replay-execution implementation. There should not be a second copied replay loop.

The PvP verifier performs these steps:

1. Decode either a full external slot save or a direct chess checkpoint.
2. Check version and Hard difficulty.
3. Reconstruct a fresh session from the replay root seed and options.
4. Replay every decision.
5. Drain automatic battle transitions.
6. Verify evidence and footer data.
7. Compare reconstructed state and random state with the checkpoint.
8. Recalculate and compare all final-state hashes.
9. Extract the opponent composition.

Replay verification may include several complete battles. It must not freeze the browser or Android UI. The audit should support cooperative stepping by a limited number of decisions or battle frames per rendered UI frame.

While verification is running, show operation progress rather than campaign progress:

```text
Verifying opponent save...
Action 187 / 412
```

Example errors:

- `Game version mismatch: save 1.3.0, current game 1.4.0`
- `Offline Battle accepts Hard-mode saves only`
- `Replay verification failed at action 187`
- `The final save snapshot does not match the replay result`
- `The imported save has no deployed pieces`

## PvP Composition

Only battle-relevant state is copied out of the verified session:

```cpp
struct ChessPvpPiece
{
    int chessInstanceId = -1;
    int roleId = -1;
    int star = 1;
    int weaponItemId = -1;
    int armorItemId = -1;
    int fightsWon{};
};

struct ChessPvpComposition
{
    std::vector<ChessPvpPiece> pieces;
    std::vector<int> formationSlots;
    std::set<int> obtainedNeigongIds;
};
```

The extractor resolves equipment instance IDs from the session roster to their actual item IDs. It includes deployed pieces only.

Combos, character base values, martial arts, equipment effects, inner-power effects, and battle rules continue to come from the current Hard-mode game content.

## Persistent Formation State

The current battle planner derives player unit order from the roster map's instance-ID ordering. This does not represent a player-owned formation and cannot preserve empty positions.

Add ten persistent formation slots to `ChessSessionState`:

```cpp
std::vector<int> formationSlots;
```

The vector always contains exactly ten values. Each value is either a deployed chess instance ID or `-1` for an empty slot.

Formation invariants:

- Every deployed piece appears exactly once.
- A non-deployed piece never appears.
- No instance ID is duplicated.
- Empty positions remain `-1`.
- A player with fewer than ten deployed pieces may choose which positions remain empty.

Add a replayed management action:

```cpp
ChessActionType::SetFormation
```

The action carries the complete ten-slot formation. The rules validate it as an exact placement of the current deployed set.

Management operations that change the roster or deployment maintain formation as part of their existing application logic:

- Retained deployed pieces keep their slots.
- Removed, sold, merged, or benched instance IDs are cleared.
- Newly deployed pieces enter the first empty slots.
- The player may then rearrange them directly on the offline battle screen.

Selecting two slots immediately submits one complete `SetFormation` action. There is no separate draft or confirmation step.

## Offline Battle Screen

Add `Offline Battle` to the main chess context menu.

The screen uses the current session as the local side and maintains one currently imported opponent.

```text
+--------------------------- Offline Battle ---------------------------+
|                                                                      |
|  Local formation                           Opponent formation         |
|                                                                      |
|  [editable tap-to-swap formation]          [formation preview]       |
|                                             or                       |
|                                             Import Opponent Save      |
|                                                                      |
|  Hard mode                                  Verified                  |
|                                                                      |
|       [Export My Save] [Import/Replace] [Battle] [Back]              |
+----------------------------------------------------------------------+
```

Do not show campaign fight number, level, money, or other unrelated progress.

`Start` is enabled only when:

- The current local session is Hard.
- The local session has at least one deployed piece.
- An opponent has been imported and verified.
- Both saves use the same game version.
- Both formations are valid.

## Inline Formation Editing

The offline battle screen displays the local formation as editable and the imported opponent formation as read-only. Counter-positioning is allowed.

Primary interaction is tap-to-swap so the same behavior works with mouse and touch:

1. Select a local occupied or empty slot.
2. Highlight the selected slot.
3. Select another local slot.
4. Swap the two slot contents.

Mouse drag-and-drop may be supported as an additional interaction, but it must not be required.

Keyboard and gamepad controls:

- Directional input moves the selected slot.
- Confirm selects or swaps.
- Cancel closes the editor.

```text
+-------------------------- Adjust Formation --------------------------+
|                                                                      |
|  Editable local side                       Read-only opponent side    |
|                                                                      |
|       [01] [02] [03] [04]                    [01] [02] [03] [04]    |
|          [05] [06] [07]                        [05] [06] [07]        |
|          [08] [09] [10]                        [08] [09] [10]        |
|                                                                      |
|             [Reset]               [Cancel]               [Confirm]  |
+----------------------------------------------------------------------+
```

Buttons and formation cells should retain approximately 52 to 60 logical pixels of touch height within the existing 1280x720 logical UI.

## File Import and Export

The offline battle screen provides `Export My Save` directly. The player does not need to save to a numbered slot, return to the title screen, and then use the existing external-save submenu.

The direct export is JSON containing `ChessSessionCheckpointData`, including:

- Session state.
- Full random state.
- Embedded authoritative replay.
- Snapshot hash.
- Game version in the embedded replay header; the checkpoint does not duplicate it.

Suggested filename:

```text
kys-opponent-2026-07-18-14-30.json
```

The importer accepts both:

- The existing full external slot JSON envelope.
- A directly exported chess checkpoint JSON.

Platform behavior:

- Windows uses the SDL3 asynchronous open/save file dialogs.
- Android uses the SDL3 Android file-dialog backend and the system document picker.
- Browser uses the existing HTML file input, `FileReader`, and Blob download flow.

The current external JSON transfer code in `TitleScene` and `wasm/shell.html` should be refactored into a reusable platform service. The title-screen save flow and offline PvP flow must not contain separate copies of file import/export logic.

V1 maintains one current opponent. Persisting that opponent across application restarts is optional and remains an open decision.

## Fixed Symmetric PvP Map

V1 uses:

- Battle ID: `133`.
- Battlefield ID: `21`.
- Offline display name: `PvP Arena`.

Battlefield 21 has an exactly 180-degree rotationally symmetric collision mask around grid centre `(30.5, 29.5)`. It is an enclosed, open rectangular arena with no water and no one-sided central choke point. The battlefield resources already exist for the supported platforms.

Battle ID 133 must not be added to the normal campaign random-map pool. Offline PvP uses a dedicated map layout that references battlefield 21 and supplies explicit positions for both teams.

### Initial Formation Slots

| Slot | Local coordinate | Opponent coordinate |
|---:|:---:|:---:|
| 1 | `(28,24)` | `(33,35)` |
| 2 | `(28,28)` | `(33,31)` |
| 3 | `(28,32)` | `(33,27)` |
| 4 | `(28,35)` | `(33,24)` |
| 5 | `(26,26)` | `(35,33)` |
| 6 | `(26,30)` | `(35,29)` |
| 7 | `(26,34)` | `(35,25)` |
| 8 | `(24,24)` | `(37,35)` |
| 9 | `(24,29)` | `(37,30)` |
| 10 | `(24,35)` | `(37,24)` |

### Additional Spawn Cells

These cells are reserved for clone, summon, or other effects that create additional battle units. Each side therefore supports ten initial units plus three additional spawn cells, for thirteen spawn points per side.

| Spawn cell | Local coordinate | Opponent coordinate |
|---:|:---:|:---:|
| 1 | `(22,26)` | `(39,33)` |
| 2 | `(22,30)` | `(39,29)` |
| 3 | `(22,34)` | `(39,25)` |

Opponent positions use the exact rotation:

```cpp
opponentX = 61 - playerX;
opponentY = 59 - playerY;
```

All 26 positions are distinct and walkable.

The dedicated layout stores both teams' ten formation slots and three additional spawn cells explicitly. It does not use battle ID 133's original story-battle enemy coordinates.

## Standalone Battle Changes

The standalone battle request currently carries one inner-power collection that is applied only to team 0. PvP requires independent team state:

```cpp
struct ChessStandaloneBattleTeam
{
    std::vector<ChessStandaloneBattlePiece> pieces;
    std::set<int> obtainedNeigongIds;
};
```

The standalone request contains two teams. Each team supplies its own:

- Pieces.
- Stars.
- Equipment.
- Individual fights won.
- Formation slots.
- Inner powers.

The offline controller creates an initial battle seed when the screen opens. The player may edit, copy, paste, or reroll it, and the displayed value is used verbatim for the battle.

The battle uses `ChessStandaloneBattle`, `BattleSceneHades`, post-battle statistics, and headless digest paths. Standalone progression remains ephemeral.

After post-battle statistics, the game returns automatically to the same offline battle screen and preserves the opponent and battle seed.

## Suggested Code Boundaries

- `ChessReplayAudit`: cooperative replay reconstruction that returns the final session.
- `ChessPvpSaveVerifier`: save decoding, version/difficulty checks, and checkpoint comparison.
- `ChessPvpComposition`: verified battle-relevant composition data.
- `ChessPvpMapLayout`: battlefield 21 formation and additional spawn coordinates.
- `ChessOfflineBattleScreen`: opponent import, previews, formation editing, and battle launch.
- `ExternalJsonFileTransfer`: shared Windows, Android, and browser text-file import/export abstraction.

`ChessStandaloneBattle`, console test battles, offline PvP, and the future tournament runner should share the same team payload and battle-building path.

## Required Tests

### Save Verification

- A valid Hard-mode checkpoint passes.
- A different game version is rejected.
- Easy and Normal saves are rejected.
- Modified actions, evidence hashes, or replay footer values are rejected.
- A modified checkpoint state is rejected.
- A modified checkpoint random state is rejected.
- A forged snapshot hash does not allow a mismatching state to pass.
- Full slot JSON and direct checkpoint JSON are both accepted as input envelopes.

### Formation

- Non-empty slot IDs exactly match the deployed set.
- Duplicate IDs are rejected.
- Non-deployed IDs are rejected.
- Missing deployed IDs are rejected.
- Empty slots are preserved.
- Deployment changes retain surviving piece positions.
- Selling or merging clears obsolete instance IDs.
- `SetFormation` participates in normal replay verification.

### PvP Map

- Both sides have ten walkable initial slots.
- Both sides have three walkable additional spawn cells.
- All 26 coordinates are unique.
- Every pair satisfies `(61 - x, 59 - y)`.
- Battlefield 21 remains collision-symmetric in the PvP arena region.
- Both teams can use all three clone or summon cells.

### Battle

- Each side receives its own roles, stars, equipment, fights won, and inner powers.
- Opponent inner powers are not applied to the local team and vice versa.
- Offline battle completion does not mutate campaign progression.
- GUI and headless execution produce the same result and digest for identical inputs.

### Platform UI

- Windows and Android asynchronous file-dialog callbacks handle success, cancellation, and error.
- Browser file input reads UTF-8 JSON.
- Browser export downloads the current checkpoint.
- Verification remains visually responsive and can be cancelled.
- Touch can select pieces, empty slots, and footer actions.

## Implementation Order

1. Refactor replay verification into a cooperative audit that returns the reconstructed session.
2. Implement strict PvP save verification and both supported JSON envelopes.
3. Add ten persistent formation slots to session state, management rules, observations, and replay JSON.
4. Add the replayed `SetFormation` action.
5. Add the battlefield 21 PvP layout and validated 10+3 mirrored coordinates.
6. Refactor standalone battle input into two independent teams.
7. Refactor cross-platform JSON file import/export into a shared service.
8. Implement the offline battle screen and inline counter-positioning controls.
9. Connect the screen to the existing visual battle and statistics flow.
10. Run unit tests, the Windows build, browser testing, and Android testing.

## Open Decisions

- Whether the last successfully imported opponent persists across application restarts.
- Whether direct exports remain `.json` or use a dedicated extension containing plain JSON.
- Whether `Fight Again` should also offer an explicit same-seed option.
