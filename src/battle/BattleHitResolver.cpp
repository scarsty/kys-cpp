#include "BattleHitResolver.h"

#include <cassert>

namespace KysChess::Battle
{

BattleHitResolutionResult BattleHitResolver::resolve(const BattleHitResolutionInput& input) const
{
    assert(input.attacker.id >= 0);
    assert(input.defender.id >= 0);

    BattleHitResolutionResult result;
    result.attackerCombo = input.attackerCombo;
    result.defenderCombo = input.defenderCombo;

    if (input.attackEvent.type != BattleAttackEventType::Hit)
    {
        return result;
    }

    const bool usingHiddenWeapon = input.item.id >= 0;
    const bool usingSkill = !usingHiddenWeapon && input.skill.id >= 0;
    const double baseDamage = usingHiddenWeapon
        ? input.item.resolvedDamage / 5.0
        : static_cast<double>(input.skill.resolvedBaseDamage);

    BattleLegacyHitShapeInput shapeInput;
    shapeInput.baseDamage = baseDamage;
    shapeInput.projectileCancelDamage = input.attackEvent.projectileCancelDamage;
    shapeInput.strengthMultiplier = input.attackEvent.strengthMultiplier;
    shapeInput.frame = input.attackEvent.frame;
    shapeInput.totalFrame = input.attackEvent.totalFrame;
    shapeInput.impactPosition = input.attackEvent.position;
    shapeInput.defenderPosition = input.defender.position;
    shapeInput.defenderFacing = input.defender.facing;
    shapeInput.operationType = usingHiddenWeapon ? BattleOperationType::None : input.attackEvent.operationType;
    shapeInput.usingSkill = usingSkill;
    shapeInput.attackerActProperty = input.skill.attackerActProperty;
    shapeInput.defenderActProperty = input.skill.defenderActProperty;

    const auto shaped = BattleDamageSystem().shapeLegacyHitDamage(shapeInput);
    result.shapedHpDamage = shaped.damage;
    result.finalHpDamage = static_cast<int>(shaped.damage);

    if (shaped.frozenFrames > 0)
    {
        BattleDamageRequest request;
        request.attackerUnitId = input.attacker.id;
        request.defenderUnitId = input.defender.id;
        request.acceptedHit = true;
        request.frozenFrames = shaped.frozenFrames;
        result.commands.push_back(BattleAcceptedHitSideEffectCommand{
            input.attacker.id,
            input.defender.id,
            request,
        });
    }

    auto velocityDelta = input.defender.position - input.attacker.position;
    velocityDelta.normTo(static_cast<float>(shaped.knockbackStrength));
    result.commands.push_back(BattleKnockbackCommand{
        input.defender.id,
        velocityDelta,
        shaped.knockbackVelocityCap,
        shaped.grantsHurtFrame,
    });

    return result;
}

}  // namespace KysChess::Battle
