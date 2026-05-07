#include "BattleRuntimeSession.h"

namespace KysChess::Battle
{

BattleRuntimeSession::BattleRuntimeSession(BattleRuntimeInit init)
    : runtime_(std::move(init.runtime))
{
}

BattleFrameResult BattleRuntimeSession::runFrame()
{
    return runner_.runFrame(runtime_);
}

void BattleRuntimeSession::enqueueAttackSpawn(BattleAttackSpawnRequest request)
{
    runtime_.pendingAttackSpawns.push_back(std::move(request));
}

const BattleRuntimeState& BattleRuntimeSession::runtime() const
{
    return runtime_;
}

BattleRuntimeState& BattleRuntimeSession::runtimeForTests()
{
    return runtime_;
}

}  // namespace KysChess::Battle
