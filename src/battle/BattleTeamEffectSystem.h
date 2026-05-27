#pragma once

#include "../ChessBattleEffects.h"
#include "BattleStatusSystem.h"

#include <map>
#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeUnit;
class BattleRuntimeUnitRecords;
class BattleRuntimeUnitRecords;

enum class BattleTeamEffectEventType
{
    Heal,
    MpRestore,
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
    std::vector<BattleTeamEffectEvent> applySelfHeal(BattleRuntimeUnitRecords& units,
                                                     int sourceUnitId,
                                                     int pctHeal) const;

    std::vector<BattleTeamEffectEvent> applyTeamHeal(BattleRuntimeUnitRecords& units,
                                                     int sourceUnitId,
                                                     int flatHeal,
                                                     int pctHeal) const;

    std::vector<BattleTeamEffectEvent> applyTeamMp(BattleRuntimeUnitRecords& units,
                                                   int sourceUnitId,
                                                   int amount) const;

    std::vector<BattleTeamEffectEvent> applyTeamShield(BattleRuntimeUnitRecords& units,
                                                       int sourceUnitId,
                                                       int amount,
                                                       bool refreshOnly) const;

    std::vector<BattleTeamEffectEvent> applyHealAura(BattleRuntimeUnitRecords& units,
                                                     int sourceUnitId,
                                                     int flatHeal,
                                                     int pctHeal,
                                                     double radius,
                                                     int healedCooldownReducePct) const;

private:
    BattleRuntimeUnit& unitById(BattleRuntimeUnitRecords& units, int unitId) const;
};

}  // namespace KysChess::Battle
