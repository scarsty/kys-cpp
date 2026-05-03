#pragma once

namespace KysChess::Battle
{

struct BattleCastUnitState
{
    int id = -1;
    bool alive = true;
    bool frozen = false;
    bool stunned = false;
    int mp = 0;
    int maxMp = 0;
    double meleeAttackReach = 0.0;
    double dashAttackReach = 0.0;
    bool hasEquippedSkill = false;
    bool movementDashActive = false;
    bool dashAttackEnabled = false;
};

struct BattleCastSkillState
{
    int id = -1;
    int attackAreaType = -1;
    int magicType = -1;
    double reach = 0.0;
    bool forceRanged = false;
    bool rangedStyle = false;
};

struct BattleCastInput
{
    BattleCastUnitState unit;
    BattleCastSkillState normalSkill;
    BattleCastSkillState ultimateSkill;
    int targetUnitId = -1;
    double targetDistance = 0.0;
};

enum class BattleCastBlockReason
{
    None,
    Dead,
    Frozen,
    Stunned,
    NoTarget,
    NoSkill,
    OutOfRange,
    MovementDashActive
};

struct BattleCastDecision
{
    bool canCast = false;
    bool ultimate = false;
    bool equipSkill = false;
    bool announceUltimate = false;
    int unitId = -1;
    int targetUnitId = -1;
    int skillId = -1;
    int operationType = -1;
    BattleCastBlockReason reason = BattleCastBlockReason::None;
};

struct BattleCastResult
{
    BattleCastDecision decision;
};

class BattleCastPlanner
{
public:
    BattleCastResult plan(const BattleCastInput& input) const;

private:
    const BattleCastSkillState& selectSkill(const BattleCastInput& input, bool& ultimate) const;
    BattleCastBlockReason blockedReason(const BattleCastInput& input) const;
};

}  // namespace KysChess::Battle
