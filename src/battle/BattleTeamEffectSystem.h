#pragma once

#include "../ChessBattleEffects.h"
#include "BattleStatusSystem.h"

#include <map>
#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeUnit;
class BattleRuntimeUnits;

enum class BattleTeamEffectEventType
{
    Heal,
    MpRestore,
    MpDamage,
    ShieldGain,
    CooldownReduced,
};

struct BattleTeamEffectEvent
{
    BattleTeamEffectEventType type{};
    int sourceUnitId{};
    int targetUnitId{};
    int value{};
    int before{};
    int after{};
};

class BattleTeamEffectSystem
{
public:
    std::vector<BattleTeamEffectEvent> applySelfHeal(BattleRuntimeUnits& units,
                                                     int sourceUnitId,
                                                     int pctHeal) const;

    std::vector<BattleTeamEffectEvent> applyTeamHeal(BattleRuntimeUnits& units,
                                                     int sourceUnitId,
                                                     int flatHeal,
                                                     int pctHeal) const;

    std::vector<BattleTeamEffectEvent> applyLowestAllyHeal(BattleRuntimeUnits& units,
                                                           int sourceUnitId,
                                                           int flatHeal,
                                                           int pctHeal) const;

    std::vector<BattleTeamEffectEvent> applyTeamMp(BattleRuntimeUnits& units,
                                                   int sourceUnitId,
                                                   int amount) const;

    std::vector<BattleTeamEffectEvent> applyEnemyMpDamageAll(BattleRuntimeUnits& units,
                                                             int sourceUnitId,
                                                             int amount) const;

    std::vector<BattleTeamEffectEvent> applyTeamShield(BattleRuntimeUnits& units,
                                                       int sourceUnitId,
                                                       int amount,
                                                       bool refreshOnly) const;

    std::vector<BattleTeamEffectEvent> applyHealAura(BattleRuntimeUnits& units,
                                                     int sourceUnitId,
                                                     int flatHeal,
                                                     int pctHeal,
                                                     double radius,
                                                     int healedCooldownReducePct) const;

private:
    BattleRuntimeUnit& unitById(BattleRuntimeUnits& units, int unitId) const;
};

}  // namespace KysChess::Battle
