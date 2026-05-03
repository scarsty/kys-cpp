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
    case BattlePresentationEventType::ProjectileSpawned:
        return BattlePresentationCommandType::SpawnProjectile;
    case BattlePresentationEventType::ProjectileMoved:
        return BattlePresentationCommandType::MoveProjectile;
    case BattlePresentationEventType::ProjectileHit:
        return BattlePresentationCommandType::ImpactProjectile;
    case BattlePresentationEventType::ProjectileExpired:
        return BattlePresentationCommandType::ExpireProjectile;
    case BattlePresentationEventType::ProjectileTargetLost:
        return BattlePresentationCommandType::CancelProjectile;
    case BattlePresentationEventType::ProjectileCancelled:
        return BattlePresentationCommandType::CancelProjectile;
    case BattlePresentationEventType::ProjectileBounced:
        return BattlePresentationCommandType::BounceProjectile;
    }

    assert(false);
    return BattlePresentationCommandType::RecordStatus;
}

bool isProjectileEvent(BattlePresentationEventType type)
{
    switch (type)
    {
    case BattlePresentationEventType::ProjectileSpawned:
    case BattlePresentationEventType::ProjectileMoved:
    case BattlePresentationEventType::ProjectileHit:
    case BattlePresentationEventType::ProjectileExpired:
    case BattlePresentationEventType::ProjectileTargetLost:
    case BattlePresentationEventType::ProjectileCancelled:
    case BattlePresentationEventType::ProjectileBounced:
        return true;
    case BattlePresentationEventType::DamageLog:
    case BattlePresentationEventType::HealLog:
    case BattlePresentationEventType::StatusLog:
    case BattlePresentationEventType::FloatingText:
    case BattlePresentationEventType::RoleEffect:
    case BattlePresentationEventType::DamageNumber:
    case BattlePresentationEventType::CameraFocus:
        return false;
    }

    assert(false);
    return false;
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
    BattlePresentationCommand command;
    command.type = commandTypeFor(event.type);
    command.frame = event.frame;
    command.sourceUnitId = event.sourceUnitId;
    command.targetUnitId = event.targetUnitId;
    command.amount = event.amount;
    command.durationFrames = event.durationFrames;
    command.effectId = event.effectId;
    command.textSize = event.textSize;
    command.textMotionType = event.textMotionType;
    command.text = event.text;
    command.skillName = event.skillName;
    command.detailText = event.detailText;
    command.color = event.color;
    command.position = event.position;
    command.visualEffectId = isProjectileEvent(event.type) ? event.visualEffectId : event.effectId;
    command.projectileAttackId = event.effectId;
    command.projectileRelatedAttackId = event.amount;
    command.projectileSourceUnitId = event.sourceUnitId;
    command.projectileTargetUnitId = event.targetUnitId;
    command.projectilePosition = event.position;
    command.projectileVelocity = event.velocity;
    command.projectileDurationFrames = event.durationFrames;
    command.projectileOperationKind = event.operationKind;
    return command;
}

}  // namespace KysChess::Battle
