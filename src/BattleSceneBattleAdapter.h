#pragma once

#include "BattleSceneAct.h"
#include "battle/BattleAttackSystem.h"
#include "battle/BattleCastSystem.h"
#include "battle/BattleCore.h"
#include "battle/BattleProjectileTargetingSystem.h"
#include "battle/BattleRuntimeRules.h"
#include "battle/BattleRuntimeSession.h"

#include <cstddef>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class BattleTracker;

namespace KysChess::BattleSceneBattleAdapter
{

struct BattleActionFrameApplyContext
{
    const std::unordered_map<int, Role*>* rolesByBattleId = nullptr;
    int battleFrame = 0;
    float gravity = 0.0f;
};

struct BattleActionFrameApplyResult
{
    int blinkSoundCount = 0;
    std::vector<int> faceTowardsNearestUnitIds;
};

struct BattleLifecycleApplicationContext
{
    BattleTracker* tracker = nullptr;
    const std::unordered_map<int, Role*>* rolesByBattleId = nullptr;
    int currentBattleResult = -1;
};

struct BattleLifecycleApplicationResult
{
    bool battleEnded = false;
    int battleResult = -1;
    bool unitDied = false;
    std::vector<int> diedUnitIds;
};

struct BattleRuntimeSceneSetupInput
{
    std::vector<Role*> roles;
    std::map<int, RoleComboState>* comboStates = nullptr;
    std::vector<Battle::BattleTerrainCell> terrainCells;
    std::vector<Battle::BattleSetupRosterUnit> allyRoster;
    std::vector<Battle::BattleSetupRosterUnit> enemyRoster;
    std::vector<int> obtainedNeigongMagicIds;
    std::vector<std::pair<int, int>> cloneSpawnCells;
    std::vector<Battle::BattleRescueCellSnapshot> rescueCells;
    std::map<std::pair<int, int>, Pointf> rescuePositionsByCell;
    std::map<int, int> pendingAliveByTeam;
    int battleFrame = 0;
    int rescueCounterAttackSkillId = -1;
    int nextCloneUnitId = 100000;
};

struct BattleRuntimeBuildContext
{
    BattleRuntimeSceneSetupInput setup;
    Battle::BattleRuntimeRulesConfig rules;
};

struct BattleRuntimeCreationResult
{
    Battle::BattleRuntimeSession session;
    Battle::BattleInitializationResult initializationResult;
    std::unordered_map<int, Role*> rolesByBattleId;
};

struct BattleInitializationApplyContext
{
    std::vector<Role*>* battleRoles = nullptr;
    std::deque<Role>* friendsObj = nullptr;
    std::map<int, RoleComboState>* comboStates = nullptr;
    std::unordered_map<int, Role*>* rolesByBattleId = nullptr;
};

Role* findSetupRoleByBattleId(const std::vector<Role*>& roles, int unitId);
BattleRuntimeCreationResult createInitializedBattleRuntimeSession(const BattleRuntimeBuildContext& context);
void commitInitializedBattleRuntimeConfiguration(
    Battle::BattleRuntimeSession& session,
    const BattleRuntimeBuildContext& context,
    const std::unordered_map<int, Role*>& rolesByBattleId);
void applyBattleInitializationResult(
    const Battle::BattleInitializationResult& result,
    const BattleInitializationApplyContext& context);
Battle::BattleSetupPlacementInput makeBattleSetupPlacementInput(const std::vector<Role*>& roles);

void applyBattleFrameUnitApplication(Role* role, const Battle::BattleFrameUnitApplication& application);
void applyBattleProjectileCancelDamage(Role* role, int damage);

void writeBattleDamageRenderUnit(Role* role, RoleComboState* state, const Battle::BattleDamageUnitState& unit);
void applyBattleMovementPresentationResults(
    const std::vector<Battle::BattleFrameMovementPresentationUnitResult>& movementResults,
    const std::unordered_map<int, Role*>& rolesByBattleId);
BattleActionFrameApplyResult applyBattleActionFrameResults(
    const std::vector<Battle::BattleFrameActionUnitResult>& actionResults,
    const BattleActionFrameApplyContext& context);
BattleLifecycleApplicationResult applyBattleLifecycleEvents(
    const BattleLifecycleApplicationContext& context,
    const std::vector<Battle::BattleGameplayEvent>& events);
}  // namespace KysChess::BattleSceneBattleAdapter
