#pragma once

#include "BattleInitialization.h"

#include <optional>
#include <utility>

namespace KysChess::Battle
{

class BattleRuntimeSession
{
public:
    explicit BattleRuntimeSession(BattleRuntimeInit init);

    BattleFrameResult runFrame();
    void enqueueAttackSpawn(BattleAttackSpawnRequest request);
    BattleInitializationResult releaseInitializationResult();

    const BattleRuntimeState& runtime() const;
    BattleRuntimeState& runtimeForTests();

private:
    BattleRuntimeState runtime_;
    std::optional<BattleInitializationResult> initialization_result_;
    BattleFrameRunner runner_;
};

}  // namespace KysChess::Battle
