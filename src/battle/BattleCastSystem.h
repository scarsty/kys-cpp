#pragma once

#include "../Point.h"
#include "BattleAttackSystem.h"
#include "BattleEffectSystem.h"
#include "BattlePresentation.h"

#include <string>
#include <vector>

namespace KysChess::Battle
{

struct BattleCastUnitState
{
    int id = -1;
    Pointf position;
    Pointf facing = { 1.0f, 0.0f, 0.0f };
    bool alive = true;
    bool frozen = false;
    bool stunned = false;
    bool canStartAttack = true;
    int mp = 0;
    int maxMp = 0;
    int actProperty = 0;
    int speed = 0;
    int cooldownReductionPct = 0;
    double meleeAttackReach = 0.0;
    double dashAttackReach = 0.0;
    bool hasEquippedSkill = false;
    bool movementDashActive = false;
    bool dashAttackEnabled = false;
};

struct BattleCastSkillState
{
    int id = -1;
    std::string name;
    int attackAreaType = -1;
    int magicType = -1;
    int visualEffectId = -1;
    int selectDistance = 1;
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
    Pointf targetPosition;
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
    AttackNotReady,
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

struct BattleCastAnimationTiming
{
    int castFrame = 0;
    int cooldownFrames = 0;
    int recoveryFrames = 0;
};

struct BattleCastResult
{
    BattleCastDecision decision;
    int cooldownDelta = 0;
    int mpDelta = 0;
    BattleCastAnimationTiming animation;
    std::vector<BattleAttackSpawnRequest> attackSpawnRequests;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattlePresentationEvent> presentationEvents;
    std::vector<BattleEffectEvent> effectEvents;
};

class BattleCastPlanner
{
public:
    BattleCastResult plan(const BattleCastInput& input) const;

private:
    const BattleCastSkillState& selectSkill(const BattleCastInput& input, bool& ultimate) const;
    BattleCastBlockReason blockedReason(const BattleCastInput& input) const;
    void appendCommittedCastOutput(BattleCastResult& result,
                                   const BattleCastInput& input,
                                   const BattleCastSkillState& selectedSkill) const;
};

}  // namespace KysChess::Battle
