#pragma once

#include "../Point.h"
#include "BattleAttackSystem.h"
#include "BattleEffectSystem.h"
#include "BattleOperation.h"
#include "BattlePresentation.h"

#include <array>
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
    int speed = 0;
    int cooldownReductionPct = 0;
    int operationCount = 0;
    double meleeAttackReach{};
    double dashAttackReach{};
    bool hasEquippedSkill = false;
    bool movementDashActive = false;
    bool dashAttackEnabled = false;
    Pointf dashVelocity;
    int dashHitCount = 1;
    bool emitDashFollowUpSkillAttack = false;
    BattleOperationType dashFollowUpOperationType = BattleOperationType::None;
};

struct BattleCastSkillState
{
    int id = -1;
    std::string name;
    int attackAreaType = -1;
    int magicType = -1;
    int visualEffectId = -1;
    int selectDistance = 1;
    int projectileSpeedMultiplierPct = 100;
    int actProperty = 0;
    int meleeSplashCount = 0;
    int extraProjectileCount = 0;
    bool strengthenedMelee = false;
    double reach{};
    bool forceRanged = false;
    bool rangedStyle = false;
};

struct BattleCastGeometry
{
    double meleeAttackEffectOffset{};
    double projectileSpeed{};
    double projectileSpawnOffset{};
    double projectileBaseTravel{};
    double projectileTravelPerSelectDistance{};
    double meleeSplashProjectileSpeed{};
    double dashHitPositionSpacing{};
    int dashHitFrameStep{};
};

struct BattleCastConfig
{
    std::array<int, 4> castFrames{};
    std::array<int, 4> baseCooldownFrames{};
    std::array<int, 4> minimumCooldownFrames{};
    std::array<int, 4> cooldownActPropertyDivisors{};
    std::array<int, 4> recoveryFrames{};
    double maxCooldownSpeed{};
    double speedCooldownReductionRatio{};
    int minimumCooldownAfterCastPadding{};
    int normalCastMpDelta{};
    double minimumFacingNorm{};
    int meleeHitTotalFrame{};
    int strengthenedMeleeTotalFrame{};
    double strengthenedMeleeSelectDistanceDivisor{};
    float strengthenedMeleeMultiplier{};
    int meleeSplashTotalFrame{};
    int meleeSplashInitialFrame{};
    float meleeSplashStrengthMultiplier{};
    int trackingProjectileTotalFrame{};
    int dashHitTotalFrame{};
    int strengthenedMeleeOperationCountThreshold{};
};

struct BattleCastInput
{
    BattleCastConfig config;
    BattleCastGeometry geometry;
    BattleCastUnitState unit;
    BattleCastSkillState normalSkill;
    BattleCastSkillState ultimateSkill;
    int targetUnitId = -1;
    Pointf targetPosition;
    double targetDistance{};
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
    BattleOperationType operationType = BattleOperationType::None;
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
    BattleCastResult commitSelectedCast(const BattleCastInput& input, bool ultimate, BattleOperationType operationType) const;

private:
    const BattleCastSkillState& selectSkill(const BattleCastInput& input, bool& ultimate) const;
    BattleCastBlockReason blockedReason(const BattleCastInput& input) const;
    void appendCommittedCastOutput(BattleCastResult& result,
                                   const BattleCastInput& input,
                                   const BattleCastSkillState& selectedSkill) const;
};

}  // namespace KysChess::Battle
