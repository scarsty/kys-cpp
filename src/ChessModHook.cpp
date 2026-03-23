#include "ChessModHook.h"
#include "ChessCombo.h"
#include "ChessSelector.h"
#include "Event.h"
#include "Menu.h"
#include "Talk.h"
#include "GameState.h"
#include "UISave.h"
#include <format>

namespace KysChess
{

static constexpr int MOD_SCENE_ID = 53;    // 擂鼓山
static bool needIntro_ = false;

ChessMod::ChessMod(GameState& gameState)
    : gameState_(gameState)
    , selector_(std::make_unique<ChessSelector>(
        gameState.roleSave(),
        gameState.equipmentInventory(),
        gameState.roster(),
        gameState.shop(),
        gameState.progress(),
        gameState.economy(),
        gameState.random()))
{
}

    ChessMod::~ChessMod() = default;

bool ChessModHook::overrideNewGame(int& scene, int& x, int& y, int& event)
{
    scene = MOD_SCENE_ID;
    x = 21;
    y = 54;
    event = -1;
    GameState::get().reset();
    ChessCombo::clearActiveStates();
    needIntro_ = true;
    return true;
}

bool ChessMod::interceptEvent(int submap_id, int event_id)
{
    if (submap_id == MOD_SCENE_ID && event_id > 0)
    {
        showContextMenu();
        return true;
    }
    return false;
}

void ChessMod::onSubSceneEntrance(int submap_id)
{
    if (submap_id == MOD_SCENE_ID && needIntro_)
    {
        needIntro_ = false;
        auto talk = std::make_shared<Talk>(
            "少俠，你來了。此處乃擂鼓山，天下英雄匯聚之地。"
            "老夫蘇星河，奉師命在此守護珍瓏棋局。"
            "這珍瓏棋局，非尋常棋局。棋盤之上，江湖群俠化為棋子，各展所長，自行廝殺。"
            "弈者不執棋，而在於選將、佈陣、運籌帷幄。"
            "你若有意一試，便來與老夫說話罷。", 115);
        talk->run();
        
        int diff = -1;
        while (diff == -1) {
            auto diffMenu = std::make_shared<MenuText>();
            diffMenu->setStrings({ "正常難度", "挑戰難度" });
            diffMenu->setFontSize(40);
            diffMenu->arrange(0, 0, 0, 49);
            diffMenu->runAtPosition(500, 300);
            diff = diffMenu->getResult();
        }
        auto difficulty = (diff == 1) ? Difficulty::Normal : Difficulty::Easy;
        gameState_.difficulty() = difficulty;
        ChessBalance::setDifficulty(difficulty);
        gameState_.syncBanRuleFromBalance();
        const auto& cfg = ChessBalance::config();
        gameState_.economy().setMoney(cfg.initialMoney);
        if (difficulty == Difficulty::Normal)
        {
            auto hardModeTalk = std::make_shared<Talk>(
                std::format(
                    "此局乃困難棋式，江湖譜牒更廣，棋池尤深。"
                    "為免少俠在茫茫人海中錯失機緣，老夫命商肆多開一席，每回合可見{}路人選。"
                    "但旁門雜流既多，若不先行剪除，便難聚攏心中武學。"
                    "故自開局起，你可先封禁{}名棋子；往後每升一重境界，便再添{}道禁令。"
                    "境界愈高，可斷去的岔路愈多，方能在大江湖中收束成局。",
                    cfg.shopSlotCount,
                    gameState_.banBaseCount(),
                    gameState_.banCountPerLevel()),
                115);
            hardModeTalk->run();
        }
    }
}

bool ChessMod::blockExit(int submap_id) const
{
    return submap_id == MOD_SCENE_ID;
}

void ChessMod::showContextMenu()
{
    selector_->showContextMenu();
}

void ChessMod::showMenu()
{
    auto menu = std::make_shared<MenuText>();
    menu->setStrings({ "讀取進度", "保存進度", "返回開頭" });
    menu->setFontSize(40);
    menu->arrange(0, 0, 0, 49);
    menu->runAtPosition(200, 200);
    int r = menu->getResult();
    if (r == 0 || r == 1)
    {
        auto ui_save = std::make_shared<UISave>();
        ui_save->setMode(r);
        ui_save->setFontSize(36);
        ui_save->arrange(0, 0, 0, 45);
        ui_save->runAtPosition(200, 260);
    }
    else if (r == 2)
    {
        auto confirm = std::make_shared<MenuText>();
        confirm->setStrings({ "確認返回開頭", "     否     " });
        confirm->setFontSize(40);
        confirm->arrange(0, 0, 0, 49);
        confirm->runAtPosition(200, 200);
        if (confirm->getResult() == 0)
        {
            RunNode::exitAll(1);
            Event::getInstance()->forceExit();
        }
    }
}

GameDataStore ChessModHook::exportGameData()
{
    return GameState::get().exportStore();
}

void ChessModHook::importGameData(const GameDataStore& store)
{
    GameState::get().importStore(store);
}

}    // namespace KysChess
