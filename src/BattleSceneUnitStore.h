#pragma once

#include "BattlePostBattleSummary.h"
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

class BattleSceneUnitStore
{
public:
    void initializeFromRuntimeCreation(
        const KysChess::Battle::BattleRuntimeSession& runtimeSession,
        const KysChess::Battle::BattleRuntimeSessionCreationInput& input);

    const BattleSceneUnitPresentationState& requirePresentation(int unitId) const;
    BattleSceneUnitPresentationState& requirePresentation(int unitId);
    const KysChess::Battle::BattleRuntimeUnit& requireRuntimeUnit(int unitId) const;

    std::span<const KysChess::Battle::BattleRuntimeUnit> runtimeUnits() const;
    std::vector<int> allyUnitIds() const;
    int aliveUnitsOnTeam(int team) const;

    void setUnitShake(int unitId, int shake);
    void decreaseTransientPresentationState();
    Pointf facingTowardNearestEnemy(int unitId) const;
    BattlePostBattleSummary makePostBattleSummary(const BattleReport& report, int battleResult) const;

private:
    void initialize(
        const KysChess::Battle::BattleRuntimeSession& runtimeSession,
        std::vector<BattleSceneUnitPresentationState> presentationStates);

    const KysChess::Battle::BattleRuntimeSession* runtime_session_ = nullptr;
    std::vector<BattleSceneUnitPresentationState> presentation_;
};
