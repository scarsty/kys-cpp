#include "Script.h"
#include "others/libconvert.h"

Script Script::script_;

Script::Script()
{
    lua_state_ = luaL_newstate();

    luaL_openlibs(lua_state_);
    luaopen_base(lua_state_);
    luaopen_table(lua_state_);
    luaopen_math(lua_state_);
    luaopen_string(lua_state_);

    registerEventFunctions();
}

Script::~Script()
{
    lua_close(lua_state_);
}

int Script::runScript(std::string filename)
{
    std::string content = convert::readStringFromFile(filename);
    luaL_loadbuffer(lua_state_, content.c_str(), content.size(), "code");
    return lua_pcall(lua_state_, 0, 0, 0);
}

int Script::registerEventFunctions()
{
    SCRIPT_VOID(additem, addItem, FUNC2);
    return 0;
}

