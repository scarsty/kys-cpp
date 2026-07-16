#pragma once

#include <cstddef>
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
    OpenOverviewMenu,
    ShowExpeditionChallenge,
    OpenSystemMenu,
    LoadProgress,
    SaveProgress,
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
    ReturnToTitle,
};

struct ChessContextMenuItem
{
    std::string label;
    ChessContextMenuAction action;
};

std::vector<ChessContextMenuItem> buildChessContextMenu(bool banEnabled);
std::vector<ChessContextMenuItem> buildChessOverviewMenu();
std::vector<ChessContextMenuItem> buildChessEquipmentMenu(bool legendaryShopUnlocked);
std::vector<ChessContextMenuItem> buildChessSystemMenu();
std::vector<std::string> chessContextMenuLabels(const std::vector<ChessContextMenuItem>& items);
int centerChessContextMenuY(
    std::size_t itemCount,
    int contentY = 45,
    int contentHeight = 630,
    int rowSpacing = 45);

}    // namespace KysChess
