#pragma once

#include "BattleCore.h"

#include <string>
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
    int baseFist = 0;
    int baseSword = 0;
    int baseKnife = 0;
    int baseUnusual = 0;
    int baseHiddenWeapon = 0;
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
    int star = 0;
    int chessInstanceId = -1;
    int sourceOrder = 0;
};

struct BattleSetupComboThreshold
{
    int count = 0;
    std::vector<ComboEffect> effects;
};

struct BattleSetupComboDefinition
{
    int id = -1;
    std::string name;
    std::vector<int> memberRoleIds;
    std::vector<BattleSetupComboThreshold> thresholds;
    bool isAntiCombo = false;
    bool starSynergyBonus = false;
};

struct BattleSetupEquipmentDefinition
{
    int itemId = -1;
    int equipType = 0;
    std::vector<ComboEffect> effects;
    std::vector<std::string> actAsComboNames;
};

struct BattleSetupEquipmentSynergyDefinition
{
    std::vector<int> roleIds;
    int equipmentId = -1;
    std::vector<ComboEffect> effects;
    std::vector<std::string> actAsComboNames;
};

struct BattleSetupNeigongDefinition
{
    int magicId = -1;
    std::vector<ComboEffect> effects;
};

struct BattleSetupRosterUnit
{
    int unitId = -1;
    int realRoleId = -1;
    int team = 0;
    int star = 1;
    int cost = 0;
    int weaponId = -1;
    int armorId = -1;
    int chessInstanceId = -1;
    int fightsWon = 0;
    int sourceOrder = 0;
};

struct BattleRuntimeSetupSeed
{
    std::vector<BattleInitializationUnitSeed> units;
    std::vector<BattleSetupRosterUnit> allyRoster;
    std::vector<BattleSetupRosterUnit> enemyRoster;
    std::vector<BattleSetupComboDefinition> comboDefinitions;
    std::vector<BattleSetupEquipmentDefinition> equipmentDefinitions;
    std::vector<BattleSetupEquipmentSynergyDefinition> equipmentSynergies;
    std::vector<BattleSetupNeigongDefinition> neigongDefinitions;
    std::vector<int> obtainedNeigongMagicIds;
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
    int star = 1;
    int maxHp = 0;
    int hp = 0;
    int attack = 0;
    int defence = 0;
    int speed = 0;
    int fist = 0;
    int sword = 0;
    int knife = 0;
    int unusual = 0;
    int hiddenWeapon = 0;
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
    std::map<int, RoleComboState> comboStates;
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