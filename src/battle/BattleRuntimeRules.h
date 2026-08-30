#pragma once

#include "BattleLimits.h"
#include "BattleCastSystem.h"
#include "BattleCore.h"
#include "BattleHitResolver.h"
#include "BattleMovement.h"
#include "BattleMovementPhysics.h"

namespace KysChess::Battle
{

struct BattleRuntimeRulesConfig
{
    BattleGridTransform gridTransform;
    BattleMovementConfig movementConfig;
    BattleMovementPhysicsConfig movementPhysicsConfig;
    BattleMovementPhysicsCollisionWorld movementCollisionWorld;
    BattleCastConfig castConfig;
    BattleCastGeometry castGeometry;
    BattleFrameRescueCounterAttackConfig rescueCounterAttack;
    BattleProjectileFollowUpContext projectileFollowUps;
    BattleActionRulesConfig action;
    double rescueExecuteUnattendedRadius = 0.0;
    double meleeAttackHitRadius = 0.0;
    int maximumFrames = kBattleFrameLimit;
};

BattleRuntimeRulesConfig makeHadesBattleRuntimeRules(double tileWidth, int coordCount);

}  // namespace KysChess::Battle
