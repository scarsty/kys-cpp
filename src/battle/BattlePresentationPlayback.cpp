#include "BattlePresentationPlayback.h"

#include <cassert>

namespace KysChess::Battle
{

namespace
{
BattlePresentationCommandType commandTypeFor(BattleVisualEventType type)
{
    switch (type)
    {
    case BattleVisualEventType::FloatingText:
        return BattlePresentationCommandType::SpawnFloatingText;
    case BattleVisualEventType::RoleEffect:
        return BattlePresentationCommandType::SpawnRoleEffect;
    case BattleVisualEventType::DamageNumber:
        return BattlePresentationCommandType::SpawnDamageNumber;
    case BattleVisualEventType::CameraFocus:
        return BattlePresentationCommandType::FocusCamera;
    case BattleVisualEventType::ProjectileSpawned:
        return BattlePresentationCommandType::SpawnProjectile;
    case BattleVisualEventType::ProjectileMoved:
        return BattlePresentationCommandType::MoveProjectile;
    case BattleVisualEventType::ProjectileHit:
        return BattlePresentationCommandType::ImpactProjectile;
    case BattleVisualEventType::ProjectileExpired:
        return BattlePresentationCommandType::ExpireProjectile;
    case BattleVisualEventType::ProjectileTargetLost:
        return BattlePresentationCommandType::CancelProjectile;
    case BattleVisualEventType::ProjectileCancelled:
        return BattlePresentationCommandType::CancelProjectile;
    case BattleVisualEventType::ProjectileBounced:
        return BattlePresentationCommandType::BounceProjectile;
    }

    assert(false);
    return BattlePresentationCommandType::SpawnFloatingText;
}

bool isProjectileEvent(BattleVisualEventType type)
{
    switch (type)
    {
    case BattleVisualEventType::ProjectileSpawned:
    case BattleVisualEventType::ProjectileMoved:
    case BattleVisualEventType::ProjectileHit:
    case BattleVisualEventType::ProjectileExpired:
    case BattleVisualEventType::ProjectileTargetLost:
    case BattleVisualEventType::ProjectileCancelled:
    case BattleVisualEventType::ProjectileBounced:
        return true;
    case BattleVisualEventType::FloatingText:
    case BattleVisualEventType::RoleEffect:
    case BattleVisualEventType::DamageNumber:
    case BattleVisualEventType::CameraFocus:
        return false;
    }

    assert(false);
    return false;
}

int projectileRelatedAttackIdFor(const BattleVisualEvent& event)
{
    switch (event.type)
    {
    case BattleVisualEventType::ProjectileCancelled:
    case BattleVisualEventType::ProjectileBounced:
        return event.amount;
    case BattleVisualEventType::ProjectileSpawned:
    case BattleVisualEventType::ProjectileMoved:
    case BattleVisualEventType::ProjectileHit:
    case BattleVisualEventType::ProjectileExpired:
    case BattleVisualEventType::ProjectileTargetLost:
    case BattleVisualEventType::FloatingText:
    case BattleVisualEventType::RoleEffect:
    case BattleVisualEventType::DamageNumber:
    case BattleVisualEventType::CameraFocus:
        return -1;
    }

    assert(false);
    return -1;
}
}  // namespace

BattlePresentationPlaybackPlan BattlePresentationPlaybackPlanner::build(const BattlePresentationFrame& frame) const
{
    BattlePresentationPlaybackPlan plan;
    plan.commands.reserve(frame.visualEvents.size());
    for (const auto& event : frame.visualEvents)
    {
        plan.commands.push_back(makeCommand(event));
    }
    return plan;
}

BattlePresentationCommand BattlePresentationPlaybackPlanner::makeCommand(const BattleVisualEvent& event) const
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
    command.projectileRelatedAttackId = projectileRelatedAttackIdFor(event);
    command.projectileSourceUnitId = event.sourceUnitId;
    command.projectileTargetUnitId = event.targetUnitId;
    command.projectilePosition = event.position;
    command.projectileVelocity = event.velocity;
    command.projectileDurationFrames = event.durationFrames;
    command.projectileOperationKind = event.operationKind;
    return command;
}

}  // namespace KysChess::Battle
