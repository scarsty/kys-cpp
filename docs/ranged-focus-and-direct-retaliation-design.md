# Ranged Focus Allocation and Direct Retaliation Design

Status: approved design; implementation not started
Last updated: 2026-08-30

## 1. Summary

Melee characters currently become the shared nearest target of an opposing ranged formation as soon as they advance ahead of their team. Every ready ranged character independently selects that nearest enemy, so ranged attackers can apply effectively unlimited simultaneous pressure to one melee target. Projectile cancellation provides some mitigation during projectile flight, but the global five-frame cancellation grace makes it unreliable against nearby attackers.

This design addresses those problems in two independently validated milestones:

1. **Milestone A — ranged focus allocation**
   - Keep nearest-enemy selection for movement and immediate threat awareness.
   - Give ranged casts a separate, persistent fire target.
   - Select ranged fire targets through a deterministic, frame-shared focus ledger.
   - Apply focus pressure, in-pass reservations, and hysteresis.
   - Route ordinary casts, auto ultimates, and dead-target retargeting through the same policy.
   - Validate behavior across multiple volleys, not only the first assignment.

2. **Milestone B — direct retaliation and projectile grace removal**
   - Record pair-specific accepted melee engagements against ranged defenders.
   - Mark only the directly engaged ranged unit's qualifying main retaliation projectile as non-cancellable.
   - Remove the global five-frame projectile cancellation grace for ordinary attacks.
   - Keep the current hit-before-cancellation frame ordering.

Milestone A is the melee-survivability fix. Milestone B improves the semantic distinction between a ranged unit personally fighting off a melee attacker and surrounding ranged units contributing ordinary focus fire. They must be built, tested, scenario-validated, and reviewed separately so their replay and balance effects remain attributable.

## 2. Problem statement

The current target flow is effectively:

```text
nearest living enemy
    -> movement target
    -> cast target
    -> pending cast target
    -> projectile preferred target
```

The movement planner recalculates nearest enemy independently for every unit. It does not account for:

- Allied ranged attackers already targeting the enemy.
- Pending casts against the enemy.
- Live projectiles already committed to the enemy.
- Ranged engagement capacity.
- Target persistence beyond simple pending-cast commitment.

This differs from melee focus. Melee attackers are naturally constrained by collision, approach paths, and available positions around a defender. Ranged attackers have no comparable spatial capacity and can all select the same target.

Projectile cancellation does not solve the base problem:

- It is an incidental swept-segment interaction rather than target-aware protection.
- Both attacks currently require five frames of age before becoming candidates.
- Unit hits settle before projectile cancellation is evaluated.
- A nearby projectile can hit and become spent before it is eligible for cancellation.

The design must distribute ranged targeting without making melee units intrinsically tankier or introducing hidden attacker-count damage reduction.

## 3. Goals

### 3.1 Primary goals

- Prevent every ranged character from independently selecting the same marginally closest enemy when reasonable alternatives exist.
- Preserve nearest-enemy behavior for movement and immediate threat response.
- Keep target assignment deterministic and replay-safe.
- Preserve target commitment during a cast windup.
- Make staggered, steady-state ranged fire meaningfully less concentrated, not only the first simultaneous volley.
- Make direct melee engagement pair-specific: the ranged unit being physically engaged can retaliate differently, while surrounding ranged units cannot inherit that behavior.
- Remove the arbitrary global projectile-age grace without adding a launch bubble.
- Keep the implementation decomposed into independently reviewable milestones.

### 3.2 Secondary goals

- Centralize implicit ranged target selection so normal casts, auto ultimates, and dead-target retargeting cannot diverge.
- Make movement-threat and fire-target ownership explicit in runtime state.
- Keep effect-selected, reflected, bounced, and other explicitly targeted attacks outside the general allocator unless they originate from a normal unit cast already counted by the ledger.
- Make the new rules observable through deterministic scenario metrics and focused tests.

## 4. Non-goals

This design does not include:

- Additional melee HP, defence, shields, or raw tank statistics.
- Anti-focus damage reduction based on attacker count.
- A ranged minimum-range penalty or generic ranged retreat behavior.
- A melee-specific targeting immunity.
- A hard ranged-attacker cap per target.
- Target scoring based on melee/ranged role, star, cost, defence, HP, or predicted lethality.
- Full within-frame time-of-impact ordering for projectile and unit contacts.
- Retroactive cover protection when a projectile reaches a unit on the same frame as a projectile intersection.
- A projectile launch bubble or speed-scaled grace radius.
- A general projectile-delivery cleanup in the cancellation system.
- Removal of the existing ultimate projectile cancellation exemption.
- Changes to explicit effect selectors, reflected return targeting, bounce targeting, or scripted target selection unless they currently pass through an implicit nearest-enemy fallback intended for ordinary casts.

## 5. Terminology

### 5.1 Threat target

The enemy a unit reacts to for movement, approach, spacing, and immediate positional danger. This remains the geometrically nearest living enemy under the current movement rules.

### 5.2 Fire target

The enemy selected for a new ranged cast. This is persistent between cast opportunities and chosen through the ranged focus allocator.

### 5.3 Focus ledger

A frame-scoped record of ranged pressure already committed to each living target. It is initialized from persistent battle facts and updated as new casts successfully commit during the frame.

### 5.4 Full focus load

One distinct ranged source's current committed pressure against a target, represented by a valid pending ranged cast or live main ranged projectile.

### 5.5 Residual focus load

An optional, evidence-gated, decaying contribution from a recently completed source-target commitment. It is not part of the initial Milestone A implementation and is added only if multi-volley metrics show that focus collapses between active commitment windows.

### 5.6 Direct melee engagement

A pair-specific relationship established when a melee-style attacker makes accepted contact with a ranged-style defender and the pair remains within the configured engagement exit band.

### 5.7 Direct retaliation projectile

The directly engaged ranged defender's qualifying main projectile when it is explicitly targeted at that exact melee engager. Its cancellation exemption is snapshotted at projectile construction.

## 6. Current code landmarks

The following locations define the existing behavior and expected integration seams:

- Nearest-enemy movement selection: `src/battle/BattleMovement.cpp`
- Movement state and decisions: `src/battle/BattleTypes.h`
- Runtime unit ownership: `src/battle/BattleRuntimeUnits.h`
- Runtime cast-input materialization: `src/battle/BattleCoreActions.cpp`
- Normal action selection: `src/battle/BattleCoreActions.cpp::advanceActionFrameUnits`
- Auto-ultimate commit path: `src/battle/BattleCoreActions.cpp::tryCommitAutoUltimate`
- Cast range and projectile construction: `src/battle/BattleCastSystem.cpp`
- Accepted typed hit path: `src/battle/BattleCoreAttacks.cpp::resolveTypedHitEvent`
- Projectile cancellation candidates: `src/battle/BattleAttackSystem.cpp::appendProjectileCancelEvents`
- Frame pipeline: `src/battle/BattleCore.cpp`
- Runtime geometry: `src/battle/BattleGeometry.cpp` and `src/battle/BattleRuntimeRules.cpp`

## 7. High-level runtime design

The target flow after Milestone A becomes:

```text
movement planning
    -> nearest threat target

prospective cast materialization
    -> exact normal-or-ultimate selection
    -> prospective ranged style and reach

frame focus ledger query
    -> candidate validity
    -> distance + committed focus score
    -> hysteresis
    -> selected fire target

cast planning
    -> exact target-dependent policies
    -> final reach/cast validation
    -> pending cast commitment
    -> focus-ledger reservation
```

After Milestone B, projectile cancellation policy becomes:

```text
ordinary projectile
    -> cancellation-eligible on its first cancellation pass

directly engaged ranged unit's main projectile at exact engager
    -> ignore projectile cancellation for that projectile lifetime

surrounding ranged unit's projectile at the same melee
    -> ordinary cancellation behavior
```

## 8. Runtime state ownership

### 8.1 Movement state

The current generic movement `targetId` fields should be renamed internally to express their actual ownership:

- `BattleUnitState::targetId` -> `threatTargetId`
- `MovementDecision::targetId` -> `threatTargetId`
- `BattleMovementAgentState::targetId` -> `threatTargetId`

All movement tests and helpers must be migrated. This is an internal semantic rename; presentation DTOs or generic event fields need not be renamed unless they directly expose this state.

### 8.2 Targeting runtime state

Add targeting-owned state to each `BattleRuntimeUnitRecord`, conceptually:

```cpp
struct BattleTargetingRuntimeUnit
{
    int fireTargetUnitId = -1;
    std::set<int> meleeEngagerUnitIds;
};
```

Ownership is:

- `movement.threatTargetId`: nearest threat used by movement.
- `targeting.fireTargetUnitId`: persistent preference for the next ranged cast.
- `targeting.meleeEngagerUnitIds`: active enemy melee units directly engaging this ranged unit.
- `pendingCast.targetUnitId`: target already committed by a started cast.

Do not duplicate the fire target in `BattleRuntimeUnit`.

### 8.3 Frame-scoped focus ledger

Use a frame-scoped ledger rather than a fully precomputed assignment map.

The ledger is shared by every implicit ranged target-selection entry point in the frame. It is initialized lazily or at frame start from current persistent commitments, then updated as new casts succeed.

This structure is necessary because auto ultimates can commit through effect-command reduction at different points in the frame. A one-time plan computed only after movement would miss pre-movement or effect-triggered auto-ultimate calls.

The ledger records focus by target and distinct source commitment. It must not rely only on an integer count if doing so could count one source's pending cast and main projectile simultaneously. The semantic unit is one source cast commitment, not every generated projectile.

If a commitment ends later in the same frame, the frame ledger may conservatively retain its reservation until the next frame. The next frame is reseeded from actual persistent state. This avoids order-sensitive removal and does not create long-lived stale load.

## 9. Prospective cast materialization

### 9.1 Required ordering

The allocator must not guess which skill a unit will cast.

For each ordinary ready caster:

1. Read the unit's action plan seed.
2. Materialize the prospective cast input through the existing normal-versus-ultimate selection.
3. Determine the selected prospective skill, ranged style, effective reach, and relevant cast policies.
4. Pass that exact prospective result to the focus allocator.
5. Inject the selected fire target into the same input.
6. Apply exact target-dependent runtime policies.
7. Run final cast validation.
8. Add a focus reservation only if the cast is accepted.

The action system must consume the same materialized prospective cast object used by the allocator. It must not independently select normal versus ultimate a second time.

### 9.2 Target-dependent policies

Target- and provenance-dependent runtime policies can rematerialize reach or ranged style after a target exists. The implementation must explicitly audit whether any such policy can:

- Reduce effective reach.
- Change ranged versus melee style.
- Change forced-ranged behavior.
- Change the selected operation in a way that invalidates allocator eligibility.

The preferred implementation is to factor a shared target-aware cast-profile helper used by both candidate validation and final cast planning. The selected target and materialized target-aware profile should travel together.

Regardless of implementation structure, the final `BattleCastPlanner` remains authoritative:

- A target that becomes invalid under exact policies must not start a cast.
- A rejected cast must not leave a focus reservation.
- A rejected ranged cast must not silently fall back to nearest enemy and bypass the allocator.

### 9.3 Ordinary action path

The current ordinary action path already creates a prospective `BattleCastInput` before `refreshedCastInput` supplies a target. The allocator should operate at that seam inside action processing rather than before prospective skill selection.

### 9.4 Auto-ultimate path

The auto-ultimate path currently calls `refreshedCastInput` with an empty movement result, which forces the nearest-enemy fallback. It must explicitly:

1. Materialize the actual ultimate prospective cast.
2. Query the shared frame focus ledger.
3. Select and inject a fire target.
4. Run exact policy and cast validation.
5. Reserve focus only after a successful auto-ultimate commit.

An empty movement result must no longer imply nearest-enemy targeting for a ranged auto ultimate.

### 9.5 Dead pending-target retargeting

An already-started cast keeps its target while that target is alive.

If the target dies before release:

- A ranged pending cast requests a focus-aware replacement using its stored, already-selected pending skill profile.
- A melee pending cast may retain the current nearest-enemy fallback.
- If no valid replacement exists, the existing cast cancellation/lifecycle cleanup remains authoritative.
- A successful replacement creates or transfers the ledger reservation to the new target.

## 10. Nearest-enemy call-site policy

The implementation audit must classify nearest-enemy calls by purpose rather than ban every use.

Allowed uses:

- Movement threat selection.
- Melee attack targeting.
- A pure `any live enemy exists` gate before constructing a prospective action.
- Explicit behavior whose design is specifically nearest-target based, such as an intentional effect or bounce policy.

Disallowed uses:

- Ordinary implicit ranged cast target acquisition.
- Ranged auto-ultimate target acquisition.
- Ranged dead-pending-target replacement.
- Any fallback that silently bypasses the focus ledger after a ranged allocation or cast-validation failure.

The existing `findNearestEnemyUnitId(...) >= 0` check in the normal action loop is an existence gate and should be explicitly whitelisted or replaced by a clearer `hasLiveEnemy()` query.

## 11. Focus-ledger semantics

### 11.1 Full-load contributors

Count one full focus load for each distinct source commitment represented by:

- A valid pending ranged cast targeting a living enemy.
- A valid live main ranged or tracking projectile with a living preferred target.
- A newly accepted ordinary ranged cast in the current frame.
- A newly accepted ranged auto ultimate in the current frame.
- A direct retaliation projectile/cast; cancellation exemption does not remove its offensive pressure from focus accounting.

Ultimate commitments count toward focus even though ultimate projectiles retain their cancellation exemption.

### 11.2 Non-contributors

Do not add independent full loads for:

- A remembered `fireTargetUnitId` with no current commitment.
- A cooling-down unit merely facing or moving relative to a target.
- Melee or dash contact attacks.
- Side projectiles generated by a cast already represented by its source commitment.
- Spread projectiles.
- Bounce descendants.
- Nearby or area follow-up projectiles.
- Reflected returns.
- Finished, spent, target-lost, or invalid attacks.
- Explicit effect-created attacks that are not the ordinary unit cast represented by the ledger.

### 11.3 Reservation timing

- Seed the ledger from valid persistent commitments.
- Do not reserve merely because a candidate target was considered.
- Reserve after final cast acceptance.
- Make the reservation visible to later target queries in the same frame.
- Do not reserve a failed, cancelled, or invalid cast.
- Use deterministic frame processing order already defined by the battle pipeline and stable unit/effect command order.

## 12. Candidate validity

A target is initially valid for a prospective ranged cast when:

- The target is alive.
- The target is on the opposing team.
- The target satisfies the selected prospective skill's targeting domain.
- The target is within the prospective target-aware effective reach.
- The cast system would otherwise permit targeting it.

Do not assign an out-of-range target merely to spread focus. If no target is currently valid, the unit does not begin a cast; movement continues using its nearest threat.

When only one enemy is valid, every ready ranged attacker may target it. Focus load is a soft cost, never a prohibition.

Explicit target selectors remain authoritative and do not enter general candidate selection.

## 13. Target scoring and determinism

### 13.1 Initial score

Use a simple score:

```text
candidate score = deterministic distance cost
                + full focus load * focus penalty
                + optional residual focus load
```

Initial tuning values:

- Full focus penalty: equivalent to approximately **3 battlefield tiles** of distance per committed ranged source.
- Fire-target hysteresis margin: equivalent to approximately **0.75 battlefield tile**.
- Equal-score target tie-break: lower target unit ID.

These are initial tuning values, not permanent semantic guarantees. They may be adjusted only through the scenario-validation process.

### 13.2 Distance representation

Use deterministic integer/fixed-point distance cost derived from the battle math layer. Do not depend on platform-sensitive floating-point equality for target order.

The score should approximate linear distance so the three-tile focus penalty retains an understandable meaning. Avoid simply adding a linear penalty to raw squared distance without explicitly accounting for the changed scale.

### 13.3 In-frame incremental load

After a cast is accepted, increment the selected target's ledger load before the next implicit ranged target query.

This ensures that staggered processing within one frame does not allow every ready unit to observe the same zero-load state.

### 13.4 Soft pressure, not a hard cap

The allocator never declares a target saturated or forbidden.

A heavily focused nearby target may still win when all alternatives are much farther away. The last living enemy remains targetable by every attacker.

## 14. Fire-target persistence and hysteresis

The unit's existing `fireTargetUnitId` is considered during a new cast opportunity.

Retain the current fire target when:

- It remains alive and hostile.
- It remains valid for the selected prospective skill.
- Its adjusted score is no worse than the best candidate by more than the hysteresis margin.

Switch when:

- The target dies or becomes invalid.
- It leaves usable reach.
- Another candidate's adjusted score beats it by more than the hysteresis margin.

The remembered target does not create a full team load on its own. Hysteresis is a per-source preference, not committed team pressure.

Pending casts do not use hysteresis because their target is already committed. They retarget only on target invalidation under the dead-target policy.

## 15. Steady-state focus validation

### 15.1 Why first-assignment validation is insufficient

Pending casts and live main projectiles provide focus visibility only during windup and flight. Once the projectile is spent, a long cooldown gap can leave the ledger empty.

A staggered ranged attacker making a decision during that gap may see pure distance scores and select the advancing nearest melee. A hysteresis margin smaller than the distance gap cannot preserve a previously spread target.

Therefore Milestone A must be evaluated over multiple complete attack cycles.

### 15.2 Required metrics

For each validation frame, calculate the number of **distinct ranged source commitments** against each target.

Report at least:

- Time-averaged maximum target load, conditioned on at least one active ranged commitment that frame.
- 90th or 95th percentile maximum target load.
- Maximum observed target load.
- Fraction of active-fire frames where one target has at least three committed ranged sources.
- Fraction of active-fire frames where one target owns more than half of all committed ranged sources.
- Per-source fire-target switch counts.
- Target-load distribution across multiple volleys.

Do not average only over all battle frames; long zero-load cooldown periods would hide burst concentration.

The metrics may be implemented in test/scenario helpers rather than production presentation state.

### 15.3 Evidence-gated residual load

The initial Milestone A implementation does not include residual focus load.

If the required metrics show that staggered steady-state fire remains concentrated, revise the scoring model with a short decaying post-commitment contribution:

```text
pending or live main attack:
    full load

recently completed source-target commitment:
    linearly or stepwise decaying residual load

remembered target without recent commitment:
    zero team load
```

Requirements for the residual model:

- Key it to an actual source-target ranged commitment.
- Ignore it when the target is dead or invalid.
- Do not stack full and residual contribution for the same source commitment.
- Do not derive it from role, HP, or melee status.
- Select the decay duration from observed cooldown gaps rather than choosing it in advance.
- Rerun the same multi-volley metrics before accepting it.

This is a scoring-model revision, not a specialized melee exception.

## 16. Direct melee engagement

### 16.1 Activation

Record a direct engagement pair after a hit passes attack suppression and dodge handling and is accepted by the hit resolver.

The pair qualifies when:

- Source and target are living enemies at accepted contact.
- The attacker is currently melee style.
- The defender is currently ranged style.
- The attack delivery is contact rather than projectile or effect.
- The hit is accepted rather than suppressed or dodged.

Use accepted physical contact, not final HP loss. The pair is still established when:

- Defence reduces final damage to zero.
- A shield absorbs the damage.
- A block or damage-layer effect prevents HP loss after contact was accepted.

Do not establish a pair for:

- Suppressed hits.
- Dodged hits.
- Projectile or splash damage.
- Scripted effect damage without direct contact.

### 16.2 Pair ownership

When melee `M` contacts ranged `R`:

```text
R.meleeEngagerUnitIds contains M
```

The relationship is pair-specific. It does not place `R` into a global close-combat mode against every enemy, and it does not grant surrounding ranged allies any special behavior against `M`.

A ranged defender may track multiple active melee engagers. One melee attacker may simultaneously engage multiple ranged defenders if it establishes accepted contact with each and remains within each pair's exit band.

### 16.3 No forced retaliation target

Direct engagement does not force the ranged unit to target its engager.

The normal focus allocator still selects the ranged unit's fire target. The direct retaliation policy applies only if the selected target is an active melee engager of that ranged unit.

The direct ranged unit's accepted cast still contributes full focus load to the target.

### 16.4 Expiration

An active pair remains valid while:

- Both units are alive.
- They remain enemies.
- The attacker remains melee style.
- The defender remains ranged style.
- Their distance is less than or equal to the engagement exit band.

The exit band is:

```text
meleeAttackReach + engagementDeadband
```

With current derived geometry:

- `meleeAttackReach = 2.75 tiles`
- `engagementDeadband = 0.50 tile`
- Exit band = `3.25 tiles`, inclusive

Validate the pair whenever it is queried and prune invalid pairs after movement. This prevents early or effect-triggered cast paths from using stale engagement state.

A dash-through or other contact that finishes with the units already outside the exit band does not create a usable retaliation state on the next selection. Direct engagement represents a melee unit that remains in the ranged unit's face, not any historical contact.

No separate frame timer or per-pair custom radius is used.

## 17. Direct retaliation projectile policy

### 17.1 Qualification

A spawned attack receives direct retaliation cancellation exemption only when all are true:

- Source is currently ranged style for the selected cast.
- Preferred target is an active melee engager of the source.
- The attack is the cast's main projectile.
- The attack is explicitly targeted at that engager.

When qualified, set the existing projectile cancellation policy:

```cpp
request.initial.ignoreProjectileCancel = true;
```

Do not add a duplicate boolean unless presentation later needs a typed reason.

### 17.2 Non-qualifying descendants

Do not grant the exemption automatically to:

- Side projectiles.
- Spread projectiles.
- Extra projectiles aimed at other enemies.
- Bounce descendants.
- Nearby or area follow-ups.
- Reflected returns.
- Every projectile in a multi-projectile cast.
- Projectiles from surrounding ranged allies attacking the same melee.

Only a main projectile actually targeted at the exact active engager qualifies.

### 17.3 Snapshot semantics

Cancellation exemption is snapshotted into the attack payload at construction.

- If engagement exists at spawn and ends afterward, the existing projectile remains exempt.
- If engagement does not exist at spawn and begins afterward, the existing projectile remains ordinary.
- A later cast re-evaluates current engagement and may receive a different policy.

The projectile system does not query live engagement state every frame.

### 17.4 Multiple defenders

If melee `M` is actively engaging ranged `R1` and `R2`, and both independently select `M` as fire target, each qualifying main projectile receives the exemption.

An unengaged ranged `R3` attacking `M` remains ordinary in the same scenario.

There is no per-melee quota on retaliation exemption.

## 18. Projectile cancellation grace removal

### 18.1 Removed rule

Remove the global `projectileGraceFrames` age filter from:

- `BattleAttackState`.
- Runtime-session configuration.
- Headless runtime snapshots and diagnostics.
- Unit/integration test setup.
- Replay/hash state if represented.

Ordinary cancellable attacks become eligible on their first cancellation pass.

### 18.2 Retained rules

Retain the existing exclusions and behavior for:

- `noHurt` or spent attacks.
- `ignoreProjectileCancel` attacks.
- Ultimate casts.
- Same-team attack pairs.
- Current cancellation-strength and accumulated weakening rules.
- Current deterministic candidate pairing unless a separate future design changes it.

### 18.3 Retained frame ordering

Do not reorder the frame pipeline.

The accepted contract remains:

```text
advance attacks
    -> detect and settle unit hits
    -> detect projectile cancellation
```

Consequences:

- An earlier-flight-frame intersection can weaken or stop a projectile.
- A projectile that reaches and is spent on a unit before the cancellation pass is not retroactively stopped.
- If unit contact and projectile intersection occur on the same frame, hit settlement remains first.
- Cover fire is an in-flight interaction, not a last-instant shield.

No time-of-impact scheduler is introduced.

### 18.4 General ranged-duel behavior change

Grace removal affects all ordinary opposing projectiles, not only focus fire against melee units.

Opposing casts can cancel on their first sweeps near their sources when:

- Their swept segments overlap.
- Neither attack is already spent on a same-frame unit hit.
- Other cancellation rules permit the pair.

This is intended new behavior and must be pinned by tests and reviewed as a separate replay-impact milestone.

### 18.5 Contact-volume caveat

Do not assume ordinary melee contact volumes already carry `ignoreProjectileCancel`.

The `ignoreProjectileCancel = skillId < 0` assignment in `BattleHitResolver.cpp` belongs to a skill-less nearby tracking follow-up projectile, not an ordinary melee volume. Area follow-up projectiles also carry explicit immunity, but neither case solves ordinary contact-volume eligibility.

Ordinary melee volumes normally hit immediately, settle, become spent/`noHurt`, and therefore do not enter the later cancellation pass. Preserve and test that practical behavior.

Do not proactively add a projectile-delivery filter in Milestone B. If grace removal exposes a real missed or stationary contact volume cancelling projectiles on its first frame, stop and review that concrete case rather than silently expanding scope.

## 19. Detailed test plan

### 19.1 Prospective cast and allocator unit tests

1. Normal versus ultimate selection is materialized before allocation.
2. Allocator and final cast consume the same selected prospective skill.
3. Target-dependent reach changes are reflected in candidate validity or final rejection without nearest fallback.
4. Rejected casts do not leave ledger reservations.
5. No focus present selects the nearest valid enemy.
6. One committed ranged source makes a slightly farther unfocused enemy win when the focus penalty exceeds the distance gap.
7. Multiple same-frame ready attackers see incremental reservations and distribute deterministically.
8. Soft overflow permits a focused target to win when alternatives are sufficiently worse.
9. The only valid enemy remains targetable by every ranged attacker.
10. Out-of-range alternatives are not selected merely to spread focus.
11. A pending ranged cast contributes one full load.
12. A live main projectile contributes one full load.
13. One cast with main, side, extra, and bounce projectiles still represents one source commitment.
14. Ultimate commitment contributes focus load.
15. Direct retaliation commitment contributes focus load.
16. Remembered fire target without a live commitment contributes no full load.
17. Hysteresis retains a near-equal current fire target.
18. A materially better candidate overrides hysteresis.
19. Equal scores use stable target-ID tie-breaking.
20. Reordered unit storage does not change results where semantic processing order is unchanged.
21. Both teams' ledgers remain independent.

### 19.2 Action integration tests

1. A ranged unit's threat target remains the nearest enemy while its fire target is another enemy.
2. Melee units continue using nearest threat for their ordinary target.
3. Four ranged units do not all acquire the first melee when reasonable alternatives exist.
4. Two advancing melee units receive distributed assignments.
5. An active pending cast is not retargeted because a later frame produces another preferred fire target.
6. A dead pending ranged target is replaced through focus-aware selection.
7. A dead pending melee target retains nearest-enemy replacement behavior.
8. A ranged auto ultimate explicitly receives a focus-aware target despite having no movement result.
9. Auto-ultimate reservation is visible to later ordinary selections in the same frame.
10. Ordinary reservation is visible to a later auto ultimate in the same frame.
11. Explicit effect-selected targets bypass the allocator unchanged.
12. The `any live enemy exists` gate remains functional and is not treated as target selection.
13. Once other enemies die or leave validity, all ranged attackers may converge on the last valid target.

### 19.3 Steady-state scenario tests and metrics

Run Scenario A and Scenario B for multiple complete cooldown cycles and report every metric from Section 15.

Required assertions should focus on stable invariants, while exact tuning thresholds may be reviewed from deterministic reports before being pinned:

- Focus is more distributed than the nearest-only baseline.
- Distribution persists beyond the first volley.
- The allocator does not produce rapid target oscillation.
- The allocator does not leave attackers idle when valid targets exist.
- A far alternative does not always defeat a reasonably focused nearby target.

If residual load is added, rerun identical seeded scenarios and compare the same reports.

### 19.4 Direct engagement tests

1. Accepted melee contact records the pair.
2. Suppressed contact does not record the pair.
3. Dodged contact does not record the pair.
4. Accepted contact with zero final HP damage records the pair.
5. Shield-absorbed accepted contact records the pair.
6. Projectile or effect damage does not record the pair.
7. One ranged defender can track two active melee engagers.
8. One melee can engage two ranged defenders after accepted contact with each.
9. Contact at exactly ordinary `meleeAttackReach` establishes the pair.
10. A maximum-distance ordinary pair survives its first expiry pass.
11. Normal accepted-hit knockback near the boundary does not unintentionally clear the pair.
12. A pair remains active at the inclusive exit-band boundary.
13. A pair clears immediately outside the exit band.
14. Death, team invalidation, or style invalidation clears or invalidates the pair.
15. A dash-through/contact ending outside the exit band does not provide a later active retaliation state.

### 19.5 Retaliation snapshot tests

1. Qualifying main projectile receives cancellation exemption.
2. Surrounding unengaged ranged projectile at the same melee remains ordinary.
3. Side, spread, bounce, follow-up, and unrelated-target projectiles do not inherit the exemption.
4. Engagement active at spawn then ending leaves the existing projectile exempt.
5. Engagement absent at spawn then beginning leaves the existing projectile ordinary.
6. A future cast after engagement ends is ordinary.
7. Two ranged defenders engaged by the same melee each receive exemption on qualifying main projectiles.
8. An unengaged third ranged defender remains ordinary in that same scenario.
9. Existing ultimate exemption remains unchanged.

### 19.6 Grace-removal and cancellation tests

1. Ordinary opposing projectiles are eligible on their first cancellation pass.
2. Two newly spawned opposing projectiles with overlapping first sweeps cancel or weaken when no unit hit spends them first.
3. Newly spawned opposing projectiles that hit units on the same frame preserve hit-before-cancellation ordering.
4. Direct retaliation projectile ignores cancellation.
5. Same-team attacks remain excluded.
6. Ultimate attacks remain excluded.
7. Existing equal-strength weakening behavior remains unchanged.
8. A normally settled melee contact volume does not participate in later cancellation.
9. A missed or stationary contact-volume case is explicitly characterized; if it begins cancelling only because grace was removed, stop for design review rather than silently changing candidate typing.

### 19.7 Determinism and replay tests

- Replay hashes.
- Golden battle outputs.
- Headless battle digests.
- GUI/headless parity.
- Scenario target IDs and death order.
- Unit-storage-order tests where the battle contract promises order independence.
- Stable seeded target assignment across repeated runs.

Do not update golden results blindly. Confirm that every changed result belongs to the milestone under review.

## 20. Scenario validation matrix

### Scenario A — one melee versus four ranged, alternatives available

Verify across multiple volleys:

- The melee remains meaningfully threatened.
- It is not selected by all four ranged units merely for being marginally closest.
- Focus does not collapse back onto it during cooldown gaps.
- Surrounding ranged projectiles use ordinary cancellation behavior in Milestone B.
- A ranged unit actually engaged by the melee can use direct retaliation.

### Scenario B — two melee advancing together

Verify:

- Pressure distributes rather than breaking entirely toward lower unit ID.
- Distribution remains stable across staggered cooldowns.
- Target switching stays within acceptable bounds.
- Both melee units remain valid focus targets; neither is protected by a hard cap.

### Scenario C — one remaining enemy

Verify:

- Every ready ranged unit can eventually target it.
- Focus pressure never causes idle attackers when no alternative exists.

### Scenario D — melee reaches one ranged unit in a backline group

Verify:

- The contacted ranged unit can produce direct retaliation when it selects that melee.
- Nearby unengaged ranged allies remain ordinary.
- Every attacker still contributes normally to focus load.
- No global melee immunity or damage reduction exists.

### Scenario E — large distance difference

Verify:

- The soft focus penalty does not force an absurdly distant target.
- Additional nearby focus remains possible when alternatives are materially worse.

### Scenario F — close ranged duel after grace removal

Verify separately in Milestone B:

- First-sweep projectile cancellation can occur when attacks remain live through the cancellation pass.
- Same-frame unit hits retain priority.
- Replay changes are attributable to grace removal rather than target allocation.

## 21. Implementation milestones

### 21.1 Milestone A — ranged focus allocation

### A1. Refactor prospective cast materialization

- Expose the exact prospective normal/ultimate selection before implicit target acquisition.
- Reuse the same prospective input/profile through allocation and action planning.
- Audit target-dependent policy effects on reach and style.

### A2. Separate threat and fire target state

- Rename internal movement target fields to `threatTargetId`.
- Add `fireTargetUnitId` targeting state.
- Preserve pending-cast commitment semantics.

### A3. Add the shared frame focus ledger

- Seed from pending ranged casts and live main projectiles.
- Deduplicate by source commitment.
- Support ordinary and effect-triggered auto-ultimate queries.
- Reserve only accepted casts.

### A4. Integrate every implicit ranged target path

- Normal cast planning.
- Auto ultimates.
- Dead pending-target replacement.
- Preserve and whitelist the any-enemy existence gate.

### A5. Add allocator, integration, and determinism tests

- Implement Sections 19.1 through 19.3.

### A6. Run multi-volley validation

- Produce the Section 15 metrics for Scenarios A through E.
- Compare to the nearest-only baseline.

### A7. Decide residual load from evidence

- If steady-state focus remains spiky, implement and validate decaying residual pressure.
- If the initial model is stable, do not add residual state.

### A8. Verify and land Milestone A independently

- Run `.github/build-command.ps1`.
- Run the complete unit-test suite.
- Review replay/golden changes individually.
- Confirm GUI/headless parity.
- Land before beginning Milestone B.

### 21.2 Milestone B — direct retaliation and grace removal

### B1. Add and expire pair-specific engagement state

- Record accepted melee contact.
- Validate and prune spatially.
- Add boundary, knockback, multi-pair, and dash-through coverage.

### B2. Snapshot direct retaliation policy

- Mark only qualifying main projectiles.
- Add both snapshot directions and multiple-defender coverage.

### B3. Remove global grace

- Remove runtime, session, headless, and test state.
- Preserve hit ordering and all other agreed cancellation rules.

### B4. Add general ranged-duel and contact-volume coverage

- Pin first-sweep cancellation.
- Pin same-frame hit priority.
- Preserve ordinary settled melee behavior.

### B5. Verify and land Milestone B independently

- Run `.github/build-command.ps1` again.
- Run the complete unit-test suite again.
- Run Scenario D and F plus relevant replay cases.
- Review Milestone B replay churn separately.

## 22. Initial tuning values and allowed tuning surface

Initial values:

- Full focus penalty: 3 tiles.
- Fire-target hysteresis margin: 0.75 tile.
- Engagement exit band: `meleeAttackReach + engagementDeadband`, inclusive.
- Residual focus duration: none initially; evidence-gated.

Allowed initial tuning changes after scenario evidence:

- Full focus penalty.
- Hysteresis margin.
- Residual-load duration and decay shape if the validation gate requires it.
- Engagement exit band only if the exact boundary tests reveal an actual geometry mismatch.

Do not respond to an unusual scenario by adding target-role, melee-status, HP, defence, or attacker-count exceptions without a separate design review.

## 23. Migration and cleanup

Milestone A migration:

- Rename internal movement target fields and migrate all callers/tests.
- Remove ranged reliance on movement target in `refreshedCastInput`.
- Replace ranged nearest fallback paths with focus-aware selection.
- Preserve melee and existence-gate nearest uses.
- Update replay/golden fixtures only after semantic review.

Milestone B migration:

- Remove `projectileGraceFrames` from all runtime and test structures.
- Migrate headless serialization/analysis state without backward compatibility.
- Preserve existing `ignoreProjectileCancel` users.
- Update replay/golden fixtures separately from Milestone A.

## 24. Completion criteria

### Milestone A is complete when

- Movement threat and ranged fire target are structurally separate.
- Prospective skill selection occurs before ranged allocation and is not duplicated inconsistently.
- The allocator uses a shared frame focus ledger.
- Normal casts, auto ultimates, and ranged dead-target replacement use the allocator.
- The any-enemy existence gate remains intact and clearly classified.
- Pending casts remain committed while their target lives.
- Focus assignment is deterministic.
- Multi-volley metrics demonstrate acceptable steady-state distribution, with residual load added only if required.
- Full build, unit tests, scenario validation, replay review, and GUI/headless parity pass.

### Milestone B is complete when

- Accepted direct melee engagement is pair-specific and spatially validated.
- Boundary behavior is pinned.
- Only qualifying main retaliation projectiles receive exemption.
- Exemption is snapshotted at spawn.
- Multiple ranged defenders can independently retaliate against the same engager.
- Ordinary attacks no longer use a global five-frame cancellation grace.
- First-sweep ranged cancellation and same-frame hit priority are both pinned.
- Ordinary settled melee contact behavior is preserved or any newly exposed case is brought back for review.
- Full build, unit tests, scenario validation, replay review, and GUI/headless parity pass separately from Milestone A.

## 25. Final accepted limitations

- Ranged focus remains soft rather than capped; sufficiently bad alternatives can still produce concentrated fire.
- An advanced melee can still be punished by multiple ranged attackers.
- Cover fire is opportunistic projectile interaction, not guaranteed ally protection.
- Same-frame projectile intersection does not prevent a unit hit already settled earlier in the frame.
- Direct retaliation is a projectile construction policy, not a dynamically changing mid-flight state.
- Ultimate projectile cancellation exemption remains unchanged.
- Explicit effect targeting remains outside the general allocator unless separately designed.

These limitations are intentional. The design solves unbounded implicit nearest-target focus without converting melee survivability into a collection of specialized defensive exceptions.
