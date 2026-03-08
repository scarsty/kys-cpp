#pragma once

#include "Chess.h"
#include "ChessBalance.h"
#include <vector>

struct DynamicBattleRoles;

namespace KysChess
{

class ChessEconomy;
class ChessEquipmentInventory;
class ChessProgress;
class ChessRandom;
class ChessRoleSave;
class ChessRoster;
class ChessShop;

class ChessSelector
{
public:
    ChessSelector(
        ChessRoleSave& roleSave,
        ChessEquipmentInventory& equipmentInventory,
        ChessRoster& roster,
        ChessShop& shop,
        ChessProgress& progress,
        ChessEconomy& economy,
        ChessRandom& random);

    void getChess();
    void sellChess();
    void selectForBattle();
    void enterBattle();
    int runBattle(const DynamicBattleRoles& roles, const std::vector<Chess>& allyChess, int battle_id = -1, int seed = -1);
    void buyExp();
    void showContextMenu();
    void viewCombos();
    void showNeigongReward();
    void viewNeigong();
    void manageEquipment();
    void showExpeditionChallenge();
    void showGameGuide();
    void showPositionSwap();

private:
    int selectChallengeReward(const std::vector<BalanceConfig::ChallengeReward>& rewards);
    bool applyReward(const BalanceConfig::ChallengeReward& reward);
    bool rewardGold(int amount);
    bool rewardPiece(int maxTier);
    bool rewardNeigong(int maxTier);
    bool rewardStarUp(int fromStar, int maxTier);
    bool rewardEquipment(int maxTier, int specificId = -1);
    bool addChessPiece(Role* role, bool showMessage = true);

    Role* getRoleById(int roleId) const;

    ChessRoleSave& roleSave_;
    ChessEquipmentInventory& equipmentInventory_;
    ChessRoster& roster_;
    ChessShop& shop_;
    ChessProgress& progress_;
    ChessEconomy& economy_;
    ChessRandom& random_;
};

}    //namespace KysChess