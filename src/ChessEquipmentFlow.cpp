#include "ChessEquipmentFlow.h"

#include "ChessBalance.h"
#include "ChessDetailPanels.h"
#include "ChessEquipment.h"
#include "ChessMenuHelpers.h"
#include "ChessScreenLayout.h"

#include <assert.h>
#include <algorithm>
#include <format>

namespace KysChess
{

namespace
{

constexpr int kLegendaryEquipmentTier = 4;

std::vector<std::string> buildEquippedByNames(const ChessSelectorServices& services, int itemId)
{
    std::vector<std::string> equippedBy;
    for (const auto& [instanceId, chess] : services.roster.items())
    {
        if (chess.weaponInstance.itemId == itemId || chess.armorInstance.itemId == itemId)
        {
            equippedBy.push_back(chessPresenter().buildChessNameWithStar(chess));
        }
    }
    return equippedBy;
}

}    // namespace

ChessEquipmentFlow::ChessEquipmentFlow(const ChessSelectorServices& services)
    : services_(services)
{
}

void ChessEquipmentFlow::manageEquipment()
{
    if (!isLegendaryShopUnlocked())
    {
        showEquipmentInventory();
        return;
    }

    while (true)
    {
        IndexedMenuData menuData;
        menuData.labels = {
            std::format("神兵商店               ${}", ChessBalance::config().legendaryShop.price),
            "裝備一覽",
        };
        menuData.colors = {
            {255, 140, 255, 255},
            {220, 220, 220, 255},
        };
        IndexedMenuConfig menuConfig;
        menuConfig.fontSize = 32;
        auto menuAnchor = ChessScreenLayout::browseMenuAnchor();
        menuConfig.x = menuAnchor.x;
        menuConfig.y = menuAnchor.y;
        auto menu = makeIndexedMenu(std::format("裝備管理 ${}", services_.economy.getMoney()), menuData, menuConfig);
        menu->run();

        int selected = menu->getResult();
        if (selected < 0)
        {
            return;
        }
        if (selected == 0)
        {
            buyLegendaryEquipment();
            continue;
        }
        showEquipmentInventory();
        return;
    }
}

bool ChessEquipmentFlow::isLegendaryShopUnlocked() const
{
    const auto unlockFight = ChessBalance::config().legendaryShop.unlockFight;
    return unlockFight > 0 && services_.progress.battleProgress().getFight() >= unlockFight;
}

void ChessEquipmentFlow::buyLegendaryEquipment()
{
    auto legendaryEquipments = ChessEquipment::getByTier(kLegendaryEquipmentTier);
    if (legendaryEquipments.empty())
    {
        showChessMessage("神兵商店暫無可購買裝備");
        return;
    }

    std::string tierName[] = {"初階", "中階", "高階", "傳說"};
    Color tierColor = {255, 100, 255, 255};
    auto menuAnchor = ChessScreenLayout::browseMenuAnchor();

    while (true)
    {
        IndexedMenuData menuData;
        for (const auto* eq : legendaryEquipments)
        {
            auto* item = eq ? eq->getItem() : nullptr;
            std::string name = item ? item->Name : "???";
            while (name.size() < 15) name += "\xe3\x80\x80";
            menuData.labels.push_back(std::format("[{}] {} ${}", tierName[kLegendaryEquipmentTier - 1], name, ChessBalance::config().legendaryShop.price));
            menuData.colors.push_back(tierColor);
        }

        IndexedMenuConfig menuConfig;
        menuConfig.fontSize = 32;
        menuConfig.x = menuAnchor.x;
        menuConfig.y = menuAnchor.y;
        auto detailFrame = ChessScreenLayout::browseDetailRegionForMenu(menuAnchor, menuData.labels, menuConfig.fontSize);
        auto menu = makeIndexedMenu(
            std::format("神兵商店 ${}", services_.economy.getMoney()),
            menuData,
            menuConfig,
            {std::make_shared<EquipmentDetailPanel>(legendaryEquipments, [this](const EquipmentDef& equipment) {
                auto stats = services_.equipmentInventory.getItemStats(equipment.itemId);
                return EquipmentDetailState{stats.totalCount, buildEquippedByNames(services_, equipment.itemId)};
            }, detailFrame)});
        menu->run();

        int selected = menu->getResult();
        if (selected < 0)
        {
            return;
        }

        const auto* equipment = legendaryEquipments[selected];
        auto* item = equipment ? equipment->getItem() : nullptr;
        int price = ChessBalance::config().legendaryShop.price;
        if (!services_.economy.spend(price))
        {
            showChessMessage("金錢不足，無法購買神兵");
            continue;
        }

        services_.equipmentInventory.storeItem(equipment->itemId);
        showChessMessage(std::format("消費${}，獲得神兵：{}", price, item ? item->Name : "???"));
    }
}

void ChessEquipmentFlow::showEquipmentInventory()
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
