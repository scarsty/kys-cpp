#pragma once

#include "ChessBattleEffectTypes.h"

namespace KysChess
{

std::optional<int> effectiveConstantEffectNumberValue(const EffectNumber& number);
bool battleAttributeUsesPercentagePoints(BattleAttribute attribute);
bool attributeModifierIsNegative(AttributeOperation operation, int amount);
bool hasOrdinaryAttackModification(const ModifyAttackAction& action);

}  // namespace KysChess
