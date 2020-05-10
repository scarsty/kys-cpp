#pragma once
#include <cstddef>
#include <type_traits>

template <typename F, typename C, typename... Args>
struct arg_counter
{
};

template <typename F, typename C, typename... Args>
struct arg_counter<F (C::*)(Args...), C>
{
    static constexpr std::size_t value = sizeof...(Args);
};

template <typename F, typename C, typename T, typename... Args>
struct check_return_type
{
};

template <typename F, typename C, typename T, typename... Args>
struct check_return_type<F (C::*)(Args...), C, T>
{
    static constexpr bool value = std::is_same<F, T>::value;
};
