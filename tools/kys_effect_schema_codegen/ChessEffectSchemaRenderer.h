#pragma once

#include <array>
#include <expected>
#include <string>

namespace KysChess::EffectSchemaCodegen
{

struct RenderedSchemaFile
{
    std::string filename;
    std::string content;
};

using RenderedSchemaFiles = std::array<RenderedSchemaFile, 4>;

std::expected<RenderedSchemaFiles, std::string> renderChessEffectSchemas();

}
