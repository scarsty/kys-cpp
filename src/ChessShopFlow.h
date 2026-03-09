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

private:
    ChessSelectorServices services_;
};

}    // namespace KysChess
