#pragma once

#include "BattleOperation.h"

#include <compare>

namespace KysChess::Battle
{

enum class BattleAttackOriginKind;

enum class BattleAttackCastSubrequestKind
{
    None,
    SkillHit,
    DashHit,
    DashFollowUpSkill,
    MeleeSplash,
    ExtraProjectile,
    DualWieldFollowUp,
};

enum class BattleAttackDeliveryKind
{
    Contact,
    Projectile,
    Effect,
};

enum class BattleProjectilePayloadKind
{
    Combat,
    ScriptedDamage,
    ScriptedControl,
};

enum class BattleAttackReflectionLineageKind
{
    Ordinary,
    ReflectedReturn,
};

class BattleAttackDelivery
{
public:
    BattleAttackDelivery() = delete;

    static constexpr BattleAttackDelivery contact()
    {
        return BattleAttackDelivery(BattleAttackDeliveryKind::Contact);
    }

    static constexpr BattleAttackDelivery projectile()
    {
        return BattleAttackDelivery(BattleAttackDeliveryKind::Projectile);
    }

    static constexpr BattleAttackDelivery effect()
    {
        return BattleAttackDelivery(BattleAttackDeliveryKind::Effect);
    }

    constexpr BattleAttackDeliveryKind kind() const { return kind_; }
    auto operator<=>(const BattleAttackDelivery&) const = default;

private:
    constexpr explicit BattleAttackDelivery(BattleAttackDeliveryKind kind)
        : kind_(kind)
    {
    }

    BattleAttackDeliveryKind kind_;
};

class BattleProjectilePayloadClass
{
public:
    BattleProjectilePayloadClass() = delete;

    static constexpr BattleProjectilePayloadClass combat()
    {
        return BattleProjectilePayloadClass(BattleProjectilePayloadKind::Combat);
    }

    static constexpr BattleProjectilePayloadClass scriptedDamage()
    {
        return BattleProjectilePayloadClass(BattleProjectilePayloadKind::ScriptedDamage);
    }

    static constexpr BattleProjectilePayloadClass scriptedControl()
    {
        return BattleProjectilePayloadClass(BattleProjectilePayloadKind::ScriptedControl);
    }

    constexpr BattleProjectilePayloadKind kind() const { return kind_; }
    auto operator<=>(const BattleProjectilePayloadClass&) const = default;

private:
    constexpr explicit BattleProjectilePayloadClass(BattleProjectilePayloadKind kind)
        : kind_(kind)
    {
    }

    BattleProjectilePayloadKind kind_;
};

struct BattleProjectileReflectionDescriptor
{
    BattleAttackDelivery delivery;
    BattleProjectilePayloadClass payloadClass;
    BattleOperationType operationType;
    BattleAttackCastSubrequestKind subrequestKind;
    BattleAttackOriginKind origin;
    BattleAttackReflectionLineageKind reflectionLineage;

    BattleProjectileReflectionDescriptor(
        BattleAttackDelivery delivery,
        BattleProjectilePayloadClass payloadClass,
        BattleOperationType operationType,
        BattleAttackCastSubrequestKind subrequestKind,
        BattleAttackOriginKind origin,
        BattleAttackReflectionLineageKind reflectionLineage)
        : delivery(delivery)
        , payloadClass(payloadClass)
        , operationType(operationType)
        , subrequestKind(subrequestKind)
        , origin(origin)
        , reflectionLineage(reflectionLineage)
    {
    }
};

}  // namespace KysChess::Battle
