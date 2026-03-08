#pragma once

#include "ChessUiCommon.h"

namespace KysChess
{

class ChessInfoFlow
{
public:
    explicit ChessInfoFlow(const ChessSelectorServices& services);

    void viewCombos();
    void viewNeigong();
    void showGameGuide();
    void showPositionSwap();

private:
    ChessSelectorServices services_;
};

}    // namespace KysChess