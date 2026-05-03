#include "BattleGeometry.h"

#include <cassert>

namespace KysChess::Battle
{

namespace
{

BattleMovementConfig deriveMovementConfig(const BattleMovementGeometry& geometry)
{
    assert(geometry.tileWidth > 0.0);
    assert(geometry.reservationHorizonFrames >= 0);
    assert(geometry.dashFrames > 0);
    assert(geometry.dashCooldownFrames >= 0);
    assert(geometry.slotSwitchCooldownFrames >= 0);
    assert(geometry.maxRangedReach > 0.0);
    assert(geometry.meleeAttackEffectOffset > 0.0);
    assert(geometry.meleeAttackHitRadius > 0.0);

    BattleMovementConfig config;
    config.tileWidth = geometry.tileWidth;
    config.reservationHorizonFrames = geometry.reservationHorizonFrames;
    config.dashFrames = geometry.dashFrames;
    config.dashCooldownFrames = geometry.dashCooldownFrames;
    config.slotSwitchCooldownFrames = geometry.slotSwitchCooldownFrames;
    config.bodyRadius = geometry.tileWidth * 1.5;
    config.engagementDeadband = geometry.tileWidth / 2.0;
    config.engagementArriveDistance = geometry.tileWidth - config.engagementDeadband / 2.0;
    double meleeAttackSafetyMargin = geometry.meleeAttackHitRadius - config.engagementArriveDistance;
    config.meleeAttackReach = geometry.meleeAttackEffectOffset + geometry.meleeAttackHitRadius - meleeAttackSafetyMargin;
    config.meleeLocalTargetRadius = config.meleeAttackReach + geometry.meleeAttackEffectOffset;
    config.maxDashDistance = config.meleeLocalTargetRadius;
    config.maxRangedReach = geometry.maxRangedReach;
    config.movementDashDistanceMultiplier = geometry.meleeAttackEffectOffset / geometry.tileWidth;
    return config;
}

}  // namespace

BattleGeometry::BattleGeometry(BattleMovementGeometry geometry)
    : source_(geometry),
      movementConfig_(deriveMovementConfig(source_))
{
}

}  // namespace KysChess::Battle
