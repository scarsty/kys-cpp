#pragma once

#include "BattlefieldData.h"
#include "PreparedChessBattle.h"
#include "battle/BattleRuntimeSession.h"

#include <vector>

namespace KysChess
{

class BattleSetupFactory
{
public:
    static int requiredFormationSlots(
        const PreparedChessBattle& prepared,
        int team);

    static void populateBaseFormation(
        PreparedChessBattle& prepared,
        const ChessGameContent& content);

    static std::vector<PreparedChessBattleUnit> resolvePreparedFormation(
        const PreparedChessBattle& prepared,
        const ChessGameContent& content);

    static Battle::BattleRuntimeSessionCreationInput build(
        const PreparedChessBattle& prepared,
        const ChessGameContent& content,
        int maximumFrames);
};

}
