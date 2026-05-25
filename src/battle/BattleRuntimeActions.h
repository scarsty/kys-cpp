#pragma once

#include "BattleCastSystem.h"

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace KysChess::Battle
{

struct BattleActionRulesConfig
{
    double tileWidth = 0.0;
    double maxEffectiveBattleReach = 0.0;
    double meleeAttackHitRadius = 0.0;
    double meleeAttackReach = 0.0;
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
    BattleCastSkillState skill;
};

class BattleRuntimeUnitActionView;

class BattleRuntimeActions
{
public:
    std::map<int, BattleActionPlanSeed>& planSeeds() { return planSeeds_; }
    const std::map<int, BattleActionPlanSeed>& planSeeds() const { return planSeeds_; }

    const BattlePendingCastAction* pendingCastForTest(int unitId) const
    {
        auto it = pendingCasts_.find(unitId);
        return it == pendingCasts_.end() ? nullptr : &it->second;
    }

    bool hasPendingCastForTest(int unitId) const
    {
        return hasPendingCast(unitId);
    }

    std::size_t pendingCastCountForTest() const
    {
        return pendingCasts_.size();
    }

    void setPendingCastForTest(int unitId, BattlePendingCastAction action)
    {
        setPendingCast(unitId, std::move(action));
    }

    void markUltimateCasterForTest(int unitId)
    {
        markUltimateCaster(unitId);
    }

    bool hasUltimateCasterForTest(int unitId) const
    {
        return isUltimateCaster(unitId);
    }

    std::size_t ultimateCasterCountForTest() const
    {
        return ultimateCasterUnitIds_.size();
    }

    BattleCastConfig castConfig;
    BattleCastGeometry castGeometry;
    BattleActionRulesConfig actionRules;
    std::vector<int> castFrames;
    int actionRecoveryFrames = 0;
    int dashRecoveryFrames = 0;
    double blinkWeakTargetDefWeight = 0.0;
    int strengthenedMeleeOperationCountThreshold = 0;
    int projectileBounceRange = 0;

private:
    friend class BattleRuntimeUnitActionView;

    BattlePendingCastAction* findPendingCast(int unitId)
    {
        auto it = pendingCasts_.find(unitId);
        return it == pendingCasts_.end() ? nullptr : &it->second;
    }

    void setPendingCast(int unitId, BattlePendingCastAction action)
    {
        action.unitId = unitId;
        pendingCasts_[unitId] = std::move(action);
    }

    void clearPendingCast(int unitId)
    {
        pendingCasts_.erase(unitId);
    }

    bool hasPendingCast(int unitId) const
    {
        return pendingCasts_.count(unitId) != 0;
    }

    void markUltimateCaster(int unitId)
    {
        ultimateCasterUnitIds_.insert(unitId);
    }

    void clearUltimateCaster(int unitId)
    {
        ultimateCasterUnitIds_.erase(unitId);
    }

    bool isUltimateCaster(int unitId) const
    {
        return ultimateCasterUnitIds_.count(unitId) != 0;
    }

    void clearActionOwners(int unitId)
    {
        clearPendingCast(unitId);
        clearUltimateCaster(unitId);
    }

    std::map<int, BattleActionPlanSeed> planSeeds_;
    std::map<int, BattlePendingCastAction> pendingCasts_;
    std::set<int> ultimateCasterUnitIds_;
};

class BattleRuntimeUnitActionView
{
public:
    BattleRuntimeUnitActionView(BattleRuntimeActions& actions, int unitId)
        : actions_(&actions),
          unitId_(unitId)
    {
    }

    BattlePendingCastAction* pendingCast() const
    {
        return actions_->findPendingCast(unitId_);
    }

    void setPendingCast(BattlePendingCastAction action) const
    {
        actions_->setPendingCast(unitId_, std::move(action));
    }

    void clearPendingCast() const
    {
        actions_->clearPendingCast(unitId_);
    }

    bool hasPendingCast() const
    {
        return actions_->hasPendingCast(unitId_);
    }

    void markUltimateCaster() const
    {
        actions_->markUltimateCaster(unitId_);
    }

    void clearUltimateCaster() const
    {
        actions_->clearUltimateCaster(unitId_);
    }

    bool isUltimateCaster() const
    {
        return actions_->isUltimateCaster(unitId_);
    }

    void clearOwners() const
    {
        actions_->clearActionOwners(unitId_);
    }

    const BattleActionPlanSeed* planSeed() const
    {
        auto it = actions_->planSeeds_.find(unitId_);
        return it == actions_->planSeeds_.end() ? nullptr : &it->second;
    }

    void setPendingCastForTest(BattlePendingCastAction action) const
    {
        setPendingCast(std::move(action));
    }

private:
    BattleRuntimeActions* actions_{};
    int unitId_{};
};

}  // namespace KysChess::Battle
