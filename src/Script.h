#pragma once
#include "lua/lua.hpp"
#include <string>
#include "Event.h"

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

