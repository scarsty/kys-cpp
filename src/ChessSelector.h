#pragma once

#include "Chess.h"
#include "ChessBalance.h"
#include <vector>

struct DynamicBattleRoles;

namespace KysChess
{

class ChessSelector
{
public:
    static void getChess();
    static void sellChess();
    static void selectForBattle();
    static void enterBattle();
    static int runBattle(const DynamicBattleRoles& roles, const std::vector<Chess>& allyChess, int battle_id = -1, int seed = -1);
    static void buyExp();
    static void showContextMenu();
    static void viewCombos();
    static void showNeigongReward();
    static void viewNeigong();
    static void manageEquipment();
    static void showExpeditionChallenge();
    static void showGameGuide();
    static void showPositionSwap();

private:
    static int selectChallengeReward(const std::vector<BalanceConfig::ChallengeReward>& rewards);
    static bool applyReward(const BalanceConfig::ChallengeReward& reward);
    static bool rewardGold(int amount);
    static bool rewardPiece(int maxTier);
    static bool rewardNeigong(int maxTier);
    static bool rewardStarUp(int fromStar, int maxTier);
    static bool rewardEquipment(int maxTier, int specificId = -1);
    static bool addChessPiece(Role* role, bool showMessage = true);
};

}    //namespace KysChess