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
private:
    static int _i(lua_State* L, int i) { return lua_tonumber(L, i); }  //¼ò»¯´úÂëÓÃ



#define FUNC0(function) function()
#define FUNC1(function) function(_i(L, 1))
#define FUNC2(function) function(_i(L, 1), _i(L, 2))

#define SCRIPT_VOID(name, function, FUNC) \
    { auto name = [](lua_State* L)->int { \
    Event::getInstance()->FUNC(function); \
    return 0; }; lua_register(lua_state_, #name, name); }

};

