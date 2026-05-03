#pragma once

#include "ChessBattleEffects.h"

#include <set>
#include <vector>

namespace KysChess::Battle
{

struct BattleDeathEffectUnit
{
    int id = -1;
    int team = 0;
    bool alive = true;
    int hp = 0;
    int maxHp = 0;
    int attack = 0;
    int defence = 0;
    int shield = 0;
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

struct BattleDeathEffectWorld
{
    std::vector<BattleDeathEffectUnit> units;
    std::set<int> regularSynergyComboIds;
};

class BattleDeathEffectSystem
{
public:
    std::vector<BattleDeathEffectEvent> applyAllyDeathEffects(BattleDeathEffectWorld& world,
                                                              int deadUnitId) const;

private:
    BattleDeathEffectUnit& unitById(BattleDeathEffectWorld& world, int unitId) const;
    bool comboAppliesToUnit(const BattleDeathEffectWorld& world,
                            const BattleDeathEffectUnit& unit,
                            int comboId) const;
};

}  // namespace KysChess::Battle
