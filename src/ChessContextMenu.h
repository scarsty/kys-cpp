#pragma once

#include <string>
#include <vector>

namespace KysChess
{

enum class ChessContextMenuAction
{
    EnterBattle,
    OpenOfflineBattle,
    OpenEquipmentMenu,
    OpenOverviewMenu,
    ShowExpeditionChallenge,
    OpenSystemMenu,
    LoadProgress,
    SaveProgress,
    ShowSystemSettings,
    RerollBattleSeed,
    ManageBans,
    ViewCombos,
    ViewChessPool,
    ViewNeigong,
    ViewEffects,
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

std::vector<ChessContextMenuItem> buildChessOverviewMenu();
std::vector<ChessContextMenuItem> buildChessEquipmentMenu(bool legendaryShopUnlocked);
std::vector<ChessContextMenuItem> buildChessSystemMenu();
std::vector<std::string> chessContextMenuLabels(const std::vector<ChessContextMenuItem>& items);
}    // namespace KysChess
