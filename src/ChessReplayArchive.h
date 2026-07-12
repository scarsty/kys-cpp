#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace KysChess
{

enum class ChessReplayArchiveError
{
    None,
    OpenFailed,
    WriteFailed,
    MissingAuthoritativeReplay,
    DuplicateAuthoritativeReplay,
    CorruptArchive,
};

class ChessReplayArchive
{
public:
    static ChessReplayArchiveError write(
        const std::filesystem::path& path,
        std::string_view replayJsonl,
        std::string_view summary = {},
        std::string_view diagnostics = {});
    static std::optional<std::string> readReplayJsonl(
        const std::filesystem::path& path,
        ChessReplayArchiveError& error);
};

}
