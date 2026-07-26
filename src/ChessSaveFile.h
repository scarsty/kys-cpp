#pragma once

#include "ChessSessionCheckpoint.h"

#include <expected>
#include <filesystem>

namespace KysChess
{

std::expected<std::string, std::string> serializeChessSaveSlotJson(
    const ChessSaveSlotData& data);
std::expected<ChessSaveSlotData, std::string> readChessSaveSlotFile(
    const std::filesystem::path& path);
std::expected<void, std::string> writeChessSaveSlotFile(
    const std::filesystem::path& path,
    const ChessSaveSlotData& data);
std::expected<ChessSessionCheckpoint, std::string> readChessCheckpointFile(
    const std::filesystem::path& path);
std::expected<void, std::string> writeChessCheckpointFile(
    const std::filesystem::path& path,
    const ChessSessionCheckpoint& checkpoint);

}
