#pragma once

#include "battle/BattleEffectSystem.h"

namespace KysChess::Battle::Test
{

inline ApplyStatusEffectCommand statusApplication(const ApplyStatusAction& action)
{
    assert(!action.duration);
    return prepareStatusApplication(action, action.durationFrames, action.behavior);
}

inline RemoveStatusEffectCommand statusRemoval(const RemoveStatusAction& action)
{
    assert(action.source == StatusSourceMatch::Any);
    return prepareStatusRemoval(action, {});
}

// 手寫命令 fixture 必須與 dispatch 輸出一樣持有完整執行輸入。
inline EffectCommand commandFixture(EffectCommand command, EffectExecutionInputs execution)
{
    command.execution = std::move(execution);
    return command;
}

inline std::vector<EffectCommand> commandFixture(
    std::span<const EffectCommand> commands, const EffectExecutionInputs& execution)
{
    std::vector<EffectCommand> result(commands.begin(), commands.end());
    for (auto& command : result) command.execution = execution;
    return result;
}

}  // namespace KysChess::Battle::Test
