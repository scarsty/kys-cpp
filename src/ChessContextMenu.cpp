#include "ChessContextMenu.h"

#include <algorithm>
#include <assert.h>

namespace KysChess
{

std::vector<ChessContextMenuItem> buildChessContextMenu(bool banEnabled)
{
    std::vector<ChessContextMenuItem> items{
        {"購買棋子", ChessContextMenuAction::BuyChess},
        {"出售棋子", ChessContextMenuAction::SellChess},
        {"選擇出戰", ChessContextMenuAction::SelectForBattle},
        {"進入戰鬥", ChessContextMenuAction::EnterBattle},
        {"購買經驗", ChessContextMenuAction::BuyExp},
    };
    if (banEnabled)
    {
        items.push_back({"禁棋管理", ChessContextMenuAction::ManageBans});
    }
    items.insert(
        items.end(),
        {
            {"裝備管理", ChessContextMenuAction::OpenEquipmentMenu},
            {"棋局總覽", ChessContextMenuAction::OpenOverviewMenu},
            {"遠征挑戰", ChessContextMenuAction::ShowExpeditionChallenge},
            {"系統設定", ChessContextMenuAction::ShowSystemSettings},
            {"遊戲說明", ChessContextMenuAction::ShowGameGuide},
        });
    return items;
}

std::vector<ChessContextMenuItem> buildChessOverviewMenu()
{
    return {
        {"排兵佈陣", ChessContextMenuAction::ShowPositionSwap},
        {"逆天改命", ChessContextMenuAction::RerollBattleSeed},
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

int centerChessContextMenuY(std::size_t itemCount, int contentY, int contentHeight, int rowSpacing)
{
    assert(contentHeight > 0);
    assert(rowSpacing > 0);

    const int menuHeight = static_cast<int>(itemCount) * rowSpacing;
    return std::max(contentY, contentY + (contentHeight - menuHeight) / 2);
}

}    // namespace KysChess
