#pragma once

#include "ChessBattleEffects.h"

#include <set>
#include <vector>

namespace KysChess::Battle
{

struct BattleUnitStore;
class BattleRuntimeUnitRecords;

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
    BattleDeathEffectEventType type{};
    int sourceUnitId{};
    int targetUnitId{};
    int value{};
    int comboId = -1;
};

struct BattleDeathEffectStore
{
    std::set<int> regularSynergyComboIds;
};

class BattleDeathEffectSystem
{
public:
    std::vector<BattleDeathEffectEvent> applyAllyDeathEffects(BattleUnitStore& units,
                                                              BattleRuntimeUnitRecords& records,
                                                              BattleDeathEffectStore& effects,
                                                              int deadUnitId) const;

private:
    bool comboAppliesToUnit(const BattleDeathEffectStore& effects,
                            const BattleDeathEffectExtras& extras,
                            int comboId) const;
};

}  // namespace KysChess::Battle
