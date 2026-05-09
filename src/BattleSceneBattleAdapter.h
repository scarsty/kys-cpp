#pragma once

#include "BattlePostBattleSummary.h"
#include "BattleSceneUnitStore.h"
#include "ChessBattleEffects.h"
#include "Types.h"
#include "battle/BattleAttackSystem.h"
#include "battle/BattleCastSystem.h"
#include "battle/BattleCore.h"
#include "battle/BattleProjectileTargetingSystem.h"
#include "battle/BattleRuntimeRules.h"
#include "battle/BattleRuntimeSession.h"

#include <array>
#include <cstddef>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

class BattleTracker;

namespace KysChess::BattleSceneBattleAdapter
{

struct BattleSetupSkillSnapshot
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

struct BattleSetupRoleSnapshot
{
    int unitId = -1;
    int realRoleId = -1;
    std::string name;
    int headId = -1;
    int team = -1;
    bool alive = true;
    int gridX = 0;
    int gridY = 0;
    int faceTowards = 0;
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    Pointf facing;
    int star = 1;
    int cost = 0;
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;
    int attack = 0;
    int defence = 0;
    int speed = 0;
    int fist = 0;
    int sword = 0;
    int knife = 0;
    int unusual = 0;
    int hiddenWeapon = 0;
    int cooldown = 0;
    int cooldownMax = 0;
    bool haveAction = false;
    int actFrame = 0;
    Battle::BattleOperationType operationType = Battle::BattleOperationType::None;
    int actType = -1;
    int operationCount = 0;
    int physicalPower = 0;
    int invincible = 0;
    int hurtFrame = 0;
    int frozen = 0;
    int frozenMax = 0;
    std::array<int, 5> fightFrames{};
    std::array<int, 5> actPropertiesByMagicType{};
    bool hasEquippedSkill = false;
    std::string skillNames;
    BattleSetupSkillSnapshot normalSkill;
    BattleSetupSkillSnapshot ultimateSkill;
};

struct BattleRuntimeSceneSetupInput
{
    std::vector<BattleSetupRoleSnapshot> roles;
    std::map<int, RoleComboState>* comboStates = nullptr;
    std::vector<Battle::BattleTerrainCell> terrainCells;
    std::vector<Battle::BattleSetupRosterUnit> allyRoster;
    std::vector<Battle::BattleSetupRosterUnit> enemyRoster;
    std::vector<int> obtainedNeigongMagicIds;
    std::vector<std::pair<int, int>> cloneSpawnCells;
    std::vector<Battle::BattleRescueCellSnapshot> rescueCells;
    std::map<std::pair<int, int>, Pointf> rescuePositionsByCell;
    int battleFrame = 0;
    int rescueCounterAttackSkillId = -1;
};

struct BattleRuntimeBuildContext
{
    BattleRuntimeSceneSetupInput setup;
    Battle::BattleRuntimeRulesConfig rules;
};

struct BattleInitializationSceneApplyResult
{
    std::map<int, RoleComboState> comboStates;
    std::vector<BattleSceneUnit> units;
    std::vector<Battle::BattleLogEvent> logEvents;
    std::vector<Battle::BattleVisualEvent> visualEvents;
};

struct BattleRuntimeCreationResult
{
    Battle::BattleRuntimeSession session;
    BattleInitializationSceneApplyResult sceneInitialization;
};

BattleRuntimeCreationResult createInitializedBattleRuntimeSession(const BattleRuntimeBuildContext& context);
}  // namespace KysChess::BattleSceneBattleAdapter
