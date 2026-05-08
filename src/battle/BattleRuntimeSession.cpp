#include "BattleRuntimeSession.h"

#include <cassert>

namespace KysChess::Battle
{

BattleRuntimeSession::BattleRuntimeSession(BattleRuntimeInit init)
    : runtime_(std::move(init.runtime))
{
    initialization_result_ = BattleInitializationSystem().initialize(runtime_, init.setup);
}

BattleFrameResult BattleRuntimeSession::runFrame()
{
    return runner_.runFrame(runtime_);
}

void BattleRuntimeSession::enqueueAttackSpawn(BattleAttackSpawnRequest request)
{
    runtime_.pendingAttackSpawns.push_back(std::move(request));
}

BattleInitializationResult BattleRuntimeSession::releaseInitializationResult()
{
    assert(initialization_result_.has_value());
    BattleInitializationResult result = std::move(*initialization_result_);
    initialization_result_.reset();
    return result;
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
