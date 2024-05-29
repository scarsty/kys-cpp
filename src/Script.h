#pragma once
#include "FunctionTrait.h"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <string>
#include <array>
#include <type_traits>

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

    int runScript(const std::string& filename);

    int registerEventFunctions();

    template <typename F, typename C, std::size_t... I, std::size_t N>
    static auto runner_impl(F f, C* c, const std::array<int, N>& e, std::index_sequence<I...>)
    {
        return (c->*f)(e[I]...);
    }

    template <typename F, typename C, std::size_t N>
    static typename std::enable_if<check_return_type<F, C, void>::value, int>::type
    runner(F f, C* c, const std::array<int, N>& e, lua_State* L)
    {
        runner_impl(f, c, e, std::make_index_sequence<arg_counter<F, C>::value> {});
        return 0;
    }

    template <typename F, typename C, std::size_t N>
    static typename std::enable_if<check_return_type<F, C, bool>::value, int>::type
    runner(F f, C* c, const std::array<int, N>& e, lua_State* L)
    {
        lua_pushboolean(L, runner_impl(f, c, e, std::make_index_sequence<arg_counter<F, C>::value> {}));
        return 1;
    }
};
