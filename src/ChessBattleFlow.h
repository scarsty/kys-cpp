#pragma once

#include "ChessUiCommon.h"

struct DynamicBattleRoles;

namespace KysChess
{

class ChessRewardFlow;

class ChessBattleFlow
{
public:
    ChessBattleFlow(const ChessSelectorServices& services, ChessRewardFlow& rewardFlow);

    void selectForBattle();
    void enterBattle();
    int runBattle(
        const DynamicBattleRoles& roles,
        const std::vector<Chess>& allyChess,
        unsigned int battleSeed,
        int battle_id = -1,
        bool countFightsWon = true);

private:
    ChessSelectorServices services_;
    ChessRewardFlow& rewardFlow_;
};

}    // namespace KysChess
