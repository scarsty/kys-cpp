#pragma once

#include <vector>

namespace KysChess::Battle
{

struct BattleTeamEffectUnit
{
    int id = -1;
    int team = 0;
    bool alive = true;
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;
    int cooldown = 0;
    int shield = 0;
    bool mpBlocked = false;
    int mpRecoveryBonusPct = 0;
    double x = 0.0;
    double y = 0.0;
};

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

struct BattleTeamEffectWorld
{
    std::vector<BattleTeamEffectUnit> units;
};

class BattleTeamEffectSystem
{
public:
    std::vector<BattleTeamEffectEvent> applySelfHeal(BattleTeamEffectWorld& world,
                                                     int sourceUnitId,
                                                     int pctHeal) const;

    std::vector<BattleTeamEffectEvent> applyTeamHeal(BattleTeamEffectWorld& world,
                                                     int sourceUnitId,
                                                     int flatHeal,
                                                     int pctHeal) const;

    std::vector<BattleTeamEffectEvent> applyTeamMp(BattleTeamEffectWorld& world,
                                                   int sourceUnitId,
                                                   int amount) const;

    std::vector<BattleTeamEffectEvent> applyTeamShield(BattleTeamEffectWorld& world,
                                                       int sourceUnitId,
                                                       int amount,
                                                       bool refreshOnly) const;

    std::vector<BattleTeamEffectEvent> applyHealAura(BattleTeamEffectWorld& world,
                                                     int sourceUnitId,
                                                     int flatHeal,
                                                     int pctHeal,
                                                     double radius,
                                                     int healedCooldownReducePct) const;

private:
    BattleTeamEffectUnit& unitById(BattleTeamEffectWorld& world, int unitId) const;
};

}  // namespace KysChess::Battle
