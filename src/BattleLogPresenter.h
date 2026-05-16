#pragma once

#include "BattleLogViewModel.h"
#include "BattlePostBattleSummary.h"
#include "BattleReport.h"

class BattleLogPresenter
{
public:
    BattleLogViewModel present(const BattlePostBattleSummary& summary, const BattleReport& report) const;
};

bool battleLogEntryMatchesFilter(const BattleLogEntryView& entry, const BattleLogFilter& filter);
int countVisibleBattleLogEntries(const BattleLogViewModel& model, const BattleLogFilter& filter);
