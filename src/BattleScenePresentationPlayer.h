#pragma once

#include "BattlePresentationEffects.h"
#include "BattleSceneUnitStore.h"
#include "battle/BattlePresentationPlayback.h"

#include <deque>
#include <optional>

class BattleScenePresentationPlayer
{
public:
    struct UnitView
    {
        Pointf position;
        int team = -1;
        int maxHp = 0;
    };

    struct Bindings
    {
        std::deque<BattleTextEffect>* textEffects = nullptr;
        std::deque<BattleAttackEffect>* attackEffects = nullptr;
        const BattleSceneUnitStore* units = nullptr;
    };

    void play(const KysChess::Battle::BattlePresentationFrame& frame, const Bindings& bindings) const;
    void playVisuals(const std::vector<KysChess::Battle::BattleVisualEvent>& visualEvents, const Bindings& bindings) const;

private:
    void playCommand(const KysChess::Battle::BattlePresentationCommand& command, const Bindings& bindings) const;
    void spawnFloatingText(const KysChess::Battle::BattlePresentationCommand& command, const Bindings& bindings) const;
    void spawnRoleEffect(const KysChess::Battle::BattlePresentationCommand& command, const Bindings& bindings) const;
    void spawnDamageNumber(const KysChess::Battle::BattlePresentationCommand& command, const Bindings& bindings) const;
    void spawnProjectile(const KysChess::Battle::BattlePresentationCommand& command, const Bindings& bindings) const;
    void moveProjectile(const KysChess::Battle::BattlePresentationCommand& command, const Bindings& bindings) const;
    void impactProjectile(const KysChess::Battle::BattlePresentationCommand& command, const Bindings& bindings) const;
    void expireProjectile(const KysChess::Battle::BattlePresentationCommand& command, const Bindings& bindings) const;
    void cancelProjectile(const KysChess::Battle::BattlePresentationCommand& command, const Bindings& bindings) const;
    void bounceProjectile(const KysChess::Battle::BattlePresentationCommand& command, const Bindings& bindings) const;

    KysChess::Battle::BattlePresentationPlaybackPlanner planner_;
};
