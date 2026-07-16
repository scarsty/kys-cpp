#include "ChessModHook.h"
#include "ChessApplicationSessionHost.h"
#include "ChessGuiSavePolicy.h"
#include "ChessGuiSessionAdapter.h"
#include "ChessPresentationHelpers.h"
#include "GameDataStore.h"
#include "GameUtil.h"
#include "Talk.h"
#include "Save.h"
#include "ChessSessionCheckpoint.h"
#include <algorithm>
#include <chrono>
#include <exception>
#include <format>
#include <optional>

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

static std::optional<ChessSessionCheckpoint> parseGuiSaveCheckpoint(const GameDataStore& store)
{
    if (store.chessSessionCheckpointJson.empty())
    {
        return std::nullopt;
    }
    ChessCheckpointError error;
    auto checkpoint = ChessSessionCheckpoint::parseJson(
        store.chessSessionCheckpointJson,
        error);
    if (!checkpoint
        || checkpoint->gameVersion != GameUtil::VERSION()
        || !isChessGuiSavePhaseContinuable(checkpoint->state.phase))
    {
        return std::nullopt;
    }
    return checkpoint;
}

std::uint64_t nextGuiSaveRevision()
{
    static std::uint64_t previous{};
    const auto now = static_cast<std::uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    previous = std::max(previous + 1, now);
    return previous;
}

ChessMod::ChessMod(ChessGameSession& session)
    : session_(session)
{
}

    ChessMod::~ChessMod() = default;

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
    resetApplicationChessSession(difficulty);
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
        auto difficulty = session_.content().difficulty();
        if (difficulty != Difficulty::Easy)
        {
            const auto& cfg = session_.content().balance();
            auto advancedModeTalk = std::make_shared<Talk>(
                std::format(
                    "此局乃{}棋式，江湖譜牒更廣，棋池尤深。"
                    "為免少俠在茫茫人海中錯失機緣，老夫命商肆多開一席，每回合可見{}路人選。"
                    "但旁門雜流既多，若不先行剪除，便難聚攏心中武學。"
                    "故自開局起，你可先封禁{}名棋子；往後每升一重境界，便再添{}道禁令。"
                    "境界愈高，可斷去的岔路愈多，方能在大江湖中收束成局。此局共{}關，且看少俠能走到哪一步。",
                    chessDifficultyDisplayName(difficulty),
                    cfg.shopSlotCount,
                    cfg.banBaseCount,
                    cfg.banCountPerLevel,
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
    ChessGuiSessionAdapter(session_).showContextMenu();
}

void ChessMod::showSystemMenu()
{
    ChessGuiSessionAdapter(session_).showSystemMenu();
}

bool ChessModHook::canSaveGameData()
{
    return canPersistChessGuiState(applicationChessSession());
}

GameDataStore ChessModHook::exportGameData()
{
    GameDataStore store;
    store.chessSessionCheckpointJson = ChessSessionCheckpoint::capture(
        applicationChessSession(),
        nextGuiSaveRevision(),
        "圖形介面存檔").serializeJson();
    return store;
}

bool ChessModHook::isGameDataReadable(const GameDataStore& store)
{
    return parseGuiSaveCheckpoint(store).has_value();
}

bool ChessModHook::importGameData(const GameDataStore& store, ::Save& save)
{
    const auto checkpoint = parseGuiSaveCheckpoint(store);
    if (!checkpoint)
    {
        return false;
    }

    ChessCheckpointError error;
    std::unique_ptr<ChessGameSession> replacement;
    try
    {
        auto& host = ChessApplicationSessionHost::instance();
        error = host.prepareRestore(*checkpoint, replacement);
        if (error != ChessCheckpointError::None)
        {
            return false;
        }
        if (!save.prepareChessMode())
        {
            return false;
        }
        host.commitRestore(std::move(replacement));
    }
    catch (const std::exception& exception)
    {
        LOG("讀取自走棋存檔失敗：{}\n", exception.what());
        return false;
    }
    return true;
}

}    // namespace KysChess
