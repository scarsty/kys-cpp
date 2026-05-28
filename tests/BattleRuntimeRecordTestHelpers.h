#pragma once

#include "battle/BattleRuntimeUnitSpawn.h"
#include "battle/BattleRuntimeUnits.h"

#include <initializer_list>
#include <utility>

namespace KysChess::Battle::Test
{

inline void appendRuntimeRecord(BattleRuntimeUnits& records, BattleRuntimeUnit unit, RoleComboState combo = {})
{
    auto record = makeRuntimeUnitSpawn(std::move(unit), std::move(combo)).makeRecord();
    records.append(std::move(record));
}

inline BattleRuntimeUnits runtimeRecords(std::initializer_list<BattleRuntimeUnit> units)
{
    BattleRuntimeUnits records;
    records.reserve(units.size());
    for (auto unit : units)
    {
        appendRuntimeRecord(records, std::move(unit));
    }
    return records;
}

}  // namespace KysChess::Battle::Test
