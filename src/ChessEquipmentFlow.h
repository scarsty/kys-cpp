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
    bool isLegendaryShopUnlocked() const;
    void showEquipmentInventory();
    void buyLegendaryEquipment();

    ChessSelectorServices services_;
};

}    // namespace KysChess