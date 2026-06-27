#pragma once

#include "BattleCastSystem.h"

#include <string>
#include <vector>

namespace KysChess::Battle
{

struct BattleActionRulesConfig
{
    double tileWidth = 0.0;
    double maxEffectiveBattleReach = 0.0;
    double meleeAttackHitRadius = 0.0;
    double meleeAttackReach = 0.0;
    double heavyAttackReach = 0.0;
    double dashAttackMeleeReach = 0.0;
    double blinkWeakTargetDefWeight = 0.0;
    int dashMomentumFrames = 0;
    int movementDashCooldownFrames = 0;
    int actionRecoveryFrames = 0;
    int dashRecoveryFrames = 0;
    int strengthenedMeleeOperationCountThreshold = 0;
    int projectileBounceRange = 0;
    int coordCount = 0;
};

struct BattleActionSkillSeed
{
    int id = -1;
    std::string name;
    int soundId = -1;
    int hurtType = 0;
    int attackAreaType = -1;
    int magicType = -1;
    int visualEffectId = -1;
    int selectDistance = 1;
    int actProperty = 0;
    int magicPower = 0;
};

struct BattleActionPlanSeed
{
    int unitId{};
    bool hasEquippedSkill = false;
    BattleActionSkillSeed normalSkill;
    BattleActionSkillSeed ultimateSkill;
};

struct BattlePendingCastAction
{
    int unitId = -1;
    int targetUnitId = -1;
    bool ultimate = false;
    BattleOperationType operationType = BattleOperationType::None;
    Pointf dashVelocity;
    BattleCastSkillState skill;
};

class BattleRuntimeActions
{
public:
    BattleCastConfig castConfig;
    BattleCastGeometry castGeometry;
    BattleActionRulesConfig actionRules;
    std::vector<int> castFrames;
    int actionRecoveryFrames = 0;
    int dashRecoveryFrames = 0;
    double blinkWeakTargetDefWeight = 0.0;
    int strengthenedMeleeOperationCountThreshold = 0;
    int projectileBounceRange = 0;
};

}  // namespace KysChess::Battle
