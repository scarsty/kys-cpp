#include "ChessModHook.h"
#include "ChessCombo.h"
#include "ChessSelector.h"
#include "Event.h"
#include "Menu.h"
#include "Talk.h"
#include "GameState.h"
#include "Save.h"
#include "UISave.h"
#include <format>

namespace KysChess
{

static constexpr int MOD_SCENE_ID = 53;    // 擂鼓山
static constexpr int MOD_ENTRY_X = 21;
static constexpr int MOD_ENTRY_Y = 54;
static constexpr int MAIN_MAP_X = 358;
static constexpr int MAIN_MAP_Y = 235;
static constexpr int SHIP_X = 109;
static constexpr int SHIP_Y = 100;
static constexpr int SHIP_X1 = 109;
static constexpr int SHIP_Y1 = 99;
static bool needIntro_ = false;

ChessMod::ChessMod(GameState& gameState)
    : gameState_(gameState)
{
}

    ChessMod::~ChessMod() = default;

ChessSelector ChessMod::makeSelector() const
{
    return ChessSelector(
        gameState_.roleSave(),
        gameState_.equipmentInventory(),
        gameState_.roster(),
        gameState_.shop(),
        gameState_.progress(),
        gameState_.economy(),
        gameState_.random());
}

void ChessModHook::initializeSaveState(::Save& save)
{
    save.InShip = 0;
    save.InSubMap = MOD_SCENE_ID;
    save.MainMapX = MAIN_MAP_X;
    save.MainMapY = MAIN_MAP_Y;
    save.SubMapX = MOD_ENTRY_X;
    save.SubMapY = MOD_ENTRY_Y;
    save.FaceTowards = 1;
    save.ShipX = SHIP_X;
    save.ShipY = SHIP_Y;
    save.ShipX1 = SHIP_X1;
    save.ShipY1 = SHIP_Y1;
    save.Encode = 65001;

    for (int i = 0; i < TEAMMATE_COUNT; ++i)
    {
        save.Team[i] = -1;
    }
    save.Team[0] = 0;

    for (int i = 0; i < ITEM_IN_BAG_COUNT; ++i)
    {
        save.Items[i] = {};
    }
}

bool ChessModHook::overrideNewGame(int& scene, int& x, int& y, int& event, Difficulty difficulty)
{
    scene = MOD_SCENE_ID;
    x = MOD_ENTRY_X;
    y = MOD_ENTRY_Y;
    event = -1;
    GameState::get().reset(difficulty);
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

        // Difficulty was already chosen at the title screen; show explanation if non-Easy.
        auto difficulty = gameState_.difficulty();
        if (difficulty != Difficulty::Easy)
        {
            const auto& cfg = ChessBalance::config();
            auto advancedModeTalk = std::make_shared<Talk>(
                std::format(
                    "此局乃{}棋式，江湖譜牒更廣，棋池尤深。"
                    "為免少俠在茫茫人海中錯失機緣，老夫命商肆多開一席，每回合可見{}路人選。"
                    "但旁門雜流既多，若不先行剪除，便難聚攏心中武學。"
                    "故自開局起，你可先封禁{}名棋子；往後每升一重境界，便再添{}道禁令。"
                    "境界愈高，可斷去的岔路愈多，方能在大江湖中收束成局。此局共{}關，且看少俠能走到哪一步。",
                    ChessBalance::difficultyDisplayNameTraditional(difficulty),
                    cfg.shopSlotCount,
                    gameState_.banBaseCount(),
                    gameState_.banCountPerLevel(),
                    cfg.totalFights),
                115);
            advancedModeTalk->run();
        }
    }
}

bool ChessMod::blockExit(int submap_id) const
{
    return submap_id == MOD_SCENE_ID;
}

void ChessMod::showContextMenu()
{
    auto selector = makeSelector();
    selector.showContextMenu();
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
