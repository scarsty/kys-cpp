#pragma once

#include "BattleSummaryBuilder.h"
#include "ChessGameContent.h"
#include "PreparedChessBattle.h"
#include "battle/BattleDigestEvent.h"

namespace KysChess
{

class ChessBattleText
{
public:
    static std::string formatHeader(
        int fight,
        const PreparedChessBattle& prepared,
        const ChessGameContent& content);
    static std::string formatEvents(
        const PreparedChessBattle& prepared,
        const std::vector<Battle::BattleDigestEvent>& events);
    static std::string formatSummary(const BattleSummary& summary);
};

}
