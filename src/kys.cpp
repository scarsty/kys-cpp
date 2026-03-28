#include "Application.h"
#include "Engine.h"
#include "GameUtil.h"

#include "SDL3/SDL_main.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/wasmfs.h>
#include <sstream>
#include <string>
#include <cerrno>
#include <cstring>


// Reads the OPFS test result from the main thread (where the page JS ran).
int is_opfs_available() {
    return MAIN_THREAD_EM_ASM_INT({ return opfsReady ? 1 : 0; });
}

#include "../wasm/wasm_manifest.inc"

static void mount_wasmfs_backends()
{
    // backend_t fetch = wasmfs_create_fetch_backend("kys/game", 65536 * 2);
    backend_t fetch = wasmfs_create_fetch_backend("kys/game", 0);
    int fd = wasmfs_create_directory("/game", 0777, fetch);
    if (fd < 0)
    {
        LOG("FATAL: wasmfs_create_directory /game failed: {} ({})\n", std::strerror(errno), errno);
        return;
    }

    std::istringstream manifest(wasm_manifest_data);
    int dirs_ok = 0, dirs_fail = 0, files_ok = 0, files_fail = 0;
    std::string line;
    while (std::getline(manifest, line))
    {
        if (line.size() < 3) { continue; }
        char type = line[0];
        std::string path = line.substr(2);

        if (type == 'D')
        {
            fd = wasmfs_create_directory(path.c_str(), 0777, fetch);
            if (fd < 0)
            {
                LOG("manifest: mkdir FAILED {}: {} ({})\n", path, std::strerror(errno), errno);
                dirs_fail++;
            }
            else
            {
                dirs_ok++;
            }
        }
        else if (type == 'F')
        {
            fd = wasmfs_create_file(path.c_str(), 0666, fetch);
            if (fd < 0)
            {
                LOG("manifest: mkfile FAILED {}: {} ({})\n", path, std::strerror(errno), errno);
                files_fail++;
            }
            else
            {
                files_ok++;
            }
        }
    }
    LOG("manifest: {} dirs ok, {} dirs failed, {} files ok, {} files failed\n",
        dirs_ok, dirs_fail, files_ok, files_fail);

    backend_t persist_backend;
    if (is_opfs_available())
    {
        persist_backend = wasmfs_create_opfs_backend();
        LOG("Using OPFS for /persist\n");
    }
    else
    {
        LOG("OPFS not available, using memory backend\n");
        persist_backend = wasmfs_create_memory_backend();
    }
    if (wasmfs_create_directory("/persist", 0777, persist_backend) < 0)
    {
        LOG("FATAL: failed to create /persist: {} ({})\n", std::strerror(errno), errno);
    }
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
