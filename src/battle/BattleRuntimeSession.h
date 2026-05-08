#pragma once

#include "BattleInitialization.h"

#include <utility>

namespace KysChess::Battle
{

class BattleRuntimeSession
{
public:
    explicit BattleRuntimeSession(BattleRuntimeInit init);

    BattleFrameResult runFrame();
    void enqueueAttackSpawn(BattleAttackSpawnRequest request);

    const BattleRuntimeState& runtime() const;
    BattleRuntimeState& runtimeForTests();

private:
    BattleRuntimeState runtime_;
    BattleFrameRunner runner_;
};

}  // namespace KysChess::Battle
