#include "Application.h"
#include "Engine.h"
#include "GameUtil.h"

#include "SDL3/SDL_main.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/wasmfs.h>

// Both fetch and OPFS backends require a worker thread (not the main browser
// thread). With PROXY_TO_PTHREAD, main() runs on a worker, so mount here.
static void mount_wasmfs_backends()
{
    backend_t fetch = wasmfs_create_fetch_backend("game", 0);
    wasmfs_create_directory("/game", 0777, fetch);

    // Pre-register all game asset directories and files so the fetch backend
    // knows they exist and will lazily fetch them over HTTP when accessed.
#include "../wasm/wasm_manifest.inc"

    backend_t opfs = wasmfs_create_opfs_backend();
    wasmfs_create_directory("/persist", 0777, opfs);
}
#endif

int main(int argc, char* argv[])
{
#ifdef __EMSCRIPTEN__
    mount_wasmfs_backends();
#endif
#ifdef _WIN32
    system("chcp 65001");
#endif
    if (argc >= 2)
    {
        std::string path = argv[1];
        if (path.back() != '/' && path.back() != '\\')
        {
            path += '/';
        }
        GameUtil::PATH() = path;
    }
    LOG("Game path is {}\n", GameUtil::PATH());
    Application app;
    return app.run();
}
