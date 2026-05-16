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

namespace KysChess::BattleSceneBattleAdapter
{

using BattleSetupUnitInput = Battle::BattleSetupUnitInput;

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
