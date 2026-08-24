#pragma once

#include "ChessEffectSchemaRenderer.h"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>

namespace KysChess::EffectSchemaCodegen
{

struct SchemaWriteResult
{
    std::size_t changedFiles{};
};

std::expected<SchemaWriteResult, std::string> writeChessEffectSchemas(
    const std::filesystem::path& outputDirectory,
    const RenderedSchemaFiles& files);

}
