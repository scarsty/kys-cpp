# Effect evaluation and execution refactor

**Status: the agreed functional refactor is complete.** The final repository build and all 998 C++ test cases pass. Deterministic battle/replay coverage matches the original baseline. All 44 Python tests pass after updating the two stale description-format expectations. Only the explicitly measurement-dependent optimizations remain deferred. Changes are uncommitted.

This refactor keeps evaluation separate from real execution. Commands own the facts they need after dispatch, while execution retains its existing live-state reads. The existing command batches, rule/action ordering, and damage continuations remain the execution path.

## Stages

1. **Borrowed evaluation and direct ordinary-rule evaluation.** `EffectEventData` owns the event payload. `EffectEventContext` borrows it and keeps binding, observed owner, formula overrides, contribution context, and effective provenance in `EffectEvaluationScope`. Configured rules and status behaviors call `evaluateOrdinaryRule` with their actual activation runtime by reference. Configured source/cast slots remain in the rule store; status behavior slots retain their previous per-evaluation lifetime.
2. **Command input ownership.** Every emitted command owns `EffectExecutionInputs`. Queues and rule-local continuations move those commands without assembling a second context. Area commands own `BattleAreaCreateRequest`, including the resolved anchor, team domain, and normalized modifier values. Anti-combo transfer and interceptor-generated shield commands capture their inputs at construction as well.
3. **One liveness implementation.** `BattleEffectDispatchPrediction` is used by production and test fixtures. Detached and status-only fallback projections are removed. Test helpers construct runtime state and call the production predictor; they do not execute or simulate commands themselves.
4. **Shared transitions and reduced prediction copying.** Status application, removal, consumption/depletion, and persistent-modifier protection use `StatusProtectionState`, which borrows units and persistent modifiers and carries status configuration. Contribution lookup is shared by dispatch prediction and execution; execution additionally requires the snapshotted quantity to match. Prediction still invokes the production reducer, stopping after the entire action group that starts a damage continuation.

## Command dependencies

| Family | Owned inputs | Reads retained at execution |
|---|---|---|
| Attribute and persistent damage modifiers | Evaluated amount, operation, stacking/duration, metadata, application frame | Current protection, existing modifiers, expiration, negative-effect sequencing |
| Resource changes and healing | Amount, selected transfer destinations, metadata, triggering cast, healing parameters/modifiers | Current resources/limits, full-MP condition, healing eligibility, healing transactions and nested events |
| Status application/consumption/removal | Resolved application parameters, bound immutable behavior, one duration, contribution identity/filter, depletion behavior, application policies/frame | Current protection, contribution existence/kind/quantity, merge and depletion transitions; later behavior formulas |
| Damage and deferred HP changes | Amount/count, intent, triggering provenance, origin metadata, descendant-retention policy | Damage transaction processing, mitigation, resulting events and continuation completion |
| Area creation | Resolved creation request, fixed position or followed-unit ID, team domain, geometry, modifiers, duration/merge policies | Followed unit position, merge/lifetime processing and later area effects |
| Attack/cast modifications and interceptors | Selected source facts, evaluated overrides/costs, command metadata and triggering lineage | Existing phase-specific attack/cast operations and lifecycle work |
| Forced movement | Target identity and movement intent | Existing movement-phase position, geometry and eligibility |
| State-machine actions | Evaluated slot values/outputs, selected sources, metadata and triggering lineage | Existing runtime operations, settlement and descendant handling; evaluation-time slot changes remain in evaluation |
| Heal transaction modifiers | Modifier intent, event/source/target identity | Existing transaction-phase modifier application |

Application time is distinct from evaluation time. In particular, frame-start rules observe the current event frame but apply their commands in the upcoming frame. `EffectEventHeader::executionFrame` records this override at event creation; execution and prediction consume the captured application frame.

Effective borrowed-rule provenance and the original triggering event lineage have different existing roles. Effective provenance is centralized for evaluation; command metadata retains rule identity, and execution inputs retain the original triggering lineage. Healing's execution cast is captured while its owned event is created without making healing events eligible for cast-observation rules.

## Removed paths and representations

- Payload copies for owner observation, borrowed provenance rewriting, and stored-state formula inputs.
- In-place rewriting of copied event payload variants.
- Temporary status `BattleEffectRuleStore`, rule append/validation during dispatch, and activation runtime copy-in/copy-out.
- Separate execution-context construction in attack, cast, damage, healing, frame, and continuation callers.
- `BattleEffectCommandContext` and the context parameter on the public command reducer and queue interface.
- Context fields on queued and frame command batches.
- Area `CreateAreaAction` retention, `modifierAmounts`, and execution-time authored-anchor reconstruction.
- `HolderLiveness`, `DetachedContributionLiveness`, fallback `updateLiveness`/`applyLivenessStatus`, and `ActiveStatusBehaviorView::holderEffects`.
- Separate contribution lookup logic and duplicated consume/depletion handling.
- Whole-runtime copying for dispatch prediction.

## Prediction dependencies

`BattleEffectCommandSystem::copyDispatchState` eagerly copies units, the current movement frame, grid transform, cast lifecycle, healing IDs/committed transactions, RNG, areas, effect rules/state, persistent modifiers, status configuration, and the event ordinal.

It does not copy the attack world, movement/physics terrain, rescue state, projectile follow-up queues, queued command batches, damage continuations, or previously emitted healing events. Units still carry their full records: control/status transitions synchronize those records, and healing can dispatch nested events that require runtime snapshots. No separate simulator, snapshot cache, custom allocator, or lazy-copy mechanism was introduced.

## Deliberately retained runtime inputs

The command payload normalization is complete. No remaining modifier, resource, damage, attack, or cast command keeps both an authored formula/selector and its resolved execution value.

- Initialization resources carry `InitializationResourceAmount` instead of an already evaluated integer. Their formula is evaluated against the existing resource read view after base attributes are finalized. The resulting anti-combo initialization record carries only the resolved integer. These alternatives are mutually exclusive.
- Immutable status behavior definitions retain formulas for later events. Persistent absorption retains its settlement selector until expiry or source death, preserving that later snapshot and RNG timing.
- Forced movement and heal-transaction modifiers retain their existing concrete intent types; they have no parallel evaluated formula or selector to remove.
- Area runtime modifiers still use the existing `AreaModifier` type, with attribute and periodic-damage amounts normalized to constant `EffectNumber` values. Replacing this storage, further shrinking prediction state, snapshot caching, custom allocation, and lazy copying remain measurement-dependent optimizations as explicitly deferred in the original scope.

## Initial-slice verification

The pre-refactor baseline build succeeded. All 994 baseline C++ cases passed (43,015 assertions). The baseline Python discovery run executed 44 tests and had two description-format failures in `test_kys_chess_cli.py`.

Before changing production behavior, a new deterministic scenario test recorded these 600-frame real-ultimate battles using seed 12345:

| Magic ID | Battle digest | RNG draws |
|---|---|---:|
| 18 | `05226f72c8cfb3743356b707ea1d271a6950d7ba63776b0a2fb7b64a983a377b` | 45 |
| 94 | `09dabbdddebb6d229f45f5fd089000d270a9f46cad74d268d3881e04a0300295` | 60 |
| 133 | `3de24dfd20cecebd407d86999d37899d79b22f8d0dec29e6fcc894a6767a06b3` | 60 |

The focused effect, scenario, and replay checks passed at each completed stage. Committed replay goldens remain unchanged; those goldens exercise management actions, so combat equivalence also uses the battle digests above and the existing poison, borrowed-rule, formula-binding, protection, and continuation coverage.

Final verification:

| Check | Result |
|---|---|
| `.github/build-command.ps1` (Debug x64: game, C++ tests, CLI, schema consumer tests) | Passed; all targets linked successfully |
| `x64/Debug/kys_tests.exe --reporter compact` | Passed: 995 cases, 43,029 assertions |
| `python -X utf8 -m unittest discover -s tests -p 'test_*.py' -v` | 44 tests run; 42 passed, the same two baseline failures remained |
| Fixed-seed battle goldens | All three digests, 600-frame end times and RNG draw counts matched |
| Committed replay goldens and existing scenario/parity tests | Passed; committed replay files unchanged |
| `git diff --check` | Passed |

The two pre-existing Python failures are:

- `test_equipment_reward_description_separates_character_effects`: expects the text `主彈命中：`, which the current equipment description does not contain.
- `test_protocol_exposes_semantics_schemas_and_actionable_parse_errors`: expects description rows without a final `。`, but the current renderer produces one.

No description renderer or expected replay output was changed to make verification pass. Logs are under `work/effect-refactor-verified-build.log`, `work/effect-refactor-verified-tests.log`, `work/effect-refactor-verified-python.log`, and `work/effect-refactor-final-goldens.log`. Pre-refactor logs use the `work/effect-refactor-baseline-` prefix.

## Follow-up stage 1: poison settlement continuation correctness

The focused regression reproduces the mismatch from the review: a configured rule settles positive remaining poison damage and then removes poison; a later poison behavior responds to the same hit. Without reducer preparation, prediction removes the contribution early and emits two commands rather than three. The companion case has an existing poison contribution whose next tick is beyond its remaining duration; it schedules no damage, so prediction correctly continues through removal and suppresses the later behavior.

The existing canonical poison-schedule calculation moved from `BattleCoreEffects.cpp` to `BattleEffectCommandSystem.cpp`. The reducer prepares the routed settlement command's output from current state. Both dispatch prediction and real execution now consume that amount. Real execution keeps the existing damage request, provenance, descendant reservation, and continuation scheduling. No damage is applied during evaluation or prediction. This is a correctness fix for the reproduced eligibility case, not a claim that all previously incorrect dispatch outputs remain identical.

The regression was run with the preparation branch removed and failed on positive settlement (2 commands instead of 3; zero output instead of 9). Restoring the shared preparation passed both cases. The existing frame-runner tests cover canonical tick ordering, poison replacement after settlement, and actual damage execution.

## Follow-up stage 2: resolved status payloads

- `prepareStatusApplication` resolves quantity, reapplication, producer-family limits, and target-wide limits into concrete execution parameters. The emitter shares behavior binding and duration evaluation across direct application and both depletion branches, preserving binding-before-duration order.
- `ApplyStatusEffectCommand` contains one duration and one set of application parameters. It no longer stores `ApplyStatusAction`, its formula, or an optional duration override.
- Both consumption commands contain a prepared consume request and at most one prepared depletion application. The authored depletion duplicate is removed.
- Consumption and removal resolve their contribution filters during emission. Execution still checks live contributions, captured contribution quantity, actual depletion, and protection state. Producer provenance is still derived from owned metadata, so initialization transfers preserve their existing rebinding behavior.
- Poison aggregation combines prepared durations, charges, and immutable behavior payloads. Poison reporting and semantic cues use those same resolved fields.
- `CommandEmitter::append` constructs metadata, payload, and owned execution inputs together. The rule evaluator's finishing pass is removed.

The status reducer no longer chooses an authored versus evaluated duration or reconstructs application semantics from authored actions. Modifier/resource normalization and broader damage/state-machine routing were completed in the final stage below. Prediction-state copying was not reduced further.

The follow-up changes remain uncommitted together with the initial slice. The two stages above identify the correctness correction and representation cleanup separately for review; they are not separate commits.

### Follow-up verification

| Check | Final result |
|---|---|
| `.github/build-command.ps1` | Passed: game, C++ tests, CLI, and schema consumer tests |
| All C++ unit tests | Passed: 996 cases, 43,044 assertions |
| Focused continuation and depletion preparation cases | Passed: 3 cases, 27 assertions |
| Python discovery | 44 run, 42 passed; the same two baseline description-format failures |
| Three original 600-frame battle goldens | Identical digests and RNG counts (45, 60, 60) |
| Existing replay/scenario coverage | Passed as part of the full C++ suite; expected outputs unchanged |
| `git diff --check` | Passed |

Logs: `work/effect-status-final-build.log`, `work/effect-status-final-tests.log`, `work/effect-status-final-python.log`, and `work/effect-status-final-goldens.log`. The deliberately failing regression run is in `work/effect-status-stage1-before-test.log`.

An intermediate full build overlapped the test executable and failed to copy locked debug CRT DLLs. The final build ran after those processes exited and all targets succeeded. No production changes were made to work around the lock. The duration regression fixture was corrected to satisfy existing authoring validation (formula-only duration with a positive minimum); the validator and runtime behavior were unchanged.

## Final stage: complete command normalization and shared damage execution

Modifier and resource commands now contain concrete policy fields and one amount. Attack and cast commands contain the selected source, evaluated damage override or MP cost, and phase-specific intent, without retaining the original selectors or formulas. Preparation functions are used by production emission and migrated command fixtures. Initialization's later resource evaluation remains explicit rather than being folded into event-time evaluation.

`DealDamageEffectCommand` contains the resolved amount/count and damage policy. `EffectDamageDelivery` contains area/per-cast delivery and only the source-maximum-HP percentages needed by area-projectile follow-up construction and reporting. It does not retain `DealDamageAction` or its formula. Mixed flat-plus-percentage damage preserves the previous distinction between the follow-up percentage and absolute-damage log text.

`StateMachineEffectCommand` is a variant of specific execution payloads. Borrowing and copying carry selected source IDs and required runtime policy. Damage settlement carries an amount, damage kind, and ordered target IDs. Initialization actions and persistent absorption retain their concrete runtime intent. Already-evaluated state changes emit `EvaluatedStateEffectCommand` to preserve command order and reduction receipts; state changes themselves remain in evaluation. The generic `stateValueBefore`, `stateValueAfter`, `outputValue`, and `selectedSourceUnitIds` fields are removed.

Ordinary damage, recorded-damage output, absorption settlement, remaining-poison settlement, and deferred HP damage now use `prepareDamageOutput` and `appendEffectDamageOutput`. Expiry/source-death absorption also uses this construction and scheduling path, while explicitly preserving its detached attack/cast-work behavior and original effect origin. Target selection at expiry and live poison schedule preparation keep their existing timing.

The separate `appendStateMachineOutput` executor, `BattleDeferredHpResourceOutput`, routed state-machine reduction output, and handwritten state-machine continuation classification are removed. Prediction recognizes the same prepared damage-output type that real execution schedules. Empty settlement target lists and zero settlement damage produce skipped reductions; ordinary damage keeps its previous zero-damage behavior.

Additional verification covers selected settlement target order, empty settlement, detached expiry lineage, and area-projectile captured damage/reporting. State-memory tests check the actual store and specific output payloads rather than removed intermediate bookkeeping fields. No production Python scripts, alternate simulator, new queue, snapshot cache, or allocation strategy was introduced.

### Final verification of the completed refactor

| Check | Result |
|---|---|
| `.github/build-command.ps1` | Passed: Debug x64 game, C++ tests, CLI, schema consumer checks; all targets linked |
| `x64/Debug/kys_tests.exe --reporter compact` | Passed: **998 cases, 43,065 assertions** |
| Focused settlement, expiry provenance, poison continuation, projectile reporting | Passed: 4 cases, 50 assertions |
| Python discovery | Passed: **44 tests** after description-expectation cleanup |
| Original battle goldens | All three 600-frame digests identical; RNG draws remain 45, 60, 60 |
| Existing replay, scenario, and GUI/headless parity coverage | Passed in the complete C++ run; no expected outputs changed |
| Old-path source audit | No remaining old context, area amount array, optional status duration override, generic state-machine bookkeeping fields, routed state-machine output, deferred HP output, or separate state-machine executor |
| `git diff --check` | Passed |

Final logs: `work/effect-completion-final-build.log`, `work/effect-completion-final-tests.log`, `work/effect-completion-final-python.log`, and `work/effect-completion-final-goldens.log`. The unchanged Python failures are `test_equipment_reward_description_separates_character_effects` and `test_protocol_exposes_semantics_schemas_and_actionable_parse_errors`, with the same missing `主彈命中：` and trailing `。` expectations documented in the baseline.

### Python description-expectation cleanup

The two baseline failures were obsolete test expectations. The structured-description assertion now verifies non-empty string content without rejecting the sentence punctuation intentionally emitted by `describeGameplayEffects`. The equipment reward assertion matches the current prose and verifies that both indented character bonuses appear under 韓小瑩 rather than in the general equipment effects. Production descriptions and gameplay were unchanged.

`python -X utf8 -m unittest discover -s tests -p 'test_*.py' -v` passed all **44 tests**. Log: `work/effect-completion-python-cleanup.log`. `git diff --check` passed. No C++ rebuild was required for this test-only cleanup; the completed refactor's build and C++ verification above remain applicable.
