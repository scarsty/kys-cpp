#include "BattleCore.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <set>
#include <utility>
#include <vector>

namespace KysChess::Battle
{
namespace
{
BattlePresentationUnitSnapshot toPresentationUnit(const BattleUnitState& unit)
{
    return {
        unit.id,
        unit.realRoleId,
        unit.name,
        unit.team,
        unit.alive,
        0,
        0,
        0,
        0,
        0,
        0,
        unit.position,
        unit.velocity,
    };
}

void applyStatusSnapshot(BattlePresentationUnitSnapshot& snapshot, const std::vector<BattleStatusUnitState>& statusUnits)
{
    auto it = std::find_if(statusUnits.begin(), statusUnits.end(), [&](const BattleStatusUnitState& unit)
        {
            return unit.id == snapshot.id;
        });
    if (it == statusUnits.end())
    {
        return;
    }

    snapshot.alive = it->alive;
    snapshot.hp = it->hp;
    snapshot.maxHp = it->maxHp;
    snapshot.invincible = it->invincible;
}

BattlePresentationSnapshot makePresentationSnapshot(const BattleFrameState& state)
{
    BattlePresentationSnapshot snapshot;
    snapshot.frame = state.world.frame;
    snapshot.units.reserve(state.world.units.size());
    for (const auto& unit : state.world.units)
    {
        auto presentationUnit = toPresentationUnit(unit);
        applyStatusSnapshot(presentationUnit, state.status.units);
        snapshot.units.push_back(std::move(presentationUnit));
    }
    return snapshot;
}

const char* textForMovementEvent(BattleEventType type)
{
    switch (type)
    {
    case BattleEventType::Movement:
        return "movement";
    case BattleEventType::BlockedByAlly:
        return "blocked-by-ally";
    case BattleEventType::BlockedByWall:
        return "blocked-by-wall";
    case BattleEventType::SlotChanged:
        return "slot-changed";
    case BattleEventType::DashStart:
        return "dash-start";
    case BattleEventType::DashEnd:
        return "dash-end";
    case BattleEventType::AttackReady:
        return "attack-ready";
    }
    assert(false);
    return "movement";
}

BattlePresentationEvent toPresentationEvent(const BattleEvent& event)
{
    BattlePresentationEvent presentation;
    presentation.type = BattlePresentationEventType::StatusLog;
    presentation.sourceUnitId = event.unitId;
    presentation.targetUnitId = event.targetId;
    presentation.text = textForMovementEvent(event.type);
    presentation.position = event.to;
    presentation.amount = static_cast<int>(event.value);
    return presentation;
}

const BattleAttackInstance* findAttack(const BattleAttackWorld& world, int attackId)
{
    if (auto it = std::find_if(world.attacks.begin(), world.attacks.end(), [&](const BattleAttackInstance& attack)
        {
            return attack.id == attackId;
        }); it != world.attacks.end())
    {
        return &*it;
    }
    return nullptr;
}

const BattleProjectileCancelBaseDamage* findProjectileCancelBaseDamage(
    const BattleFrameState& state,
    int attackId,
    int otherAttackId)
{
    auto it = std::find_if(
        state.projectileCancel.baseDamages.begin(),
        state.projectileCancel.baseDamages.end(),
        [&](const BattleProjectileCancelBaseDamage& input)
        {
            return input.attackId == attackId
                && (input.otherAttackId < 0 || input.otherAttackId == otherAttackId);
        });
    return it != state.projectileCancel.baseDamages.end() ? &*it : nullptr;
}

double takeRuntimePercentRoll(BattleFrameState& state)
{
    if (state.runtime.nextPercentRoll < state.runtime.percentRolls.size())
    {
        const double roll = state.runtime.percentRolls[state.runtime.nextPercentRoll++];
        assert(roll >= 0.0 && roll < 100.0);
        return roll;
    }
    return 0.0;
}

void applyAttackContext(BattlePresentationEvent& presentation, const BattleAttackWorld& world, int attackId)
{
    presentation.effectId = attackId;
    if (const auto* attack = findAttack(world, attackId))
    {
        presentation.sourceUnitId = attack->state.attackerUnitId;
        presentation.targetUnitId = attack->state.preferredTargetUnitId;
        presentation.durationFrames = attack->state.totalFrame;
        presentation.visualEffectId = attack->state.visualEffectId;
        presentation.position = attack->state.position;
        presentation.velocity = attack->state.velocity;
        presentation.operationKind = toLegacyOperationType(attack->state.operationType);
    }
}

void applyAttackContext(BattleGameplayEvent& gameplay, const BattleAttackWorld& world, int attackId)
{
    gameplay.effectId = attackId;
    if (const auto* attack = findAttack(world, attackId))
    {
        gameplay.sourceUnitId = attack->state.attackerUnitId;
        gameplay.position = attack->state.position;
    }
}

BattlePresentationEvent toProjectileSpawnPresentationEvent(
    const BattleAttackWorld& world,
    int attackId)
{
    BattlePresentationEvent presentation;
    presentation.type = BattlePresentationEventType::ProjectileSpawned;
    applyAttackContext(presentation, world, attackId);
    return presentation;
}

std::vector<BattlePresentationEvent> toPresentationEvents(
    const BattleAttackEvent& event,
    const BattleAttackWorld& world)
{
    BattlePresentationEvent presentation;
    applyAttackContext(presentation, world, event.attackId);

    switch (event.type)
    {
    case BattleAttackEventType::AttackSpawned:
        presentation.type = BattlePresentationEventType::ProjectileSpawned;
        presentation.effectId = event.attackId;
        presentation.sourceUnitId = event.sourceUnitId;
        presentation.targetUnitId = event.unitId;
        presentation.durationFrames = event.totalFrame;
        presentation.visualEffectId = event.visualEffectId;
        presentation.position = event.position;
        presentation.velocity = event.velocity;
        presentation.operationKind = toLegacyOperationType(event.operationType);
        break;
    case BattleAttackEventType::Moved:
        presentation.type = BattlePresentationEventType::ProjectileMoved;
        break;
    case BattleAttackEventType::Hit:
        presentation.type = BattlePresentationEventType::ProjectileHit;
        presentation.targetUnitId = event.unitId;
        break;
    case BattleAttackEventType::Expired:
        presentation.type = BattlePresentationEventType::ProjectileExpired;
        break;
    case BattleAttackEventType::TargetLost:
        presentation.type = BattlePresentationEventType::ProjectileTargetLost;
        presentation.targetUnitId = event.unitId;
        presentation.amount = -1;
        break;
    case BattleAttackEventType::ProjectileCancel:
        presentation.type = BattlePresentationEventType::ProjectileCancelled;
        presentation.amount = event.otherAttackId;
        break;
    case BattleAttackEventType::Bounce:
        presentation.type = BattlePresentationEventType::ProjectileBounced;
        presentation.targetUnitId = event.unitId;
        presentation.amount = event.otherAttackId;
        break;
    }
    std::vector<BattlePresentationEvent> presentations;
    presentations.push_back(std::move(presentation));
    if (event.type == BattleAttackEventType::Bounce)
    {
        presentations.push_back(toProjectileSpawnPresentationEvent(world, event.otherAttackId));
    }
    return presentations;
}

BattleGameplayEvent toGameplayEvent(
    const BattleAttackEvent& event,
    const BattleAttackWorld& world)
{
    BattleGameplayEvent gameplay;
    applyAttackContext(gameplay, world, event.attackId);

    switch (event.type)
    {
    case BattleAttackEventType::AttackSpawned:
        gameplay.type = BattleGameplayEventType::AttackSpawned;
        gameplay.effectId = event.attackId;
        gameplay.sourceUnitId = event.sourceUnitId;
        gameplay.targetUnitId = event.unitId;
        gameplay.position = event.position;
        break;
    case BattleAttackEventType::Moved:
        gameplay.type = BattleGameplayEventType::ProjectileMoved;
        break;
    case BattleAttackEventType::Hit:
        gameplay.type = BattleGameplayEventType::ProjectileHit;
        gameplay.targetUnitId = event.unitId;
        break;
    case BattleAttackEventType::Expired:
        gameplay.type = BattleGameplayEventType::ProjectileExpired;
        break;
    case BattleAttackEventType::TargetLost:
        gameplay.type = BattleGameplayEventType::ProjectileCancelled;
        gameplay.targetUnitId = event.unitId;
        break;
    case BattleAttackEventType::ProjectileCancel:
        gameplay.type = BattleGameplayEventType::ProjectileCancelled;
        gameplay.otherAttackId = event.otherAttackId;
        break;
    case BattleAttackEventType::Bounce:
        gameplay.type = BattleGameplayEventType::AttackSpawned;
        gameplay.effectId = event.otherAttackId;
        gameplay.targetUnitId = event.unitId;
        if (const auto* sourceAttack = findAttack(world, event.attackId))
        {
            gameplay.sourceUnitId = sourceAttack->state.attackerUnitId;
        }
        if (const auto* spawnedAttack = findAttack(world, event.otherAttackId))
        {
            gameplay.position = spawnedAttack->state.position;
        }
        break;
    }
    return gameplay;
}

void syncAttackUnitsFromWorld(BattleFrameState& state)
{
    for (auto& attackUnit : state.attacks.units)
    {
        auto it = std::find_if(state.world.units.begin(), state.world.units.end(), [&](const BattleUnitState& unit)
            {
                return unit.id == attackUnit.id;
            });
        if (it != state.world.units.end())
        {
            attackUnit.alive = it->alive;
            attackUnit.team = it->team;
            attackUnit.position = it->position;
        }
    }
}

void advanceRuntimeUnits(BattleFrameState& state, std::vector<BattleGameplayCommand>& commands)
{
    BattleComboTriggerSystem comboSystem;
    state.runtime.committedResults.clear();
    for (const auto& input : state.runtime.units)
    {
        assert(input.unitId >= 0);

        BattleFrameRuntimeUnitResult committed;
        committed.unitId = input.unitId;
        committed.result = BattleFrameUnitRuntimeSystem().advance(input.input);

        auto comboIt = state.combo.units.find(input.unitId);
        if (committed.result.skillFinished && comboIt != state.combo.units.end())
        {
            committed.comboEvents = comboSystem.collectSkillFinishedRuntimeEvents(
                comboIt->second,
                input.alive);

            auto teamHeal = comboSystem.collectPendingSkillTeamHeal(
                comboIt->second,
                { BattleComboTriggerHook::AfterSkillCast, input.unitId, -1 },
                [&]() { return takeRuntimePercentRoll(state); });
            if (teamHeal.flatHeal > 0 || teamHeal.pctHeal > 0)
            {
                commands.push_back(BattleTeamHealCommand{
                    input.unitId,
                    teamHeal.flatHeal,
                    teamHeal.pctHeal,
                    "技能群療",
                });
            }
            committed.comboState = comboIt->second;
        }
        state.runtime.committedResults.push_back(std::move(committed));
    }
}

void applyProjectileCancelDamageResults(
    BattleFrameState& state,
    std::vector<BattleAttackEvent>& events,
    std::vector<BattleGameplayCommand>& commands)
{
    BattleAttackSystem attackSystem;
    state.projectileCancel.committedCommands.clear();
    for (auto& event : events)
    {
        if (event.type != BattleAttackEventType::ProjectileCancel)
        {
            continue;
        }

        assert(event.attackId >= 0);
        assert(event.otherAttackId >= 0);
        const auto* attack = findAttack(state.attacks, event.attackId);
        const auto* otherAttack = findAttack(state.attacks, event.otherAttackId);
        assert(attack);
        assert(otherAttack);

        if (const auto* damage = findProjectileCancelBaseDamage(state, event.attackId, event.otherAttackId))
        {
            event.projectileCancelDamage = scaleProjectileCancelDamage(
                damage->baseDamage,
                attack->state.operationType);
        }
        if (const auto* damage = findProjectileCancelBaseDamage(state, event.otherAttackId, event.attackId))
        {
            event.otherProjectileCancelDamage = scaleProjectileCancelDamage(
                damage->baseDamage,
                otherAttack->state.operationType);
        }

        BattleProjectileCancelDamageCommand command;
        command.attackId = event.attackId;
        command.otherAttackId = event.otherAttackId;
        command.sourceUnitId = event.sourceUnitId;
        command.otherSourceUnitId = event.otherSourceUnitId;
        command.damage = event.projectileCancelDamage;
        command.otherDamage = event.otherProjectileCancelDamage;
        commands.push_back(command);
        state.projectileCancel.committedCommands.push_back(command);
        attackSystem.applyProjectileCancelDamage(state.attacks, event);
    }
}

void assertFrameAttackWorldConfigured(const BattleAttackWorld& attacks)
{
    assert(attacks.hitRadius > 0.0);
    assert(attacks.minimumVectorNorm > 0.0);
    assert(attacks.projectileGraceFrames >= 0);
    assert(attacks.bounceSpawnDistance > 0.0);
    assert(attacks.defaultProjectileSpeed > 0.0);
    assert(attacks.minimumBounceTotalFrame > 0);
}

void assertFrameMovementConfigConfigured(const BattleMovementConfig& config)
{
    assert(config.tileWidth > 0.0);
    assert(config.reservationHorizonFrames >= 0);
    assert(config.dashFrames > 0);
    assert(config.dashCooldownFrames >= 0);
    assert(config.slotSwitchCooldownFrames >= 0);
    assert(config.bodyRadius > 0.0);
    assert(config.engagementDeadband > 0.0);
    assert(config.engagementArriveDistance > 0.0);
    assert(config.meleeAttackReach > 0.0);
    assert(config.meleeLocalTargetRadius > 0.0);
    assert(config.maxDashDistance > 0.0);
    assert(config.maxRangedReach > 0.0);
    assert(config.movementDashDistanceMultiplier > 0.0);
}

BattleStatusUnitState* findStatusUnit(std::vector<BattleStatusUnitState>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleStatusUnitState& unit)
        {
            return unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

BattleUnitState* findWorldUnit(BattleWorldState& world, int unitId)
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleUnitState& unit)
        {
            return unit.id == unitId;
        });
    return it != world.units.end() ? &*it : nullptr;
}

const BattleUnitState* findWorldUnit(const BattleWorldState& world, int unitId)
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleUnitState& unit)
        {
            return unit.id == unitId;
        });
    return it != world.units.end() ? &*it : nullptr;
}

const BattleStatusUnitState* findStatusUnit(const std::vector<BattleStatusUnitState>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleStatusUnitState& unit)
        {
            return unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

BattleDeathEffectUnit* findDeathEffectUnit(BattleDeathEffectWorld& world, int unitId)
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleDeathEffectUnit& unit)
        {
            return unit.id == unitId;
        });
    return it != world.units.end() ? &*it : nullptr;
}

void applyDamageUnitToDeathEffectUnit(BattleDeathEffectUnit& deathUnit, const BattleDamageUnitState& damageUnit)
{
    deathUnit.alive = damageUnit.alive;
    deathUnit.hp = damageUnit.hp;
    deathUnit.maxHp = damageUnit.maxHp;
    deathUnit.attack = damageUnit.attack;
    deathUnit.shield = damageUnit.shield;
}

double distance2d(Pointf lhs, Pointf rhs)
{
    return EuclidDis(lhs.x - rhs.x, lhs.y - rhs.y);
}

BattleCastInput refreshedCastInput(const BattleFrameState& state,
                                   const BattleTickResult& movement,
                                   BattleCastInput input)
{
    if (const auto* source = findWorldUnit(state.world, input.unit.id))
    {
        input.unit.position = source->position;
        input.unit.alive = source->alive;
        input.unit.canStartAttack = source->canAttack;
        if (auto decision = movement.decisions.find(source->id); decision != movement.decisions.end())
        {
            input.unit.canStartAttack = decision->second.action == MovementAction::AttackReady;
        }
    }
    if (const auto* status = findStatusUnit(state.status.units, input.unit.id))
    {
        input.unit.alive = status->alive;
        input.unit.frozen = status->frozenTimer > 0;
    }
    if (const auto* target = findWorldUnit(state.world, input.targetUnitId))
    {
        if (target->alive)
        {
            input.targetPosition = target->position;
            input.targetDistance = distance2d(input.unit.position, target->position);
        }
        else
        {
            input.targetUnitId = -1;
        }
    }
    else
    {
        input.targetUnitId = -1;
    }
    return input;
}

BattleGameplayEvent toGameplayEvent(const BattleDamageEvent& event)
{
    BattleGameplayEvent gameplay;
    gameplay.sourceUnitId = event.sourceUnitId;
    gameplay.targetUnitId = event.targetUnitId;
    gameplay.amount = event.value;
    switch (event.type)
    {
    case BattleDamageEventType::DamageApplied:
    case BattleDamageEventType::MpDamageApplied:
    case BattleDamageEventType::ShieldAbsorbed:
        gameplay.type = BattleGameplayEventType::DamageApplied;
        break;
    case BattleDamageEventType::UnitDied:
        gameplay.type = BattleGameplayEventType::UnitDied;
        break;
    case BattleDamageEventType::StatusApplied:
        gameplay.type = BattleGameplayEventType::StatusApplied;
        break;
    case BattleDamageEventType::HpRestored:
    case BattleDamageEventType::MpRestored:
    case BattleDamageEventType::MpDrained:
    case BattleDamageEventType::CooldownExtended:
    case BattleDamageEventType::KillRewardApplied:
        gameplay.type = BattleGameplayEventType::ResourceChanged;
        break;
    case BattleDamageEventType::BlockedByInvincible:
    case BattleDamageEventType::DeathPrevented:
    case BattleDamageEventType::ExecuteTriggered:
        gameplay.type = BattleGameplayEventType::StatusApplied;
        break;
    }
    return gameplay;
}

void applyDamageResultToFrameState(BattleFrameState& state, const BattleDamageTransactionResult& transaction)
{
    if (auto* attacker = findWorldUnit(state.world, transaction.attacker.id))
    {
        attacker->alive = transaction.attacker.alive;
    }
    if (auto* defender = findWorldUnit(state.world, transaction.defender.id))
    {
        defender->alive = transaction.defender.alive;
    }
    if (auto* status = findStatusUnit(state.status.units, transaction.defender.id))
    {
        *status = transaction.defenderStatus;
        status->alive = transaction.defender.alive;
        status->hp = transaction.defender.hp;
        status->maxHp = transaction.defender.maxHp;
        status->invincible = transaction.defender.invincible;
    }
    if (auto* deathUnit = findDeathEffectUnit(state.deathEffects.world, transaction.defender.id))
    {
        applyDamageUnitToDeathEffectUnit(*deathUnit, transaction.defender);
    }
    if (auto* deathUnit = findDeathEffectUnit(state.deathEffects.world, transaction.attacker.id))
    {
        applyDamageUnitToDeathEffectUnit(*deathUnit, transaction.attacker);
    }
}

std::vector<int> unitDeathsIn(const std::vector<BattleDamageEvent>& events)
{
    std::vector<int> deadUnitIds;
    for (const auto& event : events)
    {
        if (event.type == BattleDamageEventType::UnitDied)
        {
            deadUnitIds.push_back(event.targetUnitId);
        }
    }
    return deadUnitIds;
}

void updateBattleResult(BattleFrameState& state, std::vector<BattleGameplayEvent>& gameplayEvents)
{
    if (state.result.ended)
    {
        return;
    }

    std::set<int> aliveTeams;
    for (const auto& unit : state.world.units)
    {
        if (unit.alive)
        {
            aliveTeams.insert(unit.team);
        }
    }
    for (const auto& [team, count] : state.result.pendingAliveByTeam)
    {
        if (count > 0)
        {
            aliveTeams.insert(team);
        }
    }
    if (aliveTeams.size() != 1)
    {
        return;
    }

    state.result.ended = true;
    state.result.winningTeam = *aliveTeams.begin();
    state.result.endedFrame = state.world.frame;
    if (!state.result.eventEmitted)
    {
        gameplayEvents.push_back({
            BattleGameplayEventType::BattleEnded,
            BattlePresentationCurrentFrame,
            -1,
            -1,
            state.result.winningTeam,
        });
        state.result.eventEmitted = true;
    }
}
}  // namespace

BattleCore::BattleCore(BattleWorldState& world)
    : world_(world)
{
}

BattleTickResult BattleCore::tickMovement()
{
    return BattleMovementPlanner(world_).tick();
}

BattleFrameUnitRuntimeResult BattleFrameUnitRuntimeSystem::advance(
    const BattleFrameUnitRuntimeInput& input) const
{
    assert(input.frame >= 0);
    assert(input.mpRegenIntervalFrames > 0);
    assert(input.physicalPowerRegenIntervalFrames > 0);
    assert(input.state.cooldown >= 0);
    assert(input.state.physicalPower >= 0);

    BattleFrameUnitRuntimeResult result;
    result.state = input.state;

    const int previousCooldown = result.state.cooldown;
    if (!input.frozen && result.state.cooldown > 0)
    {
        --result.state.cooldown;
    }
    result.skillFinished = previousCooldown > 0 && result.state.cooldown == 0;

    if (result.state.cooldown == 0)
    {
        if (input.frame % input.physicalPowerRegenIntervalFrames == 0)
        {
            ++result.state.physicalPower;
        }
        result.state.actFrame = 0;
        result.resetDashVelocity = result.state.operationType == BattleOperationType::Dash;
        result.state.operationType = BattleOperationType::None;
        result.state.actType = -1;
        result.state.haveAction = false;
    }

    if (input.frame % input.mpRegenIntervalFrames == 0)
    {
        result.mpDelta = 1;
    }

    return result;
}

BattleFrameResult BattleFrameRunner::advanceFrame(BattleFrameState& state) const
{
    assertFrameMovementConfigConfigured(state.world.config);
    assertFrameAttackWorldConfigured(state.attacks);

    BattleFrameResult result;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattlePresentationEvent> presentationEvents;

    state.status.config.frame = state.world.frame;
    auto statusTick = BattleStatusSystem(state.status.config).tick(state.status.units);
    state.status.events.insert(
        state.status.events.end(),
        statusTick.events.begin(),
        statusTick.events.end());

    advanceRuntimeUnits(state, result.commands);

    result.movement = BattleCore(state.world).tickMovement();

    for (const auto& input : state.casts.pendingInputs)
    {
        auto cast = BattleCastPlanner().plan(refreshedCastInput(state, result.movement, input));
        gameplayEvents.insert(gameplayEvents.end(), cast.gameplayEvents.begin(), cast.gameplayEvents.end());
        presentationEvents.insert(presentationEvents.end(), cast.presentationEvents.begin(), cast.presentationEvents.end());
        if (cast.decision.canCast)
        {
            state.pendingAttackSpawns.insert(
                state.pendingAttackSpawns.end(),
                cast.attackSpawnRequests.begin(),
                cast.attackSpawnRequests.end());
        }
        state.casts.committedResults.push_back(std::move(cast));
    }
    state.casts.pendingInputs.clear();

    syncAttackUnitsFromWorld(state);
    state.attacks.frame = state.world.frame;
    BattleAttackSystem attackSystem;
    for (const auto& request : state.pendingAttackSpawns)
    {
        result.attackEvents.push_back(attackSystem.spawn(state.attacks, request));
    }
    state.pendingAttackSpawns.clear();
    auto tickEvents = attackSystem.tick(state.attacks);
    result.attackEvents.insert(
        result.attackEvents.end(),
        std::make_move_iterator(tickEvents.begin()),
        std::make_move_iterator(tickEvents.end()));
    applyProjectileCancelDamageResults(state, result.attackEvents, result.commands);

    for (const auto& input : state.damage.pendingTransactions)
    {
        auto transaction = BattleDamageSystem().resolveTransaction(input);
        auto deadUnitIds = unitDeathsIn(transaction.events);
        applyDamageResultToFrameState(state, transaction);
        for (const auto& event : transaction.events)
        {
            gameplayEvents.push_back(toGameplayEvent(event));
        }
        state.damage.committedTransactions.push_back(std::move(transaction));
        for (int deadUnitId : deadUnitIds)
        {
            if (!findDeathEffectUnit(state.deathEffects.world, deadUnitId))
            {
                continue;
            }
            auto events = BattleDeathEffectSystem().applyAllyDeathEffects(state.deathEffects.world, deadUnitId);
            state.deathEffects.events.insert(state.deathEffects.events.end(), events.begin(), events.end());
        }
    }
    state.damage.pendingTransactions.clear();

    updateBattleResult(state, gameplayEvents);

    BattlePresentationRecorder recorder;
    recorder.beginFrame(makePresentationSnapshot(state));
    for (auto event : gameplayEvents)
    {
        recorder.recordGameplay(std::move(event));
    }
    for (auto event : presentationEvents)
    {
        recorder.recordPresentation(std::move(event));
    }
    for (const auto& event : result.movement.events)
    {
        recorder.recordPresentation(toPresentationEvent(event));
    }
    for (const auto& event : result.attackEvents)
    {
        recorder.recordGameplay(toGameplayEvent(event, state.attacks));
        for (auto presentation : toPresentationEvents(event, state.attacks))
        {
            recorder.recordPresentation(std::move(presentation));
        }
    }
    result.frame = recorder.consumeFrame();
    return result;
}

}  // namespace KysChess::Battle
