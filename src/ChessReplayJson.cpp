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

struct OptionsDto
{
    bool position_swap_enabled = true;
    int battle_frame_limit = 36000;
};

struct ActionDto
{
    std::string type;
    std::optional<int> slot;
    std::optional<bool> locked;
    std::optional<int> chess_instance_id;
    std::optional<std::vector<int>> chess_instance_ids;
    std::optional<int> role_id;
    std::optional<int> equipment_instance_id;
    std::optional<int> target_chess_instance_id;
    std::optional<int> item_id;
    std::optional<bool> enabled;
    std::optional<int> map_id;
    std::optional<int> first_unit_id;
    std::optional<int> second_unit_id;
    std::optional<std::string> reward_id;
    std::optional<std::string> challenge_id;
};

struct HeaderDto
{
    std::string record;
    std::string magic;
    std::string game_version;
    std::string difficulty;
    std::string root_seed;
    OptionsDto options;
};

struct DecisionDto
{
    std::string record;
    std::uint64_t sequence{};
    std::string phase;
    std::string decision_kind;
    glz::raw_json action;
    std::string pre_state_hash;
    std::string post_state_hash;
    std::string event_hash;
    std::string rng_digest;
    std::string previous_chain_hash;
    std::string chain_hash;
};

struct FooterDto
{
    std::string record;
    std::string status;
    std::string terminal_chain_hash;
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

std::string phaseId(ChessSessionPhase phase)
{
    switch (phase)
    {
    case ChessSessionPhase::Management: return "Management";
    case ChessSessionPhase::BattlePreparation: return "BattlePreparation";
    case ChessSessionPhase::BattleResolution: return "BattleResolution";
    case ChessSessionPhase::RewardChoice: return "RewardChoice";
    case ChessSessionPhase::Complete: return "Complete";
    }
    std::unreachable();
}

std::optional<ChessSessionPhase> phaseFromId(std::string_view id)
{
    if (id == "Management") return ChessSessionPhase::Management;
    if (id == "BattlePreparation") return ChessSessionPhase::BattlePreparation;
    if (id == "BattleResolution") return ChessSessionPhase::BattleResolution;
    if (id == "RewardChoice") return ChessSessionPhase::RewardChoice;
    if (id == "Complete") return ChessSessionPhase::Complete;
    return std::nullopt;
}

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

ActionDto actionDto(const ChessAction& action)
{
    ActionDto dto;
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
    case ChessActionType::StartChallenge: dto.challenge_id = action.challengeId; break;
    default: break;
    }
    return dto;
}

std::optional<ChessAction> actionFromDto(const ActionDto& dto)
{
    const auto type = chessActionTypeFromId(dto.type);
    if (!type)
    {
        return std::nullopt;
    }
    ChessAction action;
    action.type = *type;
    switch (*type)
    {
    case ChessActionType::BuyShopSlot:
        if (!dto.slot) return std::nullopt;
        action.shopSlot = *dto.slot;
        break;
    case ChessActionType::SetShopLocked:
        if (!dto.locked) return std::nullopt;
        action.value = *dto.locked;
        break;
    case ChessActionType::SellChess:
        if (!dto.chess_instance_id) return std::nullopt;
        action.chessInstanceId = *dto.chess_instance_id;
        break;
    case ChessActionType::SetDeployment:
        if (!dto.chess_instance_ids) return std::nullopt;
        action.chessInstanceIds = *dto.chess_instance_ids;
        break;
    case ChessActionType::AddBan:
        if (!dto.role_id) return std::nullopt;
        action.roleId = *dto.role_id;
        break;
    case ChessActionType::Equip:
        if (!dto.equipment_instance_id || !dto.target_chess_instance_id) return std::nullopt;
        action.equipmentInstanceId = *dto.equipment_instance_id;
        action.targetChessInstanceId = *dto.target_chess_instance_id;
        break;
    case ChessActionType::BuyLegendaryEquipment:
        if (!dto.item_id) return std::nullopt;
        action.itemId = *dto.item_id;
        break;
    case ChessActionType::SetPositionSwapEnabled:
        if (!dto.enabled) return std::nullopt;
        action.value = *dto.enabled;
        break;
    case ChessActionType::ChooseMap:
        if (!dto.map_id) return std::nullopt;
        action.mapId = *dto.map_id;
        break;
    case ChessActionType::SwapPositions:
        if (!dto.first_unit_id || !dto.second_unit_id) return std::nullopt;
        action.chessInstanceId = *dto.first_unit_id;
        action.targetChessInstanceId = *dto.second_unit_id;
        break;
    case ChessActionType::ChooseReward:
        if (!dto.reward_id) return std::nullopt;
        action.rewardId = *dto.reward_id;
        break;
    case ChessActionType::StartChallenge:
        if (!dto.challenge_id) return std::nullopt;
        action.challengeId = *dto.challenge_id;
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
    return writeLine(actionDto(action));
}

std::optional<ChessAction> parseChessActionJson(std::string_view json)
{
    const auto dto = readLine<ReplayJsonDetail::ActionDto>(json);
    return dto ? actionFromDto(*dto) : std::nullopt;
}

std::string serializeChessReplayJsonl(const ChessReplay& replay)
{
    using namespace ReplayJsonDetail;
    std::string output;
    HeaderDto header{
        "header",
        "KYS_CHESS_REPLAY",
        replay.header.gameVersion,
        replay.header.difficulty,
        rootSeedText(replay.header.rootSeed),
        {replay.header.options.positionSwapEnabled, replay.header.options.battleFrameLimit},
    };
    output += writeLine(header) + "\n";
    for (const auto& record : replay.decisions)
    {
        DecisionDto decision;
        decision.record = "decision";
        decision.sequence = record.sequence;
        decision.phase = phaseId(record.phase);
        decision.decision_kind = chessActionTypeId(record.action.type);
        decision.action = writeLine(actionDto(record.action));
        decision.pre_state_hash = chessSha256Hex(record.preStateHash);
        decision.post_state_hash = chessSha256Hex(record.postStateHash);
        decision.event_hash = chessSha256Hex(record.eventHash);
        decision.rng_digest = chessSha256Hex(record.rngDigest);
        decision.previous_chain_hash = chessSha256Hex(record.previousChainHash);
        decision.chain_hash = chessSha256Hex(record.chainHash);
        output += writeLine(decision) + "\n";
    }
    FooterDto footer{
        "footer",
        replay.footer.complete ? "complete" : "in_progress",
        chessSha256Hex(replay.footer.terminalChainHash),
        chessSha256Hex(replay.footer.finalStateHash),
        replay.footer.complete ? "campaign_complete" : "in_progress",
        replay.footer.fightReached,
    };
    output += writeLine(footer) + "\n";
    return output;
}

std::optional<ChessReplay> parseChessReplayJsonl(
    std::string_view jsonl,
    ChessReplayJsonError& error)
{
    using namespace ReplayJsonDetail;
    ChessReplay replay;
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
            if (sawHeader || !replay.decisions.empty() || sawFooter)
            {
                error = {lineNumber, "重複或錯置的標頭"};
                return std::nullopt;
            }
            const auto dto = readLine<HeaderDto>(line);
            const auto seed = dto ? parseRootSeed(dto->root_seed) : std::nullopt;
            if (!dto
                || dto->magic != "KYS_CHESS_REPLAY"
                || dto->game_version.empty()
                || !seed)
            {
                error = {lineNumber, "重播標頭無效"};
                return std::nullopt;
            }
            replay.header.gameVersion = dto->game_version;
            replay.header.difficulty = dto->difficulty;
            replay.header.rootSeed = *seed;
            replay.header.options = {dto->options.position_swap_enabled, dto->options.battle_frame_limit};
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
            const auto phase = dto ? phaseFromId(dto->phase) : std::nullopt;
            const auto actionDtoValue = dto ? readLine<ActionDto>(dto->action.str) : std::nullopt;
            const auto action = actionDtoValue ? actionFromDto(*actionDtoValue) : std::nullopt;
            ChessReplayDecisionRecord record;
            if (!dto || !phase || !action
                || dto->decision_kind != chessActionTypeId(action->type)
                || !parseHash(dto->pre_state_hash, record.preStateHash)
                || !parseHash(dto->post_state_hash, record.postStateHash)
                || !parseHash(dto->event_hash, record.eventHash)
                || !parseHash(dto->rng_digest, record.rngDigest)
                || !parseHash(dto->previous_chain_hash, record.previousChainHash)
                || !parseHash(dto->chain_hash, record.chainHash))
            {
                error = {lineNumber, "決策記錄無效"};
                return std::nullopt;
            }
            record.sequence = dto->sequence;
            record.phase = *phase;
            record.action = *action;
            replay.decisions.push_back(std::move(record));
        }
        else if (tag->record == "footer")
        {
            if (!sawHeader || sawFooter)
            {
                error = {lineNumber, "重複或錯置的頁尾"};
                return std::nullopt;
            }
            const auto dto = readLine<FooterDto>(line);
            if (!dto
                || !parseHash(dto->terminal_chain_hash, replay.footer.terminalChainHash)
                || !parseHash(dto->final_state_hash, replay.footer.finalStateHash)
                || (dto->status != "in_progress" && dto->status != "complete")
                || (dto->status == "complete" && dto->result != "campaign_complete")
                || (dto->status == "in_progress" && dto->result != "in_progress"))
            {
                error = {lineNumber, "重播頁尾無效"};
                return std::nullopt;
            }
            replay.footer.complete = dto->status == "complete";
            replay.footer.fightReached = dto->fight_reached;
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
    return replay;
}

}
