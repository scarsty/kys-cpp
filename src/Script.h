#pragma once
#include "Event.h"
#ifdef _WIN32
#include "lua/lua.hpp"
#else
#include "lua5.3/lua.hpp"
#endif
#include <string>

class Script
{
private:
    static Script script_;

public:
    Script();
    ~Script();

    lua_State* lua_state_ = nullptr;

    static Script* getInstance() { return &script_; }

    int runScript(std::string filename);

    int registerEventFunctions();
};
