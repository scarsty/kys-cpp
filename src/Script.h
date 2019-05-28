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

    template <typename F, typename C, typename... Args>
    static constexpr auto arg_counter(F(C::*)(Args...))
    {
        return std::integral_constant<std::size_t, sizeof...(Args)> {};
    }

    template <class F, typename C, typename T, typename... Args>
    struct check_return_type { };

    template <typename F, typename C, typename T, typename... Args>
    struct check_return_type<F(C::*)(Args...), C, T>
    {
        static constexpr bool value = std::is_same<F, T>::value;
    };

    template <typename F, typename C, int... I, std::size_t N>
    static auto runner_impl(F f, C* c, const std::array<int, N>& e, std::index_sequence<I...>)
    {
        return (c->*f)(e[I]...);
    }

    template <typename F, typename C, std::size_t N>
    static typename std::enable_if<check_return_type<F, C, void>::value, int>::type
    runner(F f, C* c, const std::array<int, N>& e, lua_State* L)
    {
        runner_impl(f, c, e, std::make_index_sequence<decltype(arg_counter(f))::value> {});
        return 0;
    }

    template <typename F, typename C, std::size_t N>
    static typename std::enable_if<check_return_type<F, C, bool>::value, int>::type
    runner(F f, C* c, const std::array<int, N>& e, lua_State* L)
    {
        lua_pushboolean(L, runner_impl(f, c, e, std::make_index_sequence<decltype(arg_counter(f))::value> {}));
        return 1;
    }
};
