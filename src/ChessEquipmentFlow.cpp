#include "ChessEquipmentFlow.h"

#include "ChessDetailPanels.h"
#include "ChessMenuHelpers.h"
#include "ChessScreenLayout.h"

#include <assert.h>
#include <algorithm>
#include <format>

namespace KysChess
{

ChessEquipmentFlow::ChessEquipmentFlow(const ChessSelectorServices& services)
    : services_(services)
{
}

void ChessEquipmentFlow::manageEquipment()
{
    auto manager = makeChessManager(services_);
    auto& allEquip = ChessEquipment::getAll();
    if (allEquip.empty())
    {
        return;
    }

    std::string tierName[] = {"初階", "中階", "高階", "傳說"};
    struct EquipmentListEntry
    {
        int index;
        std::string label;
        Color color;
        int inventoryCount;
    };

    while (true)
    {
        std::vector<EquipmentListEntry> entries;
        for (size_t i = 0; i < allEquip.size(); ++i)
        {
            auto& eq = allEquip[i];
            auto stats = services_.equipmentInventory.getItemStats(eq.itemId);
            auto* item = eq.getItem();
            std::string name = item ? item->Name : "???";
            while (name.size() < 15) name += "\xe3\x80\x80";
            std::string label = std::format("[{}] {} x{} (已裝:{})", tierName[std::min(eq.tier - 1, 3)], name, stats.totalCount, stats.equippedCount);
            entries.push_back({static_cast<int>(i), std::move(label), stats.totalCount > 0 ? Color{0, 255, 0, 255} : Color{120, 120, 120, 255}, stats.totalCount});
        }

        std::sort(entries.begin(), entries.end(), [&](const auto& left, const auto& right) {
            if ((left.inventoryCount > 0) != (right.inventoryCount > 0))
            {
                return left.inventoryCount > right.inventoryCount;
            }
            return left.index < right.index;
        });

        IndexedMenuData menuData;
        std::vector<const EquipmentDef*> detailEquipments;
        for (auto& entry : entries)
        {
            menuData.labels.push_back(entry.label);
            menuData.colors.push_back(entry.color);
            detailEquipments.push_back(&allEquip[entry.index]);
        }
        auto detailPanel = std::make_shared<EquipmentDetailPanel>(detailEquipments, [this](const EquipmentDef& equipment) {
            auto stats = services_.equipmentInventory.getItemStats(equipment.itemId);
            return EquipmentDetailState{stats.totalCount, chessPresenter().buildEquippedBy(services_.roster.items(), equipment.itemId)};
        });
        IndexedMenuConfig menuConfig;
        auto menuAnchor = ChessScreenLayout::browseMenuAnchor();
        menuConfig.x = menuAnchor.x;
        menuConfig.y = menuAnchor.y;
        auto menu = makeIndexedMenu("裝備一覽", menuData, menuConfig, {detailPanel});
        menu->run();
        int sel = menu->getResult();
        if (sel < 0)
        {
            break;
        }

        auto& equipment = allEquip[entries[sel].index];
        auto stats = services_.equipmentInventory.getItemStats(equipment.itemId);
        if (stats.totalCount <= 0)
        {
            continue;
        }
        assert(stats.availableInstanceId != k_nonExistentItem);

        std::vector<ChessMenuEntry> chessEntries;
        for (const auto& [instanceId, chess] : services_.roster.items())
        {
            chessEntries.push_back({chess, chess.selectedForBattle ? "[出戰] " : ""});
        }
        if (chessEntries.empty())
        {
            showChessMessage("沒有可裝備的棋子");
            continue;
        }

        auto chessMenuData = chessPresenter().buildChessMenuData(chessEntries);
        IndexedMenuConfig chessMenuConfig;
        chessMenuConfig.perPage = 12;
        chessMenuConfig.fontSize = 32;
        chessMenuConfig.x = menuAnchor.x;
        chessMenuConfig.y = menuAnchor.y;
        auto menu2 = makeIndexedMenu("選擇棋子", chessMenuData, chessMenuConfig, {}, chessMenuData.previewData);
        menu2->run();
        int selectedIdx = menu2->getResult();
        if (selectedIdx < 0)
        {
            continue;
        }
        manager.equipItem(chessEntries[selectedIdx].chess.id, equipment, stats.availableInstanceId);
    }
}

}    // namespace KysChess