#pragma once
#include "FunctionTrait.h"
#include "Cifa.h"
#include <array>
#include <string>
#include <type_traits>

struct ScriptExitException
{
};

class ScriptCifa
{
public:
    ScriptCifa();
    ~ScriptCifa() = default;

    cifa::Cifa cifa_;

    static ScriptCifa* getInstance()
    {
        static ScriptCifa s;
        return &s;
    }

    int runScript(const std::string& filename);

    int runScriptString(const std::string& content);

    int registerEventFunctions();

    template <typename F, typename C, std::size_t... I>
    static auto runner_impl(F f, C* c, const cifa::ObjectVector& args, std::index_sequence<I...>)
    {
        return (c->*f)((I < args.size() ? int(args[I]) : -1)...);
    }

    template <typename F, typename C>
        requires check_return_type<F, C, void>::value
    static cifa::Object runner(F f, C* c, const cifa::ObjectVector& args)
    {
        runner_impl(f, c, args, std::make_index_sequence<arg_counter<F, C>::value>{});
        return {};
    }

    template <typename F, typename C>
        requires check_return_type<F, C, bool>::value
    static cifa::Object runner(F f, C* c, const cifa::ObjectVector& args)
    {
        return cifa::Object(runner_impl(f, c, args, std::make_index_sequence<arg_counter<F, C>::value>{}));
    }
};
