#include "ChessSelector.h"

#include "ChessBattleFlow.h"
#include "ChessChallengeFlow.h"
#include "ChessEquipmentFlow.h"
#include "ChessInfoFlow.h"
#include "ChessRewardFlow.h"
#include "ChessShopFlow.h"
#include "ChessUiCommon.h"
#include "Menu.h"
#include "UISave.h"

namespace KysChess
{

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

void ChessSelector::showContextMenu()
{
    while (true)
    {
        auto menu = std::make_shared<MenuText>(std::vector<std::string>{ "購買棋子", "出售棋子", "選擇出戰", "進入戰鬥", "購買經驗", "查看羈絆", "查看內功", "遠征挑戰", "排兵佈陣", "裝備管理", "遊戲說明" });
        menu->setFontSize(36);
        menu->arrange(0, 0, 0, 45);
        menu->runAtPosition(200, 120);

        switch (menu->getResult())
        {
        case 0: getChess(); break;
        case 1: sellChess(); break;
        case 2: selectForBattle(); break;
        case 3: enterBattle(); break;
        case 4: buyExp(); break;
        case 5: viewCombos(); break;
        case 6: viewNeigong(); break;
        case 7: showExpeditionChallenge(); break;
        case 8: showPositionSwap(); break;
        case 9: manageEquipment(); break;
        case 10: showGameGuide(); break;
        case -1: return;
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

void ChessSelector::showPositionSwap()
{
    ChessInfoFlow(services()).showPositionSwap();
}

}    // namespace KysChess