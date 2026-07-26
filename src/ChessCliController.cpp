#include "ChessCliController.h"

#include "ChessActionText.h"
#include "ChessAsciiBoard.h"
#include "ChessBattleText.h"
#include "ChessCliCommands.h"
#include "ChessJsonCodec.h"
#include "ChessObservationText.h"
#include "ChessReplayArchive.h"
#include "ChessReplayJson.h"

#include <glaze/json.hpp>

#include <charconv>
#include <algorithm>
#include <format>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <optional>

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

bool atEnd(std::istringstream& stream)
{
    stream >> std::ws;
    return stream.eof();
}

std::optional<std::vector<int>> parseIdList(std::string text)
{
    std::vector<int> result;
    std::ranges::replace(text, ',', ' ');
    std::istringstream stream(text);
    std::string token;
    while (stream >> token)
    {
        const auto id = parseInt(token);
        if (!id)
        {
            return std::nullopt;
        }
        result.push_back(*id);
    }
    return result;
}

std::string remainingText(std::istringstream& stream)
{
    std::string result;
    std::getline(stream >> std::ws, result);
    return result;
}

std::optional<bool> parseToggle(std::string_view text)
{
    if (text == "on" || text == "true")
    {
        return true;
    }
    if (text == "off" || text == "false")
    {
        return false;
    }
    return std::nullopt;
}

template<typename T>
std::optional<T> protocolResult(
    std::string_view responseText,
    std::string& error)
{
    const auto response = ProtocolDetail::readJson<ProtocolDetail::ResponseDto>(responseText);
    if (!response)
    {
        error = "CLI 無法解析遊戲核心回應";
        return std::nullopt;
    }
    if (!response->ok)
    {
        error = response->error_message.value_or("遊戲核心拒絕要求");
        return std::nullopt;
    }
    if (!response->result)
    {
        error = "遊戲核心回應缺少結果";
        return std::nullopt;
    }
    const auto result = ProtocolDetail::readJson<T>(response->result->str);
    if (!result)
    {
        error = "CLI 無法解析遊戲核心結果";
        return std::nullopt;
    }
    return result;
}

std::string commandUsage(const ChessCliActionCommand& command)
{
    return std::format("用法：{}\n", chessCliActionSyntax(command));
}

std::string ensureTrailingNewline(std::string text)
{
    if (!text.empty() && !text.ends_with('\n'))
    {
        text += '\n';
    }
    return text;
}

std::optional<std::string> exportableReplayJsonl(
    const ChessJsonProtocol& protocol)
{
    const auto* session = protocol.session();
    if (!session)
    {
        return std::nullopt;
    }
    const auto replay = session->exportReplay();
    return replay
        ? std::optional(serializeChessReplayJsonl(*replay))
        : std::nullopt;
}

}

ChessCliController::ChessCliController(
    ChessJsonProtocol::ContentProvider contentProvider,
    ChessJsonProtocolOptions options)
    : protocol_(std::move(contentProvider), std::move(options))
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
    if (mode == ChessCliOutputMode::Json)
    {
        return response;
    }
    std::string error;
    if (!protocolResult<ProtocolDetail::SessionObservationDto>(response, error))
    {
        return std::format("建立遊戲失敗：{}\n", error);
    }
    return renderCurrent(mode);
}

std::string ChessCliController::helpText()
{
    std::string text =
        "工作階段指令：\n"
        "  observe                         顯示目前遊戲狀態\n"
        "  legal                           顯示目前可用的遊戲操作\n"
        "  new <easy|normal|hard> <種子>   建立新遊戲；種子可用十進位或 0x 十六進位\n"
        "  save <欄位>                     儲存目前遊戲\n"
        "  load <欄位>                     載入遊戲\n"
        "  replay                          在終端輸出重播 JSONL\n"
        "  export_replay <路徑>            輸出 .jsonl 或 .kysreplay 重播\n"
        "  help                            顯示本說明\n"
        "  quit                            離開\n"
        "遊戲操作：\n";
    for (const auto& command : kChessCliActionCommands)
    {
        text += std::format(
            "  {:<36} {}\n",
            chessCliActionSyntax(command),
            chessActionDescription(command.type));
    }
    return text;
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
    std::string error;
    const auto result = protocolResult<ProtocolDetail::SummaryActionResultDto>(
        response,
        error);
    if (!result)
    {
        return std::format("操作失敗：{}\n", error);
    }
    if (!result->accepted)
    {
        return std::format(
            "操作失敗（{}）：{}\n{}",
            result->error_code,
            result->description,
            renderCurrent(mode));
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
    if (verb.empty() || verb == "observe")
    {
        if (!verb.empty() && !atEnd(stream))
        {
            return "用法：observe\n";
        }
        return mode == ChessCliOutputMode::Json
            ? submitRequest("observe", "{}")
            : renderCurrent(mode);
    }
    if (verb == "help")
    {
        return atEnd(stream) ? helpText() : "用法：help\n";
    }
    if (verb == "legal")
    {
        if (!atEnd(stream))
        {
            return "用法：legal\n";
        }
        if (mode == ChessCliOutputMode::Json)
        {
            return submitRequest("legal_actions", "{}");
        }
        const auto* session = protocol_.session();
        return session
            ? ChessObservationText::formatLegalActions(session->legalActions())
            : "尚未建立遊戲。\n";
    }
    if (verb == "replay")
    {
        if (!atEnd(stream))
        {
            return "用法：replay\n";
        }
        if (mode == ChessCliOutputMode::Json)
        {
            return submitRequest("export_replay", "{}");
        }
        const auto jsonl = exportableReplayJsonl(protocol_);
        return jsonl
            ? ensureTrailingNewline(*jsonl)
            : "自動流程尚未完成，不能匯出重播。\n";
    }
    if (verb == "export_replay")
    {
        std::filesystem::path path;
        stream >> path;
        if (path.empty() || !atEnd(stream))
        {
            return "用法：export_replay <路徑>\n";
        }
        const auto jsonl = exportableReplayJsonl(protocol_);
        if (!jsonl)
        {
            return "自動流程尚未完成，不能匯出重播。\n";
        }
        if (path.extension() == ".kysreplay")
        {
            return ChessReplayArchive::write(path, *jsonl) == ChessReplayArchiveError::None
                ? "重播封裝完成。\n"
                : "重播封裝失敗。\n";
        }
        std::ofstream output(path, std::ios::binary);
        output << *jsonl;
        return output ? "重播輸出完成。\n" : "重播輸出失敗。\n";
    }
    if (verb == "save" || verb == "load")
    {
        std::string slot;
        stream >> slot;
        if (slot.empty() || !atEnd(stream))
        {
            return std::format("用法：{} <欄位>\n", verb);
        }
        const auto response = submitRequest(
            verb == "save" ? "save_game" : "load_game",
            std::format("{{\"slot\":{}}}", glz::write_json(slot).value()));
        if (mode == ChessCliOutputMode::Json)
        {
            return response;
        }
        std::string error;
        if (verb == "save")
        {
            const auto result = protocolResult<ProtocolDetail::SaveResultDto>(response, error);
            return result
                ? std::format("已建立存檔「{}」（修訂 {}）。\n", result->slot, result->revision)
                : std::format("存檔失敗：{}\n", error);
        }
        const auto result = protocolResult<ProtocolDetail::TimelineReplacementDto>(response, error);
        return result
            ? std::format(
                "已載入存檔「{}」（還原至序號 {}，捨棄 {} 個後續行動）。\n{}",
                result->loaded_slot,
                result->restored_sequence,
                result->discarded_active_actions,
                renderCurrent(mode))
            : std::format("載入失敗：{}\n", error);
    }
    if (verb == "new")
    {
        std::string difficultyText;
        std::string seedText;
        stream >> difficultyText >> seedText;
        const auto difficulty = parseChessCliDifficulty(difficultyText);
        const auto seed = parseChessCliSeed(seedText);
        if (!difficulty || !seed || !atEnd(stream))
        {
            return "用法：new <easy|normal|hard> <種子>\n";
        }
        return newSession(*difficulty, *seed, mode);
    }

    const auto* commandInfo = chessCliActionCommand(verb);
    if (!commandInfo)
    {
        return "未知指令；輸入 help 查看可用指令。\n";
    }
    ChessAction action;
    action.type = commandInfo->type;
    const auto readSingleInt = [&](int& value)
    {
        std::string text;
        stream >> text;
        const auto parsed = parseInt(text);
        if (!parsed || !atEnd(stream))
        {
            return false;
        }
        value = *parsed;
        return true;
    };
    const auto readTwoInts = [&](int& first, int& second)
    {
        std::string firstText;
        std::string secondText;
        stream >> firstText >> secondText;
        const auto parsedFirst = parseInt(firstText);
        const auto parsedSecond = parseInt(secondText);
        if (!parsedFirst || !parsedSecond || !atEnd(stream))
        {
            return false;
        }
        first = *parsedFirst;
        second = *parsedSecond;
        return true;
    };
    bool validSyntax = true;
    switch (action.type)
    {
    case ChessActionType::BuyShopSlot:
        validSyntax = readSingleInt(action.shopSlot);
        break;
    case ChessActionType::RefreshShop:
    case ChessActionType::BuyExp:
    case ChessActionType::SkipForcedBans:
    case ChessActionType::RerollEnemySeed:
    case ChessActionType::PrepareBattle:
    case ChessActionType::StartBattle:
    case ChessActionType::FinishRun:
        validSyntax = atEnd(stream);
        break;
    case ChessActionType::SetShopLocked:
    case ChessActionType::SetPositionSwapEnabled:
    {
        std::string valueText;
        stream >> valueText;
        const auto value = parseToggle(valueText);
        validSyntax = value && atEnd(stream);
        if (validSyntax)
        {
            action.value = *value;
        }
        break;
    }
    case ChessActionType::SellChess:
        validSyntax = readSingleInt(action.chessInstanceId);
        break;
    case ChessActionType::SetDeployment:
    case ChessActionType::SetFormation:
    {
        const auto ids = parseIdList(remainingText(stream));
        validSyntax = ids
            && (action.type != ChessActionType::SetFormation || !ids->empty());
        if (validSyntax)
        {
            action.chessInstanceIds = *ids;
        }
        break;
    }
    case ChessActionType::AddBan:
        validSyntax = readSingleInt(action.roleId);
        break;
    case ChessActionType::Equip:
        validSyntax = readTwoInts(
            action.equipmentInstanceId,
            action.targetChessInstanceId);
        break;
    case ChessActionType::BuyLegendaryEquipment:
        validSyntax = readSingleInt(action.itemId);
        break;
    case ChessActionType::ChooseMap:
        validSyntax = readSingleInt(action.mapId);
        break;
    case ChessActionType::SwapPositions:
        validSyntax = readTwoInts(
            action.chessInstanceId,
            action.targetChessInstanceId);
        break;
    case ChessActionType::ChooseReward:
        action.rewardId = remainingText(stream);
        validSyntax = !action.rewardId.empty();
        break;
    case ChessActionType::StartChallenge:
        action.challengeName = remainingText(stream);
        validSyntax = !action.challengeName.empty();
        break;
    }
    if (!validSyntax)
    {
        return commandUsage(*commandInfo);
    }
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

int ChessCliController::runMcp(std::istream& input, std::ostream& output)
{
    std::string line;
    while (std::getline(input, line))
    {
        const auto response = protocol_.handleMcpLine(line);
        if (!response.empty())
        {
            output << response << '\n';
            output.flush();
        }
    }
    return 0;
}

int ChessCliController::runInteractive(
    std::istream& input,
    std::ostream& output,
    ChessCliOutputMode mode)
{
    if (mode == ChessCliOutputMode::Json)
    {
        output << "互動模式不支援 JSON 輸出；請使用 JSONL 模式。\n";
        return 2;
    }
    std::string line;
    while (std::getline(input, line))
    {
        if (line == "quit" || line == "exit")
        {
            return 0;
        }
        output << ensureTrailingNewline(executeInteractive(line, mode));
        output.flush();
    }
    return 0;
}

}
