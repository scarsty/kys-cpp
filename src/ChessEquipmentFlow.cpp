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
    static int k_equipSuffixWidth = Font::getTextDrawSize(" [已裝]");
    struct EquipmentInstanceEntry
    {
        const EquipmentDef* equipment;
        ItemInstanceID instanceId;
        std::string label;
        Color color;
        bool owned;
    };

    while (true)
    {
        std::vector<EquipmentInstanceEntry> entries;
        auto buildLabel = [&](const EquipmentDef& eq, bool equipped)
        {
            auto* item = eq.getItem();
            std::string name = item ? item->Name : "???";
            while (name.size() < 15) name += "\xe3\x80\x80";

            std::string label = std::format("[{}] {}", tierName[std::min(eq.tier - 1, 3)], name);
            if (equipped)
            {
                label += " [已裝]";
            }
            else
            {
                for (int i = 0; i < k_equipSuffixWidth; ++i)
                {
                    label += " ";
                }
            }
            return label;
        };
        for (auto& eq : allEquip)
        {
            auto instances = services_.equipmentInventory.getInstancesForItem(eq.itemId);
            if (instances.empty())
            {
                std::string label = buildLabel(eq, false);
                entries.push_back({&eq, k_nonExistentItem, std::move(label), {120, 120, 120, 255}, false});
            }
            else
            {
                for (auto [instId, chessId] : instances)
                {
                    std::string label = buildLabel(eq, chessId != k_nonExistentChess);
                    entries.push_back({&eq, instId, std::move(label), {0, 255, 0, 255}, true});
                }
            }
        }

        std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
            if (left.owned != right.owned) return left.owned;
            return left.equipment->itemId < right.equipment->itemId;
        });

        IndexedMenuData menuData;
        std::vector<std::pair<const EquipmentDef*, ItemInstanceID>> detailData;
        for (auto& entry : entries)
        {
            menuData.labels.push_back(entry.label);
            menuData.colors.push_back(entry.color);
            detailData.push_back({entry.equipment, entry.instanceId});
        }
        auto menuAnchor = ChessScreenLayout::browseMenuAnchor();
        auto detailFrame = ChessScreenLayout::browseDetailRegionForMenu(menuAnchor, menuData.labels, 36);
        auto detailPanel = std::make_shared<EquipmentInstanceDetailPanel>(detailData, [this](const EquipmentDef& equipment, ItemInstanceID instanceId) {
            if (instanceId == k_nonExistentItem)
            {
                return EquipmentInstanceDetailState{""};
            }
            for (const auto& [chessInstId, chess] : services_.roster.items())
            {
                if (chess.weaponInstance.id == instanceId || chess.armorInstance.id == instanceId)
                {
                    return EquipmentInstanceDetailState{chessPresenter().buildChessNameWithStar(chess)};
                }
            }
            return EquipmentInstanceDetailState{""};
        }, detailFrame);
        IndexedMenuConfig menuConfig;
        menuConfig.x = menuAnchor.x;
        menuConfig.y = menuAnchor.y;
        auto menu = makeIndexedMenu("裝備一覽", menuData, menuConfig, {detailPanel});
        menu->run();
        int sel = menu->getResult();
        if (sel < 0)
        {
            break;
        }

        auto& entry = entries[sel];
        if (!entry.owned)
        {
            continue;
        }

        std::vector<ChessMenuEntry> chessEntries;
        for (const auto& [instanceId, chess] : services_.roster.items())
        {
            chessEntries.push_back({chess, chess.selectedForBattle ? "[戰]" : ""});
        }
        if (chessEntries.empty())
        {
            showChessMessage("沒有可裝備的棋子");
            continue;
        }

        auto chessMenuData = chessPresenter().buildChessMenuData(chessEntries);
        IndexedMenuConfig chessMenuConfig;
        chessMenuConfig.perPage = 12;
        chessMenuConfig.x = menuAnchor.x;
        chessMenuConfig.y = menuAnchor.y;
        auto shopPanels = ChessScreenLayout::shopPanelsForMenu(menuAnchor, chessMenuData.labels, chessMenuConfig.fontSize);
        chessMenuConfig.previewFrame = shopPanels.status;
        auto menu2 = makeIndexedMenu("選擇棋子", chessMenuData, chessMenuConfig, {}, chessMenuData.previewData);
        menu2->run();
        int selectedIdx = menu2->getResult();
        if (selectedIdx < 0)
        {
            continue;
        }
        manager.equipItem(chessEntries[selectedIdx].chess.id, *entry.equipment, entry.instanceId);
    }
}

}    // namespace KysChess
