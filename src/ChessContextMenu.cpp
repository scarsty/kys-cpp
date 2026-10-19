#include "ChessContextMenu.h"

namespace KysChess
{

std::vector<ChessContextMenuItem> buildChessOverviewMenu()
{
    return {
        {"查看羈絆", ChessContextMenuAction::ViewCombos},
        {"棋子一覽", ChessContextMenuAction::ViewChessPool},
        {"查看內功", ChessContextMenuAction::ViewNeigong},
        {"效果全覽", ChessContextMenuAction::ViewEffects},
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

std::vector<ChessContextMenuItem> buildChessSystemMenu()
{
    return {
        {"讀取進度", ChessContextMenuAction::LoadProgress},
        {"儲存進度", ChessContextMenuAction::SaveProgress},
        {"系統設定", ChessContextMenuAction::ShowSystemSettings},
        {"遊戲說明", ChessContextMenuAction::ShowGameGuide},
        {"返回開頭", ChessContextMenuAction::ReturnToTitle},
    };
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
