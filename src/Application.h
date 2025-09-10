#pragma once
#include <string>

class Application
{
public:
    Application();
    virtual ~Application();
    int run();
    void config();

    std::string renderer_, title_;
};
