#pragma once

#include "BattleOperation.h"

#include <string>

namespace KysChess::Battle
{

struct BattleSkillState
{
    int id = -1;
    std::string name;
    int attackAreaType = -1;
    int magicType = -1;
    double reach = 0.0;
    bool forceRanged = false;
    bool rangedStyle = false;
};

struct CombatIntentInput
{
    bool canStartAttack = false;
    bool hasEquippedSkill = false;
    bool ultimateReady = false;
    bool movementDashActive = false;
    bool dashAttackEnabled = false;
    double targetDistance = 0.0;
    double meleeAttackReach = 0.0;
    double dashAttackReach = 0.0;
    BattleSkillState plannedSkill;
};

struct CombatIntent
{
    bool equipPlannedSkill = false;
    bool announceUltimate = false;
    bool startAttack = false;
    BattleOperationType operationType = BattleOperationType::None;
};

class BattleCombatIntentPlanner
{
public:
    BattleOperationType operationTypeForAttackArea(int attackAreaType) const;
    CombatIntent select(const CombatIntentInput& input) const;
};

}  // namespace KysChess::Battle
