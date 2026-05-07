#pragma once

#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeUnit;
struct BattleUnitStore;

enum class BattleTeamEffectEventType
{
    Heal,
    MpRestore,
    ShieldGain,
    CooldownReduced,
};

struct BattleTeamEffectEvent
{
    BattleTeamEffectEventType type = BattleTeamEffectEventType::Heal;
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int value = 0;
    int before = 0;
    int after = 0;
};

class BattleTeamEffectSystem
{
public:
    std::vector<BattleTeamEffectEvent> applySelfHeal(BattleUnitStore& units,
                                                     int sourceUnitId,
                                                     int pctHeal) const;

    std::vector<BattleTeamEffectEvent> applyTeamHeal(BattleUnitStore& units,
                                                     int sourceUnitId,
                                                     int flatHeal,
                                                     int pctHeal) const;

    std::vector<BattleTeamEffectEvent> applyTeamMp(BattleUnitStore& units,
                                                   int sourceUnitId,
                                                   int amount) const;

    std::vector<BattleTeamEffectEvent> applyTeamShield(BattleUnitStore& units,
                                                       int sourceUnitId,
                                                       int amount,
                                                       bool refreshOnly) const;

    std::vector<BattleTeamEffectEvent> applyHealAura(BattleUnitStore& units,
                                                     int sourceUnitId,
                                                     int flatHeal,
                                                     int pctHeal,
                                                     double radius,
                                                     int healedCooldownReducePct) const;

private:
    BattleRuntimeUnit& unitById(BattleUnitStore& units, int unitId) const;
};

}  // namespace KysChess::Battle
