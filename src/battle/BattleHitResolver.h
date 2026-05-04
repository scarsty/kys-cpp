#pragma once

#include "../ChessBattleEffects.h"
#include "../Point.h"
#include "BattleAttackSystem.h"
#include "BattleDamageSystem.h"
#include "BattleOperation.h"
#include "BattlePresentation.h"

#include <string>
#include <variant>
#include <vector>

namespace KysChess::Battle
{

struct BattleHitUnitSnapshot
{
    int id = -1;
    int team = 0;
    bool alive = true;
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;
    int attack = 0;
    double defence = 0.0;
    int speed = 0;
    int invincible = 0;
    int hurtFrame = 0;
    int cooldown = 0;
    int cooldownMax = 0;
    bool haveAction = false;
    BattleOperationType operationType = BattleOperationType::None;
    int actType = -1;
    Pointf position;
    Pointf facing;
};

struct BattleHitSkillSnapshot
{
    int id = -1;
    std::string name;
    int hurtType = 0;
    int magicType = 0;
    int effectId = -1;
    int attackerActProperty = 0;
    int defenderActProperty = 0;
    int magicPower = 0;
    int resolvedBaseDamage = 0;
};

struct BattleHitItemSnapshot
{
    int id = -1;
    std::string name;
    int hiddenWeaponEffectId = -1;
    int resolvedDamage = 0;
};

struct BattleHpDamageCommand
{
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int damage = 0;
    bool critical = false;
    bool ultimate = false;
    bool executed = false;
    std::string skillName;
    std::string detailText;
};

struct BattleMpDamageCommand
{
    int sourceUnitId = -1;
    int targetUnitId = -1;
    BattleDamageRequest damage;
};

struct BattleAcceptedHitSideEffectCommand
{
    int sourceUnitId = -1;
    int targetUnitId = -1;
    BattleDamageRequest damage;
};

struct BattleTeamHealCommand
{
    int sourceUnitId = -1;
    int flatHeal = 0;
    int pctHeal = 0;
    std::string reason;
};

struct BattleTeamMpRestoreCommand
{
    int sourceUnitId = -1;
    int amount = 0;
    std::string reason;
};

struct BattleTeamShieldCommand
{
    int sourceUnitId = -1;
    int amount = 0;
    bool refreshOnly = false;
    std::string reason;
};

struct BattleProjectileSpawnCommand
{
    BattleAttackSpawnRequest request;
    std::string reason;
};

struct BattleAutoUltimateCommand
{
    int unitId = -1;
    bool consumeMp = false;
};

struct BattleKnockbackCommand
{
    int targetUnitId = -1;
    Pointf velocityDelta;
    bool grantHurtFrame = false;
};

struct BattleTempAttackBuffCommand
{
    int unitId = -1;
    int attackBonus = 0;
    int durationFrames = 0;
    std::string reason;
};

struct BattleLastAttackerCommand
{
    int targetUnitId = -1;
    int attackerUnitId = -1;
};

struct BattleRumbleCommand
{
    int lowFrequency = 0;
    int highFrequency = 0;
    int durationMs = 0;
};

using BattleGameplayCommand = std::variant<
    BattleHpDamageCommand,
    BattleMpDamageCommand,
    BattleAcceptedHitSideEffectCommand,
    BattleTeamHealCommand,
    BattleTeamMpRestoreCommand,
    BattleTeamShieldCommand,
    BattleProjectileSpawnCommand,
    BattleAutoUltimateCommand,
    BattleKnockbackCommand,
    BattleTempAttackBuffCommand,
    BattleLastAttackerCommand,
    BattleRumbleCommand>;

struct BattleHitResolutionInput
{
    BattleAttackEvent attackEvent;
    BattleHitUnitSnapshot attacker;
    BattleHitUnitSnapshot defender;
    BattleHitSkillSnapshot skill;
    BattleHitItemSnapshot item;
    RoleComboState attackerCombo;
    RoleComboState defenderCombo;
    int pendingDefenderHpDamage = 0;
    int sharedBleedMaxStacks = 1;
    int randomDamageVariance = 0;
    std::vector<double> percentRolls;
};

struct BattleHitResolutionResult
{
    RoleComboState attackerCombo;
    RoleComboState defenderCombo;
    std::vector<BattleGameplayCommand> commands;
    std::vector<BattlePresentationEvent> presentationEvents;
    bool dodged = false;
    bool reflected = false;
    bool critical = false;
    bool executed = false;
    int finalHpDamage = 0;
    int finalMpDamage = 0;
};

class BattleHitResolver
{
public:
    BattleHitResolutionResult resolve(const BattleHitResolutionInput& input) const;
};

}  // namespace KysChess::Battle
