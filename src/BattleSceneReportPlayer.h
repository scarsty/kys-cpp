#pragma once

#include "BattleReport.h"
#include "BattleSceneUnitStore.h"
#include "battle/BattleHitResolver.h"
#include "battle/BattlePresentation.h"

#include <vector>

class BattleSceneReportPlayer
{
public:
    struct Bindings
    {
        BattleReportBuilder* report = nullptr;
        const BattleSceneUnitStore* units = nullptr;
    };

    void playLogs(
        const std::vector<KysChess::Battle::BattleLogEvent>& logEvents,
        const Bindings& bindings) const;

private:
    void recordLog(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordDamage(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordHeal(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordStatus(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordUnitDied(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordBattleEnded(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
};
