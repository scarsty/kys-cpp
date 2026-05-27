#pragma once

#include "battle/BattleRuntimeUnits.h"

#include <initializer_list>
#include <utility>

namespace KysChess::Battle::Test
{

inline void appendRuntimeRecord(BattleRuntimeUnitRecords& records, BattleRuntimeUnit unit, RoleComboState combo = {})
{
    BattleRuntimeUnitRecord record;
    record.core = std::move(unit);
    record.combo = std::move(combo);
    record.status.id = record.core.id;
    record.damage.id = record.core.id;
    record.deathEffects.id = record.core.id;
    record.rescue.unitId = record.core.id;
    record.movement.active = record.core.alive;
    record.movement.physics.position = record.core.motion.position;
    record.movement.physics.velocity = record.core.motion.velocity;
    record.movement.physics.acceleration = record.core.motion.acceleration;
    records.append(std::move(record));
}

inline BattleRuntimeUnitRecords runtimeRecords(std::initializer_list<BattleRuntimeUnit> units)
{
    BattleRuntimeUnitRecords records;
    records.reserve(units.size());
    for (auto unit : units)
    {
        appendRuntimeRecord(records, std::move(unit));
    }
    return records;
}

}  // namespace KysChess::Battle::Test
