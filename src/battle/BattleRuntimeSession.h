#pragma once

#include "BattleInitialization.h"

#include <optional>
#include <utility>
#include <vector>

namespace KysChess::Battle
{

struct BattleSetupPlacementUnit
{
    int unitId = -1;
    int x = 0;
    int y = 0;
    int faceTowards = 0;
};

struct BattleSetupPlacementInput
{
    std::vector<BattleSetupPlacementUnit> units;
};

class BattleRuntimeSession
{
public:
    explicit BattleRuntimeSession(BattleRuntimeInit init);

    BattleFrameResult runFrame();
    void enqueueAttackSpawn(BattleAttackSpawnRequest request);
    void commitSetupPlacement(const BattleSetupPlacementInput& input);
    BattleInitializationResult releaseInitializationResult();

    const BattleRuntimeState& runtime() const;
    BattleRuntimeState& runtimeForTests();
    BattleRuntimeState& runtimeForSetupConfiguration();

private:
    BattleRuntimeState runtime_;
    std::optional<BattleInitializationResult> initialization_result_;
    BattleFrameRunner runner_;
    bool setupPlacementCommitted_ = false;
    bool frameStarted_ = false;
};

}  // namespace KysChess::Battle
