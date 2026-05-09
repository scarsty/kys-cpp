#pragma once

#include "BattlePostBattleSummary.h"
#include "ChessCombo.h"
#include "Point.h"
#include "battle/BattleRuntimeSession.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

class BattleTracker;

namespace KysChess::BattleSceneBattleAdapter
{
struct BattleInitializationSceneApplyResult;
}

struct BattleSceneRenderComboState
{
    int shield = 0;
    int blockFirstHitsRemaining = 0;
};

struct BattleSceneUnit
{
    BattleUnitIdentity identity;
    int unitId = -1;
    int sourceUnitId = -1;

    int gridX = 0;
    int gridY = 0;
    int faceTowards = Towards_None;
    int star = 1;
    int chessInstanceId = -1;
    int weaponId = -1;
    int armorId = -1;
    int cost = 0;
    std::string skillNames;

    bool alive = true;
    bool active = true;
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    Pointf realTowards;
    std::array<int, 5> fightFrames{};
    int actType = -1;
    int actFrame = 0;
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;
    int attack = 0;
    int defence = 0;
    int speed = 0;
    int cooldown = 0;
    int cooldownMax = 0;
    int frozen = 0;
    int frozenMax = 0;
    int invincible = 0;
    int shake = 0;
    int attention = 0;
    BattleSceneRenderComboState combo;
};

class BattleSceneUnitStore
{
public:
    using PositionForCell = std::function<Pointf(int, int)>;

    void initialize(const KysChess::BattleSceneBattleAdapter::BattleInitializationSceneApplyResult& result);

    BattleSceneUnit& requireUnit(int unitId);
    const BattleSceneUnit& requireUnit(int unitId) const;

    std::vector<BattleSceneUnit>& units() { return units_; }
    const std::vector<BattleSceneUnit>& units() const { return units_; }

    void swapSetupUnitPositions(int firstUnitId, int secondUnitId, PositionForCell positionForCell);
    KysChess::Battle::BattleSetupPlacementInput makeSetupPlacementInput() const;
    std::vector<KysChess::ChessComboBattleUnitRef> makeComboBattleUnitRefs() const;
    Pointf facingTowardNearestEnemy(int unitId) const;
    int aliveUnitsOnTeam(int team) const;
    std::vector<int> allyUnitIds() const;
    BattlePostBattleSummary makePostBattleSummary(const BattleTracker& tracker, int battleResult) const;

private:
    std::vector<BattleSceneUnit> units_;
};
