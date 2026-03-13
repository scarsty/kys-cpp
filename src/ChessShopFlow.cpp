#include "ChessShopFlow.h"

#include "ChessBalance.h"
#include "ChessDetailPanels.h"
#include "ChessMenuHelpers.h"
#include "ChessScreenLayout.h"
#include "GameState.h"

#include <algorithm>
#include <format>

namespace KysChess
{

namespace
{

std::string buildWeightString(int level)
{
    auto& cfg = ChessBalance::config();
    if (level >= static_cast<int>(cfg.shopWeights.size()))
    {
        return "已滿級";
    }
    auto& weights = cfg.shopWeights[level];
    return std::format("1費{}% 2費{}% 3費{}% 4費{}% 5費{}%", weights[0], weights[1], weights[2], weights[3], weights[4]);
}

std::vector<int> getSortedSeenRoleIds(const std::set<int>& seenRoleIds)
{
    std::vector<int> sortedRoleIds(seenRoleIds.begin(), seenRoleIds.end());
    std::sort(sortedRoleIds.begin(), sortedRoleIds.end(), [](int lhs, int rhs) {
        const auto lhsTier = ChessPool::GetChessTier(lhs);
        const auto rhsTier = ChessPool::GetChessTier(rhs);
        if (lhsTier != rhsTier)
        {
            return lhsTier < rhsTier;
        }
        return lhs < rhs;
    });
    return sortedRoleIds;
}

}

ChessShopFlow::ChessShopFlow(const ChessSelectorServices& services)
    : services_(services)
{
}

void ChessShopFlow::getChess()
{
    auto manager = makeChessManager(services_);
    auto& gameState = GameState::get();
    for (;;)
    {
        services_.shop.pool().setBannedRoleIds(gameState.bannedRoleIds());
        auto rollOfChess = services_.shop.pool().getChessFromPool(services_.economy.getLevel());
        for (auto [role, tier] : rollOfChess)
        {
            gameState.seenRoleIds().insert(role->ID);
        }

        for (;;)
        {
            ChessMenuData menuData;
            IndexedMenuConfig menuConfig;
            menuConfig.fontSize = 32;
            menuConfig.showNav = false;
            auto menuAnchor = ChessScreenLayout::shopMenuAnchor();
            menuConfig.x = menuAnchor.x;
            menuConfig.y = menuAnchor.y;

            menuData.labels.push_back(std::format("刷新               ${}", ChessBalance::config().refreshCost));
            menuData.colors.push_back({255, 204, 229, 255});
            menuData.previewData.push_back({});
            menuConfig.outlineColors.push_back({0, 0, 0, 0});
            menuConfig.animateOutlines.push_back(false);
            menuConfig.outlineThicknesses.push_back(1);

            menuData.labels.push_back(services_.shop.isLocked() ? "[已鎖定] 點擊解鎖" : "[未鎖定] 點擊鎖定");
            menuData.colors.push_back(services_.shop.isLocked() ? Color{255, 80, 80, 255} : Color{128, 128, 128, 255});
            menuData.previewData.push_back({});
            menuConfig.outlineColors.push_back({0, 0, 0, 0});
            menuConfig.animateOutlines.push_back(false);
            menuConfig.outlineThicknesses.push_back(1);

            auto chessMap = services_.roster.getChessCountMap();
            auto addRoleWithTier = [&](Role* role, int tier) {
                auto [name, color] = chessPresenter().formatChessName(role, tier, {}, {});
                menuData.labels.push_back(name);
                menuData.colors.push_back(color);
                menuData.previewData.push_back({role, 1, -1});

                Chess chess = {role, 1};
                bool owned = false;
                for (auto& [roleAndStar, count] : chessMap)
                {
                    if (roleAndStar.role == role && count > 0)
                    {
                        if (roleAndStar.star == 3 && count == 1)
                        {
                            bool hasOtherEntries = false;
                            for (auto& [otherRoleAndStar, otherCount] : chessMap)
                            {
                                if (otherRoleAndStar.role == role && otherRoleAndStar.star != 3 && otherCount > 0)
                                {
                                    hasOtherEntries = true;
                                    break;
                                }
                            }
                            if (!hasOtherEntries)
                            {
                                continue;
                            }
                        }
                        owned = true;
                        break;
                    }
                }

                bool wouldStarUp = services_.roster.wouldMerge(chess.role, chess.star);
                if (wouldStarUp)
                {
                    menuConfig.outlineColors.push_back({255, 215, 0, 255});
                    menuConfig.animateOutlines.push_back(true);
                    menuConfig.outlineThicknesses.push_back(3);
                }
                else if (owned)
                {
                    menuConfig.outlineColors.push_back({100, 200, 255, 220});
                    menuConfig.animateOutlines.push_back(true);
                    menuConfig.outlineThicknesses.push_back(1);
                }
                else
                {
                    menuConfig.outlineColors.push_back({0, 0, 0, 0});
                    menuConfig.animateOutlines.push_back(false);
                    menuConfig.outlineThicknesses.push_back(1);
                }
            };

            for (auto [role, tier] : rollOfChess)
            {
                addRoleWithTier(role, tier);
            }

            menuConfig.perPage = static_cast<int>(menuData.labels.size());
            auto menu = makeIndexedMenu(
                std::format("購買棋子 等級{} ${} 背包{}/{}", services_.economy.getLevel() + 1, services_.economy.getMoney(), manager.getBenchCount(), ChessBalance::config().benchSize),
                menuData,
                menuConfig,
                {std::make_shared<ComboInfoPanel>(manager), std::make_shared<OwnedRosterPanel>(services_.roster, manager)},
                menuData.previewData);
            menu->run();

            int selectedId = menu->getResult();
            if (selectedId < 0)
            {
                return;
            }
            if (selectedId == 0)
            {
                if (!services_.economy.spend(ChessBalance::config().refreshCost))
                {
                    continue;
                }
                services_.shop.pool().refresh();
                services_.shop.setLocked(false);
                break;
            }
            if (selectedId == 1)
            {
                services_.shop.setLocked(!services_.shop.isLocked());
                continue;
            }

            auto actualIdx = selectedId - 2;
            auto [role, tier] = rollOfChess[actualIdx];
            if (manager.isBenchFull() && !services_.roster.wouldMerge(role, 1))
            {
                showChessMessage("背包已滿！請先出售棋子");
                continue;
            }

            auto result = manager.purchaseChess(role, tier);
            if (!result.success)
            {
                continue;
            }
            services_.shop.pool().removeChessAt(actualIdx);
            if (result.merged)
            {
                showChessMessage(std::format("消費{}，{}升星！", result.cost, role->Name));
                playChessUpgradeSound();
            }
            else
            {
                showChessMessage(std::format("消費{}，獲取{}", result.cost, role->Name));
            }
            break;
        }
    }
}

void ChessShopFlow::sellChess()
{
    auto manager = makeChessManager(services_);
    for (;;)
    {
        auto chessList = services_.roster.items();
        if (chessList.empty())
        {
            return;
        }

        std::vector<ChessMenuEntry> entries;
        for (const auto& [instanceId, chess] : chessList)
        {
            entries.push_back({chess, chess.selectedForBattle ? "[戰]" : ""});
        }

        auto menuData = chessPresenter().buildChessMenuData(entries);
        IndexedMenuConfig menuConfig;
        menuConfig.perPage = 12;
        menuConfig.fontSize = 32;
        auto menuAnchor = ChessScreenLayout::shopMenuAnchor();
        menuConfig.x = menuAnchor.x;
        menuConfig.y = menuAnchor.y;
        auto menu = makeIndexedMenu(std::format("出售棋子 背包{}/{}", manager.getBenchCount(), ChessBalance::config().benchSize), menuData, menuConfig, {}, menuData.previewData);
        menu->run();

        int selectedId = menu->getResult();
        if (selectedId < 0)
        {
            break;
        }
        auto result = manager.sellChess(entries[selectedId].chess.id);
        showChessMessage(std::format("售出棋子{}，獲得${}", result.role->Name, result.price));
    }
}

void ChessShopFlow::buyExp()
{
    auto& config = ChessBalance::config();
    if (services_.economy.getLevel() >= config.maxLevel)
    {
        showChessMessage("已達最高等級！");
        return;
    }

    int currentLevel = services_.economy.getLevel();
    int nextLevel = currentLevel + 1;
    int currentPieces = services_.economy.getMaxDeploy();
    int nextPieces = std::max(nextLevel + 1, config.minBattleSize);
    bool nextAtMax = nextLevel >= config.maxLevel;
    auto previewPanel = std::make_shared<BuyExpPreviewPanel>(
        std::format("等級 {}    經驗 {}/{}    金幣 ${}", currentLevel + 1, services_.economy.getExp(), services_.economy.getExpForNextLevel(), services_.economy.getMoney()),
        std::format("花費 ${}  獲得 {} 經驗", config.buyExpCost, config.buyExpAmount),
        std::format("出戰人數: {}", currentPieces),
        (!nextAtMax && nextPieces > currentPieces) ? std::format("  → {}", nextPieces) : "",
        "  " + buildWeightString(currentLevel),
        "  " + (nextAtMax ? std::string("已滿級") : buildWeightString(nextLevel)));

    auto menu = std::make_shared<MenuText>(std::vector<std::string>{"確認購買", "取消"});
    menu->setFontSize(36);
    menu->arrange(0, 0, 0, 45);
    menu->addChild(previewPanel);
    auto menuAnchor = ChessScreenLayout::buyExpMenuAnchor();
    menu->runAtPosition(menuAnchor.x, menuAnchor.y);
    if (menu->getResult() != 0)
    {
        return;
    }
    if (!services_.economy.spend(config.buyExpCost))
    {
        return;
    }

    int oldLevel = services_.economy.getLevel();
    services_.economy.increaseExp(config.buyExpAmount);
    if (services_.economy.getLevel() > oldLevel)
    {
        showChessMessage(std::format("升級！等級{} 經驗{}/{}", services_.economy.getLevel() + 1, services_.economy.getExp(), services_.economy.getExpForNextLevel()));
    }
    else
    {
        showChessMessage(std::format("經驗{}/{}", services_.economy.getExp(), services_.economy.getExpForNextLevel()));
    }
}

void ChessShopFlow::showBanMenu()
{
    const auto maxBanCount = ChessBalance::config().maxBanCount;
    if (maxBanCount <= 0)
    {
        return;
    }

    auto& gameState = GameState::get();
    auto& seenRoleIds = gameState.seenRoleIds();
    auto& bannedRoleIds = gameState.bannedRoleIds();
    if (seenRoleIds.empty())
    {
        showChessMessage("尚未遇到可禁棋子");
        return;
    }

    for (;;)
    {
        auto sortedRoleIds = getSortedSeenRoleIds(seenRoleIds);
        ChessMenuData menuData;
        IndexedMenuConfig menuConfig;
        menuConfig.perPage = 12;
        menuConfig.fontSize = 32;
        auto menuAnchor = ChessScreenLayout::shopMenuAnchor();
        menuConfig.x = menuAnchor.x;
        menuConfig.y = menuAnchor.y;

        for (auto roleId : sortedRoleIds)
        {
            auto role = services_.roleSave.getRole(roleId);
            auto tier = std::max(1, ChessPool::GetChessTier(roleId));
            const auto isBanned = bannedRoleIds.contains(roleId);
            auto [name, color] = chessPresenter().formatChessName(role, tier, {}, {});
            menuData.labels.push_back(std::format("{} {}", isBanned ? "[已禁]" : "[未禁]", name));
            menuData.colors.push_back(color);
            menuData.previewData.push_back({role, 1, -1});
            menuConfig.outlineColors.push_back(isBanned ? Color{255, 80, 80, 255} : Color{0, 0, 0, 0});
            menuConfig.animateOutlines.push_back(false);
            menuConfig.outlineThicknesses.push_back(isBanned ? 2 : 1);
        }

        const auto bannedCount = static_cast<int>(bannedRoleIds.size());
        auto menu = makeIndexedMenu(
            std::format(
                "禁棋管理 已禁{}/{} 剩餘{}",
                bannedCount,
                maxBanCount,
                std::max(0, maxBanCount - bannedCount)),
            menuData,
            menuConfig,
            {},
            menuData.previewData);
        menu->run();

        const auto selectedId = menu->getResult();
        if (selectedId < 0)
        {
            return;
        }

        const auto roleId = sortedRoleIds[selectedId];
        auto role = services_.roleSave.getRole(roleId);
        if (bannedRoleIds.contains(roleId))
        {
            continue;
        }

        if (bannedCount >= maxBanCount)
        {
            showChessMessage(std::format("禁棋已滿 {}/{}", bannedCount, maxBanCount));
            continue;
        }

        bannedRoleIds.insert(roleId);
        services_.shop.pool().setBannedRoleIds(bannedRoleIds);
        showChessMessage(std::format("禁棋：{}", role->Name));
    }
}

}    // namespace KysChess
