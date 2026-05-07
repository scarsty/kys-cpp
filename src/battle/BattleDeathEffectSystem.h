#pragma once

#include "ChessBattleEffects.h"

#include <set>
#include <vector>

namespace KysChess::Battle
{

struct BattleUnitStore;

struct BattleDeathEffectExtras
{
    int id = -1;
    int shieldPctMaxHp = 0;
    int shieldOnAllyDeathTracker = 0;
    std::vector<int> comboIds;
    std::vector<AppliedEffectInstance> appliedEffects;
};

enum class BattleDeathEffectEventType
{
    AllyStatBoost,
    DeathMedicalHeal,
    ShieldOnAllyDeath,
};

struct BattleDeathEffectEvent
{
    BattleDeathEffectEventType type = BattleDeathEffectEventType::AllyStatBoost;
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int value = 0;
    int comboId = -1;
};

struct BattleDeathEffectStore
{
    std::vector<BattleDeathEffectExtras> units;
    std::set<int> regularSynergyComboIds;
};

class BattleDeathEffectSystem
{
public:
    std::vector<BattleDeathEffectEvent> applyAllyDeathEffects(BattleUnitStore& units,
                                                              BattleDeathEffectStore& effects,
                                                              int deadUnitId) const;

private:
    BattleDeathEffectExtras& extrasById(BattleDeathEffectStore& effects, int unitId) const;
    bool comboAppliesToUnit(const BattleDeathEffectStore& effects,
                            const BattleDeathEffectExtras& extras,
                            int comboId) const;
};

}  // namespace KysChess::Battle
