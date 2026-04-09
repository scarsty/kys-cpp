#pragma once

#include "ChessUiCommon.h"

namespace KysChess
{

class ChessShopFlow
{
public:
    explicit ChessShopFlow(const ChessSelectorServices& services);

    void getChess();
    void sellChess();
    void buyExp();
    void showBanMenu();
    int showForcedBanSelection(int slots, int maxTier);

private:
    ChessSelectorServices services_;
};

}    // namespace KysChess
