#pragma once

namespace KysChess
{

class ChessSelector
{
public:
    // Talk to TempStore to spend money and get chess
    // Responsible for the UI of chess selection by invoking SuperMenuText

    // This function will be triggered by some event
    static void getChess();

    // View my chess collection
    static void viewMyChess();

    // Select chess pieces for battle (up to level_ pieces)
    static void selectForBattle();

    static void enterBattle();

    // Calculate the cost/strength of a chess piece
    static int calculateCost(int tier, int star, int count = 1);
};

}    //namespace KysChess