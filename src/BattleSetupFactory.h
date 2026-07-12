#pragma once

#include "BattlefieldData.h"
#include "PreparedChessBattle.h"
#include "battle/BattleRuntimeSession.h"

#include <set>

namespace KysChess
{

class BattleSetupFactory
{
public:
    static void populateBaseFormation(
        PreparedChessBattle& prepared,
        const ChessGameContent& content);

    static Battle::BattleRuntimeSessionCreationInput build(
        const PreparedChessBattle& prepared,
        const ChessGameContent& content,
        const std::set<int>& obtainedNeigongIds,
        int maximumFrames);
};

}
