#include "ChessRewardFlow.h"

#include "ChessDetailPanels.h"
#include "ChessMenuHelpers.h"
#include "ChessNeigong.h"
#include "ChessPool.h"
#include "ChessScreenLayout.h"

#include <set>

namespace KysChess
{

ChessRewardFlow::ChessRewardFlow(const ChessSelectorServices& services)
    : services_(services)
{
}

void ChessRewardFlow::showNeigongReward()
{
    auto& cfg = ChessBalance::config();
    auto& ngCfg = ChessNeigong::config();
    auto& pool = ChessNeigong::getPool();
    if (pool.empty())
    {
        return;
    }

    int bossIdx = services_.progress.battleProgress().getFight() / cfg.bossInterval;
    std::vector<int> availableTiers;
    for (auto it = ngCfg.tiersByBoss.rbegin(); it != ngCfg.tiersByBoss.rend(); ++it)
    {
        if (it->first <= bossIdx)
        {
            availableTiers = it->second;
            break;
        }
    }
    if (availableTiers.empty())
    {
        availableTiers = {1};
    }

    auto& obtained = services_.progress.getObtainedNeigong();
    std::set<int> obtainedSet(obtained.begin(), obtained.end());
    std::vector<const NeigongDef*> candidates;
    for (auto& ng : pool)
    {
        if (obtainedSet.count(ng.magicId)) continue;
        for (int tier : availableTiers)
        {
            if (ng.tier == tier)
            {
                candidates.push_back(&ng);
                break;
            }
        }
    }
    if (candidates.empty())
    {
        return;
    }

    bool rerolled = false;
    auto pickChoices = [&]() {
        std::vector<const NeigongDef*> choices;
        int n = std::min(static_cast<int>(candidates.size()), ngCfg.choiceCount);
        auto shuffled = candidates;
        for (int i = static_cast<int>(shuffled.size()) - 1; i > 0; --i)
        {
            std::swap(shuffled[i], shuffled[services_.random.shopRandInt(i + 1)]);
        }
        choices.assign(shuffled.begin(), shuffled.begin() + n);
        return choices;
    };

    auto choices = pickChoices();
    std::string tierName[] = {"初階", "中階", "高階", "傳說"};
    for (;;)
    {
        IndexedMenuData menuData;
        std::vector<const NeigongDef*> detailChoices;
        for (int i = 0; i < static_cast<int>(choices.size()); ++i)
        {
            auto* ng = choices[i];
            std::string padded = ng->name;
            while (padded.size() < 15) padded += "\xe3\x80\x80";
            menuData.labels.push_back(std::format("[{}] {}", tierName[std::min(ng->tier - 1, 3)], padded));
            menuData.colors.push_back(ChessNeigong::GetTierColor(ng->tier));
            detailChoices.push_back(ng);
        }
        if (!rerolled)
        {
            menuData.labels.push_back(std::format("刷新             ${}", ngCfg.rerollCost));
            menuData.colors.push_back({128, 128, 128});
        }

        IndexedMenuConfig menuConfig;
        menuConfig.perPage = static_cast<int>(menuData.labels.size());
        menuConfig.exitable = false;
        auto menuAnchor = ChessScreenLayout::browseMenuAnchor();
        menuConfig.x = menuAnchor.x;
        menuConfig.y = menuAnchor.y;
        auto detailFrame = ChessScreenLayout::browseDetailRegionForMenu(menuAnchor, menuData.labels, menuConfig.fontSize);
        auto menu = makeIndexedMenu("内功對所有成員自動生效", menuData, menuConfig, {std::make_shared<NeigongDetailPanel>(detailChoices, detailFrame)});
        menu->run();

        int sel = menu->getResult();
        if (!rerolled && sel == static_cast<int>(choices.size()))
        {
            if (services_.economy.spend(ngCfg.rerollCost))
            {
                rerolled = true;
                choices = pickChoices();
            }
            continue;
        }
        if (sel < 0)
        {
            continue;
        }

        services_.progress.addNeigong(choices[sel]->magicId);
        showChessMessage(std::format("獲得內功：{}", choices[sel]->name));
        return;
    }
}

void ChessRewardFlow::showEquipmentReward()
{
    auto& cfg = ChessBalance::config();
    int fightNum = services_.progress.battleProgress().getFight();
    const BalanceConfig::PlayerEquipmentReward* rewardCfg = nullptr;
    for (auto& r : cfg.playerEquipmentRewards)
    {
        if (r.fight == fightNum + 1)
        {
            rewardCfg = &r;
            break;
        }
    }
    if (!rewardCfg) return;

    auto& allEquip = ChessEquipment::getAll();
    std::vector<const EquipmentDef*> candidates;
    for (auto& eq : allEquip)
    {
        if (eq.tier <= rewardCfg->maxTier) candidates.push_back(&eq);
    }
    if (candidates.empty()) return;

    bool rerolled = false;
    auto pickChoices = [&]() {
        std::vector<const EquipmentDef*> choices;
        int n = std::min(static_cast<int>(candidates.size()), rewardCfg->choices);
        auto shuffled = candidates;
        for (int i = static_cast<int>(shuffled.size()) - 1; i > 0; --i)
        {
            std::swap(shuffled[i], shuffled[services_.random.shopRandInt(i + 1)]);
        }
        choices.assign(shuffled.begin(), shuffled.begin() + n);
        return choices;
    };

    auto choices = pickChoices();
    std::string tierName[] = {"初階", "中階", "高階", "傳說"};
    for (;;)
    {
        IndexedMenuData menuData;
        std::vector<const EquipmentDef*> detailChoices;
        for (auto* eq : choices)
        {
            auto* item = eq->getItem();
            std::string name = item ? item->Name : "???";
            while (name.size() < 15) name += "\xe3\x80\x80";
            menuData.labels.push_back(std::format("[{}] {}", tierName[std::min(eq->tier - 1, 3)], name));
            Color tierColors[] = {{100, 200, 100, 255}, {100, 150, 255, 255}, {255, 150, 50, 255}, {255, 100, 255, 255}};
            menuData.colors.push_back(tierColors[std::min(eq->tier - 1, 3)]);
            detailChoices.push_back(eq);
        }
        if (!rerolled)
        {
            menuData.labels.push_back(std::format("刷新             ${}", rewardCfg->refreshCost));
            menuData.colors.push_back({128, 128, 128});
        }

        IndexedMenuConfig menuConfig;
        menuConfig.perPage = static_cast<int>(menuData.labels.size());
        menuConfig.exitable = false;
        auto menuAnchor = ChessScreenLayout::browseMenuAnchor();
        menuConfig.x = menuAnchor.x;
        menuConfig.y = menuAnchor.y;
        auto detailFrame = ChessScreenLayout::browseDetailRegionForMenu(menuAnchor, menuData.labels, menuConfig.fontSize);
        auto menu = makeIndexedMenu("選擇裝備獎勵", menuData, menuConfig, {std::make_shared<EquipmentDetailPanel>(detailChoices, [](const EquipmentDef&) { return EquipmentDetailState{}; }, detailFrame)});
        menu->run();

        int sel = menu->getResult();
        if (!rerolled && sel == static_cast<int>(choices.size()))
        {
            if (services_.economy.spend(rewardCfg->refreshCost))
            {
                rerolled = true;
                choices = pickChoices();
            }
            continue;
        }
        if (sel < 0) continue;

        services_.equipmentInventory.storeItem(choices[sel]->itemId);
        auto* item = choices[sel]->getItem();
        showChessMessage(std::format("獲得裝備：{}", item ? item->Name : "???"));
        return;
    }
}

int ChessRewardFlow::selectChallengeReward(const std::vector<BalanceConfig::ChallengeReward>& rewards)
{
    auto isRewardAvailable = [&](const BalanceConfig::ChallengeReward& reward) {
        using RT = BalanceConfig::ChallengeRewardType;
        if (reward.type == RT::StarUp1to2 || reward.type == RT::StarUp2to3)
        {
            int fromStar = (reward.type == RT::StarUp1to2) ? 1 : 2;
            for (auto& [instanceId, chess] : services_.roster.items())
            {
                if (chess.star != fromStar) continue;
                int tier = chess.role ? chess.role->Cost : -1;
                if (tier >= 0 && tier <= reward.value) return true;
            }
            return false;
        }
        if (reward.type == RT::GetNeigong)
        {
            auto& pool = ChessNeigong::getPool();
            auto& obtained = services_.progress.getObtainedNeigong();
            std::set<int> obtainedSet(obtained.begin(), obtained.end());
            for (auto& ng : pool)
            {
                if (ng.tier <= reward.value && !obtainedSet.count(ng.magicId)) return true;
            }
            return false;
        }
        return true;
    };

    IndexedMenuData menuData;
    std::vector<bool> available;
    for (int i = 0; i < static_cast<int>(rewards.size()); ++i)
    {
        bool avail = isRewardAvailable(rewards[i]);
        available.push_back(avail);
        std::string desc = chessPresenter().challengeRewardDesc(rewards[i]);
        if (!avail) desc += "（無可用）";
        menuData.labels.push_back(desc);
        menuData.colors.push_back(avail ? Color{255, 255, 100, 255} : Color{120, 120, 120, 255});
    }

    for (;;)
    {
        IndexedMenuConfig menuConfig;
        menuConfig.perPage = static_cast<int>(menuData.labels.size());
        menuConfig.exitable = false;
        auto menuAnchor = ChessScreenLayout::browseMenuAnchor();
        menuConfig.x = menuAnchor.x;
        menuConfig.y = menuAnchor.y;
        auto menu = makeIndexedMenu("選擇獎勵", menuData, menuConfig);
        menu->run();
        int sel = menu->getResult();
        if (sel < 0) sel = 0;
        if (available[sel])
        {
            return sel;
        }
        showChessMessage("該獎勵無可用項目，請重新選擇");
    }
}

bool ChessRewardFlow::applyReward(const BalanceConfig::ChallengeReward& reward)
{
    using RT = BalanceConfig::ChallengeRewardType;
    if (reward.type == RT::Gold) return rewardGold(reward.value);
    if (reward.type == RT::GetPiece) return rewardPiece(reward.value);
    if (reward.type == RT::GetNeigong) return rewardNeigong(reward.value);
    if (reward.type == RT::StarUp1to2) return rewardStarUp(1, reward.value);
    if (reward.type == RT::StarUp2to3) return rewardStarUp(2, reward.value);
    if (reward.type == RT::GetSpecificEquipment) return rewardEquipment(99, reward.value);
    if (reward.type == RT::GetEquipment) return rewardEquipment(reward.value);
    return false;
}

bool ChessRewardFlow::rewardGold(int amount)
{
    makeChessManager(services_).applyGoldReward(amount);
    showChessMessage(std::format("獲得{}金幣！", amount));
    return true;
}

bool ChessRewardFlow::addChessPiece(Role* role, bool showMessageFlag)
{
    auto manager = makeChessManager(services_);
    auto result = manager.grantChess(role);
    if (!result.success)
    {
        if (showMessageFlag)
        {
            showChessMessage("背包已滿！請先出售棋子");
        }
        return false;
    }

    if (showMessageFlag)
    {
        if (result.merged)
        {
            showChessMessage(std::format("{}升星！", role->Name));
            playChessUpgradeSound();
        }
        else
        {
            showChessMessage(std::format("獲得棋子：{}", role->Name));
        }
    }
    return true;
}

bool ChessRewardFlow::rewardPiece(int maxTier)
{
    std::vector<std::string> items;
    std::vector<Color> colors;
    std::vector<Chess> previewData;
    std::vector<int> roleIds;
    for (int tier = 1; tier <= maxTier && tier <= 5; ++tier)
    {
        for (int roleId : services_.shop.pool().getRolesOfTier(tier))
        {
            auto* role = services_.roleSave.getRole(roleId);
            if (!role) continue;
            {
                auto formatted = chessPresenter().formatChessName(role, tier, 1);
                items.push_back(formatted.text);
                colors.push_back(formatted.color);
            }
            previewData.push_back({role, 1, -1});
            roleIds.push_back(roleId);
        }
    }

    IndexedMenuData menuData{items, colors};
    IndexedMenuConfig menuConfig;
    menuConfig.perPage = 12;
    menuConfig.fontSize = 32;
    auto menuAnchor = ChessScreenLayout::browseMenuAnchor();
    menuConfig.x = menuAnchor.x;
    menuConfig.y = menuAnchor.y;
    auto shopPanels = ChessScreenLayout::shopPanelsForMenu(menuAnchor, menuData.labels, menuConfig.fontSize);
    menuConfig.previewFrame = shopPanels.status;
    auto menu = makeIndexedMenu("選擇棋子", menuData, menuConfig, {std::make_shared<ComboInfoPanel>(makeChessManager(services_))}, previewData);
    menu->run();
    int sel = menu->getResult();
    if (sel >= 0)
    {
        auto* role = services_.roleSave.getRole(roleIds[sel]);
        if (role)
        {
            return addChessPiece(role, true);
        }
    }
    return false;
}

bool ChessRewardFlow::rewardNeigong(int maxTier)
{
    auto& pool = ChessNeigong::getPool();
    auto& obtained = services_.progress.getObtainedNeigong();
    std::set<int> obtainedSet(obtained.begin(), obtained.end());
    IndexedMenuData menuData;
    std::vector<const NeigongDef*> detailChoices;
    for (auto& ng : pool)
    {
        if (ng.tier > maxTier || obtainedSet.count(ng.magicId)) continue;
        menuData.labels.push_back(ng.name);
        menuData.colors.push_back({100, 255, 100, 255});
        detailChoices.push_back(&ng);
    }
    if (menuData.labels.empty())
    {
        showChessMessage("沒有可選的內功");
        return false;
    }

    IndexedMenuConfig menuConfig;
    menuConfig.perPage = 16;
    menuConfig.exitable = false;
    auto menuAnchor = ChessScreenLayout::browseMenuAnchor();
    menuConfig.x = menuAnchor.x;
    menuConfig.y = menuAnchor.y;
    auto detailFrame = ChessScreenLayout::browseDetailRegionForMenu(menuAnchor, menuData.labels, menuConfig.fontSize);
    auto menu = makeIndexedMenu("内功對所有成員自動生效", menuData, menuConfig, {std::make_shared<NeigongDetailPanel>(detailChoices, detailFrame)});
    menu->run();
    int sel = menu->getResult();
    if (sel >= 0)
    {
        services_.progress.addNeigong(detailChoices[sel]->magicId);
        showChessMessage(std::format("獲得內功：{}", detailChoices[sel]->name));
        return true;
    }
    return false;
}

bool ChessRewardFlow::rewardStarUp(int fromStar, int maxTier)
{
    auto manager = makeChessManager(services_);
    int toStar = fromStar + 1;
    if (!manager.applyStarUpReward(fromStar, maxTier))
    {
        showChessMessage("沒有可升星的棋子");
        return false;
    }

    auto chesses = services_.roster.getByStarAndTier(fromStar, maxTier);
    std::vector<std::string> items;
    std::vector<Color> colors;
    std::vector<Chess> previewData;
    for (auto& chess : chesses)
    {
        int tier = chess.role ? chess.role->Cost : -1;
        {
            auto formatted = chessPresenter().formatChessName(chess.role, tier, chess.star);
            items.push_back(formatted.text);
            colors.push_back(formatted.color);
        }
        previewData.push_back(chess);
    }

    IndexedMenuData menuData{items, colors};
    IndexedMenuConfig menuConfig;
    menuConfig.perPage = 12;
    menuConfig.fontSize = 32;
    auto menuAnchor = ChessScreenLayout::browseMenuAnchor();
    menuConfig.x = menuAnchor.x;
    menuConfig.y = menuAnchor.y;
    auto shopPanels = ChessScreenLayout::shopPanelsForMenu(menuAnchor, menuData.labels, menuConfig.fontSize);
    menuConfig.previewFrame = shopPanels.status;
    auto menu = makeIndexedMenu(std::format("選擇升星 {}★→{}★", fromStar, toStar), menuData, menuConfig, {}, previewData);
    menu->run();
    int sel = menu->getResult();
    if (sel >= 0)
    {
        auto& picked = chesses[sel];
        manager.upgradeChess(picked.id, toStar);
        showChessMessage(std::format("{}升星至{}★！", picked.role->Name, toStar));
        return true;
    }
    return false;
}

bool ChessRewardFlow::rewardEquipment(int maxTier, int specificId)
{
    auto manager = makeChessManager(services_);
    if (specificId >= 0)
    {
        if (!manager.applyEquipmentReward(maxTier, specificId))
        {
            return false;
        }
        auto* eq = ChessEquipment::getById(specificId);
        auto* item = eq ? eq->getItem() : nullptr;
        showChessMessage(std::format("獲得裝備：{}", item ? item->Name : "???"));
        return true;
    }

    auto& allEquip = ChessEquipment::getAll();
    IndexedMenuData menuData;
    std::vector<const EquipmentDef*> detailEquipments;
    std::string tierName[] = {"初階", "中階", "高階", "傳說"};
    for (auto& eq : allEquip)
    {
        if (eq.tier > maxTier) continue;
        auto* item = eq.getItem();
        std::string name = item ? item->Name : "???";
        while (name.size() < 15) name += "\xe3\x80\x80";
        menuData.labels.push_back(std::format("[{}] {}", tierName[std::min(eq.tier - 1, 3)], name));
        Color tierColors[] = {{100, 200, 100, 255}, {100, 150, 255, 255}, {255, 150, 50, 255}, {255, 100, 255, 255}};
        menuData.colors.push_back(tierColors[std::min(eq.tier - 1, 3)]);
        detailEquipments.push_back(&eq);
    }
    if (menuData.labels.empty())
    {
        return false;
    }

    IndexedMenuConfig menuConfig;
    menuConfig.exitable = false;
    auto menuAnchor = ChessScreenLayout::browseMenuAnchor();
    menuConfig.x = menuAnchor.x;
    menuConfig.y = menuAnchor.y;
    auto detailFrame = ChessScreenLayout::browseDetailRegionForMenu(menuAnchor, menuData.labels, menuConfig.fontSize);
    auto menu = makeIndexedMenu("選擇裝備獎勵", menuData, menuConfig, {std::make_shared<EquipmentDetailPanel>(detailEquipments, [](const EquipmentDef&) { return EquipmentDetailState{}; }, detailFrame)});
    menu->run();
    int sel = menu->getResult();
    if (sel >= 0)
    {
        services_.equipmentInventory.storeItem(detailEquipments[sel]->itemId);
        auto* item = detailEquipments[sel]->getItem();
        showChessMessage(std::format("獲得裝備：{}", item ? item->Name : "???"));
        return true;
    }
    return false;
}

}    // namespace KysChess