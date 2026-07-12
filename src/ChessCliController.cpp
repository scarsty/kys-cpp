#include "ChessCliController.h"

#include "ChessAsciiBoard.h"
#include "ChessBattleText.h"
#include "ChessObservationText.h"
#include "ChessReplayJson.h"
#include "ChessReplayArchive.h"

#include <glaze/json.hpp>

#include <charconv>
#include <algorithm>
#include <format>
#include <sstream>
#include <filesystem>
#include <fstream>

namespace KysChess
{
namespace CliDetail
{

struct Request
{
    std::uint64_t id{};
    std::string method;
    glz::raw_json params;
};

}

namespace
{

std::optional<int> parseInt(std::string_view text)
{
    int value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size()
        ? std::optional(value)
        : std::nullopt;
}

Difficulty difficultyFromText(std::string_view text)
{
    if (text == "easy") return Difficulty::Easy;
    if (text == "hard") return Difficulty::Hard;
    return Difficulty::Normal;
}

std::vector<int> parseIdList(std::string text)
{
    std::vector<int> result;
    std::ranges::replace(text, ',', ' ');
    std::istringstream stream(text);
    int id{};
    while (stream >> id)
    {
        result.push_back(id);
    }
    return result;
}

}

ChessCliController::ChessCliController(ChessJsonProtocol::ContentProvider contentProvider)
    : protocol_(std::move(contentProvider))
{
}

std::string ChessCliController::submitRequest(std::string method, std::string paramsJson)
{
    CliDetail::Request request{nextRequestId_++, std::move(method), glz::raw_json(std::move(paramsJson))};
    const auto json = glz::write_json(request);
    return json ? protocol_.handleLine(json.value()) : std::string{};
}

std::string ChessCliController::newSession(
    Difficulty difficulty,
    std::uint64_t seed,
    ChessCliOutputMode mode)
{
    const auto difficultyText = difficulty == Difficulty::Easy
        ? "easy"
        : difficulty == Difficulty::Hard ? "hard" : "normal";
    const auto response = submitRequest(
        "new",
        std::format(
            "{{\"difficulty\":\"{}\",\"seed\":\"0x{:016x}\"}}",
            difficultyText,
            seed));
    return mode == ChessCliOutputMode::Json ? response : renderCurrent(mode);
}

std::string ChessCliController::renderCurrent(ChessCliOutputMode mode) const
{
    const auto* session = protocol_.session();
    if (!session)
    {
        return "尚未建立遊戲。\n";
    }
    auto text = ChessObservationText::format(
        session->observe(),
        session->content(),
        session->legalActions());
    if (mode == ChessCliOutputMode::Compact)
    {
        const auto end = text.find('\n');
        return text.substr(0, end + 1);
    }
    return text;
}

std::string ChessCliController::renderBattle(ChessCliOutputMode mode) const
{
    const auto* session = protocol_.session();
    if (!session || !session->lastBattleResult() || !session->lastBattlePrepared())
    {
        return renderCurrent(mode);
    }
    const auto& prepared = *session->lastBattlePrepared();
    const auto& battle = *session->lastBattleResult();
    std::string text = ChessBattleText::formatHeader(
        session->state().fight,
        prepared,
        session->content());
    text += ChessAsciiBoard::render(prepared, session->content());
    text += ChessBattleText::formatEvents(prepared, battle.digestEvents);
    text += ChessBattleText::formatSummary(battle.summary);
    if (mode != ChessCliOutputMode::Compact)
    {
        text += renderCurrent(mode);
    }
    return text;
}

std::string ChessCliController::submitAction(const ChessAction& action, ChessCliOutputMode mode)
{
    const auto response = submitRequest(
        "act",
        std::format("{{\"action\":{}}}", serializeChessActionJson(action)));
    if (mode == ChessCliOutputMode::Json)
    {
        return response;
    }
    return action.type == ChessActionType::StartBattle
        ? renderBattle(mode)
        : renderCurrent(mode);
}

std::string ChessCliController::executeInteractive(
    std::string_view command,
    ChessCliOutputMode mode)
{
    std::istringstream stream{std::string(command)};
    std::string verb;
    stream >> verb;
    if (verb.empty() || verb == "observe") return renderCurrent(mode);
    if (verb == "help")
    {
        return "指令：observe、legal、buy N、refresh、lock on|off、sell ID、exp、deploy ID,...、ban ROLE、skip_bans、equip EQUIP CHESS、legendary ITEM、position_swap on|off、reroll_enemy、prepare、map ID、swap UNIT UNIT、start、reward ID、reroll_reward、challenge ID、finish、save SLOT、load SLOT、replay、quit\n";
    }
    if (verb == "legal") return submitRequest("legal_actions", "{}");
    if (verb == "replay") return submitRequest("export_replay", "{}");
    if (verb == "export_replay")
    {
        std::filesystem::path path;
        stream >> path;
        const auto* session = protocol_.session();
        if (!session || path.empty())
        {
            return "缺少重播輸出路徑。\n";
        }
        const auto replay = session->exportReplay();
        if (!replay)
        {
            return "自動流程尚未完成，不能匯出重播。\n";
        }
        const auto jsonl = serializeChessReplayJsonl(*replay);
        if (path.extension() == ".kysreplay")
        {
            return ChessReplayArchive::write(path, jsonl) == ChessReplayArchiveError::None
                ? "重播封裝完成。\n"
                : "重播封裝失敗。\n";
        }
        std::ofstream output(path, std::ios::binary);
        output << jsonl;
        return output ? "重播輸出完成。\n" : "重播輸出失敗。\n";
    }
    if (verb == "save" || verb == "load")
    {
        std::string slot;
        stream >> slot;
        return submitRequest(
            verb == "save" ? "save_game" : "load_game",
            std::format("{{\"slot\":{}}}", glz::write_json(slot).value()));
    }
    if (verb == "new")
    {
        std::string difficulty;
        std::uint64_t seed = 1;
        stream >> difficulty >> seed;
        return newSession(difficultyFromText(difficulty), seed, mode);
    }

    ChessAction action;
    if (verb == "refresh") action.type = ChessActionType::RefreshShop;
    else if (verb == "buy") { action.type = ChessActionType::BuyShopSlot; stream >> action.shopSlot; }
    else if (verb == "lock") { action.type = ChessActionType::SetShopLocked; std::string value; stream >> value; action.value = value == "on" || value == "true"; }
    else if (verb == "sell") { action.type = ChessActionType::SellChess; stream >> action.chessInstanceId; }
    else if (verb == "exp") action.type = ChessActionType::BuyExp;
    else if (verb == "deploy") { action.type = ChessActionType::SetDeployment; std::string ids; std::getline(stream, ids); action.chessInstanceIds = parseIdList(ids); }
    else if (verb == "ban") { action.type = ChessActionType::AddBan; stream >> action.roleId; }
    else if (verb == "skip_bans") action.type = ChessActionType::SkipForcedBans;
    else if (verb == "equip") { action.type = ChessActionType::Equip; stream >> action.equipmentInstanceId >> action.targetChessInstanceId; }
    else if (verb == "legendary") { action.type = ChessActionType::BuyLegendaryEquipment; stream >> action.itemId; }
    else if (verb == "position_swap") { action.type = ChessActionType::SetPositionSwapEnabled; std::string value; stream >> value; action.value = value == "on" || value == "true"; }
    else if (verb == "reroll_enemy") action.type = ChessActionType::RerollEnemySeed;
    else if (verb == "prepare") action.type = ChessActionType::PrepareBattle;
    else if (verb == "map") { action.type = ChessActionType::ChooseMap; stream >> action.mapId; }
    else if (verb == "swap") { action.type = ChessActionType::SwapPositions; stream >> action.chessInstanceId >> action.targetChessInstanceId; }
    else if (verb == "start") action.type = ChessActionType::StartBattle;
    else if (verb == "reroll_reward") action.type = ChessActionType::RerollReward;
    else if (verb == "reward") { action.type = ChessActionType::ChooseReward; stream >> action.rewardId; }
    else if (verb == "challenge") { action.type = ChessActionType::StartChallenge; stream >> action.challengeId; }
    else if (verb == "finish") action.type = ChessActionType::FinishRun;
    else return "未知指令；輸入 help 查看可用指令。\n";
    return submitAction(action, mode);
}

int ChessCliController::runJsonl(std::istream& input, std::ostream& output)
{
    std::string line;
    while (std::getline(input, line))
    {
        output << protocol_.handleLine(line) << '\n';
        output.flush();
    }
    return 0;
}

int ChessCliController::runInteractive(
    std::istream& input,
    std::ostream& output,
    ChessCliOutputMode mode)
{
    std::string line;
    while (std::getline(input, line))
    {
        if (line == "quit" || line == "exit")
        {
            return 0;
        }
        output << executeInteractive(line, mode);
        output.flush();
    }
    return 0;
}

}
