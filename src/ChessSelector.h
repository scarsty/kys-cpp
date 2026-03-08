#pragma once

#include "Chess.h"
#include "ChessBalance.h"
#include <vector>

struct DynamicBattleRoles;

namespace KysChess
{

class ChessEconomy;
class ChessEquipmentInventory;
struct ChessSelectorServices;
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
    ChessSelectorServices services() const;

    ChessRoleSave& roleSave_;
    ChessEquipmentInventory& equipmentInventory_;
    ChessRoster& roster_;
    ChessShop& shop_;
    ChessProgress& progress_;
    ChessEconomy& economy_;
    ChessRandom& random_;
};

}    //namespace KysChess