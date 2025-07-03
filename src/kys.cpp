#include "Application.h"
#include "Engine.h"
#include "GameUtil.h"

#include "SDL3/SDL_main.h"

int main(int argc, char* argv[])
{
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
