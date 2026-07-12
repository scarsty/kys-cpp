#include "ChessReplayArchive.h"

#include <zip.h>

#include <cstdint>
#include <vector>

namespace KysChess
{
namespace
{

bool addText(zip_t* archive, const char* name, std::string_view text)
{
    auto* source = zip_source_buffer(archive, text.data(), text.size(), 0);
    if (!source)
    {
        return false;
    }
    if (zip_file_add(archive, name, source, ZIP_FL_ENC_UTF_8 | ZIP_FL_OVERWRITE) < 0)
    {
        zip_source_free(source);
        return false;
    }
    return true;
}

}

ChessReplayArchiveError ChessReplayArchive::write(
    const std::filesystem::path& path,
    std::string_view replayJsonl,
    std::string_view summary,
    std::string_view diagnostics)
{
    int errorCode{};
    auto* archive = zip_open(path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errorCode);
    if (!archive)
    {
        return ChessReplayArchiveError::OpenFailed;
    }
    bool success = addText(archive, "replay.jsonl", replayJsonl);
    if (success && !summary.empty())
    {
        success = addText(archive, "summary.txt", summary);
    }
    if (success && !diagnostics.empty())
    {
        success = addText(archive, "diagnostics.txt", diagnostics);
    }
    if (!success)
    {
        zip_discard(archive);
        return ChessReplayArchiveError::WriteFailed;
    }
    return zip_close(archive) == 0
        ? ChessReplayArchiveError::None
        : ChessReplayArchiveError::WriteFailed;
}

std::optional<std::string> ChessReplayArchive::readReplayJsonl(
    const std::filesystem::path& path,
    ChessReplayArchiveError& error)
{
    int errorCode{};
    auto* archive = zip_open(path.string().c_str(), ZIP_RDONLY, &errorCode);
    if (!archive)
    {
        error = ChessReplayArchiveError::OpenFailed;
        return std::nullopt;
    }
    zip_int64_t authoritativeIndex = -1;
    int authoritativeCount = 0;
    const auto entries = zip_get_num_entries(archive, 0);
    for (zip_int64_t index = 0; index < entries; ++index)
    {
        const char* name = zip_get_name(archive, index, ZIP_FL_ENC_GUESS);
        if (name && std::string_view(name) == "replay.jsonl")
        {
            authoritativeIndex = index;
            ++authoritativeCount;
        }
    }
    if (authoritativeCount != 1)
    {
        zip_close(archive);
        error = authoritativeCount == 0
            ? ChessReplayArchiveError::MissingAuthoritativeReplay
            : ChessReplayArchiveError::DuplicateAuthoritativeReplay;
        return std::nullopt;
    }
    zip_stat_t stat;
    if (zip_stat_index(archive, authoritativeIndex, 0, &stat) != 0)
    {
        zip_close(archive);
        error = ChessReplayArchiveError::CorruptArchive;
        return std::nullopt;
    }
    auto* file = zip_fopen_index(archive, authoritativeIndex, 0);
    if (!file)
    {
        zip_close(archive);
        error = ChessReplayArchiveError::CorruptArchive;
        return std::nullopt;
    }
    std::string result(static_cast<std::size_t>(stat.size), '\0');
    const auto read = zip_fread(file, result.data(), stat.size);
    zip_fclose(file);
    zip_close(archive);
    if (read != static_cast<zip_int64_t>(stat.size))
    {
        error = ChessReplayArchiveError::CorruptArchive;
        return std::nullopt;
    }
    error = ChessReplayArchiveError::None;
    return result;
}

}
