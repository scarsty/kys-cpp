#pragma once

#include "ChessGameContent.h"
#include "ChessSessionTypes.h"
#include "PreparedChessBattle.h"

namespace KysChess
{

class ChessBattlePlanner
{
public:
    static std::vector<int> fittingMaps(const ChessSessionState& state, const ChessGameContent& content,
        const BalanceConfig::ChallengeDef* challenge = nullptr);
    static bool chosenMapFits(const PreparedChessBattle& battle, const ChessGameContent& content);
    static bool preparedRosterIsReady(const ChessSessionState& state, const ChessGameContent& content);
    static PreparedChessBattle prepareRosterPreview(
        const ChessSessionState& state, const ChessGameContent& content);
    static void applyPlayerTalents(PreparedChessBattle& battle,
        const ChessSessionState& state, const ChessGameContent& content);
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
