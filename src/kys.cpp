#include "Application.h"
#include "ChessContentLoader.h"
#include "Engine.h"
#include "GameUtil.h"

#include "SDL3/SDL_main.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <filesystem>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{

std::string gamePathText(const std::filesystem::path& path)
{
    auto result = path.generic_string();
    if (!result.ends_with('/'))
    {
        result += '/';
    }
    return result;
}

}

int main(int argc, char* argv[])
{
#ifdef __EMSCRIPTEN__
    // Filesystem (lazy files, IDBFS) is set up in shell.html Module.preRun.
    LOG("Filesystem ready (set up in JS preRun)\n");
#endif
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc >= 2)
    {
        GameUtil::PATH() = gamePathText(argv[1]);
    }
#ifdef _WIN32
    else
    {
        const auto roots = KysChess::discoverChessContentRoots(KysChess::currentExecutablePath());
        std::filesystem::current_path(roots.dataRoot.parent_path());
        GameUtil::PATH() = gamePathText(roots.dataRoot.filename());
    }
#endif
    LOG("Game path is {}\n", GameUtil::PATH());
    Application app;
    return app.run();
}
