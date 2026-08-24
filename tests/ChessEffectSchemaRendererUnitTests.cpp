#include "ChessEffectSchemaRenderer.h"
#include "ChessEffectSchemaWriter.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <format>
#include <vector>

using namespace KysChess::EffectSchemaCodegen;

namespace
{

class TemporarySchemaDirectory
{
public:
    TemporarySchemaDirectory()
        : path_(std::filesystem::temp_directory_path() / std::format(
            "kys_effect_schema_codegen_{}",
            std::chrono::steady_clock::now().time_since_epoch().count()))
    {
    }

    ~TemporarySchemaDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

}

TEST_CASE("ChessEffectSchemaRenderer_IdenticalGenerationPreservesOutputTimestamps",
          "[chess][effects][schema][codegen]")
{
    const auto files = renderChessEffectSchemas();
    REQUIRE(files.has_value());
    TemporarySchemaDirectory output;

    const auto first = writeChessEffectSchemas(output.path(), *files);
    REQUIRE(first.has_value());
    CHECK(first->changedFiles == files->size());

    std::vector<std::filesystem::file_time_type> timestamps;
    for (const auto& file : *files)
        timestamps.push_back(std::filesystem::last_write_time(output.path() / file.filename));

    const auto second = writeChessEffectSchemas(output.path(), *files);
    REQUIRE(second.has_value());
    CHECK(second->changedFiles == 0);
    for (std::size_t index = 0; index < files->size(); ++index)
    {
        CHECK(std::filesystem::last_write_time(
            output.path() / (*files)[index].filename) == timestamps[index]);
    }
}
