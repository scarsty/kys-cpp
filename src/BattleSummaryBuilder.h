#pragma once

#include "BattleReport.h"
#include "battle/BattleOutcome.h"
#include "battle/BattleRuntimeSession.h"

#include <vector>

namespace KysChess
{

struct BattleSurvivorSummary
{
    int unitId = -1;
    int chessInstanceId = -1;
    int roleId = -1;
    int team = -1;
    int hp{};
    int mp{};
    bool summoned{};

    auto operator<=>(const BattleSurvivorSummary&) const = default;
};

struct BattleSummary
{
    Battle::BattleOutcome outcome = Battle::BattleOutcome::InProgress;
    int endFrame{};
    std::vector<BattleSurvivorSummary> survivors;
};

class BattleSummaryBuilder
{
public:
    static BattleSummary build(
        const Battle::BattleRuntimeSession& session,
        const BattleReport& report);
};

}
