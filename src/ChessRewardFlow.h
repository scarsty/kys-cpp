#pragma once

#include "ChessBalance.h"
#include "ChessUiCommon.h"

namespace KysChess
{

class ChessRewardFlow
{
public:
    explicit ChessRewardFlow(const ChessSelectorServices& services);

    void showNeigongReward();
    void showEquipmentReward();
    int selectChallengeReward(const std::vector<BalanceConfig::ChallengeReward>& rewards);
    bool applyReward(const BalanceConfig::ChallengeReward& reward);

private:
    bool rewardGold(int amount);
    bool rewardPiece(int maxTier);
    bool rewardNeigong(int maxTier);
    bool rewardStarUp(int fromStar, int maxTier);
    bool rewardEquipment(int maxTier, int specificId = -1);
    bool addChessPiece(Role* role, bool showMessage = true);

    ChessSelectorServices services_;
};

}    // namespace KysChess