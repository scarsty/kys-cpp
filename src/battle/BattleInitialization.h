#pragma once

#include "BattleRuntimeUnitSpawn.h"

#include <map>
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
};

struct BattleInitializationRoleDelta
{
    int unitId{};
    int star{};
    BattleUnitVitals vitals;
    BattleUnitStats stats;
    int fist{};
    int sword{};
    int knife{};
    int unusual{};
    int hiddenWeapon{};
};

struct BattleInitializationEnemyTopDebuffDelta
{
    int unitId{};
    int attackDelta{};
    int defenceDelta{};
    int appliedValue{};
};

struct BattleInitializationResult
{
    std::vector<BattleInitializationRoleDelta> roleDeltas;
    std::vector<BattleInitializationEnemyTopDebuffDelta> enemyTopDebuffs;
    std::map<int, RoleComboState> comboStates;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
};

struct BattleInitializationOutput
{
    std::vector<BattleRuntimeUnitSpawn> spawns;
    BattleInitializationResult result;
};

struct BattleInitializationContext
{
    BattleGridTransform gridTransform;
    int frame{};
};

class BattleStartInitializer
{
public:
    BattleStartInitializer(std::vector<BattleRuntimeUnitSpawn> spawns,
                           const BattleRuntimeSetupSeed& setup,
                           BattleInitializationContext context);
    BattleStartInitializer(const BattleStartInitializer&) = delete;
    BattleStartInitializer& operator=(const BattleStartInitializer&) = delete;
    BattleStartInitializer(BattleStartInitializer&&) = default;
    BattleStartInitializer& operator=(BattleStartInitializer&&) = delete;

    BattleInitializationOutput initialize() &&;

private:
    std::vector<BattleRuntimeUnitSpawn> spawns_;
    const BattleRuntimeSetupSeed& setup_;
    BattleInitializationContext context_;
};

}  // namespace KysChess::Battle
