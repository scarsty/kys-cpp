#pragma once

#include "BattleSceneBattleAdapter.h"
#include "Point.h"
#include "Types.h"

#include <array>
#include <functional>
#include <span>
#include <vector>

namespace KysChess::BattleSceneSetupBuilder
{

struct BattleSceneSetupOpponentCell
{
    int team = -1;
    int x = 0;
    int y = 0;
};

struct BattleSceneSetupUnitRequest
{
    int unitId = -1;
    const Role* source = nullptr;
    int team = -1;
    int gridX = 0;
    int gridY = 0;
    int star = 1;
    int faceTowardsFallback = Towards_None;
    int weaponId = -1;
    int armorId = -1;
    int chessInstanceId = -1;
    int fightsWon = 0;
    int sourceOrder = 0;
    float gravity = -4.0f;
    std::function<Pointf(int, int)> positionForCell;
    std::function<std::array<int, 5>(int)> fightFramesForHeadId;
    std::function<Magic*(int)> magicById;
};

struct BattleSceneSetupBuildResult
{
    std::vector<KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput> units;
};

KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput makeSetupUnit(
    const BattleSceneSetupUnitRequest& request,
    std::span<const BattleSceneSetupOpponentCell> opponents);

BattleSceneSetupBuildResult buildSetupUnits(
    std::span<const BattleSceneSetupUnitRequest> requests);

}  // namespace KysChess::BattleSceneSetupBuilder
