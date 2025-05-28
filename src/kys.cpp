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
        GameUtil::PATH() = argv[1];
    }
    LOG("Game path is {}\n", GameUtil::PATH());
    Application app;
    return app.run();
}
