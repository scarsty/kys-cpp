#include "ChessSelector.h"

#include "ChessBattleFlow.h"
#include "ChessContextMenu.h"
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

#include <assert.h>
#include <optional>

namespace KysChess
{

namespace
{
constexpr int kContextMenuX = 200;
constexpr int kContextMenuFontSize = 36;
constexpr int kContextMenuRowSpacing = 45;

std::optional<ChessContextMenuAction> runContextActionMenu(const std::vector<ChessContextMenuItem>& items)
{
    auto menu = std::make_shared<MenuText>(chessContextMenuLabels(items));
    menu->setFontSize(kContextMenuFontSize);
    menu->arrange(0, 0, 0, kContextMenuRowSpacing);
    const auto region = ChessScreenLayout::fullContentRegion();
    menu->runAtPosition(kContextMenuX, centerChessContextMenuY(items.size(), region.y, region.h, kContextMenuRowSpacing));

    const int result = menu->getResult();
    if (result < 0)
    {
        return std::nullopt;
    }
    assert(result < static_cast<int>(items.size()));
    return items[result].action;
}

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

int ChessSelector::runBattle(
    const DynamicBattleRoles& roles,
    const std::vector<Chess>& allyChess,
    unsigned int battleSeed,
    int battle_id)
{
    ChessRewardFlow rewardFlow(services());
    return ChessBattleFlow(services(), rewardFlow).runBattle(roles, allyChess, battleSeed, battle_id);
}

void ChessSelector::buyExp()
{
    ChessShopFlow flow(services());
    while (flow.buyExp())
    {
        UISave::autoSave();
    }
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
        const auto action = runContextActionMenu(buildChessContextMenu(GameState::get().hasBanSystem()));
        if (!action)
        {
            return;
        }

        bool didAction = true;
        switch (*action)
        {
        case ChessContextMenuAction::BuyChess: getChess(); break;
        case ChessContextMenuAction::SellChess: sellChess(); break;
        case ChessContextMenuAction::SelectForBattle: selectForBattle(); break;
        case ChessContextMenuAction::EnterBattle: enterBattle(); break;
        case ChessContextMenuAction::BuyExp: buyExp(); break;
        case ChessContextMenuAction::ManageBans: manageBans(); break;
        case ChessContextMenuAction::OpenEquipmentMenu: manageEquipment(); break;
        case ChessContextMenuAction::OpenOverviewMenu:
        {
            const auto overviewAction = runContextActionMenu(buildChessOverviewMenu());
            if (!overviewAction)
            {
                didAction = false;
                break;
            }
            switch (*overviewAction)
            {
            case ChessContextMenuAction::ShowPositionSwap: showPositionSwap(); break;
            case ChessContextMenuAction::RerollBattleSeed: rerollBattleSeed(); break;
            case ChessContextMenuAction::ViewCombos: viewCombos(); break;
            case ChessContextMenuAction::ViewChessPool: viewChessPool(); break;
            case ChessContextMenuAction::ViewNeigong: viewNeigong(); break;
            default: assert(false); break;
            }
            break;
        }
        case ChessContextMenuAction::ShowExpeditionChallenge: showExpeditionChallenge(); break;
        case ChessContextMenuAction::ShowSystemSettings: showSystemMenu(); break;
        case ChessContextMenuAction::ShowGameGuide: showGameGuide(); break;
        default: assert(false); break;
        }
        if (didAction)
        {
            UISave::autoSave();
        }
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

void ChessSelector::showPositionSwap()
{
    ChessInfoFlow(services()).showPositionSwap();
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
    data.musicVolume = settings.musicVolume;
    data.soundVolume = settings.soundVolume;
    data.manualCamera = settings.manualCamera;
    data.battleSpeed = settings.battleSpeed;
    data.simplifiedChinese = settings.simplifiedChinese;
    data.showBattleLog = settings.showBattleLog;
    data.debugLatencyLog = settings.debugLatencyLog;

    auto* engine = Engine::getInstance();
    engine->showBattleSystemMenu(data);

    auto waitNode = std::make_shared<BattleSystemMenuNode>();
    waitNode->run();

    auto result = engine->getBattleSystemMenuData();
    SystemSettingsData updated = settings;
    updated.musicVolume = result.musicVolume;
    updated.soundVolume = result.soundVolume;
    updated.manualCamera = result.manualCamera;
    updated.battleSpeed = result.battleSpeed;
    updated.simplifiedChinese = result.simplifiedChinese;
    updated.showBattleLog = result.showBattleLog;
    updated.debugLatencyLog = result.debugLatencyLog;
    SystemSettings::getInstance()->update(updated);
}

}    // namespace KysChess
