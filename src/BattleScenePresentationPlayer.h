#pragma once

#include "BattleSceneAct.h"
#include "BattleStatsView.h"
#include "battle/BattlePresentationPlayback.h"

#include <deque>
#include <functional>

class BattleScenePresentationPlayer
{
public:
    struct Bindings
    {
        BattleTracker* tracker = nullptr;
        std::deque<BattleSceneAct::TextEffect>* textEffects = nullptr;
        std::deque<BattleSceneAct::AttackEffect>* attackEffects = nullptr;
        std::function<Role*(int)> resolveRole;
    };

    void play(const KysChess::Battle::BattlePresentationFrame& frame, const Bindings& bindings) const;
    void playLogs(const std::vector<KysChess::Battle::BattleLogEvent>& logEvents, const Bindings& bindings) const;
    void playVisuals(const std::vector<KysChess::Battle::BattleVisualEvent>& visualEvents, const Bindings& bindings) const;

private:
    void playCommand(const KysChess::Battle::BattlePresentationCommand& command, const Bindings& bindings) const;
    void recordLog(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordDamage(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordHeal(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordStatus(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
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
