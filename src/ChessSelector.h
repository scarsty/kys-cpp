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

    static void enterBattle();
};

}    //namespace KysChess