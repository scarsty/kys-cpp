#pragma once

#include "BattlePostBattleSummary.h"
#include "Point.h"
#include "battle/BattleRuntimeSession.h"

#include <span>
#include <vector>

class BattleReport;

struct BattleSceneUnitPresentationState
{
    int unitId = -1;
    int shake = 0;
    int attention = 0;
};

class BattleSceneUnitStore
{
public:
    void initialize(const KysChess::Battle::BattleRuntimeSession& runtimeSession);

    const BattleSceneUnitPresentationState& requirePresentation(int unitId) const;
    BattleSceneUnitPresentationState& requirePresentation(int unitId);
    const KysChess::Battle::BattleRuntimeUnit& requireRuntimeUnit(int unitId) const;

    std::span<const KysChess::Battle::BattleRuntimeUnit> runtimeUnits() const;
    std::vector<int> allyUnitIds() const;

    void setUnitShake(int unitId, int shake);
    void decreaseTransientPresentationState();
    Pointf facingTowardNearestEnemy(int unitId) const;
    BattlePostBattleSummary makePostBattleSummary(const BattleReport& report, int battleResult) const;

private:
    const KysChess::Battle::BattleRuntimeSession* runtime_session_ = nullptr;
    std::vector<BattleSceneUnitPresentationState> presentation_;
};
