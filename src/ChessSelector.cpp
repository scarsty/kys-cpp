#include "ChessSelector.h"

#include "ChessBattleFlow.h"
#include "ChessDetailPanels.h"
#include "ChessChallengeFlow.h"
#include "ChessEquipmentFlow.h"
#include "ChessInfoFlow.h"
#include "ChessScreenLayout.h"
#include "ChessRewardFlow.h"
#include "ChessShopFlow.h"
#include "ChessUiCommon.h"
#include "Engine.h"
#include "GameState.h"
#include "ImGuiLayer.h"
#include "Menu.h"
#include "SystemSettings.h"
#include "UISave.h"

namespace KysChess
{

namespace
{
class BattleSystemMenuNode : public RunNode
{
public:
    void backRun() override
    {
        if (!Engine::getInstance()->isBattleSystemMenuOpen())
        {
            exitWithResult(0);
        }
    }
};
}    // namespace

ChessSelector::ChessSelector(
    ChessRoleSave& roleSave,
    ChessEquipmentInventory& equipmentInventory,
    ChessRoster& roster,
    ChessShop& shop,
    ChessProgress& progress,
    ChessEconomy& economy,
    ChessRandom& random)
    : roleSave_(roleSave)
    , equipmentInventory_(equipmentInventory)
    , roster_(roster)
    , shop_(shop)
    , progress_(progress)
    , economy_(economy)
    , random_(random)
{
}

ChessSelectorServices ChessSelector::services() const
{
    return {roleSave_, equipmentInventory_, roster_, shop_, progress_, economy_, random_};
}

void ChessSelector::getChess()
{
    ChessShopFlow(services()).getChess();
}

void ChessSelector::sellChess()
{
    ChessShopFlow(services()).sellChess();
}

void ChessSelector::selectForBattle()
{
    ChessRewardFlow rewardFlow(services());
    ChessBattleFlow(services(), rewardFlow).selectForBattle();
}

void ChessSelector::enterBattle()
{
    ChessRewardFlow rewardFlow(services());
    ChessBattleFlow(services(), rewardFlow).enterBattle();
}

int ChessSelector::runBattle(const DynamicBattleRoles& roles, const std::vector<Chess>& allyChess, int battle_id, int seed)
{
    ChessRewardFlow rewardFlow(services());
    return ChessBattleFlow(services(), rewardFlow).runBattle(roles, allyChess, battle_id, seed);
}

void ChessSelector::buyExp()
{
    ChessShopFlow(services()).buyExp();
}

void ChessSelector::manageBans()
{
    ChessShopFlow(services()).showBanMenu();
}

void ChessSelector::viewChessPool()
{
    ChessInfoFlow(services()).viewChessPool();
}

void ChessSelector::rerollBattleSeed()
{
    constexpr int kRerollCost = 3;
    auto previewPanel = std::make_shared<BattleSeedRerollPreviewPanel>(
        "逆天改命",
        std::format("花費 ${}  重新抽取下場戰鬥的敵方隨機種子", kRerollCost),
        "敵人陣容會改變，地圖不保證改變",
        "確認後立即生效");

    auto menu = std::make_shared<MenuText>(std::vector<std::string>{"確認逆天改命", "取消"});
    menu->setFontSize(36);
    menu->arrange(0, 0, 0, 45);
    menu->addChild(previewPanel);
    auto menuAnchor = ChessScreenLayout::battleSeedRerollMenuAnchor();
    menu->runAtPosition(menuAnchor.x, menuAnchor.y);

    if (menu->getResult() != 0)
    {
        return;
    }

    if (!economy_.spend(kRerollCost))
    {
        showChessMessage(std::format("金幣不足，需要${}！", kRerollCost));
        return;
    }
    // Reseed the enemy RNG so the next battle regenerates with a new sequence.
    random_.rerollEnemySeed();
    showChessMessage(std::format("花費${}逆天改命！下場戰鬥的敵人陣容和隨機種子已改變（地圖不保證改變）", kRerollCost));
}

void ChessSelector::showContextMenu()
{
    while (true)
    {
        const auto banEnabled = GameState::get().hasBanSystem();
        std::vector<std::string> menuItems{
            "購買棋子",
            "出售棋子",
            "選擇出戰",
            "進入戰鬥",
            "購買經驗",
            "逆天改命",
            "查看羈絆",
            "棋子一覽",
            "查看內功",
            "遠征挑戰",
            "系統設定",
            "裝備管理",
        };
        if (banEnabled)
        {
            menuItems.push_back("禁棋管理");
        }
        menuItems.push_back("遊戲說明");
        auto menu = std::make_shared<MenuText>(menuItems);
        menu->setFontSize(36);
        menu->arrange(0, 0, 0, 45);
        menu->runAtPosition(200, 60);

        const auto result = menu->getResult();
        const auto banIndex = banEnabled ? 12 : -1;
        const auto guideIndex = banEnabled ? 13 : 12;
        if (result == -1)
        {
            return;
        }
        if (banEnabled && result == banIndex)
        {
            manageBans();
            UISave::autoSave();
            continue;
        }
        if (result == guideIndex)
        {
            showGameGuide();
            UISave::autoSave();
            continue;
        }

        switch (result)
        {
        case 0: getChess(); break;
        case 1: sellChess(); break;
        case 2: selectForBattle(); break;
        case 3: enterBattle(); break;
        case 4: buyExp(); break;
        case 5: rerollBattleSeed(); break;
        case 6: viewCombos(); break;
        case 7: viewChessPool(); break;
        case 8: viewNeigong(); break;
        case 9: showExpeditionChallenge(); break;
        case 10: showSystemMenu(); break;
        case 11: manageEquipment(); break;
        }
        UISave::autoSave();
    }
}

void ChessSelector::viewCombos()
{
    ChessInfoFlow(services()).viewCombos();
}

void ChessSelector::showNeigongReward()
{
    ChessRewardFlow(services()).showNeigongReward();
}

void ChessSelector::viewNeigong()
{
    ChessInfoFlow(services()).viewNeigong();
}

void ChessSelector::manageEquipment()
{
    ChessEquipmentFlow(services()).manageEquipment();
}

void ChessSelector::showExpeditionChallenge()
{
    ChessRewardFlow rewardFlow(services());
    ChessBattleFlow battleFlow(services(), rewardFlow);
    ChessChallengeFlow(services(), battleFlow, rewardFlow).showExpeditionChallenge();
}

void ChessSelector::showGameGuide()
{
    ChessInfoFlow(services()).showGameGuide();
}

void ChessSelector::showSystemMenu()
{
    auto settings = SystemSettings::getInstance()->snapshot();
    BattleSystemMenuData data;
    data.positionSwapEnabled = settings.positionSwapEnabled;
    data.musicVolume = settings.musicVolume;
    data.soundVolume = settings.soundVolume;
    data.manualCamera = settings.manualCamera;
    data.battleSpeed = settings.battleSpeed;
    data.simplifiedChinese = settings.simplifiedChinese;
    data.showBattleLog = settings.showBattleLog;

    auto* engine = Engine::getInstance();
    engine->showBattleSystemMenu(data);

    auto waitNode = std::make_shared<BattleSystemMenuNode>();
    waitNode->run();

    auto result = engine->getBattleSystemMenuData();
    SystemSettingsData updated = settings;
    updated.positionSwapEnabled = result.positionSwapEnabled;
    updated.musicVolume = result.musicVolume;
    updated.soundVolume = result.soundVolume;
    updated.manualCamera = result.manualCamera;
    updated.battleSpeed = result.battleSpeed;
    updated.simplifiedChinese = result.simplifiedChinese;
    updated.showBattleLog = result.showBattleLog;
    SystemSettings::getInstance()->update(updated);
}

}    // namespace KysChess
