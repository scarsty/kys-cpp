#pragma once

// Attack-event to presentation/log conversion and projectile stop-log coalescing.
// Cancel-damage resolution intentionally lives with the attack pipeline; this
// module only converts settled attack events into display data.

#include "BattleAttackSystem.h"
#include "BattlePresentation.h"

#include <span>
#include <vector>

namespace KysChess::Battle
{

// Converts one attack event into its visual events (a bounce emits the bounce
// followed by the spawned-projectile event). Only the current-frame sentinel is
// replaced; already-resolved nonnegative frames are preserved.
void appendVisualEvents(
    const BattleAttackEvent& event,
    const BattleAttackState& attacks,
    int presentationFrame,
    std::vector<BattleVisualEvent>& output);

BattleGameplayEvent toGameplayEvent(
    const BattleAttackEvent& event,
    const BattleAttackState& world);

void appendProjectileCancellationLogEvents(
    const BattleAttackState& world,
    std::span<const BattleAttackEvent> events,
    std::vector<BattleLogEvent>& logEvents,
    bool chainedProjectileLogs);

}  // namespace KysChess::Battle
