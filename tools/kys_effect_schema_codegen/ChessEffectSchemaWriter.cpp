#include "ChessEffectSchemaWriter.h"

#include <fstream>
#include <iterator>
#include <string_view>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace KysChess::EffectSchemaCodegen
{
namespace
{

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    return std::string(std::istreambuf_iterator<char>(input), {});
}

bool replaceFile(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination,
    std::error_code& error)
{
#ifdef _WIN32
    if (MoveFileExW(
            temporary.c_str(),
            destination.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
    return false;
#else
    std::filesystem::rename(temporary, destination, error);
    return !error;
#endif
}

std::expected<bool, std::string> writeIfChanged(
    const std::filesystem::path& destination,
    std::string_view content)
{
    std::error_code error;
    if (std::filesystem::is_regular_file(destination, error)
        && !error
        && readFile(destination) == content) return false;

    const auto temporary = destination.string() + ".tmp";
    std::filesystem::remove(temporary, error);
    error.clear();
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!output.good())
            return std::unexpected("無法寫入 schema 暫存檔：" + temporary);
    }
    if (!replaceFile(temporary, destination, error))
    {
        const auto replacementError = error.message();
        std::error_code cleanupError;
        std::filesystem::remove(temporary, cleanupError);
        return std::unexpected(
            "無法更新 schema：" + destination.string() + "：" + replacementError);
    }
    return true;
}

}

std::expected<SchemaWriteResult, std::string> writeChessEffectSchemas(
    const std::filesystem::path& outputDirectory,
    const RenderedSchemaFiles& files)
{
    std::error_code error;
    std::filesystem::create_directories(outputDirectory, error);
    if (error)
    {
        return std::unexpected(
            "無法建立 schema 輸出目錄：" + outputDirectory.string()
            + "：" + error.message());
    }

    SchemaWriteResult result;
    for (const auto& file : files)
    {
        const auto written = writeIfChanged(outputDirectory / file.filename, file.content);
        if (!written) return std::unexpected(written.error());
        if (*written) ++result.changedFiles;
    }
    return result;
}

}
