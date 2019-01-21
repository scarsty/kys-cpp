#pragma once
#include "Event.h"
#ifdef _WIN32
#include "lua.hpp"
#else
#include "lua5.3/lua.hpp"
#endif
#include <string>

class Script
{
public:
    Script();
    ~Script();

    lua_State* lua_state_ = nullptr;

    static Script* getInstance()
    {
        static Script s;
        return &s;
    }

    int runScript(std::string filename);

    int registerEventFunctions();
};
