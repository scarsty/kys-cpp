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
    static void applyBattleResult(
        ChessSessionState& state,
        const ChessGameContent& content,
        ChessRunRandom& random,
        const HeadlessBattleResult& battle,
        std::vector<ChessSemanticEvent>& events);
};

}
