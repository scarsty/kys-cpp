#include "ChessBattleAnalysis.h"
#include "ChessGameSessionTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::Test;

TEST_CASE("battle analysis reconciles the authoritative report and summary",
          "[chess][battle-analysis]")
{
    ChessGameSession session(configuredMapChoiceContent(), 61);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(1)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(2)).accepted);
    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = {session.state().roster.begin()->first};
    REQUIRE(session.submitAndDrain(deploy).accepted);
    ChessAction prepare;
    prepare.type = ChessActionType::PrepareBattle;
    REQUIRE(session.submitAndDrain(prepare).accepted);
    ChessAction map;
    map.type = ChessActionType::ChooseMap;
    map.mapId = session.state().preparedBattle->mapCandidates.front();
    REQUIRE(session.submitAndDrain(map).accepted);
    ChessAction start;
    start.type = ChessActionType::StartBattle;
    REQUIRE(session.submitAndDrain(start).accepted);
    REQUIRE(session.lastBattlePrepared());
    REQUIRE(session.lastBattleResult());

    const auto analysis = analyzeChessBattleResult(
        session.content(),
        *session.lastBattlePrepared(),
        *session.lastBattleResult());
    CHECK(analysis.outcome == session.lastBattleResult()->summary.outcome);
    CHECK(analysis.endFrame == session.lastBattleResult()->summary.endFrame);
    CHECK(analysis.digest == session.lastBattleResult()->digest);
    CHECK(analysis.unitStats.size() == session.lastBattlePrepared()->units.size());
    int analyzedDamage{};
    int reportedDamage{};
    for (const auto& unit : analysis.unitStats)
    {
        analyzedDamage += unit.damageDealt;
        if (const auto found = session.lastBattleResult()->report.stats().find(unit.unitId);
            found != session.lastBattleResult()->report.stats().end())
        {
            reportedDamage += found->second.damageDealt;
        }
    }
    CHECK(analyzedDamage == reportedDamage);
    CHECK(analysis.summary.contains(analysis.outcomeDescription));
}
