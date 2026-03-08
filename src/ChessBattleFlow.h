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
    int runBattle(const DynamicBattleRoles& roles, const std::vector<Chess>& allyChess, int battle_id = -1, int seed = -1);

private:
    ChessSelectorServices services_;
    ChessRewardFlow& rewardFlow_;
};

}    // namespace KysChess