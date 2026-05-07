#pragma once

#include "BattleCore.h"

#include <utility>

namespace KysChess::Battle
{

struct BattleRuntimeInit
{
    BattleRuntimeState runtime;
};

class BattleRuntimeSession
{
public:
    explicit BattleRuntimeSession(BattleRuntimeInit init);

    BattleFrameScratch& beginFrameScratch();
    BattleFrameResult runFrame();
    void enqueueAttackSpawn(BattleAttackSpawnRequest request);

    const BattleRuntimeState& runtime() const;
    BattleRuntimeState& runtimeForTests();

private:
    BattleRuntimeState runtime_;
    BattleFrameScratch scratch_;
    BattleFrameRunner runner_;
};

}  // namespace KysChess::Battle
