#pragma once

#include "BattleOutcome.h"
#include "BattlePresentation.h"

#include <cstdint>
#include <vector>

namespace KysChess::Battle
{

struct BattleDigestEvent
{
    BattleGameplayEventType type{};
    int frame{};
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int amount{};
    int stableEffectId = -1;
    int skillId = -1;
    BattleStatusSemanticId statusId = BattleStatusSemanticId::None;
    BattleResourceSemanticId resourceId = BattleResourceSemanticId::None;
    int relatedAttackId = -1;

    auto operator<=>(const BattleDigestEvent&) const = default;
};

inline std::vector<BattleDigestEvent> battleDigestEvents(const BattlePresentationFrame& frame)
{
    std::vector<BattleDigestEvent> result;
    for (const auto& event : frame.gameplayEvents)
    {
        if (event.type == BattleGameplayEventType::ProjectileMoved)
        {
            continue;
        }
        result.push_back({
            event.type,
            event.frame,
            event.sourceUnitId,
            event.targetUnitId,
            event.amount,
            event.effectId,
            event.skillId,
            event.statusId,
            event.resourceId,
            event.otherAttackId,
        });
    }
    return result;
}

}
