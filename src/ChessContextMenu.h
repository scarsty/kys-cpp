#pragma once

#include <string>
#include <vector>

namespace KysChess
{

enum class ChessContextMenuAction
{
    BuyChess,
    SellChess,
    SelectForBattle,
    EnterBattle,
    BuyExp,
    OpenEquipmentMenu,
    OpenStrategyMenu,
    OpenInfoMenu,
    ShowExpeditionChallenge,
    ShowSystemSettings,
    ShowPositionSwap,
    RerollBattleSeed,
    ManageBans,
    ViewCombos,
    ViewChessPool,
    ViewNeigong,
    ShowGameGuide,
    ShowEquipmentInventory,
    BuyLegendaryEquipment,
};

struct ChessContextMenuItem
{
    std::string label;
    ChessContextMenuAction action;
};

std::vector<ChessContextMenuItem> buildChessContextMenu();
std::vector<ChessContextMenuItem> buildChessStrategyMenu(bool banEnabled);
std::vector<ChessContextMenuItem> buildChessInfoMenu();
std::vector<ChessContextMenuItem> buildChessEquipmentMenu(bool legendaryShopUnlocked);
std::vector<std::string> chessContextMenuLabels(const std::vector<ChessContextMenuItem>& items);

}    // namespace KysChess
