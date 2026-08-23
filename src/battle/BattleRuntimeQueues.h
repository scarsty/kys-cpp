#pragma once

#include "BattleAttackSystem.h"
#include "BattleDamageQueue.h"

#include <cassert>
#include <utility>
#include <vector>

namespace KysChess::Battle
{

class BattleNextFrameQueues
{
public:
    void queueAttack(BattleAttackSpawnRequest request)
    {
        assert(request.provenance.valid());
        assert(request.castWork.valid());
        assert(request.castWork.castId == request.provenance.cast.castId);
        attackSpawns_.push_back(std::move(request));
    }

    void queueDamage(BattlePendingDamageIntent damage)
    {
        pendingDamage_.push_back(std::move(damage));
    }

    std::vector<BattleAttackSpawnRequest> drainAttacks()
    {
        auto drained = std::move(attackSpawns_);
        attackSpawns_ = std::move(recycledAttackSpawns_);
        return drained;
    }

    std::vector<BattlePendingDamageIntent> drainDamage()
    {
        auto drained = std::move(pendingDamage_);
        pendingDamage_ = std::move(recycledPendingDamage_);
        return drained;
    }

    void recycleAttacks(std::vector<BattleAttackSpawnRequest>&& consumed)
    {
        consumed.clear();
        if (consumed.capacity() > recycledAttackSpawns_.capacity())
        {
            recycledAttackSpawns_.swap(consumed);
        }
    }

    void recycleDamage(std::vector<BattlePendingDamageIntent>&& consumed)
    {
        consumed.clear();
        if (consumed.capacity() > recycledPendingDamage_.capacity())
        {
            recycledPendingDamage_.swap(consumed);
        }
    }

    void cancelForBattleEnd(BattleCastLifecycle& lifecycle)
    {
        for (const auto& request : attackSpawns_)
        {
            assert(request.provenance.valid());
            assert(request.castWork.valid());
            lifecycle.completeWork(
                request.castWork,
                CastWorkResult::attackFinished(AttackFinishReason::BattleEnded));
        }
        for (const auto& intent : pendingDamage_)
        {
            if (intent.delayedCastWork.valid())
            {
                lifecycle.completeWork(intent.delayedCastWork);
            }
        }
        attackSpawns_.clear();
        pendingDamage_.clear();
        recycledAttackSpawns_.clear();
        recycledPendingDamage_.clear();
    }

    const std::vector<BattleAttackSpawnRequest>& queuedAttacks() const
    {
        return attackSpawns_;
    }

    const std::vector<BattlePendingDamageIntent>& queuedDamage() const
    {
        return pendingDamage_;
    }

    std::vector<BattleAttackSpawnRequest>& mutableAttacksForReducer()
    {
        return attackSpawns_;
    }

    std::vector<BattlePendingDamageIntent>& mutableDamageForReducer()
    {
        return pendingDamage_;
    }

private:
    std::vector<BattleAttackSpawnRequest> attackSpawns_;
    std::vector<BattlePendingDamageIntent> pendingDamage_;
    std::vector<BattleAttackSpawnRequest> recycledAttackSpawns_;
    std::vector<BattlePendingDamageIntent> recycledPendingDamage_;
};

}  // namespace KysChess::Battle
