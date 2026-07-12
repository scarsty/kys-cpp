#pragma once

#include "ChessGameContent.h"
#include "ChessSessionTypes.h"
#include "PreparedChessBattle.h"

namespace KysChess
{

class ChessBattlePlanner
{
public:
    static PreparedChessBattle prepareCampaign(
        const ChessSessionState& state,
        const ChessGameContent& content,
        ChessRunRandom& random);
    static PreparedChessBattle prepareChallenge(
        const ChessSessionState& state,
        const ChessGameContent& content,
        ChessRunRandom& random,
        const BalanceConfig::ChallengeDef& challenge);
};

}
