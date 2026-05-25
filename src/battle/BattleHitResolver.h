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
    bool canTriggerExecute{};
    bool canTriggerDefenderBlock{};
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
    bool canTriggerDefenderBlock{};
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

struct BattleProjectileSpawnCommand
{
    BattleAttackSpawnRequest request;
    std::string reason;
};

struct BattleNearbyTrackingProjectilesCommand
{
    BattleAttackEvent prototype;
    int centerTargetUnitId{};
    int rangePixels{};
    int damagePct{};
    double projectileSpeed{};
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

struct BattleRumbleCommand
{
    int lowFrequency{};
    int highFrequency{};
    int durationMs{};
};

struct BattleAreaProjectileFollowUp
{
    int sourceUnitId{};
    int areaSize{};
    int trackedTargetUnitId = -1;
    int maxTargets{};
    int effectId{};
    int damage{};
    int damagePct{};
    int stunFrames{};
    std::string reason;
    std::string logText;
};

using BattleGameplayCommand = std::variant<
    BattleHpDamageCommand,
    BattleMpDamageCommand,
    BattleAcceptedHitSideEffectCommand,
    BattleTeamHealCommand,
    BattleProjectileSpawnCommand,
    BattleNearbyTrackingProjectilesCommand,
    BattleAutoUltimateCommand,
    BattleKnockbackCommand,
    BattleTempAttackBuffCommand,
    BattleRumbleCommand>;

struct BattleHitResolutionInput
{
    BattleAttackEvent attackEvent;
    BattleHitUnitSnapshot attacker;
    BattleHitUnitSnapshot defender;
    BattleHitSkillSnapshot skill;
    BattleStatusEffectState attackerStatusEffects;
    BattleStatusEffectState defenderStatusEffects;
    int sharedBleedMaxStacks = 1;
    int randomDamageVariance = 0;
};

struct BattleHitResolutionResult
{
    int attackerUnitId{};
    int defenderUnitId{};
    std::vector<BattleGameplayCommand> commands;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
    bool dodged = false;
    bool reflected = false;
    bool critical = false;
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

BattleProjectileFollowUpExpansion expandBattleAreaProjectileFollowUp(
    const BattleAreaProjectileFollowUp& followUp,
    BattleProjectileFollowUpContext& context,
    const BattleUnitStore& units);

class BattleHitResolver
{
public:
    BattleHitResolutionResult resolve(
        const BattleHitResolutionInput& input,
        RoleComboState& attackerCombo,
        RoleComboState& defenderCombo,
        BattleRuntimeRandom& random) const;
};

}  // namespace KysChess::Battle
