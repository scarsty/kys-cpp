#pragma once

#include "../ChessBattleEffects.h"
#include "../Point.h"
#include "BattleAttackSystem.h"
#include "BattleDamageSystem.h"
#include "BattleOperation.h"
#include "BattlePresentation.h"
#include "BattleProjectileTargetingSystem.h"
#include "BattleUnitValues.h"

#include <string>
#include <variant>
#include <vector>

namespace KysChess::Battle
{

class BattleRuntimeRandom;
struct BattleUnitStore;
struct BattleRuntimeUnit;

struct BattleHitUnitSnapshot
{
    int id = -1;
    int team = 0;
    bool alive = true;
    BattleUnitVitals vitals;
    BattleUnitStats stats;
    BattleUnitMotion motion;
    BattleUnitAnimationState animation;
    int invincible = 0;
    bool haveAction = false;
    BattleOperationType operationType = BattleOperationType::None;
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

struct BattleHpDamageCommand
{
    int sourceUnitId{};
    int targetUnitId{};
    int damage{};
    bool critical{};
    bool ultimate{};
    bool executed{};
    int frozenFrames{};
    std::string skillName;
    std::vector<BattleLogTextSegment> segments;
    bool triggersDefenseEffects = true;
};

struct BattleMpDamageCommand
{
    int sourceUnitId{};
    int targetUnitId{};
    BattleDamageRequest damage;
};

struct BattleAcceptedHitSideEffectCommand
{
    int sourceUnitId{};
    int targetUnitId{};
    BattleDamageRequest damage;
};

struct BattleTeamHealCommand
{
    int sourceUnitId{};
    int flatHeal{};
    int pctHeal{};
    std::string reason;
};

struct BattleTeamMpRestoreCommand
{
    int sourceUnitId{};
    int amount{};
    std::string reason;
};

struct BattleTeamShieldCommand
{
    int sourceUnitId{};
    int amount{};
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
    int sourceUnitId{};
    int damagePct{};
    std::string reason;
};

struct BattleSpiralBleedProjectileCommand
{
    int sourceUnitId{};
    int bleedStacks{};
    int projectileCount{};
    double projectileSpeed{};
};

struct BattleNearbyTrackingProjectilesCommand
{
    BattleAttackEvent prototype;
    int centerTargetUnitId{};
    int rangePixels{};
    int damagePct{};
    double projectileSpeed{};
};

struct BattleHitExtraProjectilesCommand
{
    BattleAttackEvent prototype;
    int extraCount{};
    int targetUnitId{};
};

struct BattleShieldExplosionCommand
{
    int sourceUnitId{};
    int areaSize{};
    int effectId{};
    int damage{};
    std::string reason;
};

struct BattleMpRestoreCommand
{
    int unitId{};
    int amount{};
    std::string reason;
};

struct BattleAutoUltimateCommand
{
    int unitId{};
    bool consumeMp = false;
    bool announce = false;
};

struct BattleKnockbackCommand
{
    int targetUnitId{};
    Pointf velocityDelta;
    double velocityCap{};
};

struct BattleTempAttackBuffCommand
{
    int unitId{};
    int attackBonus{};
    int durationFrames{};
    std::string reason;
    int defenceBonus = 0;
    bool permanent = false;
};

struct BattleLastAttackerCommand
{
    int targetUnitId{};
    int attackerUnitId{};
};

struct BattleRumbleCommand
{
    int lowFrequency{};
    int highFrequency{};
    int durationMs{};
};

struct BattleProjectileCancelDamageCommand
{
    int attackId{};
    int otherAttackId{};
    int sourceUnitId{};
    int otherSourceUnitId{};
    int damage{};
    int otherDamage{};
};

struct BattleDeathAoeProjectileCommand
{
    int sourceUnitId{};
    int trackedTargetUnitId{};
    int damage{};
    int damagePct{};
    int stunFrames{};
    int maxTargets{};
};

struct BattleUnitHealCommand
{
    int sourceUnitId{};
    int targetUnitId{};
    int amount{};
    std::string reason;
};

struct BattleUnitShieldCommand
{
    int sourceUnitId{};
    int targetUnitId{};
    int amount{};
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
    RoleComboState attackerCombo;
    RoleComboState defenderCombo;
    int pendingDefenderHpDamage = 0;
    int sharedBleedMaxStacks = 1;
    int randomDamageVariance = 0;
};

struct BattleHitResolutionResult
{
    int attackerUnitId{};
    int defenderUnitId{};
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
    BattleHitResolutionResult resolve(const BattleHitResolutionInput& input, BattleRuntimeRandom& random) const;
};

}  // namespace KysChess::Battle
