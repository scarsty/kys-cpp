#include "BattlePresentationPlayback.h"

#include <cassert>

namespace KysChess::Battle
{

namespace
{
BattlePresentationCommandType commandTypeFor(BattlePresentationEventType type)
{
    switch (type)
    {
    case BattlePresentationEventType::DamageLog:
        return BattlePresentationCommandType::RecordDamage;
    case BattlePresentationEventType::HealLog:
        return BattlePresentationCommandType::RecordHeal;
    case BattlePresentationEventType::StatusLog:
        return BattlePresentationCommandType::RecordStatus;
    case BattlePresentationEventType::FloatingText:
        return BattlePresentationCommandType::SpawnFloatingText;
    case BattlePresentationEventType::RoleEffect:
        return BattlePresentationCommandType::SpawnRoleEffect;
    case BattlePresentationEventType::DamageNumber:
        return BattlePresentationCommandType::SpawnDamageNumber;
    case BattlePresentationEventType::CameraFocus:
        return BattlePresentationCommandType::FocusCamera;
    case BattlePresentationEventType::ProjectileMoved:
        return BattlePresentationCommandType::MoveProjectile;
    case BattlePresentationEventType::ProjectileHit:
        return BattlePresentationCommandType::HitProjectile;
    case BattlePresentationEventType::ProjectileExpired:
        return BattlePresentationCommandType::ExpireProjectile;
    case BattlePresentationEventType::ProjectileTargetLost:
        return BattlePresentationCommandType::LoseProjectileTarget;
    case BattlePresentationEventType::ProjectileCancelled:
        return BattlePresentationCommandType::CancelProjectile;
    case BattlePresentationEventType::ProjectileBounced:
        return BattlePresentationCommandType::BounceProjectile;
    }

    assert(false);
    return BattlePresentationCommandType::RecordStatus;
}
}  // namespace

BattlePresentationPlaybackPlan BattlePresentationPlaybackPlanner::build(const BattlePresentationFrame& frame) const
{
    BattlePresentationPlaybackPlan plan;
    plan.snapshot = frame.snapshot;
    plan.commands.reserve(frame.presentationEvents.size());
    for (const auto& event : frame.presentationEvents)
    {
        plan.commands.push_back(makeCommand(event));
    }
    return plan;
}

BattlePresentationCommand BattlePresentationPlaybackPlanner::makeCommand(const BattlePresentationEvent& event) const
{
    return {
        commandTypeFor(event.type),
        event.frame,
        event.sourceUnitId,
        event.targetUnitId,
        event.amount,
        event.durationFrames,
        event.effectId,
        event.textSize,
        event.textMotionType,
        event.text,
        event.skillName,
        event.detailText,
        event.color,
        event.position,
    };
}

}  // namespace KysChess::Battle
