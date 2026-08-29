#include "ChessBattleEffectSemantics.h"

#include <algorithm>
#include <cassert>

namespace KysChess
{
std::optional<int> effectiveConstantEffectNumberValue(const EffectNumber& number)
{
    if (number.base != EffectNumberBase::Constant || number.multiplierBase)
    {
        return std::nullopt;
    }

    int value = number.flat;
    if (number.minimum)
    {
        value = std::max(value, *number.minimum);
    }
    if (number.maximum)
    {
        value = std::min(value, *number.maximum);
    }
    return value;
}

bool battleAttributeUsesPercentagePoints(BattleAttribute attribute)
{
    switch (attribute)
    {
    case BattleAttribute::MaxHp:
    case BattleAttribute::Attack:
    case BattleAttribute::Defence:
    case BattleAttribute::Speed:
    case BattleAttribute::ProjectilePressureDamage:
        return false;
    case BattleAttribute::CriticalChance:
    case BattleAttribute::CriticalDamage:
    case BattleAttribute::DodgeChance:
    case BattleAttribute::BlockChance:
    case BattleAttribute::DamageReduction:
    case BattleAttribute::SkillDamage:
    case BattleAttribute::CooldownReduction:
    case BattleAttribute::MpRecoveryBonus:
    case BattleAttribute::StaggerResistance:
    case BattleAttribute::ProjectileReflectChance:
    case BattleAttribute::SkillReflectPercent:
    case BattleAttribute::CounterUltimateBlockChance:
    case BattleAttribute::CriticalAfterDodge:
    case BattleAttribute::DashChance:
    case BattleAttribute::OutgoingCooldownExtensionChance:
    case BattleAttribute::OutgoingCooldownExtensionPercent:
    case BattleAttribute::IncomingCooldownExtensionChance:
    case BattleAttribute::IncomingCooldownExtensionPercent:
        return true;
    }
    assert(false);
    return false;
}

bool attributeModifierIsNegative(AttributeOperation operation, int amount)
{
    switch (operation)
    {
    case AttributeOperation::FlatAdd:
    case AttributeOperation::PercentAdd:
    case AttributeOperation::Override:
        return amount < 0;
    case AttributeOperation::Multiply:
        return amount < 100;
    case AttributeOperation::AtLeast:
        return false;
    }
    assert(false);
    return false;
}

bool hasOrdinaryAttackModification(const ModifyAttackAction& action)
{
    auto ordinary = action;
    ordinary.runtimeBehavior = {};
    return ordinary != ModifyAttackAction{};
}

}  // namespace KysChess
