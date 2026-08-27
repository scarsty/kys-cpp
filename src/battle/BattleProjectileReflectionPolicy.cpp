#include "BattleProjectileReflectionPolicy.h"

namespace KysChess::Battle
{

bool BattleProjectileReflectionPolicy::allows(
    const BattleProjectileReflectionDescriptor& descriptor) const
{
    if (descriptor.reflectionLineage
            == BattleAttackReflectionLineageKind::ReflectedReturn
        || descriptor.delivery.kind() != BattleAttackDeliveryKind::Projectile
        || descriptor.payloadClass.kind() != BattleProjectilePayloadKind::Combat)
    {
        return false;
    }

    return descriptor.operationType == BattleOperationType::RangedProjectile
        || descriptor.operationType == BattleOperationType::TrackingProjectile;
}

}  // namespace KysChess::Battle
