#include "Application.h"
#include "Engine.h"

int main(int argc, char* argv[])
{
#ifdef _WIN32
    system("chcp 65001");
#endif
    Application app;
    return app.run();
}
