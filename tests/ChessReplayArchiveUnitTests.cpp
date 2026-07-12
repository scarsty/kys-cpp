#include "ChessReplayArchive.h"

#include <catch2/catch_test_macros.hpp>
#include <zip.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <format>
#include <string_view>
#include <vector>

using namespace KysChess;

namespace
{

std::filesystem::path tempArchive(std::string_view suffix)
{
    return std::filesystem::temp_directory_path()
        / std::format(
            "kys_replay_{}_{}.kysreplay",
            std::chrono::steady_clock::now().time_since_epoch().count(),
            suffix);
}

std::uint32_t crc32(std::string_view text)
{
    std::uint32_t crc = 0xffffffffU;
    for (const unsigned char byte : text)
    {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

void append16(std::vector<char>& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<char>(value));
    bytes.push_back(static_cast<char>(value >> 8));
}

void append32(std::vector<char>& bytes, std::uint32_t value)
{
    append16(bytes, static_cast<std::uint16_t>(value));
    append16(bytes, static_cast<std::uint16_t>(value >> 16));
}

void appendText(std::vector<char>& bytes, std::string_view text)
{
    bytes.insert(bytes.end(), text.begin(), text.end());
}

void writeDuplicateReplayArchive(const std::filesystem::path& path)
{
    constexpr std::string_view name = "replay.jsonl";
    constexpr std::string_view content = "authoritative\n";
    std::vector<char> bytes;
    std::vector<std::uint32_t> offsets;
    for (int index = 0; index < 2; ++index)
    {
        offsets.push_back(static_cast<std::uint32_t>(bytes.size()));
        append32(bytes, 0x04034b50U);
        append16(bytes, 20);
        append16(bytes, 0);
        append16(bytes, 0);
        append16(bytes, 0);
        append16(bytes, 0);
        append32(bytes, crc32(content));
        append32(bytes, static_cast<std::uint32_t>(content.size()));
        append32(bytes, static_cast<std::uint32_t>(content.size()));
        append16(bytes, static_cast<std::uint16_t>(name.size()));
        append16(bytes, 0);
        appendText(bytes, name);
        appendText(bytes, content);
    }
    const auto centralOffset = static_cast<std::uint32_t>(bytes.size());
    for (const auto offset : offsets)
    {
        append32(bytes, 0x02014b50U);
        append16(bytes, 20);
        append16(bytes, 20);
        append16(bytes, 0);
        append16(bytes, 0);
        append16(bytes, 0);
        append16(bytes, 0);
        append32(bytes, crc32(content));
        append32(bytes, static_cast<std::uint32_t>(content.size()));
        append32(bytes, static_cast<std::uint32_t>(content.size()));
        append16(bytes, static_cast<std::uint16_t>(name.size()));
        append16(bytes, 0);
        append16(bytes, 0);
        append16(bytes, 0);
        append16(bytes, 0);
        append32(bytes, 0);
        append32(bytes, offset);
        appendText(bytes, name);
    }
    const auto centralSize = static_cast<std::uint32_t>(bytes.size()) - centralOffset;
    append32(bytes, 0x06054b50U);
    append16(bytes, 0);
    append16(bytes, 0);
    append16(bytes, 2);
    append16(bytes, 2);
    append32(bytes, centralSize);
    append32(bytes, centralOffset);
    append16(bytes, 0);

    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

}

TEST_CASE("replay archive round-trips authoritative JSONL", "[chess][archive][replay]")
{
    const auto path = tempArchive("roundtrip");
    const std::string replay = "{\"type\":\"header\"}\n{\"type\":\"footer\"}\n";
    REQUIRE(ChessReplayArchive::write(path, replay, "摘要", "診斷") == ChessReplayArchiveError::None);
    ChessReplayArchiveError error;
    const auto loaded = ChessReplayArchive::readReplayJsonl(path, error);
    std::filesystem::remove(path);

    REQUIRE(loaded);
    CHECK(error == ChessReplayArchiveError::None);
    CHECK(*loaded == replay);
}

TEST_CASE("diagnostic transport never changes authoritative replay", "[chess][archive][replay]")
{
    const auto firstPath = tempArchive("first");
    const auto secondPath = tempArchive("second");
    const std::string replay = "authoritative\n";
    REQUIRE(ChessReplayArchive::write(firstPath, replay, {}, "甲") == ChessReplayArchiveError::None);
    REQUIRE(ChessReplayArchive::write(secondPath, replay, "不同摘要", "乙") == ChessReplayArchiveError::None);
    ChessReplayArchiveError firstError;
    ChessReplayArchiveError secondError;
    const auto first = ChessReplayArchive::readReplayJsonl(firstPath, firstError);
    const auto second = ChessReplayArchive::readReplayJsonl(secondPath, secondError);
    std::filesystem::remove(firstPath);
    std::filesystem::remove(secondPath);

    REQUIRE(first);
    REQUIRE(second);
    CHECK(*first == *second);
}

TEST_CASE("missing and corrupt replay archives reject precisely", "[chess][archive][replay]")
{
    const auto missingPath = tempArchive("missing");
    int zipError{};
    auto* archive = zip_open(missingPath.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &zipError);
    REQUIRE(archive);
    const std::string summary = "only summary";
    auto* source = zip_source_buffer(archive, summary.data(), summary.size(), 0);
    REQUIRE(source);
    REQUIRE(zip_file_add(archive, "summary.txt", source, 0) >= 0);
    REQUIRE(zip_close(archive) == 0);
    ChessReplayArchiveError error;
    CHECK_FALSE(ChessReplayArchive::readReplayJsonl(missingPath, error));
    CHECK(error == ChessReplayArchiveError::MissingAuthoritativeReplay);
    std::filesystem::remove(missingPath);

    const auto corruptPath = tempArchive("corrupt");
    {
        std::ofstream output(corruptPath, std::ios::binary);
        output << "not a zip";
    }
    CHECK_FALSE(ChessReplayArchive::readReplayJsonl(corruptPath, error));
    CHECK(error == ChessReplayArchiveError::OpenFailed);
    std::filesystem::remove(corruptPath);
}

TEST_CASE("duplicate authoritative replay archive entry is rejected", "[chess][archive][replay]")
{
    const auto path = tempArchive("duplicate");
    writeDuplicateReplayArchive(path);
    ChessReplayArchiveError error;
    CHECK_FALSE(ChessReplayArchive::readReplayJsonl(path, error));
    CHECK(error == ChessReplayArchiveError::DuplicateAuthoritativeReplay);
    std::filesystem::remove(path);
}
