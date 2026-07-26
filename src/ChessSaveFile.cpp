#include "ChessSaveFile.h"

#include "Utf8Path.h"

#include <glaze/json.hpp>

#include <fstream>

namespace KysChess
{
namespace
{

constexpr auto kWriteOptions = glz::opts{.prettify = true};
constexpr auto kReadOptions = glz::opts{.error_on_unknown_keys = false};

std::expected<std::string, std::string> readTextFile(
    const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return std::unexpected("找不到或無法讀取存檔檔案");
    }
    return std::string(std::istreambuf_iterator<char>(input), {});
}

std::expected<void, std::string> writeTextFile(
    const std::filesystem::path& path,
    std::string_view payload)
{
    std::error_code filesystemError;
    const auto parent = path.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, filesystemError);
        if (filesystemError)
        {
            return std::unexpected("無法建立存檔目錄：" + pathToUtf8(parent));
        }
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!output.good())
    {
        return std::unexpected("無法寫入存檔檔案");
    }
    return {};
}

std::expected<ChessSaveSlotData, std::string> parseChessSaveSlotJson(
    std::string_view payload)
{
    ChessSaveSlotData data;
    if (const auto result = glz::read<kReadOptions>(data, payload); result)
    {
        return std::unexpected(glz::format_error(result, payload));
    }
    return data;
}

}

std::expected<std::string, std::string> serializeChessSaveSlotJson(
    const ChessSaveSlotData& data)
{
    std::string payload;
    if (const auto result = glz::write<kWriteOptions>(data, payload); result)
    {
        return std::unexpected(glz::format_error(result));
    }
    return payload;
}

std::expected<ChessSaveSlotData, std::string> readChessSaveSlotFile(
    const std::filesystem::path& path)
{
    const auto payload = readTextFile(path);
    if (!payload)
    {
        return std::unexpected(payload.error());
    }
    return parseChessSaveSlotJson(*payload);
}

std::expected<void, std::string> writeChessSaveSlotFile(
    const std::filesystem::path& path,
    const ChessSaveSlotData& data)
{
    const auto payload = serializeChessSaveSlotJson(data);
    if (!payload)
    {
        return std::unexpected(payload.error());
    }
    return writeTextFile(path, *payload);
}

std::expected<ChessSessionCheckpoint, std::string> readChessCheckpointFile(
    const std::filesystem::path& path)
{
    const auto payload = readTextFile(path);
    if (!payload)
    {
        return std::unexpected(payload.error());
    }
    ChessCheckpointError checkpointError;
    auto parsed = parseChessSavePayload(*payload, checkpointError);
    if (!parsed)
    {
        return std::unexpected(
            std::string(chessCheckpointErrorDescription(checkpointError)));
    }
    return std::move(parsed->checkpoint);
}

std::expected<void, std::string> writeChessCheckpointFile(
    const std::filesystem::path& path,
    const ChessSessionCheckpoint& checkpoint)
{
    return writeTextFile(path, checkpoint.serializeJson());
}

}
