#pragma once

#include "ChessBattleEffectTypes.h"

#include <span>

namespace KysChess
{

enum class EffectRuleAuthoringContext
{
    Configured,
    StatusBehavior,
    RuntimeIntrinsicStatusBehavior,
};

bool validateEffectRule(
    const EffectRule& rule,
    std::string& error,
    EffectRuleAuthoringContext context = EffectRuleAuthoringContext::Configured);
bool validateEffectRules(std::span<const EffectRule> rules, std::string& error);

}  // namespace KysChess
