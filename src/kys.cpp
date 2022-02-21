#include "Application.h"
#include "Engine.h"
#include "GameUtil.h"

int main(int argc, char* argv[])
{
#ifdef _WIN32
    system("chcp 65001");
#endif
    if (argc >= 2)
    {
        GameUtil::PATH() = argv[1];
    }
    fmt1::print("Game path is {}\n", GameUtil::PATH());
    Application app;
    return app.run();
}
