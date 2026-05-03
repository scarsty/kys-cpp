#pragma once

#include "BattleSceneAct.h"
#include "battle/BattleAttackSystem.h"

#include <cstddef>
#include <deque>
#include <set>
#include <unordered_map>
#include <vector>

namespace KysChess::BattleSceneBattleAdapter
{

Role* findRoleByBattleId(const std::vector<Role*>& roles, int unitId);

Battle::BattleAttackWorld makeBattleAttackWorld(
    const std::vector<Role*>& roles,
    const std::deque<BattleSceneAct::AttackEffect>& effects,
    size_t effectCount,
    const std::unordered_map<int, std::set<int>>& sharedHitGroupTargets);

void writeBattleAttackWorld(
    const Battle::BattleAttackWorld& world,
    std::deque<BattleSceneAct::AttackEffect>& effects,
    const std::vector<Role*>& roles,
    std::unordered_map<int, std::set<int>>& sharedHitGroupTargets);

}  // namespace KysChess::BattleSceneBattleAdapter
