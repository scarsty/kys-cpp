#include "ChessContextMenu.h"

namespace KysChess
{

std::vector<ChessContextMenuItem> buildChessContextMenu()
{
    return {
        {"購買棋子", ChessContextMenuAction::BuyChess},
        {"出售棋子", ChessContextMenuAction::SellChess},
        {"選擇出戰", ChessContextMenuAction::SelectForBattle},
        {"進入戰鬥", ChessContextMenuAction::EnterBattle},
        {"購買經驗", ChessContextMenuAction::BuyExp},
        {"裝備管理", ChessContextMenuAction::OpenEquipmentMenu},
        {"棋局策略", ChessContextMenuAction::OpenStrategyMenu},
        {"情報一覽", ChessContextMenuAction::OpenInfoMenu},
        {"遠征挑戰", ChessContextMenuAction::ShowExpeditionChallenge},
        {"系統設定", ChessContextMenuAction::ShowSystemSettings},
        {"遊戲說明", ChessContextMenuAction::ShowGameGuide},
    };
}

std::vector<ChessContextMenuItem> buildChessStrategyMenu(bool banEnabled)
{
    std::vector<ChessContextMenuItem> items{
        {"排兵佈陣", ChessContextMenuAction::ShowPositionSwap},
        {"逆天改命", ChessContextMenuAction::RerollBattleSeed},
    };
    if (banEnabled)
    {
        items.push_back({"禁棋管理", ChessContextMenuAction::ManageBans});
    }
    return items;
}

std::vector<ChessContextMenuItem> buildChessInfoMenu()
{
    return {
        {"查看羈絆", ChessContextMenuAction::ViewCombos},
        {"棋子一覽", ChessContextMenuAction::ViewChessPool},
        {"查看內功", ChessContextMenuAction::ViewNeigong},
    };
}

std::vector<ChessContextMenuItem> buildChessEquipmentMenu(bool legendaryShopUnlocked)
{
    std::vector<ChessContextMenuItem> items{
        {"裝備一覽", ChessContextMenuAction::ShowEquipmentInventory},
    };
    if (legendaryShopUnlocked)
    {
        items.push_back({"神兵商店", ChessContextMenuAction::BuyLegendaryEquipment});
    }
    return items;
}

std::vector<std::string> chessContextMenuLabels(const std::vector<ChessContextMenuItem>& items)
{
    std::vector<std::string> labels;
    labels.reserve(items.size());
    for (const auto& item : items)
    {
        labels.push_back(item.label);
    }
    return labels;
}

}    // namespace KysChess
