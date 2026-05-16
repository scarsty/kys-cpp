#pragma once

#include "BattleInitialization.h"
#include "BattleRuntimeRules.h"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace KysChess::Battle
{

struct BattleSetupPlacementUnit
{
    int unitId = -1;
    int x = 0;
    int y = 0;
    int faceTowards = 0;
};

struct BattleSetupPlacementInput
{
    std::vector<BattleSetupPlacementUnit> units;
};

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
    BattleUnitVitals vitals;
    BattleUnitStats stats;
    BattleUnitMotion motion;
    BattleUnitAnimationState animation;
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
    BattleOperationType operationType = BattleOperationType::None;
    int operationCount = 0;
    int physicalPower = 0;
    int invincible = 0;
    int frozen = 0;
    int frozenMax = 0;
    std::array<int, 5> fightFrames{};
    std::array<int, 5> actPropertiesByMagicType{};
    bool hasEquippedSkill = false;
    std::string skillNames;
    BattleActionSkillSeed normalSkill;
    BattleActionSkillSeed ultimateSkill;
};

struct BattleRuntimeSessionCreationInput
{
    std::vector<BattleSetupUnitInput> units;
    BattleRuntimeSetupSeed setup;
    std::map<int, RoleComboState> comboStates;
    std::vector<BattleTerrainCell> terrainCells;
    std::vector<BattleRescueCellSnapshot> rescueCells;
    std::vector<BattleActionPlanSeed> actionPlanSeeds;
    BattleRuntimeRulesConfig rules;
    unsigned int randomSeed = 1;
    int battleFrame = 0;
    int rescueCounterAttackSkillId = -1;
};

class BattleRuntimeSession
{
public:
    explicit BattleRuntimeSession(BattleRuntimeInit init);

    static BattleRuntimeSession createInitialized(BattleRuntimeSessionCreationInput input);

    BattleFrameResult runFrame();
    void commitSetupPlacement(const BattleSetupPlacementInput& input);
    BattleInitializationResult releaseInitializationResult();

    const BattleRuntimeState& runtime() const;

private:
    BattleRuntimeState runtime_;
    std::optional<BattleInitializationResult> initialization_result_;
    BattleFrameRunner runner_;
    bool setupPlacementCommitted_ = false;
    bool frameStarted_ = false;
};

}  // namespace KysChess::Battle
