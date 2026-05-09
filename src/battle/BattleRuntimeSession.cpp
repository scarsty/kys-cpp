#include "BattleRuntimeSession.h"

#include "BattleFind.h"

#include <algorithm>
#include <cassert>

namespace KysChess::Battle
{
namespace
{

Pointf setupPlacementPosition(const BattleGridTransform& gridTransform, int x, int y)
{
    assert(gridTransform.tileWidth > 0.0);
    assert(gridTransform.coordCount > 0);
    return {
        static_cast<float>(-y * gridTransform.tileWidth + x * gridTransform.tileWidth + gridTransform.coordCount * gridTransform.tileWidth),
        static_cast<float>(y * gridTransform.tileWidth + x * gridTransform.tileWidth),
        0.0f,
    };
}

Pointf setupPlacementFacing(int faceTowards)
{
    switch (faceTowards)
    {
    case 0:
        return { 1.0f, 0.0f, 0.0f };
    case 1:
        return { 0.0f, 1.0f, 0.0f };
    case 2:
        return { -1.0f, 0.0f, 0.0f };
    case 3:
        return { 0.0f, -1.0f, 0.0f };
    default:
        assert(false && "invalid setup facing");
        return { 1.0f, 0.0f, 0.0f };
    }
}

}  // namespace

BattleRuntimeSession::BattleRuntimeSession(BattleRuntimeInit init)
    : runtime_(std::move(init.runtime))
{
    initialization_result_ = BattleInitializationSystem().initialize(runtime_, init.setup);
}

BattleFrameResult BattleRuntimeSession::runFrame()
{
    frameStarted_ = true;
    return runner_.runFrame(runtime_);
}

void BattleRuntimeSession::commitSetupConfiguration(BattleRuntimeSetupConfiguration config)
{
    assert(!setupConfigured_);
    assert(!frameStarted_);
    setupConfigured_ = true;

    runtime_.world = std::move(config.world);
    runtime_.attacks = std::move(config.attacks);
    runtime_.damage = std::move(config.damage);
    runtime_.result = std::move(config.result);
    runtime_.teamEffects = std::move(config.teamEffects);
    runtime_.rescue = std::move(config.rescue);
    runtime_.movementPhysics = std::move(config.movementPhysics);
    runtime_.action = std::move(config.action);
    runtime_.projectileFollowUps = std::move(config.projectileFollowUps);
    runtime_.deathEffects = std::move(config.deathEffects);
}

void BattleRuntimeSession::commitSetupPlacement(const BattleSetupPlacementInput& input)
{
    assert(!setupPlacementCommitted_);
    assert(!frameStarted_);
    setupPlacementCommitted_ = true;

    for (const auto& unitPlacement : input.units)
    {
        const auto position = setupPlacementPosition(runtime_.units.gridTransform, unitPlacement.x, unitPlacement.y);
        runtime_.units.setPosition(unitPlacement.unitId, position);
        auto& unit = runtime_.units.requireUnit(unitPlacement.unitId);
        unit.facing = setupPlacementFacing(unitPlacement.faceTowards);

        auto& worldUnit = requireById(runtime_.world.units, unitPlacement.unitId);
        worldUnit.position = position;
        worldUnit.velocity = { 0, 0, 0 };
    }
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

}  // namespace KysChess::Battle
