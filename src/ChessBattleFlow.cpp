#include "ChessBattleFlow.h"

#include "BattleMap.h"
#include "BattleStatsView.h"
#include "ChessBalance.h"
#include "ChessCombo.h"
#include "ChessDetailPanels.h"
#include "ChessMenuHelpers.h"
#include "ChessRewardFlow.h"
#include "DynamicChessMap.h"
#include "Talk.h"
#include "TextBox.h"
#include "UISave.h"

#include <algorithm>
#include <climits>
#include <numeric>

namespace KysChess
{

ChessBattleFlow::ChessBattleFlow(const ChessSelectorServices& services, ChessRewardFlow& rewardFlow)
    : services_(services)
    , rewardFlow_(rewardFlow)
{
}

void ChessBattleFlow::selectForBattle()
{
    auto manager = makeChessManager(services_);
    auto& pieces = services_.roster.items();
    if (pieces.empty())
    {
        return;
    }
    int maxSelection = services_.economy.getMaxDeploy();

    for (;;)
    {
        std::vector<ChessMenuEntry> entries;
        int selectedCount = 0;
        for (const auto& [id, chess] : services_.roster.items())
        {
            entries.push_back({chess, chess.selectedForBattle ? "[戰]" : ""});
            if (chess.selectedForBattle) ++selectedCount;
        }
        std::sort(entries.begin(), entries.end(), [](const ChessMenuEntry& left, const ChessMenuEntry& right) {
            return std::make_pair(left.chess.selectedForBattle ? 0 : 1, left.chess.id) < std::make_pair(right.chess.selectedForBattle ? 0 : 1, right.chess.id);
        });

        auto menuData = chessPresenter().buildChessMenuData(entries);
        for (size_t i = 0; i < static_cast<size_t>(selectedCount); ++i)
        {
            menuData.colors[i] = {255, 215, 0, 255};
        }

        IndexedMenuConfig menuConfig;
        menuConfig.perPage = 12;
        menuConfig.fontSize = 32;
        auto shopMenu = makeShopIndexedMenuSetup(menuData, menuConfig);
        auto menu = makeChessMenu(
            std::format("選擇出戰棋子 {}/{} 背包{}/{}", selectedCount, maxSelection, manager.getBenchCount(), ChessBalance::config().benchSize),
            menuData,
            shopMenu.config,
            {std::make_shared<ComboInfoPanel>(manager, shopMenu.panels.combo)});
        menu->run();

        int selectedIdx = menu->getResult();
        if (selectedIdx < 0)
        {
            return;
        }

        auto chess = entries[selectedIdx].chess;
        if (chess.selectedForBattle)
        {
            chess.selectedForBattle = false;
            services_.roster.update(chess);
        }
        else if (selectedCount >= maxSelection)
        {
            showChessMessage(std::format("最多只能選擇{}個棋子", maxSelection));
        }
        else
        {
            chess.selectedForBattle = true;
            services_.roster.update(chess);
        }
    }
}

void ChessBattleFlow::enterBattle()
{
    auto manager = makeChessManager(services_);
    auto& battleProgress = services_.progress.battleProgress();
    if (battleProgress.isGameComplete())
    {
        showChessMessage("恭喜通關！");
        return;
    }

    auto selectedChess = manager.getSelectedForBattle();
    if (selectedChess.empty())
    {
        showChessMessage("請先選擇出戰棋子");
        return;
    }

    auto savedEnemyCallCount = services_.random.getEnemyCallCount();
    std::vector<int> enemyIds;
    std::vector<int> enemyStars;
    int fightNum = battleProgress.getFight();
    bool isBoss = battleProgress.isBossFight();
    auto& config = ChessBalance::config();
    int tableIdx = std::min(fightNum, static_cast<int>(config.enemyTable.size()) - 1);
    auto& slots = config.enemyTable[tableIdx];
    for (auto& slot : slots)
    {
        auto role = services_.shop.pool().selectEnemyFromPool(slot.tier);
        enemyIds.push_back(role->ID);
        enemyStars.push_back(std::min(slot.star, 3));
    }

    DynamicBattleRoles roles;
    for (auto& chess : selectedChess)
    {
        roles.teammate_ids.push_back(chess.role->ID);
        roles.teammate_stars.push_back(chess.star);
        roles.teammate_instances.push_back(chess.id.value);
    }
    roles.enemy_ids = enemyIds;
    roles.enemy_stars = enemyStars;
    roles.enemy_weapons.resize(enemyIds.size(), -1);
    roles.enemy_armors.resize(enemyIds.size(), -1);

    int maxTier = 0;
    int equipCount = 0;
    for (auto& level : config.enemyEquipmentLevels)
    {
        if (fightNum + 1 >= level.fight)
        {
            maxTier = level.maxTier;
            equipCount = level.count;
        }
    }
    if (maxTier > 0 && equipCount > 0)
    {
        auto tierEquip = ChessEquipment::getByTier(maxTier);
        if (!tierEquip.empty())
        {
            std::vector<int> indices(enemyStars.size());
            std::iota(indices.begin(), indices.end(), 0);
            std::sort(indices.begin(), indices.end(), [&](int a, int b) { return enemyStars[a] > enemyStars[b]; });
            for (int i = 0; i < std::min(equipCount, static_cast<int>(indices.size())); ++i)
            {
                auto* equip = tierEquip[services_.random.enemyRandInt(static_cast<int>(tierEquip.size()))];
                if (equip->equipType == 0) roles.enemy_weapons[indices[i]] = equip->itemId;
                else roles.enemy_armors[indices[i]] = equip->itemId;
            }
        }
    }

    int battleSeed = static_cast<int>(services_.random.enemyRandInt(INT_MAX));
    int result = runBattle(roles, selectedChess, -1, battleSeed);

    for (const auto& chess : selectedChess)
    {
        chess.role->HP = chess.role->MaxHP;
        chess.role->MP = chess.role->MaxMP;
    }

    if (result == 0)
    {
        std::vector<Chess> survivors;
        for (auto& chess : selectedChess)
        {
            if (chess.role->HP > 0)
                survivors.push_back(chess);
        }
        auto combos = ChessCombo::detectCombos(selectedChess);
        int goldBonus = ChessCombo::calculateGoldBonus(combos, survivors);

        int reward = config.rewardBase + fightNum * config.rewardGrowth / (config.totalFights - 1) + (isBoss ? config.bossRewardBonus : 0);
        int interest = std::min(services_.economy.getMoney() * config.interestPercent / 100, config.interestMax);
        services_.economy.make(reward + interest + goldBonus);
        int expGain = isBoss ? config.bossBattleExp : config.battleExp;
        int oldLevel = services_.economy.getLevel();
        services_.economy.increaseExp(expGain);
        if (isBoss)
        {
            rewardFlow_.showNeigongReward();
        }
        rewardFlow_.showEquipmentReward();
        battleProgress.advance();
        if (!services_.shop.isLocked())
        {
            services_.shop.pool().refresh();
        }

        auto text = std::make_shared<TextBox>();
        std::string levelMsg = (services_.economy.getLevel() > oldLevel) ? std::format(" 升級！等級{}", services_.economy.getLevel() + 1) : "";
        std::string nextInfo = battleProgress.isGameComplete() ? " 通關！"
            : battleProgress.isBossFight() ? std::format(" 下一關：第{}關(Boss)", battleProgress.getFight() + 1)
            : std::format(" 下一關：第{}關", battleProgress.getFight() + 1);
        std::string interestMsg = interest > 0 ? std::format("(利息+${})", interest) : "";
        std::string bonusMsg = goldBonus > 0 ? std::format("(丐帮+${})", goldBonus) : "";
        text->setText(std::format("勝利！獲得${}{}{} 經驗+{}{}{}", reward, interestMsg, bonusMsg, expGain, levelMsg, nextInfo));
        text->setFontSize(32);
        text->runCentered(Engine::getInstance()->getUIHeight() / 2);

        if (battleProgress.isGameComplete())
        {
            auto outro = std::make_shared<Talk>(std::format("少俠果然不凡！珍瓏棋局已破。若有興趣，可嘗試「遠征挑戰」，那裡有更強的對手等著你。當然，你也可以嘗試其他羈絆組合，探索更多可能。"), 115);
            outro->run();
        }
        UISave::autoSave();
    }
    else
    {
        services_.random.setEnemyCallCount(savedEnemyCallCount);
        services_.random.restore();
        auto text = std::make_shared<TextBox>();
        text->setText("戰鬥失敗！請調整陣容後再試");
        text->setFontSize(32);
        text->runCentered(Engine::getInstance()->getUIHeight() / 2);
    }
}

int ChessBattleFlow::runBattle(const DynamicBattleRoles& roles, const std::vector<Chess>& allyChess, int battle_id, int seed)
{
    ChessManager chessManager(services_.roster, services_.equipmentInventory, services_.economy);
    std::vector<Chess> enemyChessVec;
    for (size_t i = 0; i < roles.enemy_ids.size(); ++i)
    {
        auto* role = services_.roleSave.getRole(roles.enemy_ids[i]);
        if (role)
        {
            enemyChessVec.push_back({role, i < roles.enemy_stars.size() ? roles.enemy_stars[i] : 1});
        }
    }

    auto allyCombos = ChessCombo::detectCombos(allyChess);
    auto enemyCombos = ChessCombo::detectCombos(enemyChessVec);
    {
        auto info = BattleMap::getInstance()->getBattleInfo(battle_id);
        int musicId = info ? info->Music : -1;
        auto view = std::make_shared<BattleStatsView>(services_.roleSave, chessManager);
        view->setupPreBattle(allyChess, roles.enemy_ids, roles.enemy_stars, allyCombos, enemyCombos, musicId, roles.enemy_weapons, roles.enemy_armors);
        view->run();
    }

    auto battle = DynamicChessMap::createBattle(roles, services_.random, services_.roleSave, services_.progress, chessManager, battle_id);
    if (seed >= 0)
    {
        battle->rand_.set_seed(static_cast<unsigned int>(seed));
    }
    battle->run();

    {
        auto view = std::make_shared<BattleStatsView>(services_.roleSave, chessManager);
        view->setupPostBattle(battle->getFriendsObj(), battle->getEnemiesObj(), battle->getTracker(), battle->getResult());
        view->run();
    }

    return battle->getResult();
}

}    // namespace KysChess