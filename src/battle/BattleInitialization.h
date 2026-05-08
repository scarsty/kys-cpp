#pragma once

#include "BattleCore.h"

#include <vector>

namespace KysChess::Battle
{

struct BattleInitializationUnitSeed
{
    int unitId = -1;
    int realRoleId = -1;
    int team = 0;
    int star = 0;
    int cost = 0;
    int baseMaxHp = 0;
    int baseAttack = 0;
    int baseDefence = 0;
    int baseSpeed = 0;
    RoleComboState baseCombo;
};

struct BattleInitializationCloneSpawnCell
{
    int x = 0;
    int y = 0;
    bool walkable = false;
    bool occupied = false;
};

struct BattleInitializationCloneSource
{
    int sourceUnitId = -1;
    int sourceRealRoleId = -1;
    int power = 0;
};

struct BattleRuntimeSetupSeed
{
    std::vector<BattleInitializationUnitSeed> units;
    std::vector<BattleInitializationCloneSource> cloneSources;
    std::vector<BattleInitializationCloneSpawnCell> cloneCells;
    int cloneSummonCount = 0;
    int nextCloneUnitId = 100000;
};

struct BattleRuntimeInit
{
    BattleRuntimeState runtime;
    BattleRuntimeSetupSeed setup;
};

struct BattleInitializationRoleDelta
{
    int unitId = -1;
    int maxHp = 0;
    int hp = 0;
    int attack = 0;
    int defence = 0;
    int speed = 0;
};

struct BattleInitializationCloneIntent
{
    int sourceUnitId = -1;
    int cloneUnitId = -1;
    int gridX = 0;
    int gridY = 0;
    BattleInitializationRoleDelta roleValues;
    RoleComboState combo;
};

struct BattleInitializationEnemyTopDebuffDelta
{
    int unitId = -1;
    int attackDelta = 0;
    int defenceDelta = 0;
    int appliedValue = 0;
};

struct BattleInitializationResult
{
    std::vector<BattleInitializationRoleDelta> roleDeltas;
    std::vector<BattleInitializationCloneIntent> cloneIntents;
    std::vector<BattleInitializationEnemyTopDebuffDelta> enemyTopDebuffs;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
};

class BattleInitializationSystem
{
public:
    BattleInitializationResult initialize(BattleRuntimeState& runtime,
                                          const BattleRuntimeSetupSeed& setup) const;
};

}  // namespace KysChess::Battle