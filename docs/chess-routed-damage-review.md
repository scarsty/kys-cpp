# Routed attack and damage review

## Reproduction and findings

Save slot 1, starting 五絕論劍, reproduced the assertion in
`BattleCastLifecycle::recordActualHpDamage`.

At frame 274, 蛤蟆功's death-explosion status belonged to unit 12 and queued
120 damage against units 2, 8, and 10. Its causal attack belonged to unit 10.
The old `inheritAttackProvenance = true` default copied that attack into each
damage intent's hit-accounting field. The first explosion target was not in
the attack's accepted-contact set.

### Fixed: causal provenance was treated as hit-accounting authority

`EffectExecutionInputs::attack` and the damage origin describe causality.
They may identify the attack that killed a different unit, broke a shield, or
triggered a reaction. They do not establish who dealt the resulting damage
or which targets the original attack hit.

The automatic inheritance switch has been removed. `EffectHitDamageCredit`
is produced only while evaluating an accepted `HitEventData`. The damage
command keeps it only when its effect owner is the attacking cast's source;
each expanded damage target must also equal the accepted contact target.
This preserves same-target contact bonuses, including True-Qi, without adding
death explosions, retargeted damage, or post-resolution reactions to the
triggering attack's contact totals. Those transactions still produce their
own damage reports and retain their causal effect origins.

### Fixed: reactive area projectiles could borrow a foreign or settled cast

`appendAreaProjectileDamageOutput` previously borrowed `triggeringAttack->cast`
unconditionally, ahead of checking the retention policy. A defender-owned
reaction could therefore create a projectile whose payload source and cast
source disagreed. It could also reuse an attack merely retained as historical
event metadata.

Borrowing now requires a retained cast owned by the actual damage source.
Otherwise, the area reaction reserves a fresh source-owned root cast and its
expansion barrier. The resulting projectiles reserve their own attacks and
acquire hit records through actual contact processing.

### Fixed: healing dereferenced retired cast IDs

The subsequent slot-4 倚天屠龍 reproduction exposed a gap in the original
review: damage routes can cross the healing event bridge. Healing kept only a
cast ID, then the event bridge queried the active registry for its provenance.
Historical causes remain legitimate after cast settlement, so that lookup
asserted when the cast had already retired.

Heal requests and both attempted/applied event payloads now carry immutable
cast provenance directly. The common event provenance visitor reads it without
querying the active registry. At event construction, the bridge records whether
the referenced cast is still active; execution uses that lifetime information
when deciding whether descendant damage can reserve work. This also prevents a
nested healing or damage event from treating historical provenance as a live
cast again. The lifecycle's missing-cast assertion remains unchanged.

A synthetic regression covers active and retired casts, cast-event healing,
HealApplied-triggered damage, preserved attribution, actual HP changes, and
release of descendant work. It does not depend on shipped balance values.

## Route inventory

| Route | Attribution and lifetime handling | Review result |
| --- | --- | --- |
| Ordinary HP/MP hits | `BattleCoreAttacks` records accepted contacts; `BattleCoreDamage` queues transactions with their live attack provenance. | Strict hit-membership assertion retained. Damage-intent construction now also asserts source ownership. |
| Accepted-hit side effects | Cooldown extension, stun, and similar non-damage transactions retain causal origin without HP-hit credit. | Earlier defensive-cooldown fix remains applicable; zero HP/MP damage invariant retained. |
| Direct rule/status damage | `prepareDamageOutput` and `appendEffectDamageOutput` preserve effect origin; explicit contact credit is checked against owner and each target. | Fixed shared route, including 蛤蟆功's status-owned death explosion. |
| Retargeted/area direct damage | Each selected unit receives an independent transaction; unmatched targets use descendant work rather than a fabricated contact. | Covered by retargeted-hit regression. |
| Area projectile reactions | Source-owned retained cast, or a fresh root cast; expansion reserves attack work before releasing its barrier. Empty expansions complete or cancel that barrier. | Fixed foreign/settled-cast borrowing; covered by ownership/retention matrix. |
| Nearby tracking follow-ups | `expandBattleProjectileFollowUpCommands` preserves source; `reserveTrackedProjectileFollowUps` reserves a distinct attack under the same cast. | Added source/cast identity assertion at reservation. |
| Bounces | `BattleAttackState::makeBounceAttack` clears copied provenance/work/contact state, reserves a new attack, and transfers its work to the new live attack. | No additional defect identified. |
| Projectile reflection | `BattleCoreAttacks` creates a child cast owned by the reflector and reserves the return attack. Reflection policy excludes reflected returns. | Ownership transfer is explicit; no additional defect identified. |
| Free repeats/copied attacks | `BattleCoreEffects` and `BattleCoreActions` reserve child casts, attack work, and commit barriers before queuing. | No additional defect identified in the reviewed reservation paths. |
| Rescue counters | `makeRescueCounterAttack` creates its own source-owned root cast and attack with `NoEffectRules`. | No inherited hit record; no additional defect identified. |
| Periodic area damage | `appendAreaDamagePulses` queues source-owned rule damage without live attack credit. | Historical attacks are not required for these pulses. |
| Status ticks and absorption settlement | Origins retain producer/trigger information. No accepted-contact credit is manufactured; expired absorption explicitly declines cast retention. | Removed the redundant inheritance override from absorption settlement. |
| Guardian redirection | A separate redirected transaction retains attacker attribution, carries no live hit credit, and cannot redirect again. | No additional defect identified. |
| Healing and heal-triggered damage | Heal payloads own cast provenance; the event bridge separately captures active lifetime. | Fixed retired-cast lookup and nested-event retention; active/retired regression. |
| Descendant completion | Uncredited immediate effect damage reserves delayed cast work when required. Dead targets, blocked requests, normal completion, and battle termination release work through existing completion paths. | Death-reaction regression checks that no cast/work tokens remain. |

## Verification scope

The regression fixtures use explicit rules and amounts. They cover self-retargeted
contact effects, defender death damage, source ownership, settled-cast retention,
and preservation of existing True-Qi contact accounting. The original membership
assertion is unchanged; targets are never inserted merely to make damage recording
succeed.

The review covers the damage-intent entry points and projectile/cast reservation
paths listed above. Full-suite execution and the saved challenge provide additional
integration coverage, but do not exhaust every possible authored effect combination.

The follow-up healing fix was verified against autosave slot 4 / 倚天屠龍
(player victory at frame 515) and slot 1 / 五絕論劍 (player victory at frame 308).
Both replays used the current workspace content and Debug CLI.
Verification passed: Debug game/tests/CLI build, 1,025 C++ test cases with
44,465 assertions, and all 34 Python tests.
