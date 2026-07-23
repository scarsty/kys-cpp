#pragma once

#include "ChessDiagnostics.h"
#include "ChessGameContent.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace KysChess
{

struct ChessContentLoadOptions
{
    std::filesystem::path dataRoot;
    std::filesystem::path configRoot;
    Difficulty difficulty = Difficulty::Easy;
    ChessDiagnosticSink diagnostics;
};

struct ChessContentRoots
{
    std::filesystem::path dataRoot;
    std::filesystem::path configRoot;
};

class ChessContentLoader
{
public:
    static std::optional<ChessGameContent> load(const ChessContentLoadOptions& options);
};

bool loadChessPoolRoleIds(
    const std::filesystem::path& path,
    std::vector<int>& roleIds,
    const ChessDiagnosticSink& diagnostics = {});

ChessContentRoots chessContentRootsForDataRoot(const std::filesystem::path& dataRoot);
ChessContentRoots discoverChessContentRoots(const std::filesystem::path& executablePath);

#ifdef _WIN32
std::filesystem::path currentExecutablePath();
#endif

}
