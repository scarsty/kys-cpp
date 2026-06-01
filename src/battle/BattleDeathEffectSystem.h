#pragma once

#include "ChessBattleEffects.h"

#include <set>
#include <vector>

namespace KysChess::Battle
{

class BattleRuntimeUnits;

struct BattleDeathEffectExtras
{
    int shieldPctMaxHp = 0;
    int shieldOnAllyDeathTracker = 0;
    std::vector<int> comboIds;
    std::vector<ComboEffectSnapshot> appliedEffects;
};

enum class BattleDeathEffectEventType
{
    AllyStatBoost,
    DeathMedicalHeal,
    ShieldOnAllyDeath,
};

struct BattleDeathEffectEvent
{
    BattleDeathEffectEventType type{};
    int sourceUnitId{};
    int targetUnitId{};
    int value{};
    int comboId = -1;
};

struct BattleDeathEffectStore
{
    std::set<int> regularSynergyComboIds;

    std::vector<BattleDeathEffectEvent> applyAllyDeathEffects(BattleRuntimeUnits& records,
                                                              int deadUnitId) const;

private:
    bool comboAppliesToUnit(const BattleDeathEffectExtras& extras,
                            int comboId) const;
};

}  // namespace KysChess::Battle
