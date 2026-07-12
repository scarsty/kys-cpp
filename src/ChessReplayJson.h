#pragma once

#include "ChessReplayTypes.h"

#include <optional>
#include <string>
#include <string_view>

namespace KysChess
{

struct ChessReplayJsonError
{
    std::size_t line{};
    std::string message;
};

std::string serializeChessReplayJsonl(const ChessReplay& replay);
std::optional<ChessReplay> parseChessReplayJsonl(
    std::string_view jsonl,
    ChessReplayJsonError& error);

std::string chessActionTypeId(ChessActionType type);
std::optional<ChessActionType> chessActionTypeFromId(std::string_view id);
std::string serializeChessActionJson(const ChessAction& action);
std::optional<ChessAction> parseChessActionJson(std::string_view json);

}
