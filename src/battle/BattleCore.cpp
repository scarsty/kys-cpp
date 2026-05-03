#include "BattleCore.h"

#include <algorithm>
#include <cassert>

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

BattlePresentationEvent toPresentationEvent(
    const BattleAttackEvent& event,
    const BattleAttackWorld& world)
{
    BattlePresentationEvent presentation;
    presentation.effectId = event.attackId;
    if (auto it = std::find_if(world.attacks.begin(), world.attacks.end(), [&](const BattleAttackInstance& attack)
        {
            return attack.id == event.attackId;
        }); it != world.attacks.end())
    {
        presentation.sourceUnitId = it->attackerUnitId;
        presentation.position = it->position;
    }

    switch (event.type)
    {
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
    return presentation;
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
    result.attackEvents = BattleAttackSystem().tick(state.attacks);

    BattlePresentationRecorder recorder;
    recorder.beginFrame(makePresentationSnapshot(state.world));
    for (const auto& event : result.movement.events)
    {
        recorder.record(toPresentationEvent(event));
    }
    for (const auto& event : result.attackEvents)
    {
        recorder.record(toPresentationEvent(event, state.attacks));
    }
    result.frame = recorder.consumeFrame();
    return result;
}

}  // namespace KysChess::Battle
