#pragma once

#include "BattleReport.h"
#include "battle/BattleRuntimeSession.h"

class BattleReportCollector
{
public:
    void consumeInitialization(
        const KysChess::Battle::BattleInitializationResult& initialization,
        const KysChess::Battle::BattleRuntimeSession& session);
    void consumeFrame(
        const KysChess::Battle::BattlePresentationFrame& frame,
        const KysChess::Battle::BattleRuntimeSession& session);

    const BattleReport& report() const { return builder_.report(); }

private:
    void consumeLog(
        const KysChess::Battle::BattleLogEvent& event,
        const KysChess::Battle::BattleRuntimeSession& session);

    BattleReportBuilder builder_;
};
