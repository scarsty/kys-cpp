#pragma once

#include "BattleAttackSystem.h"
#include "BattleDamageQueue.h"

#include <utility>
#include <vector>

namespace KysChess::Battle
{

class BattleNextFrameQueues
{
public:
    void queueAttack(BattleAttackSpawnRequest request)
    {
        attackSpawns_.push_back(std::move(request));
    }

    void queueDamage(BattlePendingDamageIntent damage)
    {
        pendingDamage_.push_back(std::move(damage));
    }

    std::vector<BattleAttackSpawnRequest> drainAttacks()
    {
        return std::exchange(attackSpawns_, {});
    }

    std::vector<BattlePendingDamageIntent> drainDamage()
    {
        return std::exchange(pendingDamage_, {});
    }

    const std::vector<BattleAttackSpawnRequest>& queuedAttacksForTest() const
    {
        return attackSpawns_;
    }

    const std::vector<BattlePendingDamageIntent>& queuedDamageForTest() const
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
};

}  // namespace KysChess::Battle
