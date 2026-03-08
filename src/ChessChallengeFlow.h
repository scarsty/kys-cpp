#pragma once

#include "ChessUiCommon.h"

namespace KysChess
{

class ChessBattleFlow;
class ChessRewardFlow;

class ChessChallengeFlow
{
public:
    ChessChallengeFlow(const ChessSelectorServices& services, ChessBattleFlow& battleFlow, ChessRewardFlow& rewardFlow);

    void showExpeditionChallenge();

private:
    ChessSelectorServices services_;
    ChessBattleFlow& battleFlow_;
    ChessRewardFlow& rewardFlow_;
};

}    // namespace KysChess