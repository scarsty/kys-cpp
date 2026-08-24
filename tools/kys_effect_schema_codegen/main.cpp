#include "ChessEffectSchemaRenderer.h"
#include "ChessEffectSchemaWriter.h"

#include <filesystem>
#include <iostream>
#include <string_view>

int main(int argc, char** argv)
{
    if (argc != 3 || std::string_view(argv[1]) != "--output-dir")
    {
        std::cerr << "用法：kys_effect_schema_codegen --output-dir <路徑>\n";
        return 2;
    }

    const auto files = KysChess::EffectSchemaCodegen::renderChessEffectSchemas();
    if (!files)
    {
        std::cerr << "無法產生 chess effect schemas：" << files.error() << '\n';
        return 1;
    }

    const auto written = KysChess::EffectSchemaCodegen::writeChessEffectSchemas(argv[2], *files);
    if (!written)
    {
        std::cerr << written.error() << '\n';
        return 1;
    }
    return 0;
}
