#pragma once

#include "BattlePostBattleSummary.h"
#include "Point.h"
#include "battle/BattleRuntimeSession.h"

#include <array>
#include <cassert>
#include <vector>

class BattleReport;

struct BattleSceneUnitPresentationState
{
    int unitId = -1;
    int shake = 0;
    int attention = 0;
    std::array<int, 5> fightFrames{};
};

class BattleSceneUnitStore
{
public:
    std::vector<int> initialize(const KysChess::Battle::BattleRuntimeSession& runtimeSession);
    std::vector<int> synchronizeRuntimeUnits();

    const BattleSceneUnitPresentationState& requirePresentation(int unitId) const;
    BattleSceneUnitPresentationState& requirePresentation(int unitId);
    const KysChess::Battle::BattleRuntimeUnit& requireRuntimeUnit(int unitId) const;

    auto runtimeUnits() const
    {
        assert(runtime_session_ != nullptr);
        return runtime_session_->runtimeUnits();
    }
    std::vector<int> allyUnitIds() const;

    void setUnitShake(int unitId, int shake);
    void setFightFrames(int unitId, std::array<int, 5> fightFrames);
    void decreaseTransientPresentationState();
    BattlePostBattleSummary makePostBattleSummary(const BattleReport& report, int battleResult) const;

private:
    const KysChess::Battle::BattleRuntimeSession* runtime_session_ = nullptr;
    std::vector<BattleSceneUnitPresentationState> presentation_;
};
