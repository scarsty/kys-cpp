#pragma once

namespace KysChess
{

class ChessSelector
{
public:
    static void getChess();
    static void sellChess();
    static void selectForBattle();
    static void enterBattle();
    static void buyExp();
    static void showContextMenu();
    static void viewCombos();
    static void showNeigongReward();
    static void viewNeigong();
    static void showExpeditionChallenge();
    static int calculateCost(int tier, int star, int count = 1);
};

}    //namespace KysChess