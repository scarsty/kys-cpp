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

private:
    ChessSelectorServices services_;
};

}    // namespace KysChess