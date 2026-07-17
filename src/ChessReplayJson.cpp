#include "ChessReplayJson.h"

#include "ChessReplayHash.h"

#include <glaze/json.hpp>

#include <charconv>
#include <format>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace KysChess
{
namespace ReplayJsonDetail
{

struct HeaderDto
{
    std::string record;
    std::string magic;
    std::string game_version;
    std::string difficulty;
    std::string root_seed;
    ChessReplayOptionsData options;
};

struct DecisionDto
{
    std::string record;
    ChessActionData action;
    std::string evidence_hash;
};

struct FooterDto
{
    std::string record;
    std::string status;
    std::string terminal_evidence_hash;
    std::string final_state_hash;
    std::string result;
    int fight_reached{};
};

struct RecordTagDto
{
    std::string record;
};

}

namespace
{

using namespace ReplayJsonDetail;

std::string rootSeedText(std::uint64_t seed)
{
    return std::format("0x{:016x}", seed);
}

std::optional<std::uint64_t> parseRootSeed(std::string_view text)
{
    if (text.size() != 18 || !text.starts_with("0x"))
    {
        return std::nullopt;
    }
    std::uint64_t value{};
    const auto result = std::from_chars(text.data() + 2, text.data() + text.size(), value, 16);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
    {
        return std::nullopt;
    }
    return value;
}

ChessActionData actionData(const ChessAction& action)
{
    ChessActionData dto;
    dto.type = chessActionTypeId(action.type);
    switch (action.type)
    {
    case ChessActionType::BuyShopSlot: dto.slot = action.shopSlot; break;
    case ChessActionType::SetShopLocked: dto.locked = action.value; break;
    case ChessActionType::SellChess: dto.chess_instance_id = action.chessInstanceId; break;
    case ChessActionType::SetDeployment: dto.chess_instance_ids = action.chessInstanceIds; break;
    case ChessActionType::AddBan: dto.role_id = action.roleId; break;
    case ChessActionType::Equip:
        dto.equipment_instance_id = action.equipmentInstanceId;
        dto.target_chess_instance_id = action.targetChessInstanceId;
        break;
    case ChessActionType::BuyLegendaryEquipment: dto.item_id = action.itemId; break;
    case ChessActionType::SetPositionSwapEnabled: dto.enabled = action.value; break;
    case ChessActionType::ChooseMap: dto.map_id = action.mapId; break;
    case ChessActionType::SwapPositions:
        dto.first_unit_id = action.chessInstanceId;
        dto.second_unit_id = action.targetChessInstanceId;
        break;
    case ChessActionType::ChooseReward: dto.reward_id = action.rewardId; break;
    case ChessActionType::StartChallenge: dto.challenge_name = action.challengeName; break;
    default: break;
    }
    return dto;
}

std::optional<ChessAction> actionFromData(const ChessActionData& dto, std::string* error = nullptr)
{
    const auto type = chessActionTypeFromId(dto.type);
    if (!type)
    {
        if (error)
        {
            *error = dto.type.empty()
                ? "缺少操作 type"
                : std::format("未知操作 type：{}", dto.type);
        }
        return std::nullopt;
    }
    const auto require = [&](bool present, std::string_view field, std::string_view typeName) {
        if (!present && error)
        {
            *error = std::format(
                "操作 {} 缺少必要欄位 {}（{}）。範例：{}",
                dto.type,
                field,
                typeName,
                chessActionExampleJson(*type));
        }
        return present;
    };
    ChessAction action;
    action.type = *type;
    switch (*type)
    {
    case ChessActionType::BuyShopSlot:
        if (!require(dto.slot.has_value(), "slot", "整數")) return std::nullopt;
        action.shopSlot = *dto.slot;
        break;
    case ChessActionType::SetShopLocked:
        if (!require(dto.locked.has_value(), "locked", "布林值")) return std::nullopt;
        action.value = *dto.locked;
        break;
    case ChessActionType::SellChess:
        if (!require(dto.chess_instance_id.has_value(), "chess_instance_id", "整數")) return std::nullopt;
        action.chessInstanceId = *dto.chess_instance_id;
        break;
    case ChessActionType::SetDeployment:
        if (!require(dto.chess_instance_ids.has_value(), "chess_instance_ids", "整數陣列")) return std::nullopt;
        action.chessInstanceIds = *dto.chess_instance_ids;
        break;
    case ChessActionType::AddBan:
        if (!require(dto.role_id.has_value(), "role_id", "整數")) return std::nullopt;
        action.roleId = *dto.role_id;
        break;
    case ChessActionType::Equip:
        if (!require(dto.equipment_instance_id.has_value(), "equipment_instance_id", "整數")
            || !require(dto.target_chess_instance_id.has_value(), "target_chess_instance_id", "整數")) return std::nullopt;
        action.equipmentInstanceId = *dto.equipment_instance_id;
        action.targetChessInstanceId = *dto.target_chess_instance_id;
        break;
    case ChessActionType::BuyLegendaryEquipment:
        if (!require(dto.item_id.has_value(), "item_id", "整數")) return std::nullopt;
        action.itemId = *dto.item_id;
        break;
    case ChessActionType::SetPositionSwapEnabled:
        if (!require(dto.enabled.has_value(), "enabled", "布林值")) return std::nullopt;
        action.value = *dto.enabled;
        break;
    case ChessActionType::ChooseMap:
        if (!require(dto.map_id.has_value(), "map_id", "整數")) return std::nullopt;
        action.mapId = *dto.map_id;
        break;
    case ChessActionType::SwapPositions:
        if (!require(dto.first_unit_id.has_value(), "first_unit_id", "整數")
            || !require(dto.second_unit_id.has_value(), "second_unit_id", "整數")) return std::nullopt;
        action.chessInstanceId = *dto.first_unit_id;
        action.targetChessInstanceId = *dto.second_unit_id;
        break;
    case ChessActionType::ChooseReward:
        if (!require(dto.reward_id.has_value(), "reward_id", "字串")) return std::nullopt;
        action.rewardId = *dto.reward_id;
        break;
    case ChessActionType::StartChallenge:
        if (!require(dto.challenge_name.has_value(), "challenge_name", "字串")) return std::nullopt;
        action.challengeName = *dto.challenge_name;
        break;
    default: break;
    }
    return action;
}

template<typename T>
std::optional<T> readLine(std::string_view line)
{
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    T parsed;
    if (const auto result = glz::read<options>(parsed, line); result)
    {
        return std::nullopt;
    }
    return parsed;
}

template<typename T>
std::string writeLine(const T& value)
{
    auto written = glz::write_json(value);
    return written ? std::move(written.value()) : std::string{};
}

bool parseHash(std::string_view text, ChessSha256& output)
{
    try
    {
        output = chessSha256FromHex(text);
        return true;
    }
    catch (const std::invalid_argument&)
    {
        return false;
    }
}

bool parseHash(std::string_view text, ChessEvidenceHash& output)
{
    try
    {
        output = chessEvidenceHashFromHex(text);
        return true;
    }
    catch (const std::invalid_argument&)
    {
        return false;
    }
}

}

std::string chessActionTypeId(ChessActionType type)
{
    switch (type)
    {
    case ChessActionType::BuyShopSlot: return "buy_shop_slot";
    case ChessActionType::RefreshShop: return "refresh_shop";
    case ChessActionType::SetShopLocked: return "set_shop_locked";
    case ChessActionType::SellChess: return "sell_chess";
    case ChessActionType::SetDeployment: return "set_deployment";
    case ChessActionType::BuyExp: return "buy_exp";
    case ChessActionType::AddBan: return "add_ban";
    case ChessActionType::SkipForcedBans: return "skip_forced_bans";
    case ChessActionType::Equip: return "equip";
    case ChessActionType::BuyLegendaryEquipment: return "buy_legendary_equipment";
    case ChessActionType::SetPositionSwapEnabled: return "set_position_swap_enabled";
    case ChessActionType::RerollEnemySeed: return "reroll_enemy_seed";
    case ChessActionType::PrepareBattle: return "prepare_battle";
    case ChessActionType::ChooseMap: return "choose_map";
    case ChessActionType::SwapPositions: return "swap_positions";
    case ChessActionType::StartBattle: return "start_battle";
    case ChessActionType::RerollReward: return "reroll_reward";
    case ChessActionType::ChooseReward: return "choose_reward";
    case ChessActionType::StartChallenge: return "start_challenge";
    case ChessActionType::FinishRun: return "finish_run";
    }
    std::unreachable();
}

std::optional<ChessActionType> chessActionTypeFromId(std::string_view id)
{
    for (int value = static_cast<int>(ChessActionType::BuyShopSlot);
         value <= static_cast<int>(ChessActionType::FinishRun);
         ++value)
    {
        const auto type = static_cast<ChessActionType>(value);
        if (chessActionTypeId(type) == id)
        {
            return type;
        }
    }
    return std::nullopt;
}

std::string serializeChessActionJson(const ChessAction& action)
{
    return writeLine(actionData(action));
}

std::optional<ChessAction> parseChessActionJson(std::string_view json)
{
    const auto dto = readLine<ChessActionData>(json);
    return dto ? actionFromData(*dto) : std::nullopt;
}

std::optional<ChessAction> parseChessActionJson(std::string_view json, std::string& error)
{
    const auto dto = readLine<ChessActionData>(json);
    if (!dto)
    {
        error = "操作不是有效 JSON 物件";
        return std::nullopt;
    }
    return actionFromData(*dto, &error);
}

std::string chessActionPayloadSchema(ChessActionType type)
{
    switch (type)
    {
    case ChessActionType::BuyShopSlot: return R"({"type":"buy_shop_slot","slot":"整數"})";
    case ChessActionType::SetShopLocked: return R"({"type":"set_shop_locked","locked":"布林值"})";
    case ChessActionType::SellChess: return R"({"type":"sell_chess","chess_instance_id":"整數"})";
    case ChessActionType::SetDeployment: return R"({"type":"set_deployment","chess_instance_ids":"整數陣列"})";
    case ChessActionType::AddBan: return R"({"type":"add_ban","role_id":"整數"})";
    case ChessActionType::Equip: return R"({"type":"equip","equipment_instance_id":"整數","target_chess_instance_id":"整數"})";
    case ChessActionType::BuyLegendaryEquipment: return R"({"type":"buy_legendary_equipment","item_id":"整數"})";
    case ChessActionType::SetPositionSwapEnabled: return R"({"type":"set_position_swap_enabled","enabled":"布林值"})";
    case ChessActionType::ChooseMap: return R"({"type":"choose_map","map_id":"整數"})";
    case ChessActionType::SwapPositions: return R"({"type":"swap_positions","first_unit_id":"整數","second_unit_id":"整數"})";
    case ChessActionType::ChooseReward: return R"({"type":"choose_reward","reward_id":"字串"})";
    case ChessActionType::StartChallenge: return R"({"type":"start_challenge","challenge_name":"字串"})";
    default:
        return std::format(R"({{"type":"{}"}})", chessActionTypeId(type));
    }
}

std::string chessActionExampleJson(ChessActionType type)
{
    ChessAction action;
    action.type = type;
    switch (type)
    {
    case ChessActionType::BuyShopSlot: action.shopSlot = 0; break;
    case ChessActionType::SetShopLocked: action.value = true; break;
    case ChessActionType::SellChess: action.chessInstanceId = 1; break;
    case ChessActionType::SetDeployment: action.chessInstanceIds = {1, 2}; break;
    case ChessActionType::AddBan: action.roleId = 610; break;
    case ChessActionType::Equip:
        action.equipmentInstanceId = 1;
        action.targetChessInstanceId = 1;
        break;
    case ChessActionType::BuyLegendaryEquipment: action.itemId = 47; break;
    case ChessActionType::SetPositionSwapEnabled: action.value = true; break;
    case ChessActionType::ChooseMap: action.mapId = 1; break;
    case ChessActionType::SwapPositions:
        action.chessInstanceId = 1;
        action.targetChessInstanceId = 2;
        break;
    case ChessActionType::ChooseReward: action.rewardId = "equipment:47"; break;
    case ChessActionType::StartChallenge: action.challengeName = "聚賢莊內"; break;
    default: break;
    }
    return serializeChessActionJson(action);
}

ChessReplayData chessReplayData(const ChessReplay& replay)
{
    ChessReplayData data;
    data.header = {
        "KYS_CHESS_REPLAY",
        replay.header.gameVersion,
        replay.header.difficulty,
        rootSeedText(replay.header.rootSeed),
        {replay.header.options.positionSwapEnabled, replay.header.options.battleFrameLimit},
    };
    for (const auto& record : replay.decisions)
    {
        data.decisions.push_back({
            actionData(record.action),
            chessEvidenceHashHex(record.evidenceHash),
        });
    }
    data.footer = {
        replay.footer.complete ? "complete" : "in_progress",
        chessEvidenceHashHex(replay.footer.terminalEvidenceHash),
        chessSha256Hex(replay.footer.finalStateHash),
        replay.footer.complete ? "campaign_complete" : "in_progress",
        replay.footer.fightReached,
    };
    return data;
}

std::optional<ChessReplay> parseChessReplayData(
    const ChessReplayData& data,
    ChessReplayJsonError& error)
{
    ChessReplay replay;
    const auto seed = parseRootSeed(data.header.root_seed);
    if (data.header.magic != "KYS_CHESS_REPLAY"
        || data.header.game_version.empty()
        || !seed)
    {
        error = {0, "重播標頭無效"};
        return std::nullopt;
    }
    replay.header.gameVersion = data.header.game_version;
    replay.header.difficulty = data.header.difficulty;
    replay.header.rootSeed = *seed;
    replay.header.options = {
        data.header.options.position_swap_enabled,
        data.header.options.battle_frame_limit,
    };
    for (std::size_t index = 0; index < data.decisions.size(); ++index)
    {
        const auto action = actionFromData(data.decisions[index].action);
        ChessReplayDecisionRecord record;
        if (!action || !parseHash(data.decisions[index].evidence_hash, record.evidenceHash))
        {
            error = {index + 1, "決策記錄無效"};
            return std::nullopt;
        }
        record.action = *action;
        replay.decisions.push_back(std::move(record));
    }
    if (!parseHash(data.footer.terminal_evidence_hash, replay.footer.terminalEvidenceHash)
        || !parseHash(data.footer.final_state_hash, replay.footer.finalStateHash)
        || (data.footer.status != "in_progress" && data.footer.status != "complete")
        || (data.footer.status == "complete" && data.footer.result != "campaign_complete")
        || (data.footer.status == "in_progress" && data.footer.result != "in_progress"))
    {
        error = {data.decisions.size() + 1, "重播頁尾無效"};
        return std::nullopt;
    }
    replay.footer.complete = data.footer.status == "complete";
    replay.footer.fightReached = data.footer.fight_reached;
    error = {};
    return replay;
}

std::string serializeChessReplayJsonl(const ChessReplay& replay)
{
    using namespace ReplayJsonDetail;
    const auto data = chessReplayData(replay);
    std::string output;
    output += writeLine(HeaderDto{
        "header",
        data.header.magic,
        data.header.game_version,
        data.header.difficulty,
        data.header.root_seed,
        data.header.options,
    }) + "\n";
    for (const auto& record : data.decisions)
    {
        output += writeLine(DecisionDto{
            "decision",
            record.action,
            record.evidence_hash,
        }) + "\n";
    }
    output += writeLine(FooterDto{
        "footer",
        data.footer.status,
        data.footer.terminal_evidence_hash,
        data.footer.final_state_hash,
        data.footer.result,
        data.footer.fight_reached,
    }) + "\n";
    return output;
}

std::optional<ChessReplay> parseChessReplayJsonl(
    std::string_view jsonl,
    ChessReplayJsonError& error)
{
    using namespace ReplayJsonDetail;
    ChessReplayData data;
    bool sawHeader = false;
    bool sawFooter = false;
    std::size_t lineNumber = 0;
    std::size_t offset = 0;
    while (offset < jsonl.size())
    {
        const auto end = jsonl.find('\n', offset);
        auto line = jsonl.substr(offset, end == std::string_view::npos ? jsonl.size() - offset : end - offset);
        offset = end == std::string_view::npos ? jsonl.size() : end + 1;
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty()) continue;
        const auto tag = readLine<RecordTagDto>(line);
        if (!tag)
        {
            error = {lineNumber, "JSON 記錄格式錯誤"};
            return std::nullopt;
        }
        if (tag->record == "header")
        {
            if (sawHeader || !data.decisions.empty() || sawFooter)
            {
                error = {lineNumber, "重複或錯置的標頭"};
                return std::nullopt;
            }
            const auto dto = readLine<HeaderDto>(line);
            if (!dto)
            {
                error = {lineNumber, "重播標頭無效"};
                return std::nullopt;
            }
            data.header = {
                dto->magic,
                dto->game_version,
                dto->difficulty,
                dto->root_seed,
                dto->options,
            };
            sawHeader = true;
        }
        else if (tag->record == "decision")
        {
            if (!sawHeader || sawFooter)
            {
                error = {lineNumber, "決策記錄位置無效"};
                return std::nullopt;
            }
            const auto dto = readLine<DecisionDto>(line);
            if (!dto)
            {
                error = {lineNumber, "決策記錄無效"};
                return std::nullopt;
            }
            data.decisions.push_back({dto->action, dto->evidence_hash});
        }
        else if (tag->record == "footer")
        {
            if (!sawHeader || sawFooter)
            {
                error = {lineNumber, "重複或錯置的頁尾"};
                return std::nullopt;
            }
            const auto dto = readLine<FooterDto>(line);
            if (!dto)
            {
                error = {lineNumber, "重播頁尾無效"};
                return std::nullopt;
            }
            data.footer = {
                dto->status,
                dto->terminal_evidence_hash,
                dto->final_state_hash,
                dto->result,
                dto->fight_reached,
            };
            sawFooter = true;
        }
        else
        {
            error = {lineNumber, "未知的重播記錄類型"};
            return std::nullopt;
        }
    }
    if (!sawHeader || !sawFooter)
    {
        error = {lineNumber, "重播缺少標頭或頁尾"};
        return std::nullopt;
    }
    auto replay = parseChessReplayData(data, error);
    if (!replay)
    {
        error.line = error.line == 0 ? 1 : error.line + 1;
    }
    return replay;
}

}
