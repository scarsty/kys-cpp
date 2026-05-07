#pragma once

#include "../ChessBattleEffects.h"
#include "../Point.h"
#include "BattleAttackSystem.h"
#include "BattleDamageSystem.h"
#include "BattleOperation.h"
#include "BattlePresentation.h"
#include "BattleProjectileTargetingSystem.h"

#include <string>
#include <variant>
#include <vector>

namespace KysChess::Battle
{

struct BattleUnitStore;
struct BattleRuntimeUnit;

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

struct BattleCurrentHpBlastCommand
{
    int sourceUnitId = -1;
    int damagePct = 0;
    std::string reason;
};

struct BattleSpiralBleedProjectileCommand
{
    int sourceUnitId = -1;
    int bleedStacks = 0;
    int projectileCount = 0;
};

struct BattleNearbyTrackingProjectilesCommand
{
    BattleAttackEvent prototype;
    int centerTargetUnitId = -1;
    int rangePixels = 0;
    int damagePct = 0;
};

struct BattleHitExtraProjectilesCommand
{
    BattleAttackEvent prototype;
    int extraCount = 0;
    int targetUnitId = -1;
};

struct BattleShieldExplosionCommand
{
    int sourceUnitId = -1;
    int areaSize = 0;
    int effectId = -1;
    int damage = 0;
    std::string reason;
};

struct BattleMpRestoreCommand
{
    int unitId = -1;
    int amount = 0;
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
    double velocityCap = 0.0;
    bool grantHurtFrame = false;
};

struct BattleTempAttackBuffCommand
{
    int unitId = -1;
    int attackBonus = 0;
    int durationFrames = 0;
    std::string reason;
    int defenceBonus = 0;
    bool permanent = false;
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

struct BattleProjectileCancelDamageCommand
{
    int attackId = -1;
    int otherAttackId = -1;
    int sourceUnitId = -1;
    int otherSourceUnitId = -1;
    int damage = 0;
    int otherDamage = 0;
};

struct BattleDeathAoeProjectileCommand
{
    int sourceUnitId = -1;
    int trackedTargetUnitId = -1;
    int damage = 0;
    int damagePct = 0;
    int stunFrames = 0;
    int maxTargets = 0;
};

struct BattleUnitHealCommand
{
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int amount = 0;
    std::string reason;
};

struct BattleUnitShieldCommand
{
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int amount = 0;
    std::string reason;
};

using BattleGameplayCommand = std::variant<
    BattleHpDamageCommand,
    BattleMpDamageCommand,
    BattleAcceptedHitSideEffectCommand,
    BattleTeamHealCommand,
    BattleTeamMpRestoreCommand,
    BattleTeamShieldCommand,
    BattleProjectileSpawnCommand,
    BattleCurrentHpBlastCommand,
    BattleSpiralBleedProjectileCommand,
    BattleNearbyTrackingProjectilesCommand,
    BattleHitExtraProjectilesCommand,
    BattleShieldExplosionCommand,
    BattleMpRestoreCommand,
    BattleAutoUltimateCommand,
    BattleKnockbackCommand,
    BattleTempAttackBuffCommand,
    BattleLastAttackerCommand,
    BattleRumbleCommand,
    BattleProjectileCancelDamageCommand,
    BattleDeathAoeProjectileCommand,
    BattleUnitHealCommand,
    BattleUnitShieldCommand>;

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
    int attackerUnitId = -1;
    int defenderUnitId = -1;
    RoleComboState attackerCombo;
    RoleComboState defenderCombo;
    std::vector<BattleGameplayCommand> commands;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
    bool dodged = false;
    bool reflected = false;
    bool critical = false;
    bool executed = false;
    double shapedHpDamage = 0.0;
    int finalHpDamage = 0;
    int finalMpDamage = 0;
};

struct BattleProjectileFollowUpContext
{
    double projectileSpeed = 1.0;
    int minimumProjectileFrames = 20;
    int nearbyProjectileFramePadding = 18;
    int areaProjectileFramePadding = 15;
    double areaSpawnDistance = 54.0;
    int nextSharedHitGroupId = 1;
};

struct BattleProjectileFollowUpExpansion
{
    std::vector<BattleGameplayCommand> commands;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
};

BattleProjectileFollowUpExpansion expandBattleProjectileFollowUpCommands(
    const std::vector<BattleGameplayCommand>& commands,
    BattleProjectileFollowUpContext& context,
    const BattleUnitStore& units);

class BattleHitResolver
{
public:
    BattleHitResolutionResult resolve(const BattleHitResolutionInput& input) const;
};

}  // namespace KysChess::Battle
