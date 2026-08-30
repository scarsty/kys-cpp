#pragma once

#include "ChessBattleEffectTypes.h"

#include <span>

namespace KysChess
{

bool validateEffectRule(const EffectRule& rule, std::string& error);
bool validateEffectRules(std::span<const EffectRule> rules, std::string& error);

}  // namespace KysChess
