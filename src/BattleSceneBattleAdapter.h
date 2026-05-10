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

struct BattleSetupUnitInput
{
    int unitId = -1;
    int realRoleId = -1;
    std::string name;
    int headId = -1;
    int team = -1;
    int sourceOrder = 0;
    bool alive = true;
    int gridX = 0;
    int gridY = 0;
    int faceTowards = 0;
    Battle::BattleUnitVitals vitals;
    Battle::BattleUnitStats stats;
    Battle::BattleUnitMotion motion;
    Battle::BattleUnitAnimationState animation;
    int star = 1;
    int cost = 0;
    int weaponId = -1;
    int armorId = -1;
    int chessInstanceId = -1;
    int fightsWon = 0;
    int fist = 0;
    int sword = 0;
    int knife = 0;
    int unusual = 0;
    int hiddenWeapon = 0;
    bool haveAction = false;
    Battle::BattleOperationType operationType = Battle::BattleOperationType::None;
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
    Battle::BattleActionSkillSeed normalSkill;
    Battle::BattleActionSkillSeed ultimateSkill;
};

struct BattleRuntimeCreationInput
{
    std::vector<BattleSetupUnitInput> units;
    Battle::BattleRuntimeSetupSeed runtimeSetupSeed;
    std::map<int, RoleComboState>* comboStates = nullptr;
    std::vector<Battle::BattleTerrainCell> terrainCells;
    std::vector<Battle::BattleRescueCellSnapshot> rescueCells;
    std::vector<Battle::BattleActionPlanSeed> actionPlanSeeds;
    int battleFrame = 0;
    int rescueCounterAttackSkillId = -1;
};

struct BattleRuntimeBuildContext
{
    BattleRuntimeCreationInput input;
    Battle::BattleRuntimeRulesConfig rules;
    unsigned int randomSeed = 1;
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
