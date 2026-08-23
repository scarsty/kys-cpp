#pragma once

#include "../ChessBattleEffects.h"
#include "../Point.h"
#include "BattleAttackSystem.h"
#include "BattleDamageSystem.h"
#include "BattleOperation.h"
#include "BattlePresentation.h"
#include "BattleProjectileTargetingSystem.h"
#include "BattleUnitValues.h"

#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace KysChess::Battle
{

class BattleRuntimeRandom;
class BattleRuntimeUnits;
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
    int executeThresholdPct{};
    bool canTriggerDefenderBlock{};
    int frozenFrames{};
    std::string skillName;
    std::vector<BattleLogTextSegment> segments;
    bool triggersDefenseEffects = true;
    int criticalMultiplier{};
    int skillId = -1;
    BattleAttackProvenance provenance;
    BattleDamageKind damageKind = BattleDamageKind::Physical;
    int combinedDamageReductionBasisPoints{};
};

struct BattleMpDamageCommand
{
    int sourceUnitId{};
    int targetUnitId{};
    BattleDamageRequest damage;
    bool canTriggerDefenderBlock{};
    BattleAttackProvenance provenance;
};

struct BattleAcceptedHitSideEffectCommand
{
    int sourceUnitId{};
    int targetUnitId{};
    BattleDamageRequest damage;
    BattleAttackProvenance provenance;
};

struct BattleProjectileSpawnCommand
{
    BattleAttackSpawnRequest request;
    std::optional<BattleAttackProvenance> sourceAttack;
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
    Pointf direction;
    double distance = 0.0;
    int lockFrames = 1;
    ForceMoveDirection semanticDirection = ForceMoveDirection::AwayFromSource;
    ForceMoveCollision collision = ForceMoveCollision::StopBeforeOccupied;
    ForceMoveBlockedResult blocked = ForceMoveBlockedResult::Shorten;
};

struct BattleRumbleCommand
{
    int lowFrequency{};
    int highFrequency{};
    int durationMs{};
};

struct BattleAreaProjectileFollowUp
{
    BattleCastProvenance cast;
    std::optional<BattleAttackProvenance> sourceAttack;
    CastWorkToken expansionWork;
    bool ownsRootCast = false;
    int sourceUnitId{};
    int areaSize{};
    int trackedTargetUnitId = -1;
    int maxTargets{};
    int effectId{};
    int damage{};
    int damagePct{};
    BattleDamageKind damageKind = BattleDamageKind::Physical;
    int stunFrames{};
    std::string reason;
    std::string logText;
};

using BattleGameplayCommand = std::variant<
    BattleHpDamageCommand,
    BattleMpDamageCommand,
    BattleAcceptedHitSideEffectCommand,
    BattleProjectileSpawnCommand,
    BattleNearbyTrackingProjectilesCommand,
    BattleAutoUltimateCommand,
    BattleKnockbackCommand,
    BattleRumbleCommand>;

struct BattleRuntimeEffectRuleHandle
{
    EffectSourceBinding binding;
    EffectRuleId ruleId;
};

struct BattleKnockbackProcDescriptor
{
    int chancePct{};
    ForceMoveAction action;
};

struct BattleNearbyTrackingProcDescriptor
{
    BattleRuntimeEffectRuleHandle rule;
    int chancePct{};
    NearbyTrackingAttackBehavior behavior;
};

struct BattleHitDamageModifier
{
    DamageModifierOperation operation{};
    int amount{};
    int stackCount = 1;
};

struct BattleHitDamageModifierPhases
{
    std::vector<BattleHitDamageModifier> outgoingBeforeCritical;
    std::vector<BattleHitDamageModifier> outgoingAfterCritical;
    std::vector<BattleHitDamageModifier> incomingBase;
    std::vector<BattleHitDamageModifier> incomingAfterBase;
    std::vector<BattleHitDamageModifier> outgoingFinal;
    std::vector<BattleHitDamageModifier> incomingFinal;
};

struct BattleHitResolutionInput
{
    BattleAttackEvent attackEvent;
    BattleHitUnitSnapshot attacker;
    BattleHitUnitSnapshot defender;
    BattleHitSkillSnapshot skill;
    int attackerCriticalChancePct = 0;
    int attackerCriticalMultiplierPct = 150;
    bool forceCritical = false;
    int defenderProjectileReflectChancePct = 0;
    int defenderSkillReflectPercent = 0;
    int attackerCooldownExtensionChancePct = 0;
    int attackerCooldownExtensionPct = 0;
    int defenderCooldownExtensionChancePct = 0;
    int defenderCooldownExtensionPct = 0;
    BattleHitDamageModifierPhases damageModifiers;
    int sharedBleedMaxStacks = 1;
    int randomDamageVariance = 0;
    std::vector<BattleKnockbackProcDescriptor> knockbackProcs;
    std::vector<BattleNearbyTrackingProcDescriptor> nearbyTrackingProcs;
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
    int criticalMultiplier = 0;
    double shapedHpDamage = 0.0;
    int finalHpDamage = 0;
    int finalMpDamage = 0;
    std::vector<BattleRuntimeEffectRuleHandle> activatedRuntimeRules;
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
    std::span<const BattleGameplayCommand> commands,
    BattleProjectileFollowUpContext& context,
    const BattleRuntimeUnits& units);

BattleProjectileFollowUpExpansion expandBattleAreaProjectileFollowUp(
    const BattleAreaProjectileFollowUp& followUp,
    BattleProjectileFollowUpContext& context,
    const BattleRuntimeUnits& units);

class BattleHitResolver
{
public:
    BattleHitResolutionResult resolve(
        const BattleHitResolutionInput& input,
        BattleRuntimeRandom& random) const;
};

}  // namespace KysChess::Battle
