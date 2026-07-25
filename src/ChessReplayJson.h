#pragma once

#include "ChessRuntimeConstants.h"
#include "ChessReplayTypes.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace KysChess
{

struct ChessReplayJsonError
{
    std::size_t line{};
    std::string message;
};

struct ChessReplayOptionsData
{
    bool position_swap_enabled = true;
    int battle_frame_limit = kChessBattleFrameLimit;
};

struct ChessActionData
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
    std::optional<std::string> challenge_name;
};

struct ChessReplayHeaderData
{
    std::string magic;
    std::string game_version;
    std::string difficulty;
    std::string root_seed;
    ChessReplayOptionsData options;
};

struct ChessReplayDecisionData
{
    ChessActionData action;
    std::string evidence_hash;
};

struct ChessReplayFooterData
{
    std::string status;
    std::string terminal_evidence_hash;
    std::string final_state_hash;
    std::string result;
    int fight_reached{};
};

struct ChessReplayData
{
    ChessReplayHeaderData header;
    std::vector<ChessReplayDecisionData> decisions;
    ChessReplayFooterData footer;
};

ChessReplayData chessReplayData(const ChessReplay& replay);
std::optional<ChessReplay> parseChessReplayData(
    const ChessReplayData& data,
    ChessReplayJsonError& error);

std::string serializeChessReplayJsonl(const ChessReplay& replay);
std::optional<ChessReplay> parseChessReplayJsonl(
    std::string_view jsonl,
    ChessReplayJsonError& error);

std::string chessActionTypeId(ChessActionType type);
std::optional<ChessActionType> chessActionTypeFromId(std::string_view id);
std::string serializeChessActionJson(const ChessAction& action);
std::optional<ChessAction> parseChessActionJson(std::string_view json);
std::optional<ChessAction> parseChessActionJson(std::string_view json, std::string& error);
std::string chessActionPayloadSchema(ChessActionType type);
std::string chessActionExampleJson(ChessActionType type);

}
