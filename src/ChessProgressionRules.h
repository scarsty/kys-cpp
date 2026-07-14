#pragma once

#include "HeadlessBattleRunner.h"
#include "ChessGameContent.h"
#include "PreparedChessBattle.h"
#include "ChessSessionTypes.h"

namespace KysChess
{

class ChessProgressionRules
{
public:
    static int baseVictoryGold(const ChessSessionState& state, const ChessGameContent& content);
    static int interestGold(const ChessSessionState& state, const ChessGameContent& content);
    static std::optional<int> nextInterestThreshold(const ChessSessionState& state, const ChessGameContent& content);
    static int projectedVictoryIncome(const ChessSessionState& state, const ChessGameContent& content);
    static void applyBattleResult(
        ChessSessionState& state,
        const ChessGameContent& content,
        ChessRunRandom& random,
        const HeadlessBattleResult& battle,
        std::vector<ChessSemanticEvent>& events);
};

}
