#include <catch2/catch_session.hpp>

#include "ConsoleErrorReporting.h"

int main(int argc, char* argv[])
{
    configureConsoleErrorReporting();
    return Catch::Session().run(argc, argv);
}
