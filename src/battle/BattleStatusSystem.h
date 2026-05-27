#pragma once

#include <string>
#include <vector>

namespace KysChess
{
struct RoleComboState;
}

namespace KysChess::Battle
{

struct BattleRuntimeUnit;
struct BattleRuntimeUnitRecord;
class BattleRuntimeUnits;
class BattleRuntimeUnits;

struct TimedAttackBuff
{
    int attackBonus{};
    int remainingFrames{};
};

struct DamageReduceDebuff
{
    int remainingFrames{};
    int pct{};
};

struct BattleStatusEffectState
{
    int poisonTimer = 0;
    int poisonTickPct = 0;
    int poisonSourceId = -1;

    int bleedStacks = 0;
    int bleedTimer = 0;
    int bleedSourceId = -1;

    int frozenTimer = 0;
    int frozenMaxTimer = 0;
    int freezeReductionPct = 0;
    int shieldFreezeResPct = 0;
    int controlImmunityFrames = 0;
    int mpBlockTimer = 0;

    int damageImmunityAfterFrames = 0;
    int damageImmunityDuration = 0;
    int damageImmunityTimer = 0;

    std::vector<TimedAttackBuff> tempAttackBuffs;
    std::vector<DamageReduceDebuff> damageReduceDebuffs;
};

struct BattleStatusUnitState
{
    int id = -1;
    bool alive = true;
    int hp = 0;
    int maxHp = 0;
    int attack = 0;
    int invincible = 0;

    BattleStatusEffectState effects;
};

struct BattleStatusRuntimeUnit
{
    int id = -1;

    BattleStatusEffectState effects;
};

enum class BattleStatusEventType
{
    PoisonDamage,
    BleedDamage,
    TempAttackExpired,
    InvincibilityGranted,
};

struct BattleStatusEvent
{
    BattleStatusEventType type{};
    int unitId{};
    int sourceUnitId{};
    int value{};
    std::string reason;
};

struct BattleStatusTickResult
{
    std::vector<BattleStatusEvent> events;
};

struct BattleStatusSystemConfig
{
    int frame = 0;
    int poisonDamageIntervalFrames = 30;
    int bleedDamageIntervalFrames = 10;
};

class BattleStatusSystem
{
public:
    explicit BattleStatusSystem(BattleStatusSystemConfig config);

    BattleStatusTickResult tick(BattleRuntimeUnitRecord& unit) const;
    BattleStatusTickResult tick(BattleRuntimeUnits& records) const;

private:
    BattleStatusSystemConfig config_;
};

BattleStatusRuntimeUnit makeBattleStatusRuntimeUnit(const BattleStatusUnitState& unit);
BattleStatusUnitState makeBattleStatusUnitState(const BattleRuntimeUnit& unit, const KysChess::RoleComboState& state);
BattleStatusUnitState makeBattleStatusUnitState(const BattleStatusRuntimeUnit& status, const BattleRuntimeUnit& unit);
void writeBattleStatusRuntimeUnit(BattleStatusRuntimeUnit& status, const BattleStatusUnitState& unit);

}  // namespace KysChess::Battle
