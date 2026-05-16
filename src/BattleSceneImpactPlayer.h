#pragma once

#include "BattleSceneUnitStore.h"
#include "battle/BattleAttackSystem.h"

#include <functional>
#include <vector>

namespace KysChess::Battle
{
struct BattleFrameResult;
}

struct BattleSceneImpactCommand
{
    int unitId = -1;
    int effectSoundId = -1;
    int unitShake = 0;
    int sceneShake = 0;
    bool rumble = false;
};

class BattleSceneImpactPlanner
{
public:
    std::vector<BattleSceneImpactCommand> plan(
        const std::vector<KysChess::Battle::BattleAttackEvent>& events) const;
};

class BattleSceneImpactPlayer
{
public:
    struct Bindings
    {
        BattleSceneUnitStore* units = nullptr;
        int* sceneShake = nullptr;
        std::function<void(int)> playEffectSound;
        std::function<void(int, int, int)> rumble;
    };

    void play(
        const std::vector<KysChess::Battle::BattleAttackEvent>& events,
        const Bindings& bindings) const;
    void play(
        const KysChess::Battle::BattleFrameResult& frameResult,
        const Bindings& bindings) const;

private:
    BattleSceneImpactPlanner planner_;
};
