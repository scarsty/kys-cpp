#pragma once

#include "BattleCastSystem.h"
#include "BattleCore.h"
#include "BattleHitResolver.h"
#include "BattleMovement.h"

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
    double teamEffectHealAuraRadius = 0.0;
    double rescueExecuteUnattendedRadius = 0.0;
    double minimumVectorNorm = 0.0;
    int movementPhysicsDashMomentumFrames = 0;
};

BattleRuntimeRulesConfig makeHadesBattleRuntimeRules(double tileWidth, int coordCount);

}  // namespace KysChess::Battle
