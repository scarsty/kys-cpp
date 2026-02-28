#pragma once

#include "Chess.h"
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
    static void showExpeditionChallenge();
    static void showGameGuide();
    static void showPositionSwap();
    static int calculateCost(int tier, int star, int count = 1);
};

}    //namespace KysChess