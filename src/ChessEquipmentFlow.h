#pragma once

#include "ChessUiCommon.h"

namespace KysChess
{

class ChessEquipmentFlow
{
public:
    explicit ChessEquipmentFlow(const ChessSelectorServices& services);

    void manageEquipment();

private:
    ChessSelectorServices services_;
};

}    // namespace KysChess