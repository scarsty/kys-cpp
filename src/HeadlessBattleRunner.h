#pragma once

#include "BattleReportCollector.h"
#include "BattleSummaryBuilder.h"
#include "ChessReplayHash.h"
#include "battle/BattleDigestEvent.h"

namespace KysChess
{

struct HeadlessBattleResult
{
    Battle::BattleInitializationResult initialization;
    std::vector<Battle::BattleDigestEvent> digestEvents;
    BattleReport report;
    BattleSummary summary;
    Battle::BattleRuntimeState finalRuntime;
    ChessSha256 digest{};
};

class HeadlessBattleRunner
{
public:
    static HeadlessBattleResult run(Battle::BattleRuntimeSessionCreationInput input);
    static ChessSha256 digest(const HeadlessBattleResult& result);
};

}
