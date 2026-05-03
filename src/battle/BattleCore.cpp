#include "BattleCore.h"

#include <algorithm>
#include <cassert>
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

BattlePresentationSnapshot makePresentationSnapshot(const BattleWorldState& world)
{
    BattlePresentationSnapshot snapshot;
    snapshot.frame = world.frame;
    snapshot.units.reserve(world.units.size());
    for (const auto& unit : world.units)
    {
        snapshot.units.push_back(toPresentationUnit(unit));
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

void applyAttackContext(BattlePresentationEvent& presentation, const BattleAttackWorld& world, int attackId)
{
    presentation.effectId = attackId;
    if (const auto* attack = findAttack(world, attackId))
    {
        presentation.sourceUnitId = attack->attackerUnitId;
        presentation.targetUnitId = attack->preferredTargetUnitId;
        presentation.durationFrames = attack->totalFrame;
        presentation.visualEffectId = attack->visualEffectId;
        presentation.position = attack->position;
        presentation.velocity = attack->velocity;
        presentation.operationKind = attack->operationKind;
    }
}

void applyAttackContext(BattleGameplayEvent& gameplay, const BattleAttackWorld& world, int attackId)
{
    gameplay.effectId = attackId;
    if (const auto* attack = findAttack(world, attackId))
    {
        gameplay.sourceUnitId = attack->attackerUnitId;
        gameplay.position = attack->position;
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
            gameplay.sourceUnitId = sourceAttack->attackerUnitId;
        }
        if (const auto* spawnedAttack = findAttack(world, event.otherAttackId))
        {
            gameplay.position = spawnedAttack->position;
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
}  // namespace

BattleCore::BattleCore(BattleWorldState& world)
    : world_(world)
{
}

BattleTickResult BattleCore::tickMovement()
{
    return BattleMovementPlanner(world_).tick();
}

BattleFrameResult BattleFrameRunner::advanceFrame(BattleFrameState& state) const
{
    BattleFrameResult result;

    result.movement = BattleCore(state.world).tickMovement();
    syncAttackUnitsFromWorld(state);
    state.attacks.frame = state.world.frame;
    BattleAttackSystem attackSystem;
    result.attackEvents = attackSystem.tick(state.attacks);
    for (const auto& request : state.pendingAttackSpawns)
    {
        result.attackEvents.push_back(attackSystem.spawn(state.attacks, request));
    }
    state.pendingAttackSpawns.clear();

    BattlePresentationRecorder recorder;
    recorder.beginFrame(makePresentationSnapshot(state.world));
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
