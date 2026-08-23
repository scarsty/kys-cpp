#pragma once

#include "BattleEffectAttackCastSystem.h"
#include "BattleCastSystem.h"

#include <string>
#include <vector>

namespace KysChess::Battle
{

struct BattleActionRulesConfig
{
    double heavyAttackReach = 0.0;
    double blinkWeakTargetDefWeight = 0.0;
    int projectileBounceRange = 0;
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
    bool hasEquippedSkill = false;
    BattleActionSkillSeed normalSkill;
    BattleActionSkillSeed ultimateSkill;
};

struct BattlePendingCastAction
{
    int targetUnitId = -1;
    BattleOperationType operationType = BattleOperationType::None;
    int castFrame{};
    Pointf dashVelocity;
    // 武功身分只由 effectCast.provenance.magicId 保存；此快照僅保留規劃機制。
    BattleCastSkillState skillPlan;
    BattleCastStart effectCast{};
    BattleEffectCastPreparation effectPreparation;
    std::vector<EffectCommand> plannedAttackEffectCommands;
    EffectResourcesBeforeCastSnapshot effectResourcesBeforeCast;
};

class BattleRuntimeActions
{
public:
    BattleCastConfig castConfig;
    BattleCastGeometry castGeometry;
    BattleActionRulesConfig actionRules;
};

}  // namespace KysChess::Battle
