#include "ChessEffectSchemaRenderer.h"
#include "ChessEffectSchemaWriter.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

#ifdef _WIN32
#include <crtdbg.h>
#include <windows.h>
#endif

namespace
{

void configureErrorReporting()
{
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(_WRITE_ABORT_MSG, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
}

}  // namespace

int main(int argc, char** argv)
{
    configureErrorReporting();
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
