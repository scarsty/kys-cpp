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

    return result;
}

}  // namespace KysChess::Battle
