#pragma once

#include "BattlePostBattleSummary.h"
#include "ChessCombo.h"
#include "Point.h"
#include "battle/BattleRuntimeSession.h"

#include <array>
#include <span>
#include <string>
#include <vector>

class BattleReport;

struct BattleSceneUnitPresentationState
{
    BattleUnitIdentity identity;
    int unitId = -1;
    int sourceUnitId = -1;
    int headId = -1;
    int faceTowards = Towards_None;
    std::array<int, 5> fightFrames{};
    std::string skillNames;
    int chessInstanceId = -1;
    int weaponId = -1;
    int armorId = -1;
    int shake = 0;
    int attention = 0;
};

struct BattleSceneSetupPlacementState
{
    int unitId = -1;
    int gridX = 0;
    int gridY = 0;
    int faceTowards = Towards_None;
    Pointf position;
    bool active = true;
};

class BattleSceneUnitStore
{
public:
    void initializeFromRuntimeCreation(
        const KysChess::Battle::BattleRuntimeSession& runtimeSession,
        const KysChess::Battle::BattleRuntimeSessionCreationInput& input,
        const KysChess::Battle::BattleInitializationResult& initialization);

    const BattleSceneUnitPresentationState& requirePresentation(int unitId) const;
    BattleSceneUnitPresentationState& requirePresentation(int unitId);
    const KysChess::Battle::BattleRuntimeUnit& requireRuntimeUnit(int unitId) const;
    const BattleSceneSetupPlacementState& requireSetupPlacement(int unitId) const;

    std::span<const KysChess::Battle::BattleRuntimeUnit> runtimeUnits() const;
    std::vector<int> allyUnitIds() const;
    int aliveUnitsOnTeam(int team) const;

    void setUnitShake(int unitId, int shake);
    void decreaseTransientPresentationState();
    void swapSetupUnitPositions(int firstUnitId, int secondUnitId);
    KysChess::Battle::BattleSetupPlacementInput makeSetupPlacementInput() const;
    std::vector<KysChess::ChessComboBattleUnitRef> makeComboBattleUnitRefs() const;
    Pointf facingTowardNearestEnemy(int unitId) const;
    BattlePostBattleSummary makePostBattleSummary(const BattleReport& report, int battleResult) const;

private:
    void initialize(
        const KysChess::Battle::BattleRuntimeSession& runtimeSession,
        std::vector<BattleSceneUnitPresentationState> presentationStates,
        std::vector<BattleSceneSetupPlacementState> setupPlacements);

    const KysChess::Battle::BattleRuntimeSession* runtime_session_ = nullptr;
    std::vector<BattleSceneUnitPresentationState> presentation_;
    std::vector<BattleSceneSetupPlacementState> setup_placements_;
};
