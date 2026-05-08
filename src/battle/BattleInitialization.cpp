#include "BattleInitialization.h"

namespace KysChess::Battle
{

BattleInitializationResult BattleInitializationSystem::initialize(BattleRuntimeState& runtime,
                                                                  const BattleRuntimeSetupSeed& setup) const
{
    (void)runtime;
    (void)setup;
    return {};
}

}  // namespace KysChess::Battle