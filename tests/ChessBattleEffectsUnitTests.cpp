#include "ChessBattleEffectParser.h"
#include "ChessBattleEffectSemantics.h"
#include "ChessBattleEffectValidation.h"
#include "ChessEffectDescription.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessMagicEffectDisplay.h"
#include "ChessMenuFormatting.h"
#include "ChessReplayHash.h"
#include "ChessUiCommon.h"
#include "DisplayText.h"
#include "Types.h"
#include "battle/BattleEffectSystem.h"

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

using namespace KysChess;

namespace
{

std::string descriptionText(
    std::span<const EffectRule> rules,
    EffectDescriptionStyle style,
    const EffectDescriptionPresentationContext& context)
{
    return joinEffectDescriptionRows(renderEffectDescription(
        buildEffectDescriptionDocument({
            EffectDescriptionContainerKind::Magic,
            rules,
        }),
        style,
        context));
}

const ChessMagicEffectDefinition& definitionWithId(
    const std::vector<ChessMagicEffectDefinition>& definitions,
    int magicId)
{
    const auto it = std::ranges::find(definitions, magicId, &ChessMagicEffectDefinition::magicId);
    REQUIRE(it != definitions.end());
    return *it;
}

const EffectRule& ruleWithEvent(
    const ChessMagicEffectDefinition& definition,
    EffectEvent event,
    std::size_t occurrence = 0)
{
    for (const auto& rule : definition.rules)
    {
        if (rule.event != event) continue;
        if (occurrence == 0) return rule;
        --occurrence;
    }
    FAIL("找不到指定效果事件");
}

EffectRule parseRuleText(std::string_view yaml, std::uint64_t id = 1)
{
    EffectRule rule;
    INFO(yaml);
    REQUIRE(parseEffectRule(
        YAML::Load(std::string(yaml)),
        rule,
        EffectRuleId{ id },
        "簡式語法測試"));
    return rule;
}

std::size_t countOccurrences(std::string_view text, std::string_view fragment)
{
    std::size_t count = 0;
    for (std::size_t position = 0;
         (position = text.find(fragment, position)) != std::string_view::npos;
         position += fragment.size())
    {
        ++count;
    }
    return count;
}

void checkEffectNumberEqual(const EffectNumber& lhs, const EffectNumber& rhs)
{
    CHECK(lhs.base == rhs.base);
    CHECK(lhs.multiplierBase == rhs.multiplierBase);
    CHECK(lhs.status == rhs.status);
    CHECK(lhs.stateSlot == rhs.stateSlot);
    CHECK(lhs.flat == rhs.flat);
    CHECK(lhs.percent == rhs.percent);
    CHECK(lhs.rounding == rhs.rounding);
    CHECK(lhs.minimum == rhs.minimum);
    CHECK(lhs.maximum == rhs.maximum);
}

void checkOptionalEffectNumberEqual(
    const std::optional<EffectNumber>& lhs,
    const std::optional<EffectNumber>& rhs)
{
    REQUIRE(lhs.has_value() == rhs.has_value());
    if (lhs) checkEffectNumberEqual(*lhs, *rhs);
}

void checkSelectorEqual(const EffectSelector& lhs, const EffectSelector& rhs)
{
    CHECK(lhs.kind == rhs.kind);
    CHECK(lhs.count == rhs.count);
    CHECK(lhs.radiusTiles == rhs.radiusTiles);
    CHECK(lhs.squareSideTiles == rhs.squareSideTiles);
    CHECK(lhs.team == rhs.team);
    CHECK(lhs.tieBreak == rhs.tieBreak);
    CHECK(lhs.excludeOwner == rhs.excludeOwner);
    CHECK(lhs.requiredBoundMagic == rhs.requiredBoundMagic);
    CHECK(lhs.requiredMartialCategory == rhs.requiredMartialCategory);
    CHECK(lhs.requiredTarget == rhs.requiredTarget);
}

void checkOptionalSelectorEqual(
    const std::optional<EffectSelector>& lhs,
    const std::optional<EffectSelector>& rhs)
{
    REQUIRE(lhs.has_value() == rhs.has_value());
    if (lhs) checkSelectorEqual(*lhs, *rhs);
}

void checkConditionEqual(const EffectCondition& lhs, const EffectCondition& rhs)
{
    REQUIRE(lhs.index() == rhs.index());
    std::visit([&](const auto& left)
    {
        using T = std::decay_t<decltype(left)>;
        const auto& right = std::get<T>(rhs);
        if constexpr (std::is_same_v<T, SourceHpRatioAtMostCondition>
                           || std::is_same_v<T, SourceHpRatioBelowCondition>
                           || std::is_same_v<T, TargetHpRatioAtMostCondition>)
            CHECK(left.percent == right.percent);
        else if constexpr (std::is_same_v<T, SourceHasStateCondition>
                           || std::is_same_v<T, TargetHasStateCondition>
                           || std::is_same_v<T, TargetHasStateFromEffectOwnerCondition>)
            CHECK(left.state == right.state);
        else if constexpr (std::is_same_v<T, SourceStackAtLeastCondition>)
        {
            CHECK(left.stack == right.stack);
            CHECK(left.count == right.count);
        }
        else if constexpr (std::is_same_v<T, OtherLivingAllyUsesBoundMagicCondition>)
            CHECK(true);
        else if constexpr (std::is_same_v<T, CastDistinctTargetCountAtLeastCondition>)
            CHECK(left.count == right.count);
        else if constexpr (std::is_same_v<T, AttackOrdinalEqualsCondition>)
            CHECK(left.ordinal == right.ordinal);
        else if constexpr (std::is_same_v<T, HealKindInCondition>
                           || std::is_same_v<T, DamageKindInCondition>)
            CHECK(left.kinds == right.kinds);
        else if constexpr (std::is_same_v<T, AcceptedHitCondition>)
        {
            CHECK(left.requirePositiveDamage == right.requirePositiveDamage);
        }
        else if constexpr (std::is_same_v<T, DamagePerspectiveCondition>)
            CHECK(left.perspective == right.perspective);
    }, lhs);
}

void checkApplyStatusEqual(const ApplyStatusAction& lhs, const ApplyStatusAction& rhs)
{
    CHECK(lhs.status == rhs.status);
    CHECK(lhs.durationFrames == rhs.durationFrames);
    checkOptionalEffectNumberEqual(lhs.duration, rhs.duration);
    checkOptionalEffectNumberEqual(lhs.applicationCount, rhs.applicationCount);
    CHECK(lhs.stacks == rhs.stacks);
    checkEffectNumberEqual(lhs.potency, rhs.potency);
    checkEffectNumberEqual(lhs.secondaryPotency, rhs.secondaryPotency);
    CHECK(lhs.stack == rhs.stack);
    CHECK(lhs.stackLimit == rhs.stackLimit);
    CHECK(lhs.aggregatePotencyWithinEvent == rhs.aggregatePotencyWithinEvent);
}

void checkAttackPatternEqual(const AttackPattern& lhs, const AttackPattern& rhs)
{
    CHECK(lhs.kind == rhs.kind);
    CHECK(lhs.projectileCount == rhs.projectileCount);
    CHECK(lhs.spreadDegrees == rhs.spreadDegrees);
    CHECK(lhs.intervalFrames == rhs.intervalFrames);
}

void checkAttackRuntimeBehaviorEqual(
    const AttackRuntimeBehavior& lhs,
    const AttackRuntimeBehavior& rhs)
{
    REQUIRE(lhs.index() == rhs.index());
    std::visit([&](const auto& left)
    {
        using T = std::decay_t<decltype(left)>;
        const auto& right = std::get<T>(rhs);
        if constexpr (std::is_same_v<T, ProjectileBounceAttackBehavior>)
        {
            CHECK(left.additionalHits == right.additionalHits);
            CHECK(left.chancePct == right.chancePct);
            CHECK(left.rangePixels == right.rangePixels);
        }
        else if constexpr (std::is_same_v<T, NearbyTrackingAttackBehavior>)
        {
            CHECK(left.rangePixels == right.rangePixels);
            CHECK(left.damagePct == right.damagePct);
        }
        else if constexpr (std::is_same_v<T, DelayedAlternateAttackBehavior>)
        {
            CHECK(left.delayFrames == right.delayFrames);
            CHECK(left.damagePct == right.damagePct);
            CHECK(left.attackerBlockGainChancePct == right.attackerBlockGainChancePct);
        }
        else if constexpr (std::is_same_v<T, ExpandingSpiralAttackBehavior>)
        {
            CHECK(left.projectileCount == right.projectileCount);
            CHECK(left.bleedStacks == right.bleedStacks);
        }
    }, lhs);
}

void checkAreaModifierEqual(const AreaModifier& lhs, const AreaModifier& rhs)
{
    CHECK(lhs.kind == rhs.kind);
    CHECK(lhs.relation == rhs.relation);
    CHECK(lhs.attribute == rhs.attribute);
    checkEffectNumberEqual(lhs.amount, rhs.amount);
    CHECK(lhs.percent == rhs.percent);
    CHECK(lhs.damageChannel == rhs.damageChannel);
    CHECK(lhs.tracking == rhs.tracking);
    CHECK(lhs.speedPct == rhs.speedPct);
    CHECK(lhs.projectilePressurePct == rhs.projectilePressurePct);
    CHECK(lhs.blockedDirection == rhs.blockedDirection);
    CHECK(lhs.overlap == rhs.overlap);
    CHECK(lhs.trackingOverlap == rhs.trackingOverlap);
    CHECK(lhs.speedOverlap == rhs.speedOverlap);
    CHECK(lhs.projectilePressureOverlap == rhs.projectilePressureOverlap);
}

void checkStateMachineEqual(const StateMachineAction& lhs, const StateMachineAction& rhs)
{
    REQUIRE(lhs.index() == rhs.index());
    std::visit([&](const auto& left)
    {
        using T = std::decay_t<decltype(left)>;
        const auto& right = std::get<T>(rhs);
        if constexpr (std::is_same_v<T, ChangeStateValueAction>)
        {
            CHECK(left.slot == right.slot);
            CHECK(left.delta == right.delta);
            CHECK(left.minimum == right.minimum);
            CHECK(left.maximum == right.maximum);
        }
        else if constexpr (std::is_same_v<T, TransferStateValueAction>)
        {
            CHECK(left.sourceSlot == right.sourceSlot);
            CHECK(left.destinationSlot == right.destinationSlot);
        }
        else if constexpr (std::is_same_v<T, RecordMaximumDamageAction>)
        {
            CHECK(left.slot == right.slot);
            CHECK(left.channel == right.channel);
        }
        else if constexpr (std::is_same_v<T, ConsumeRecordedMaximumAction>)
        {
            CHECK(left.slot == right.slot);
            CHECK(left.destination == right.destination);
            CHECK(left.percent == right.percent);
            CHECK(left.clearAfterConsume == right.clearAfterConsume);
        }
        else if constexpr (std::is_same_v<T, StartDamageAbsorptionAction>)
        {
            CHECK(left.slot == right.slot);
            CHECK(left.absorbedPct == right.absorbedPct);
            CHECK(left.durationFrames == right.durationFrames);
            CHECK(left.settleOnSourceDeath == right.settleOnSourceDeath);
            checkSelectorEqual(left.settlementTarget, right.settlementTarget);
            CHECK(left.settlementDamageKind == right.settlementDamageKind);
            CHECK(left.returnedPct == right.returnedPct);
        }
        else if constexpr (std::is_same_v<T, SettleDamageAbsorptionAction>)
        {
            CHECK(left.slot == right.slot);
            checkSelectorEqual(left.target, right.target);
            CHECK(left.damageKind == right.damageKind);
            CHECK(left.returnedPct == right.returnedPct);
            CHECK(left.clearAfterSettle == right.clearAfterSettle);
        }
        else if constexpr (std::is_same_v<T, BorrowEffectRulesAction>)
        {
            checkSelectorEqual(left.sourceUnits, right.sourceUnits);
            checkEffectNumberEqual(left.sourceCount, right.sourceCount);
            CHECK(left.filter.allowedActionCategories == right.filter.allowedActionCategories);
            CHECK(left.propagation == right.propagation);
        }
        else if constexpr (std::is_same_v<T, CopyAttackDefinitionAction>)
        {
            checkSelectorEqual(left.sourceUnits, right.sourceUnits);
            CHECK(left.filter.conditions == right.filter.conditions);
            CHECK(left.copyCount == right.copyCount);
            CHECK(left.propagation == right.propagation);
        }
        else if constexpr (std::is_same_v<T, SettleRemainingStatusDamageAction>)
            CHECK(left.status == right.status);
        else if constexpr (std::is_same_v<T, GenerateClonesAction>)
            CHECK(left.count == right.count);
        else if constexpr (std::is_same_v<T, PreventDeathAction>)
            CHECK(left.invincibilityFrames == right.invincibilityFrames);
        else if constexpr (std::is_same_v<T, ConfigureRescueRepositionAction>)
        {
            CHECK(left.mode == right.mode);
            CHECK(left.activations == right.activations);
        }
    }, lhs);
}

void checkActionEqual(const EffectAction& lhs, const EffectAction& rhs);

void checkActionsEqual(
    const std::vector<EffectAction>& lhs,
    const std::vector<EffectAction>& rhs)
{
    REQUIRE(lhs.size() == rhs.size());
    for (std::size_t index = 0; index < lhs.size(); ++index)
        checkActionEqual(lhs[index], rhs[index]);
}

void checkActionEqual(const EffectAction& lhs, const EffectAction& rhs)
{
    REQUIRE(lhs.value.index() == rhs.value.index());
    std::visit([&](const auto& left)
    {
        using T = std::decay_t<decltype(left)>;
        const auto& right = std::get<T>(rhs.value);
        if constexpr (std::is_same_v<T, ModifyAttributeAction>)
        {
            CHECK(left.attribute == right.attribute);
            checkEffectNumberEqual(left.amount, right.amount);
            CHECK(left.operation == right.operation);
            CHECK(left.durationFrames == right.durationFrames);
            CHECK(left.stack == right.stack);
            CHECK(left.stackLimit == right.stackLimit);
            CHECK(left.perStack == right.perStack);
            CHECK(left.stackScope == right.stackScope);
        }
        else if constexpr (std::is_same_v<T, ModifyDamageAction>)
        {
            CHECK(left.perspective == right.perspective);
            CHECK(left.stage == right.stage);
            CHECK(left.channel == right.channel);
            checkEffectNumberEqual(left.amount, right.amount);
            CHECK(left.operation == right.operation);
            CHECK(left.durationFrames == right.durationFrames);
            CHECK(left.stack == right.stack);
            CHECK(left.stackLimit == right.stackLimit);
            CHECK(left.stackScope == right.stackScope);
        }
        else if constexpr (std::is_same_v<T, ChangeResourceAction>)
        {
            CHECK(left.resource == right.resource);
            checkEffectNumberEqual(left.amount, right.amount);
            CHECK(left.kind == right.kind);
            checkOptionalSelectorEqual(left.transferDestination, right.transferDestination);
            CHECK(left.healKind == right.healKind);
            CHECK(left.healSourcePolicy == right.healSourcePolicy);
        }
        else if constexpr (std::is_same_v<T, ModifyHealTransactionAction>)
        {
            CHECK(left.operation == right.operation);
            CHECK(left.kinds == right.kinds);
            CHECK(left.percent == right.percent);
        }
        else if constexpr (std::is_same_v<T, ApplyStatusAction>)
            checkApplyStatusEqual(left, right);
        else if constexpr (std::is_same_v<T, ConsumeStatusAction>)
        {
            CHECK(left.status == right.status);
            CHECK(left.stacks == right.stacks);
            CHECK(left.source == right.source);
            REQUIRE(left.whenDepleted.has_value() == right.whenDepleted.has_value());
            if (left.whenDepleted) checkApplyStatusEqual(*left.whenDepleted, *right.whenDepleted);
        }
        else if constexpr (std::is_same_v<T, RemoveStatusAction>)
        {
            CHECK(left.statuses == right.statuses);
            CHECK(left.negativeOnly == right.negativeOnly);
            CHECK(left.controlOnly == right.controlOnly);
            CHECK(left.clearCurrentActionStagger == right.clearCurrentActionStagger);
            CHECK(left.count == right.count);
            CHECK(left.order == right.order);
        }
        else if constexpr (std::is_same_v<T, DealDamageAction>)
        {
            checkEffectNumberEqual(left.amount, right.amount);
            checkOptionalEffectNumberEqual(left.transactionCount, right.transactionCount);
            CHECK(left.kind == right.kind);
            CHECK(left.appliesDamageModifiers == right.appliesDamageModifiers);
            CHECK(left.triggersHurtInvincibility == right.triggersHurtInvincibility);
            CHECK(left.area.kind == right.area.kind);
            CHECK(left.area.radiusTiles == right.area.radiusTiles);
            CHECK(left.area.squareSideTiles == right.area.squareSideTiles);
            CHECK(left.perCast.perTargetLimit == right.perCast.perTargetLimit);
            REQUIRE(left.areaProjectiles.has_value() == right.areaProjectiles.has_value());
            if (left.areaProjectiles)
            {
                CHECK(left.areaProjectiles->rangeTiles == right.areaProjectiles->rangeTiles);
                CHECK(left.areaProjectiles->maximumTargets == right.areaProjectiles->maximumTargets);
                CHECK(left.areaProjectiles->stunFrames == right.areaProjectiles->stunFrames);
                CHECK(left.areaProjectiles->trackEventSource == right.areaProjectiles->trackEventSource);
                CHECK(left.areaProjectiles->visual == right.areaProjectiles->visual);
            }
        }
        else if constexpr (std::is_same_v<T, ModifyAttackAction>)
        {
            checkAttackPatternEqual(left.pattern, right.pattern);
            CHECK(left.strengthPct == right.strengthPct);
            CHECK(left.through == right.through);
            CHECK(left.tracking == right.tracking);
            CHECK(left.mainProjectile == right.mainProjectile);
            CHECK(left.sameTargetHitLimit == right.sameTargetHitLimit);
            CHECK(left.targets == right.targets);
            CHECK(left.propagation == right.propagation);
            CHECK(left.addToBaseAttack == right.addToBaseAttack);
            checkOptionalSelectorEqual(left.source, right.source);
            checkOptionalEffectNumberEqual(left.damageOverride, right.damageOverride);
            CHECK(left.damageKind == right.damageKind);
            checkAttackRuntimeBehaviorEqual(left.runtimeBehavior, right.runtimeBehavior);
        }
        else if constexpr (std::is_same_v<T, ForceMoveAction>)
        {
            CHECK(left.direction == right.direction);
            CHECK(left.distanceTiles == right.distanceTiles);
            CHECK(left.distancePixels == right.distancePixels);
            CHECK(left.lockFrames == right.lockFrames);
            CHECK(left.collision == right.collision);
            CHECK(left.blocked == right.blocked);
        }
        else if constexpr (std::is_same_v<T, CreateAreaAction>)
        {
            CHECK(left.shape == right.shape);
            CHECK(left.radiusTiles == right.radiusTiles);
            CHECK(left.squareSideTiles == right.squareSideTiles);
            CHECK(left.anchor == right.anchor);
            CHECK(left.durationFrames == right.durationFrames);
            CHECK(left.sourceDeath == right.sourceDeath);
            CHECK(left.merge == right.merge);
            REQUIRE(left.modifiers.size() == right.modifiers.size());
            for (std::size_t index = 0; index < left.modifiers.size(); ++index)
                checkAreaModifierEqual(left.modifiers[index], right.modifiers[index]);
        }
        else if constexpr (std::is_same_v<T, ModifyCastAction>)
        {
            checkOptionalEffectNumberEqual(left.mpCost, right.mpCost);
            CHECK(left.rangeMode == right.rangeMode);
            CHECK(left.projectileSpeedPct == right.projectileSpeedPct);
            CHECK(left.minimumSelectDistance == right.minimumSelectDistance);
            CHECK(left.additionalProjectiles == right.additionalProjectiles);
            CHECK(left.mobility == right.mobility);
            REQUIRE(left.autoUltimate.has_value() == right.autoUltimate.has_value());
            if (left.autoUltimate)
            {
                CHECK(left.autoUltimate->consumeMp == right.autoUltimate->consumeMp);
                CHECK(left.autoUltimate->announce == right.autoUltimate->announce);
            }
            REQUIRE(left.replacementPattern.has_value() == right.replacementPattern.has_value());
            if (left.replacementPattern)
                checkAttackPatternEqual(*left.replacementPattern, *right.replacementPattern);
            CHECK(left.freeAdditionalCast == right.freeAdditionalCast);
            CHECK(left.propagation == right.propagation);
        }
        else if constexpr (std::is_same_v<T, StateMachineAction>)
            checkStateMachineEqual(left, right);
        else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
        {
            REQUIRE(static_cast<bool>(left) == static_cast<bool>(right));
            if (!left) return;
            REQUIRE(left->conditions.size() == right->conditions.size());
            for (std::size_t index = 0; index < left->conditions.size(); ++index)
                checkConditionEqual(left->conditions[index], right->conditions[index]);
            checkActionsEqual(left->whenTrue, right->whenTrue);
            checkActionsEqual(left->whenFalse, right->whenFalse);
        }
    }, lhs.value);
}

void checkRulesEqual(const EffectRule& lhs, const EffectRule& rhs)
{
    CHECK(lhs.id == rhs.id);
    CHECK(lhs.event == rhs.event);
    CHECK(lhs.observation == rhs.observation);
    CHECK(lhs.castMatch == rhs.castMatch);
    checkSelectorEqual(lhs.selector, rhs.selector);
    REQUIRE(lhs.conditions.size() == rhs.conditions.size());
    for (std::size_t index = 0; index < lhs.conditions.size(); ++index)
        checkConditionEqual(lhs.conditions[index], rhs.conditions[index]);
    CHECK(lhs.chancePct == rhs.chancePct);
    CHECK(lhs.maxActivations == rhs.maxActivations);
    CHECK(lhs.sharedCooldownFrames == rhs.sharedCooldownFrames);
    CHECK(lhs.intervalFrames == rhs.intervalFrames);
    CHECK(lhs.everyNthEvent == rhs.everyNthEvent);
    REQUIRE(lhs.activationLimit.has_value() == rhs.activationLimit.has_value());
    if (lhs.activationLimit)
    {
        CHECK(lhs.activationLimit->scope == rhs.activationLimit->scope);
        CHECK(lhs.activationLimit->maxEvaluations == rhs.activationLimit->maxEvaluations);
    }
    checkOptionalEffectNumberEqual(lhs.repetitionCount, rhs.repetitionCount);
    checkActionsEqual(lhs.actions, rhs.actions);
    for (const auto style : {
             EffectDescriptionStyle::Detailed,
             EffectDescriptionStyle::Full,
             EffectDescriptionStyle::Compact,
         })
        CHECK(descriptionText(std::span<const EffectRule>{&(lhs), 1}, style, {}) == descriptionText(std::span<const EffectRule>{&(rhs), 1}, style, {}));
}

std::set<int> poolUltimateMagicIds(const ChessGameContent& content)
{
    std::set<int> result;
    for (const int roleId : content.poolRoleIds())
    {
        const auto* role = content.role(roleId);
        REQUIRE(role != nullptr);
        int roleUltimateId = -1;
        for (int star = 1; star <= 3; ++star)
        {
            const auto magics = chessRoleMagicsForStar(content, *role, star);
            REQUIRE_FALSE(magics.empty());
            const int ultimateId = magics.back().first->ID;
            if (roleUltimateId >= 0) CHECK(ultimateId == roleUltimateId);
            roleUltimateId = ultimateId;
        }
        result.insert(roleUltimateId);
    }
    return result;
}

}  // namespace

TEST_CASE("ChessBattleEffects_DescriptorOwnedProbesReachEveryAuthorVariantAndField",
          "[battle][effects][schema][descriptor][probe]")
{
    std::string error;
    const bool valid = validateEffectAuthoringDescriptorProbes(error);
    INFO(error);
    CHECK(valid);
}

TEST_CASE("BattleEffectRuleStore_BlinkAttackTargetModeIsOwnerScoped", "[battle][effects][rule_store]")
{
    Battle::BattleEffectRuleStore store;

    CHECK_FALSE(store.blinkAttackUsesWeakestTarget(7));
    CHECK_FALSE(store.blinkAttackUsesWeakestTarget(8));

    store.advanceBlinkAttackTargetMode(7);
    CHECK(store.blinkAttackUsesWeakestTarget(7));
    CHECK_FALSE(store.blinkAttackUsesWeakestTarget(8));

    store.advanceBlinkAttackTargetMode(7);
    CHECK_FALSE(store.blinkAttackUsesWeakestTarget(7));

    store.advanceBlinkAttackTargetMode(8);
    CHECK_FALSE(store.blinkAttackUsesWeakestTarget(7));
    CHECK(store.blinkAttackUsesWeakestTarget(8));
}

TEST_CASE("ChessBattleEffects_ShorthandTimingsNormalizeToCanonicalRules",
          "[battle][effects][schema][shorthand]")
{
    struct TimingCase
    {
        std::string_view timing;
        EffectEvent event;
        EffectSelectorKind target;
        std::size_t automaticConditions;
    };
    static constexpr TimingCase cases[]{
        { "開場", EffectEvent::BattleInitialized, EffectSelectorKind::Self, 0 },
        { "每幀", EffectEvent::FrameAdvanced, EffectSelectorKind::Self, 0 },
        { "每隔", EffectEvent::FrameAdvanced, EffectSelectorKind::Self, 0 },
        { "絕招冷卻完成", EffectEvent::UltimateCooldownFinished, EffectSelectorKind::Self, 0 },
        { "施放規劃", EffectEvent::CastPlanned, EffectSelectorKind::Self, 0 },
        { "攻擊提交", EffectEvent::AttackCommitted, EffectSelectorKind::Self, 0 },
        { "絕招施放", EffectEvent::UltimateCommitted, EffectSelectorKind::Self, 0 },
        { "攻擊生成", EffectEvent::AttackSpawned, EffectSelectorKind::Self, 0 },
        { "主彈命中", EffectEvent::MainProjectileBeforeDamage, EffectSelectorKind::HitTarget, 0 },
        { "命中", EffectEvent::HitBeforeDamage, EffectSelectorKind::HitTarget, 0 },
        { "治療嘗試", EffectEvent::HealAttempted, EffectSelectorKind::Self, 0 },
        { "治療套用", EffectEvent::HealApplied, EffectSelectorKind::Self, 0 },
        { "施放延續", EffectEvent::CastContinuation, EffectSelectorKind::Self, 0 },
        { "施放結算完成", EffectEvent::CastSettled, EffectSelectorKind::Self, 0 },
        { "護盾破裂", EffectEvent::ShieldBroken, EffectSelectorKind::Self, 0 },
        { "單位死亡", EffectEvent::UnitDied, EffectSelectorKind::Self, 0 },
        { "友軍死亡", EffectEvent::AllyDied, EffectSelectorKind::Self, 0 },
        { "造成傷害後", EffectEvent::DamageResolved, EffectSelectorKind::Self, 1 },
        { "受傷後", EffectEvent::DamageResolved, EffectSelectorKind::Self, 1 },
        { "擊殺後", EffectEvent::DamageResolved, EffectSelectorKind::Self, 2 },
    };
    for (const auto& test : cases)
    {
        CAPTURE(test.timing);
        const auto action = test.event == EffectEvent::HealAttempted
            ? "治療交易修正:\n  方式: 阻止\n  治療種類:\n    - 直接治療\n"
            : "獲得護盾: 1\n";
        auto yaml = std::format(
            "時機: {}\n{}{}",
            test.timing,
            test.timing == "每隔" ? "間隔幀數: 30\n" : "",
            action);
        const auto rule = parseRuleText(yaml);
        CHECK(rule.event == test.event);
        CHECK(rule.selector.kind == test.target);
        CHECK(rule.conditions.size() == test.automaticConditions);
        if (test.timing == "每隔") CHECK(rule.intervalFrames == 30);
        CHECK(rule.actions.size() == 1);
    }

    const auto canonical = parseRuleText(R"(
時機: 造成傷害後
目標: 自身
條件:
  - 已接受命中
  - 傷害來自招式
機率: 75
次數: 4
同來源冷卻幀數: 12
每N次事件: 3
動作:
  - 資源變更:
      資源: 內力
      方式: 回復
      數值:
        基準: 實際生命傷害
        百分比: 50
)");
    const auto shorthand = parseRuleText(R"(
時機: 造成傷害後
目標: 自身
條件:
  - 已接受命中
  - 傷害來自招式
機率: 75
次數: 4
同來源冷卻幀數: 12
每N次事件: 3
回復內力:
  實際生命傷害百分比: 50
)");
    checkRulesEqual(canonical, shorthand);
}

TEST_CASE("ChessBattleEffects_NamedActionsAndClosedMacrosMatchCanonicalPayloads",
          "[battle][effects][schema][shorthand]")
{
    const auto attributeCanonical = parseRuleText(R"(
時機: 絕招施放
目標: 自身
動作:
  - 屬性修正:
      屬性: 攻擊
      方式: 固定加算
      數值: 50
      持續幀數: 90
      合併方式: 增加層數
      層數上限: 3
      每層: true
      疊加範圍: 事件來源
  - 屬性修正:
      屬性: 防禦
      方式: 固定加算
      數值: 30
      持續幀數: 90
      合併方式: 增加層數
      層數上限: 3
      每層: true
      疊加範圍: 事件來源
  - 屬性修正:
      屬性: 速度
      方式: 百分比加算
      數值: 15
      持續幀數: 90
      合併方式: 增加層數
      層數上限: 3
      每層: true
      疊加範圍: 事件來源
  - 屬性修正:
      屬性: 暴擊率
      方式: 百分比加算
      數值: 10
      持續幀數: 90
      合併方式: 增加層數
      層數上限: 3
      每層: true
      疊加範圍: 事件來源
)");
    const auto attributeShorthand = parseRuleText(R"(
時機: 絕招施放
目標: 自身
屬性加成:
  防禦: 30
  攻擊: 50
  百分比:
    暴擊率: 10
    速度: 15
  持續幀數: 90
  合併方式: 增加層數
  層數上限: 3
  每層: true
  疊加範圍: 事件來源
)");
    checkRulesEqual(attributeCanonical, attributeShorthand);

    const auto poisonShorthand = parseRuleText(R"(
時機: 命中
目標: 命中目標
施毒:
  持續幀數: 90
  層數: 3
  強度: 7
)");
    auto poisonCanonical = poisonShorthand;
    ApplyStatusAction standardPoison;
    standardPoison.status = BattleStatusKind::Poison;
    standardPoison.durationFrames = 90;
    standardPoison.stacks = 3;
    standardPoison.potency.flat = 7;
    standardPoison.stack = EffectStackPolicy::KeepStrongest;
    standardPoison.stackLimit = 3;
    standardPoison.aggregatePotencyWithinEvent = true;
    poisonCanonical.actions = { EffectAction{ standardPoison } };
    checkRulesEqual(poisonCanonical, poisonShorthand);

    const auto resetPoisonShorthand = parseRuleText(R"(
時機: 絕招施放
目標: 所有敵人
施毒:
  模式: 取代重設
  持續幀數: 150
  層數: 5
  強度: 10
)");
    auto resetPoisonCanonical = resetPoisonShorthand;
    ApplyStatusAction resetPoison;
    resetPoison.status = BattleStatusKind::Poison;
    resetPoison.durationFrames = 150;
    resetPoison.stacks = 5;
    resetPoison.potency.flat = 10;
    resetPoison.stack = EffectStackPolicy::Replace;
    resetPoison.stackLimit = 5;
    resetPoisonCanonical.actions = { EffectAction{ resetPoison } };
    checkRulesEqual(resetPoisonCanonical, resetPoisonShorthand);

    const auto boundedPoisonShorthand = parseRuleText(R"(
時機: 命中
目標: 命中目標
施毒:
  持續幀數:
    固定: 90
    最大: 30
  層數: 3
  強度: 7
)");
    auto boundedPoisonCanonical = boundedPoisonShorthand;
    standardPoison.durationFrames = 30;
    boundedPoisonCanonical.actions = { EffectAction{ standardPoison } };
    checkRulesEqual(boundedPoisonCanonical, boundedPoisonShorthand);

    const auto controlledCanonical = parseRuleText(R"(
時機: 命中
目標: 命中目標
條件:
  - 僅限主彈道
觸發限制:
  範圍: 每次施放每個目標
  次數: 2
重複次數: {固定: 2}
動作:
  - 套用狀態:
      狀態: 寒毒
      持續幀數: 90
      層數: 2
      強度: {基準: 來源最大生命, 百分比: 3}
      次要強度: 4
      合併方式: 增加層數
      層數上限: 6
)");
    const auto controlledShorthand = parseRuleText(R"(
時機: 命中
目標: 命中目標
條件: [僅限主彈道]
觸發限制:
  範圍: 每次施放每個目標
  次數: 2
重複次數: 2
動作:
  - 套用狀態:
      狀態: 寒毒
      持續幀數: 90
      層數: 2
      強度: {來源最大生命百分比: 3}
      次要強度: 4
      合併方式: 增加層數
      層數上限: 6
)");
    checkRulesEqual(controlledCanonical, controlledShorthand);

    struct RulePair { std::string_view canonical; std::string_view shorthand; };
    static constexpr RulePair pairs[]{
        { R"(
時機: 造成傷害後
目標: 自身
動作:
  - 資源變更:
      資源: 生命
      方式: 回復
      數值: {基準: 目標最大生命, 百分比: 8}
      治療種類: 擊殺獎勵
)", R"(
時機: 造成傷害後
目標: 自身
回復生命:
  數值: {目標最大生命百分比: 8}
  治療種類: 擊殺獎勵
)" },
        { R"(
時機: 造成傷害後
目標: 自身
動作:
  - 資源變更:
      資源: 護盾
      方式: 獲得
      數值: {基準: 來源星級, 百分比: 10000}
)", R"(
時機: 造成傷害後
目標: 自身
獲得護盾: {每星級: 100}
)" },
        { R"(
時機: 開場
目標: 自身
動作:
  - 傷害修正:
      方位: 造成
      階段: 防禦前
      傷害種類: 招式
      方式: 忽略防禦百分比
      數值: 30
)", R"(
時機: 開場
目標: 自身
忽略防禦: 30
)" },
        { R"(
時機: 開場
目標: 自身
動作:
  - 傷害修正:
      方位: 承受
      階段: 最終
      傷害種類: 全部
      方式: 單次承傷上限
      數值: 20
)", R"(
時機: 開場
目標: 自身
單次承傷上限: 20
)" },
        { R"(
時機: 主彈命中
目標: 命中目標
動作:
  - 強制移動:
      方向: 遠離來源
      距離格數: 4
      碰撞: 阻擋前停止
      受阻結果: 縮短
)", R"(
時機: 主彈命中
目標: 命中目標
擊退: {距離格數: 4}
)" },
        { R"(
時機: 主彈命中
目標: 命中目標
動作:
  - 強制移動:
      方向: 接近來源
      距離格數: 2
      碰撞: 阻擋前停止
      受阻結果: 縮短
)", R"(
時機: 主彈命中
目標: 命中目標
拉近: {距離格數: 2}
)" },
    };
    for (const auto& pair : pairs)
        checkRulesEqual(parseRuleText(pair.canonical), parseRuleText(pair.shorthand));

    const auto nested = parseRuleText(R"(
時機: 造成傷害後
目標: 自身
動作:
  - 條件分支:
      條件: [已接受命中]
      成立:
        - 回復內力: 10
        - 屬性加成:
            攻擊: 5
            防禦: 3
)");
    const auto& conditional = std::get<std::shared_ptr<ConditionalEffectAction>>(
        nested.actions.front().value);
    REQUIRE(conditional);
    CHECK(conditional->whenTrue.size() == 3);
}

TEST_CASE("ChessBattleEffects_AllNamedConditionFormsReachTypedConditions",
          "[battle][effects][schema][conditions]")
{
    struct ConditionCase
    {
        std::string_view timing;
        std::string_view authorNode;
        std::size_t variantIndex;
    };
    static constexpr ConditionCase cases[]{
        { "攻擊提交", "  - 僅限絕招", 0 },
        { "施放規劃", "  - 施放武功為效果來源", 1 },
        { "命中", "  - 僅限主彈道", 2 },
        { "命中", "  - 僅限根攻擊", 3 },
        { "造成傷害後", "  - 自身生命不高於: 40", 4 },
        { "造成傷害後", "  - 自身生命低於: 30", 5 },
        { "造成傷害後", "  - 自身為最後存活", 6 },
        { "造成傷害後", "  - 目標生命不高於: 20", 7 },
        { "造成傷害後", "  - 目標非無敵", 8 },
        { "造成傷害後", "  - 自身有狀態: 戰意", 9 },
        { "造成傷害後", "  - 目標有狀態: 中毒", 10 },
        { "造成傷害後", "  - 目標有此來源狀態: 寒毒", 11 },
        { "造成傷害後", "  - 自身層數至少:\n      狀態: 戰意\n      層數: 2", 12 },
        { "施放規劃", "  - 其他存活友軍使用此武功", 13 },
        { "施放結算完成", "  - 不同目標數至少: 2", 14 },
        { "命中", "  - 攻擊序號: 1", 15 },
        { "治療套用", "  - 治療種類符合: [命中, 吸血]", 16 },
        { "造成傷害後", "  - 傷害來自招式", 17 },
        { "造成傷害後", "  - 傷害造成死亡", 18 },
        { "造成傷害後", "  - 已接受命中", 19 },
        { "單位死亡", "  - 事件目標屬於綁定來源", 20 },
        { "造成傷害後", "  - 傷害種類符合: [招式, 特效]", 22 },
        { "施放規劃", "  - 受益者施放前滿內", 23 },
        { "施放規劃", "  - 有合法隨機目標", 24 },
    };
    for (const auto& test : cases)
    {
        CAPTURE(test.authorNode);
        const auto rule = parseRuleText(std::format(R"(時機: {}
目標: 自身
條件:
{}
獲得護盾: 1
)", test.timing, test.authorNode));
        CHECK(std::ranges::any_of(rule.conditions, [&](const EffectCondition& condition)
        {
            return condition.index() == test.variantIndex;
        }));
    }

    const auto configuredAcceptedHit = parseRuleText(R"(
時機: 造成傷害後
條件:
  - 已接受命中:
      需要正傷害: true
獲得護盾: 1
)");
    const auto acceptedIt = std::ranges::find_if(
        configuredAcceptedHit.conditions,
        [](const EffectCondition& condition)
        {
            return std::holds_alternative<AcceptedHitCondition>(condition);
        });
    REQUIRE(acceptedIt != configuredAcceptedHit.conditions.end());
    const auto& accepted = std::get<AcceptedHitCondition>(*acceptedIt);
    CHECK(accepted.requirePositiveDamage);
}

TEST_CASE("ChessBattleEffects_ShorthandRejectsAmbiguousAndEmptyShapes",
          "[battle][effects][schema][shorthand]")
{
    const auto rejects = [](std::string_view yaml)
    {
        EffectRule rule;
        return !parseEffectRule(
            YAML::Load(std::string(yaml)), rule, EffectRuleId{ 1 }, "不合法簡式");
    };
    const std::string_view invalidRules[]{
        R"(事件: 傷害結算後
獲得護盾: 1)",
        R"(時機: 傷害後
獲得護盾: 1)",
        R"(時機: 造成傷害後
動作:
  - 類型: 資源變更
    資源: 護盾
    方式: 獲得
    數值: 1)",
        R"(時機: 造成傷害後
條件:
  - 類型: 已接受命中
獲得護盾: 1)",
        R"(時機: 開場
屬性加成:
  固定:
    攻擊: 10)",
        R"(時機: 造成傷害後
回復內力: 1
動作: [{類型: 資源變更, 資源: 護盾, 方式: 獲得, 數值: 1}])",
        R"(時機: 造成傷害後
回復內力: 1
獲得護盾: 2)",
        R"(時機: 造成傷害後
動作: [{未知捷徑: 1}])",
        R"(時機: 造成傷害後
條件: [{僅限絕招: true}]
回復內力: 1)",
        R"(時機: 造成傷害後
條件: [{自身層數至少: 2}]
回復內力: 1)",
        R"(時機: 造成傷害後
回復內力: {})",
        R"(時機: 造成傷害後
回復生命: {})",
        R"(時機: 造成傷害後
回復生命: {數值: {}})",
        R"(時機: 造成傷害後
獲得護盾: {})",
        R"(時機: 造成傷害後
忽略防禦: {})",
        R"(時機: 造成傷害後
單次承傷上限: {})",
        R"(時機: 造成傷害後
回復生命: {數值: 10, 百分比: 20})",
        R"(時機: 開場
屬性加成:
  攻擊:
    每星級: 10)",
        R"(時機: 開場
屬性加成:
  攻擊: 10
  攻擊: 20)",
        R"(時機: 開場
屬性加成:
  攻擊: 10
  百分比:
    攻擊: 20)",
        R"(時機: 命中
套用狀態:
  狀態: 中毒
  持續幀數: 90
  層數: 3
  強度: 7
  合併方式: 保留最強
  層數上限: 3
  同事件合計強度: true)",
        R"(時機: 主彈命中
每N次事件: 2
擊退: {距離像素: 40})",
    };
    for (const auto yaml : invalidRules)
    {
        CAPTURE(yaml);
        CHECK(rejects(yaml));
    }
}

TEST_CASE("ChessBattleEffects_AllAuthorMapFamiliesRejectDuplicateRawKeys",
          "[battle][effects][schema][duplicate_keys]")
{
    const auto rejects = [](std::string_view yaml)
    {
        EffectRule rule;
        return !parseEffectRule(
            YAML::Load(std::string(yaml)), rule, EffectRuleId{ 1 }, "重複欄位測試");
    };
    const std::string_view duplicateMaps[]{
        R"(時機: 開場
時機: 開場
獲得護盾: 1)",
        R"(時機: 開場
目標:
  類型: 友軍
  數量: 1
  數量: 2
獲得護盾: 1)",
        R"(時機: 開場
獲得護盾:
  基準: 來源最大生命
  百分比: 10
  百分比: 20)",
        R"(時機: 開場
資源變更:
  資源: 護盾
  方式: 獲得
  數值: 1
  數值: 2)",
        R"(時機: 造成傷害後
條件:
  - 已接受命中:
      需要正傷害: true
      需要正傷害: false
獲得護盾: 1)",
        R"(時機: 單位死亡
造成傷害:
  數值: 10
  傷害種類: 純粹
  區域投射物:
    範圍格數: 3
    最多目標: 2
    最多目標: 3
    眩暈幀數: 1
    特效: 死亡爆炸)",
        R"(時機: 開場
屬性加成:
  攻擊: 10
  攻擊: 20)",
        R"(時機: 開場
動作:
  - 獲得護盾: 1
    獲得護盾: 2)",
    };
    for (const auto yaml : duplicateMaps)
    {
        CAPTURE(yaml);
        CHECK(rejects(yaml));
    }
}

TEST_CASE("ChessBattleEffects_PayloadViewRejectsDescriptorFieldsNotConsumedByTypedBranches",
          "[battle][effects][schema][descriptor][payload_view]")
{
    const std::string_view rules[]{
        R"(時機: 開場
目標:
  類型: 自身
  數量: 1
獲得護盾: 1)",
        R"(時機: 開場
獲得護盾:
  目標最大生命百分比: 10
  固定: 1)",
    };
    for (const auto yaml : rules)
    {
        ChessDiagnosticCollector diagnostics;
        EffectRule rule;
        CAPTURE(yaml);
        CHECK_FALSE(parseEffectRule(
            YAML::Load(std::string(yaml)),
            rule,
            EffectRuleId{ 1 },
            "PayloadView 消耗追蹤",
            diagnostics.sink()));
        CHECK(std::ranges::any_of(diagnostics.diagnostics(), [](const auto& diagnostic)
        {
            return diagnostic.message.find("未被 typed parser 消耗") != std::string::npos;
        }));
    }
}

TEST_CASE("ChessMagicEffectDisplay_InsertsCompactEffectRowsAfterUltimateSkill", "[chess][effects][magic]")
{
    const auto root = YAML::Load(R"(
絕招:
  - 武功: 26
    名稱: 降龍十八掌
    效果:
      - 時機: 主彈命中
        目標: 命中目標
        動作:
          - 套用狀態:
              狀態: 眩暈
              持續幀數: 14
              合併方式: 刷新
      - 時機: 絕招施放
        目標: 自身
        動作:
          - 資源變更:
              資源: 內力
              方式: 回復
              數值: 30
)");

    std::vector<ChessMagicEffectDefinition> definitions;
    REQUIRE(parseMagicEffects(root, definitions, "絕招顯示"));
    REQUIRE(definitions.size() == 1);
    REQUIRE(definitions[0].rules.size() == 2);

    Magic normal;
    normal.ID = 5;
    normal.Name = "寒冰綿掌";
    Magic ultimate;
    ultimate.ID = 26;
    ultimate.Name = "降龍十八掌";

    std::vector<const MagicSave*> magics{ &normal, &ultimate };
    const auto rows = buildChessMagicEffectDisplayRows(magics, definitions, ultimate.ID);

    REQUIRE(rows.size() >= 4);
    CHECK(rows[0].kind == ChessMagicEffectDisplayLineKind::Skill);
    CHECK(rows[0].text == "寒冰綿掌");
    CHECK_FALSE(rows[0].ultimate);
    CHECK(rows[1].kind == ChessMagicEffectDisplayLineKind::Skill);
    CHECK(rows[1].text == "降龍十八掌");
    CHECK(rows[1].ultimate);
    CHECK(rows[2].kind == ChessMagicEffectDisplayLineKind::Effect);
    CHECK(rows[2].text.starts_with("主彈命中"));
    std::string effectText;
    for (std::size_t index = 2; index < rows.size(); ++index)
    {
        CHECK(rows[index].kind == ChessMagicEffectDisplayLineKind::Effect);
        effectText += rows[index].text;
    }
    CHECK(effectText.find("眩暈") != std::string::npos);
    CHECK(effectText.find("14幀") != std::string::npos);
    CHECK(effectText.find("回復30內力") != std::string::npos);
}

TEST_CASE("ChessMagicEffectDisplay_FitsWrappedEffectsInOneBoundedColumn",
          "[chess][effects][magic][layout]")
{
    constexpr int viewportWidth = 244;
    constexpr int viewportHeight = 133;
    Magic normal;
    normal.ID = 1;
    normal.Name = "普通武學";
    Magic ultimate;
    ultimate.ID = 2;
    ultimate.Name = "絕學";
    std::vector<ChessMagicEffectDisplayLine> rows{
        { ChessMagicEffectDisplayLineKind::Skill, &normal, normal.Name },
        { ChessMagicEffectDisplayLineKind::Skill, &ultimate, ultimate.Name, true },
        {
            .kind = ChessMagicEffectDisplayLineKind::Effect,
            .magic = &ultimate,
            .text = "全隊，防+66，100幀，額外長文字，再追加一段完整效果，且保留所有條件與結果",
            .ultimate = true,
            .breakBefore = EffectDescriptionSemanticBreak::Block,
        },
    };

    const auto layout = layoutChessMagicEffectDisplay(rows, viewportWidth, viewportHeight);
    REQUIRE(layout.lines.size() > rows.size());
    CHECK(layout.requiredHeight <= viewportHeight);
    int previousBottom = 0;
    std::string wrappedEffect;
    int effectPhysicalLineIndex{};
    for (const auto& line : layout.lines)
    {
        CHECK(line.x >= 0);
        CHECK(line.x + line.width <= viewportWidth);
        CHECK(line.y >= previousBottom);
        CHECK(line.y + line.height <= viewportHeight);
        previousBottom = line.y + line.height;
        if (line.content.kind == ChessMagicEffectDisplayLineKind::Effect)
        {
            CHECK(line.content.breakBefore
                == (effectPhysicalLineIndex == 0
                    ? EffectDescriptionSemanticBreak::Block
                    : EffectDescriptionSemanticBreak::None));
            ++effectPhysicalLineIndex;
            wrappedEffect += line.content.text;
        }
    }
    CHECK(previousBottom == layout.requiredHeight);
    CHECK(wrappedEffect == rows.back().text);
}

TEST_CASE("ChessBattleEffects_MagicYamlRejectsDuplicateIdsAndComboMemberSelectors", "[battle][effects][magic]")
{
    const auto duplicate = YAML::Load(R"(
絕招:
  - 武功: 1
    名稱: 重複甲
    效果:
      - 時機: 主彈命中
        目標: 命中目標
        動作:
          - 套用狀態:
              狀態: 眩暈
              持續幀數: 8
              合併方式: 刷新
  - 武功: 1
    名稱: 重複乙
    效果:
      - 時機: 主彈命中
        目標: 命中目標
        動作:
          - 套用狀態:
              狀態: 眩暈
              持續幀數: 8
              合併方式: 刷新
)");
    const auto comboMemberSelector = YAML::Load(R"(
絕招:
  - 武功: 94
    名稱: 九陽神功
    效果:
      - 時機: 絕招施放
        目標: 羈絆成員
        動作:
          - 資源變更:
              資源: 生命
              方式: 回復
              數值: 60
)");

    std::vector<ChessMagicEffectDefinition> definitions;
    CHECK_FALSE(parseMagicEffects(duplicate, definitions, "重複武功"));
    CHECK_FALSE(parseMagicEffects(
        comboMemberSelector, definitions, "非法羈絆成員目標"));
}

TEST_CASE("ChessBattleEffects_RealUltimateSchemaValidatesAllDefinitions", "[battle][effects][magic][schema]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";

    REQUIRE(loadMagicEffectsFile(path.string(), definitions));
    REQUIRE(definitions.size() == 59);
    CHECK(std::ranges::all_of(definitions, [](const auto& definition) {
        return !definition.rules.empty();
    }));

    std::set<int> ids;
    std::size_t ruleCount = 0;
    std::size_t attackCommitRuleCount = 0;
    std::size_t ultimateCommitRuleCount = 0;
    for (const auto& definition : definitions)
    {
        CHECK(ids.insert(definition.magicId).second);
        ruleCount += definition.rules.size();
        for (const auto& rule : definition.rules)
        {
            attackCommitRuleCount += rule.event == EffectEvent::AttackCommitted;
            ultimateCommitRuleCount += rule.event == EffectEvent::UltimateCommitted;
            for (const auto style : {
                     EffectDescriptionStyle::Detailed,
                     EffectDescriptionStyle::Full,
                     EffectDescriptionStyle::Compact,
                })
            {
                const auto description = descriptionText(std::span<const EffectRule>{&(rule), 1}, style, {});
                CHECK_FALSE(description.empty());
                CHECK(description == descriptionText(std::span<const EffectRule>{&(rule), 1}, style, {}));
            }
        }
    }
    CHECK(ruleCount == 80);
    CHECK(attackCommitRuleCount == 41);
    CHECK(ultimateCommitRuleCount == 1);
    CHECK(ruleWithEvent(
        definitionWithId(definitions, 98),
        EffectEvent::UltimateCommitted).actions.size() == 1);
}

TEST_CASE("ChessBattleEffects_MigratedShorthandConfigsLoadAsCompleteContent",
          "[battle][effects][schema][content][shorthand]")
{
    ChessDiagnosticCollector diagnostics;
    ChessContentLoadOptions options;
    options.dataRoot = std::filesystem::current_path() / "work" / "game-dev";
    options.configRoot = std::filesystem::current_path() / "config";
    options.difficulty = Difficulty::Normal;
    options.diagnostics = diagnostics.sink();
    const auto loaded = ChessContentLoader::load(options);
    std::string report;
    for (const auto& diagnostic : diagnostics.diagnostics())
        report += std::format("{}: {}\n", diagnostic.source, diagnostic.message);
    INFO(report);
    REQUIRE(loaded.has_value());
}

TEST_CASE("ChessBattleEffects_SemanticDocumentPreservesCompoundNesting",
          "[battle][effects][magic][description][document]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto& xuanming = ruleWithEvent(
        definitionWithId(definitions, 21),
        EffectEvent::AttackCommitted);
    const std::array xuanmingRules{xuanming};
    const auto xuanmingDocument = buildEffectDescriptionDocument({
        EffectDescriptionContainerKind::Magic,
        xuanmingRules,
    });
    REQUIRE(xuanmingDocument.sections.size() == 1);
    REQUIRE(xuanmingDocument.sections.front().blocks.size() == 1);
    const auto& xuanmingBlock = xuanmingDocument.sections.front().blocks.front();
    REQUIRE(xuanmingBlock.actions.size() == 1);
    CHECK(xuanmingBlock.actions.front().sequential);
    REQUIRE(xuanmingBlock.actions.front().actions.size() == xuanming.actions.size());
    const auto xuanmingCompact = renderEffectDescription(
        xuanmingDocument,
        EffectDescriptionStyle::Compact,
        {});
    const auto xuanmingCompactText = joinEffectDescriptionRows(xuanmingCompact);
    CHECK(xuanmingCompactText.find("150幀") != std::string::npos);

    const auto& sunflower = ruleWithEvent(
        definitionWithId(definitions, 105),
        EffectEvent::AttackCommitted);
    const std::array sunflowerRules{sunflower};
    const auto sunflowerDocument = buildEffectDescriptionDocument({
        EffectDescriptionContainerKind::Magic,
        sunflowerRules,
    });
    const auto& sunflowerBlock = sunflowerDocument.sections.front().blocks.front();
    REQUIRE(sunflowerBlock.actions.size() == 1);
    CHECK_FALSE(sunflowerBlock.actions.front().sequential);
    CHECK(sunflowerBlock.actions.front().actions.size() == sunflower.actions.size());

    const auto& sanqing = ruleWithEvent(
        definitionWithId(definitions, 133),
        EffectEvent::AttackCommitted);
    const std::array sanqingRules{sanqing};
    const auto sanqingDocument = buildEffectDescriptionDocument({
        EffectDescriptionContainerKind::Magic,
        sanqingRules,
    });
    const auto& sanqingBlock = sanqingDocument.sections.front().blocks.front();
    REQUIRE(sanqingBlock.actions.size() == 1);
    REQUIRE(sanqingBlock.actions.front().actions.size() == 1);
    const auto sanqingBranch = std::get<std::shared_ptr<DescriptionBranch>>(
        sanqingBlock.actions.front().actions.front().value);
    REQUIRE(sanqingBranch);
    CHECK_FALSE(sanqingBranch->whenTrue.empty());
    CHECK_FALSE(sanqingBranch->whenFalse.empty());
    const auto sanqingFull = joinEffectDescriptionRows(renderEffectDescription(
        sanqingDocument,
        EffectDescriptionStyle::Full,
        {}));
    CHECK(sanqingFull.find("施放前內力已滿") != std::string::npos);
    CHECK(sanqingFull.find("回復20內力") != std::string::npos);
    CHECK(sanqingFull.find("獲得160護盾") != std::string::npos);

    const auto& coupleBlade = ruleWithEvent(
        definitionWithId(definitions, 62),
        EffectEvent::AttackCommitted);
    const auto coupleBladeFull = descriptionText(std::span<const EffectRule>{&(coupleBlade), 1},
        EffectDescriptionStyle::Full,
        {});
    CHECK(coupleBladeFull.find("若另一名同武功友軍存活") != std::string::npos);
    CHECK(coupleBladeFull.find("武功62") == std::string::npos);
    CHECK(coupleBladeFull.find("否則") != std::string::npos);

    const auto& taiji = definitionWithId(definitions, 16);
    const auto& taijiRecord = ruleWithEvent(taiji, EffectEvent::DamageResolved);
    const auto& taijiTransfer = ruleWithEvent(taiji, EffectEvent::AttackCommitted);
    const auto& taijiConsume = ruleWithEvent(
        taiji,
        EffectEvent::MainProjectileBeforeDamage);
    CHECK(descriptionText(std::span<const EffectRule>{&(taijiRecord), 1}, EffectDescriptionStyle::Full, {}).find(
        "首次記錄值為0") != std::string::npos);
    CHECK(descriptionText(std::span<const EffectRule>{&(taijiConsume), 1}, EffectDescriptionStyle::Compact, {}).find(
        "讀取記錄值") != std::string::npos);
    const auto& transferMachine = std::get<StateMachineAction>(
        taijiTransfer.actions[0].value);
    const auto& transfer = std::get<TransferStateValueAction>(transferMachine);
    CHECK(transfer.sourceSlot == EffectStateSlot::MaximumSkillHpDamage);
    CHECK(transfer.destinationSlot == EffectStateSlot::CastMaximumHpDamage);
    const auto& consumeMachine = std::get<StateMachineAction>(
        taijiConsume.actions[0].value);
    const auto& consume = std::get<ConsumeRecordedMaximumAction>(consumeMachine);
    CHECK(consume.slot == EffectStateSlot::CastMaximumHpDamage);
    CHECK_FALSE(consume.clearAfterConsume);
}

TEST_CASE("EffectDescriptionDocument_PreservesInlineConditionalOrderAndActionRelations",
          "[battle][effects][description][document][order]")
{
    const auto rule = parseRuleText(R"(
時機: 造成傷害後
目標: 自身
動作:
  - 回復內力: 1
  - 條件分支:
      條件: [已接受命中]
      成立:
        - 回復內力: 2
        - 條件分支:
            條件: [已接受命中]
            成立:
              - 回復內力: 20
      否則:
        - 回復內力: 22
  - 回復內力: 3
)");
    const std::array rules{ rule };
    auto document = buildEffectDescriptionDocument({
        EffectDescriptionContainerKind::Magic,
        rules,
    });
    REQUIRE(document.sections.size() == 1);
    REQUIRE(document.sections.front().blocks.size() == 1);
    const auto& block = document.sections.front().blocks.front();
    REQUIRE(block.actions.size() == 1);
    const auto& group = block.actions.front();
    REQUIRE(group.actions.size() == 3);
    CHECK(std::holds_alternative<EffectAction>(group.actions[0].value));
    CHECK(std::holds_alternative<std::shared_ptr<DescriptionBranch>>(
        group.actions[1].value));
    CHECK(std::holds_alternative<EffectAction>(group.actions[2].value));
    CHECK(group.sequential);

    const auto branch = std::get<std::shared_ptr<DescriptionBranch>>(
        group.actions[1].value);
    REQUIRE(branch);
    REQUIRE(branch->whenTrue.size() == 1);
    REQUIRE(branch->whenTrue.front().actions.size() == 2);
    const auto nestedBranch = std::get<std::shared_ptr<DescriptionBranch>>(
        branch->whenTrue.front().actions[1].value);
    REQUIRE(nestedBranch);
    CHECK_FALSE(nestedBranch->whenTrue.empty());

    const auto sequential = renderEffectDescription(
        document,
        EffectDescriptionStyle::Compact,
        {});
    const auto sequentialText = joinEffectDescriptionRows(sequential);
    const auto first = sequentialText.find("回復1內力");
    const auto conditional = sequentialText.find("若已接受命中");
    const auto last = sequentialText.find("回復3內力");
    REQUIRE(first != std::string::npos);
    REQUIRE(conditional != std::string::npos);
    REQUIRE(last != std::string::npos);
    CHECK(first < conditional);
    CHECK(conditional < last);
    CHECK(sequentialText.find("依序執行") != std::string::npos);

    auto simultaneousDocument = document;
    simultaneousDocument.sections.front().blocks.front().actions.front().sequential = false;
    const auto simultaneous = renderEffectDescription(
        simultaneousDocument,
        EffectDescriptionStyle::Compact,
        {});
    CHECK(joinEffectDescriptionRows(simultaneous).find("同時發生") != std::string::npos);

    const auto sequenceBreakCount = [](const RenderedEffectDescription& rendered)
    {
        int result{};
        for (const auto& section : rendered.sections)
            for (const auto& renderedBlock : section.blocks)
                for (const auto& row : renderedBlock.rows)
                    if (row.breakBefore == EffectDescriptionSemanticBreak::Sequence)
                        ++result;
        return result;
    };
    CHECK(sequenceBreakCount(sequential) > sequenceBreakCount(simultaneous));
}

TEST_CASE("EffectDescriptionDocument_ResolvesObservedSourceSeparatelyFromEffectOwner",
          "[battle][effects][description][document][roles]")
{
    EffectRule rule{
        .id = {1},
        .event = EffectEvent::HitBeforeDamage,
        .observation = EffectObservationScope::OwnerTeamEventSource,
        .selector = EffectSelector{ .kind = EffectSelectorKind::HitTarget },
        .conditions = {
            TargetHasStateFromEffectOwnerCondition{
                BattleStatusKind::SevenStarMark},
        },
        .actions = {
            EffectAction{ EffectActionValue{ ApplyStatusAction{
                .status = BattleStatusKind::Poison,
                .durationFrames = 30,
            } } },
        },
    };
    std::string error;
    INFO(error);
    REQUIRE(validateEffectRule(rule, error));
    const std::array rules{ rule };
    const auto document = buildEffectDescriptionDocument({
        EffectDescriptionContainerKind::Magic,
        rules,
    });
    const auto& block = document.sections.front().blocks.front();
    const auto& trigger = std::get<DescriptionTriggerFact>(
        block.trigger.front().value);
    const auto& target = std::get<DescriptionSelectorFact>(
        block.targets.front().value);
    CHECK(trigger.subject == DescriptionTriggerFact::SubjectRole::EventSource);
    CHECK(target.role == DescriptionTargetRole::EventTarget);

    const auto text = joinEffectDescriptionRows(renderEffectDescription(
        document,
        EffectDescriptionStyle::Full,
        {}));
    CHECK(text.find("目標有由效果持有者施加的七星") != std::string::npos);
    CHECK(text.find("本次施法者") == std::string::npos);
    CHECK(text.find("事件目標") == std::string::npos);
    CHECK(text.find("該次事件的目標") == std::string::npos);
}

TEST_CASE("EffectDescriptionDocument_UnitDeathConditionUsesResolvedDeadUnitRole",
          "[battle][effects][description][document][roles]")
{
    const auto rule = parseRuleText(R"(
時機: 單位死亡
觀察範圍: 事件目標
目標: 自身
條件:
  - 事件目標屬於綁定來源
獲得護盾: 10
)");
    const std::array rules{rule};
    const auto document = buildEffectDescriptionDocument({
        EffectDescriptionContainerKind::ComboThreshold,
        rules,
    });
    REQUIRE(document.sections.size() == 1);
    REQUIRE(document.sections[0].blocks.size() == 1);
    const auto& block = document.sections[0].blocks[0];
    REQUIRE(block.conditions.size() == 2);
    CHECK(block.conditions[0].absorption
        == DescriptionPhraseAbsorption::None);
    for (const auto style : {
             EffectDescriptionStyle::Full,
             EffectDescriptionStyle::Compact,
         })
    {
        const auto text = joinEffectDescriptionRows(renderEffectDescription(
            document,
            style,
            {}));
        CHECK(text.find("死亡單位屬於此羈絆") != std::string::npos);
        CHECK(text.find("事件目標") == std::string::npos);
        CHECK(text.find("該次事件的目標") == std::string::npos);
    }
}

TEST_CASE("EffectDescriptionDocument_ProjectsTypedFactLevelsAndRelationalCastSource",
          "[battle][effects][description][document][projection]")
{
    const auto rule = parseRuleText(R"(
時機: 施放規劃
目標: 自身
條件:
  - 施放武功為效果來源
動作:
  - 資源變更:
      資源: 內力
      方式: 回復
      數值: 10
)");
    const std::array rules{rule};
    const auto document = buildEffectDescriptionDocument({
        EffectDescriptionContainerKind::Magic,
        rules,
    });
    REQUIRE(document.sections.size() == 1);
    REQUIRE(document.sections[0].blocks.size() == 1);
    const auto& block = document.sections[0].blocks[0];
    REQUIRE(block.trigger.size() == 1);
    REQUIRE(block.targets.size() == 1);
    REQUIRE(block.conditions.size() == 2);
    REQUIRE(block.actions.size() == 1);
    REQUIRE(block.actions[0].actions.size() == 1);
    constexpr DescriptionPlayerProjection bothPlayerStyles{true, true};
    CHECK(block.trigger[0].level == DescriptionFactLevel::Core);
    CHECK(block.trigger[0].playerFact == DescriptionPlayerFact::Trigger);
    CHECK(block.trigger[0].projection == bothPlayerStyles);
    CHECK(block.targets[0].level == DescriptionFactLevel::Core);
    CHECK(block.targets[0].playerFact == DescriptionPlayerFact::Target);
    CHECK(block.targets[0].projection == bothPlayerStyles);
    CHECK(block.conditions[0].level == DescriptionFactLevel::Decision);
    CHECK(block.conditions[0].playerFact == DescriptionPlayerFact::Condition);
    CHECK(block.conditions[0].projection == bothPlayerStyles);
    CHECK(block.conditions[1].level == DescriptionFactLevel::Decision);
    CHECK(block.conditions[1].playerFact
        == DescriptionPlayerFact::RuleQualifiers);
    CHECK(block.actions[0].actions[0].level == DescriptionFactLevel::Core);
    CHECK(block.actions[0].actions[0].playerFact
        == DescriptionPlayerFact::Action);
    CHECK(block.actions[0].actions[0].projection == bothPlayerStyles);

    const auto findCoverage = [&](std::string_view path) -> const DescriptionCoverageEntry&
    {
        const auto entry = std::ranges::find_if(block.coverage.fields,
            [&](const DescriptionCoverageEntry& candidate)
            {
                return candidate.source.path == path;
            });
        REQUIRE(entry != block.coverage.fields.end());
        return *entry;
    };
    const auto& conditionRoot = findCoverage("conditions[0]");
    CHECK(conditionRoot.level == DescriptionFactLevel::Decision);
    CHECK(conditionRoot.playerFact == DescriptionPlayerFact::Condition);
    CHECK(conditionRoot.projection == bothPlayerStyles);
    const auto& actionRoot = findCoverage("actions[0]");
    CHECK(actionRoot.level == DescriptionFactLevel::Core);
    CHECK(actionRoot.playerFact == DescriptionPlayerFact::Action);
    CHECK(actionRoot.projection == bothPlayerStyles);
    const auto& actionDefault = findCoverage("actions[0].healKind");
    CHECK(actionDefault.level == DescriptionFactLevel::Audit);
    CHECK(actionDefault.disposition == DescriptionFieldDisposition::SchemaDefault);
    CHECK(actionDefault.projection == DescriptionPlayerProjection{});

    for (const auto style : {
             EffectDescriptionStyle::Full,
             EffectDescriptionStyle::Compact,
         })
    {
        const auto text = joinEffectDescriptionRows(renderEffectDescription(
            document,
            style,
            {}));
        CHECK(text.find("施放武功為效果來源") != std::string::npos);
        CHECK(text.find("武功1") == std::string::npos);
    }

    auto withoutFullCondition = document;
    withoutFullCondition.sections[0].blocks[0].conditions[0].projection.full = false;
    const auto fullWithoutCondition = joinEffectDescriptionRows(
        renderEffectDescription(
            withoutFullCondition,
            EffectDescriptionStyle::Full,
            {}));
    CHECK(fullWithoutCondition.find("施放武功為效果來源")
        == std::string::npos);
    CHECK(fullWithoutCondition.find("回復10內力") != std::string::npos);

    auto withoutCompactAction = document;
    withoutCompactAction.sections[0].blocks[0]
        .actions[0].actions[0].projection.compact = false;
    const auto compactWithoutAction = joinEffectDescriptionRows(
        renderEffectDescription(
            withoutCompactAction,
            EffectDescriptionStyle::Compact,
            {}));
    CHECK(compactWithoutAction.find("回復10內力") == std::string::npos);

    const auto ultimateRule = parseRuleText(R"(
時機: 絕招施放
目標: 自身
條件:
  - 僅限絕招
回復內力: 10
)");
    const std::array ultimateRules{ultimateRule};
    auto ultimateDocument = buildEffectDescriptionDocument({
        EffectDescriptionContainerKind::Magic,
        ultimateRules,
    });
    auto& ultimateCondition =
        ultimateDocument.sections[0].blocks[0].conditions[0];
    CHECK(ultimateCondition.absorption
        == DescriptionPhraseAbsorption::UltimateEvent);
    CHECK(joinEffectDescriptionRows(renderEffectDescription(
        ultimateDocument,
        EffectDescriptionStyle::Compact,
        {})).find("為絕招") == std::string::npos);
    ultimateCondition.absorption = DescriptionPhraseAbsorption::None;
    CHECK(joinEffectDescriptionRows(renderEffectDescription(
        ultimateDocument,
        EffectDescriptionStyle::Compact,
        {})).find("為絕招") != std::string::npos);
}

TEST_CASE("EffectDescriptionDocument_PlayerProjectionAuthorizesGenericFieldsAndFailsClosed",
          "[battle][effects][description][document][projection]")
{
    const auto rule = parseRuleText(R"(
時機: 命中
觀察範圍: 效果擁有者同隊事件來源
目標: 命中目標
套用狀態:
  狀態: 寒毒
  強度: 25
  次要強度: 75
  合併方式: 刷新
)");
    const std::array rules{rule};
    auto document = buildEffectDescriptionDocument({
        EffectDescriptionContainerKind::Magic,
        rules,
    });
    REQUIRE(document.sections.size() == 1);
    REQUIRE(document.sections[0].blocks.size() == 1);
    auto& block = document.sections[0].blocks[0];
    REQUIRE(block.archetype == DescriptionArchetype::Generic);

    const auto findCoverage = [](
        EffectDescriptionBlock& candidateBlock,
        std::string_view path) -> DescriptionCoverageEntry&
    {
        const auto entry = std::ranges::find_if(candidateBlock.coverage.fields,
            [&](const DescriptionCoverageEntry& candidate)
            {
                return candidate.source.path == path;
            });
        REQUIRE(entry != candidateBlock.coverage.fields.end());
        return *entry;
    };
    constexpr DescriptionPlayerProjection bothPlayerStyles{true, true};
    auto& potency = findCoverage(block, "actions[0].potency.flat");
    auto& secondaryPotency = findCoverage(
        block, "actions[0].secondaryPotency.flat");
    auto& observation = findCoverage(block, "observation");
    CHECK(potency.level == DescriptionFactLevel::Audit);
    CHECK(potency.playerFact == DescriptionPlayerFact::StatusPotency);
    CHECK(potency.projection == bothPlayerStyles);
    CHECK(secondaryPotency.level == DescriptionFactLevel::Audit);
    CHECK(secondaryPotency.playerFact
        == DescriptionPlayerFact::StatusSecondaryPotency);
    CHECK(secondaryPotency.projection == bothPlayerStyles);
    CHECK(observation.level == DescriptionFactLevel::Decision);
    CHECK(observation.playerFact == DescriptionPlayerFact::Trigger);
    CHECK(observation.projection == bothPlayerStyles);

    const auto full = joinEffectDescriptionRows(renderEffectDescription(
        document,
        EffectDescriptionStyle::Full,
        {}));
    CHECK(full.find("由效果持有者同隊事件來源觸發")
        != std::string::npos);
    CHECK(full.find("強度為25") != std::string::npos);
    CHECK(full.find("次要強度為75") != std::string::npos);

    auto withoutFullPotency = document;
    findCoverage(
        withoutFullPotency.sections[0].blocks[0],
        "actions[0].potency.flat").projection.full = false;
    CHECK_THROWS_AS(
        renderEffectDescription(
            withoutFullPotency,
            EffectDescriptionStyle::Full,
            {}),
        std::logic_error);

    auto withoutFullTriggerDecision = document;
    findCoverage(
        withoutFullTriggerDecision.sections[0].blocks[0],
        "observation").projection.full = false;
    CHECK_THROWS_AS(
        renderEffectDescription(
            withoutFullTriggerDecision,
            EffectDescriptionStyle::Full,
            {}),
        std::logic_error);

    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path()
        / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));
    const auto& poison = definitionWithId(definitions, 95);
    auto specializedDocument = buildEffectDescriptionDocument({
        EffectDescriptionContainerKind::Magic,
        poison.rules,
    });
    auto specializedBlock = std::ranges::find_if(
        specializedDocument.sections.front().blocks,
        [](const EffectDescriptionBlock& candidate)
        {
            return candidate.archetype == DescriptionArchetype::StackExplosion;
        });
    REQUIRE(specializedBlock
        != specializedDocument.sections.front().blocks.end());
    REQUIRE(!specializedBlock->actions.empty());
    REQUIRE(!specializedBlock->actions.front().actions.empty());
    specializedBlock->actions.front().actions.front().projection.full = false;
    CHECK_THROWS_AS(
        renderEffectDescription(
            specializedDocument,
            EffectDescriptionStyle::Full,
            {}),
        std::logic_error);
}

TEST_CASE("EffectDescriptionDocument_RendersEveryMartialCategoryWithoutCodes",
          "[battle][effects][description][document][selector]")
{
    struct CategoryExpectation
    {
        std::string_view authorLabel;
        EffectMartialCategory category{};
        std::string_view full;
        std::string_view compact;
    };
    constexpr std::array expectations{
        CategoryExpectation{"拳掌", EffectMartialCategory::Fist,
            "使用拳掌類武功的友軍", "友方拳掌角色"},
        CategoryExpectation{"御劍", EffectMartialCategory::Sword,
            "使用御劍類武功的友軍", "友方御劍角色"},
        CategoryExpectation{"耍刀", EffectMartialCategory::Knife,
            "使用耍刀類武功的友軍", "友方耍刀角色"},
        CategoryExpectation{"特殊", EffectMartialCategory::Unusual,
            "使用特殊類武功的友軍", "友方特殊角色"},
    };
    for (const auto& expectation : expectations)
    {
        const auto rule = parseRuleText(std::format(R"(
時機: 絕招施放
目標:
  類型: 指定武學類別友軍
  武學類別: {}
動作:
  - 屬性修正:
      屬性: 攻擊
      方式: 固定加算
      數值: 10
)", expectation.authorLabel));
        CHECK(rule.selector.kind
            == EffectSelectorKind::AlliesUsingMartialCategory);
        CHECK(rule.selector.requiredMartialCategory == expectation.category);
        const std::array rules{rule};
        const auto document = buildEffectDescriptionDocument({
            EffectDescriptionContainerKind::Magic,
            rules,
        });
        const auto full = joinEffectDescriptionRows(renderEffectDescription(
            document, EffectDescriptionStyle::Full, {}));
        const auto compact = joinEffectDescriptionRows(renderEffectDescription(
            document, EffectDescriptionStyle::Compact, {}));
        CHECK(full.find(expectation.full) != std::string::npos);
        CHECK(compact.find(expectation.compact) != std::string::npos);
        CHECK(full.find("武器類型") == std::string::npos);
        CHECK(compact.find("武器類型") == std::string::npos);
    }

    for (const auto invalidSelector : {
             R"(類型: 指定武器友軍
武器類型: 1)",
             R"(類型: 指定武學類別友軍
武學類別: 1)",
         })
    {
        EffectRule rule;
        CHECK_FALSE(parseEffectRule(
            YAML::Load(std::format(R"(
時機: 絕招施放
目標:
  {}
動作:
  - 回復內力: 10
)", invalidSelector)),
            rule,
            EffectRuleId{1},
            "移除的武器類型語法"));
    }
}

TEST_CASE("ChessBattleEffects_DescriptionIgnoresRuntimeRuleIdentity",
          "[battle][effects][description][ast]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto& original = ruleWithEvent(
        definitionWithId(definitions, 127),
        EffectEvent::AttackCommitted);
    auto changedIdentity = original;
    changedIdentity.id = EffectRuleId{ original.id.value + 0x100000000ULL };

    for (const auto style : {
             EffectDescriptionStyle::Full,
             EffectDescriptionStyle::Compact,
         })
    {
        CHECK(descriptionText(std::span<const EffectRule>{&(original), 1}, style, {})
              == descriptionText(std::span<const EffectRule>{&(changedIdentity), 1}, style, {}));
    }
    CHECK(descriptionText(std::span<const EffectRule>{&(original), 1}, EffectDescriptionStyle::Detailed, {})
          != descriptionText(std::span<const EffectRule>{&(changedIdentity), 1}, EffectDescriptionStyle::Detailed, {}));
}

TEST_CASE("ChessBattleEffects_DescriptionOrdersTriggerCadenceBeforeTargetAndAction",
          "[battle][effects][description][ast]")
{
    EffectRule rule;
    REQUIRE(parseEffectRule(YAML::Load(R"(
時機: 每隔
間隔幀數: 30
目標: 自身
動作:
  - 資源變更:
      資源: 內力
      方式: 回復
      數值: 2
)"), rule, EffectRuleId{ 7 }, "描述資訊順序"));

    const auto full = descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Full, {});
    const auto cadence = full.find("每30幀");
    const auto action = full.find("回復2內力");
    REQUIRE(cadence != std::string::npos);
    REQUIRE(action != std::string::npos);
    CHECK(cadence < action);
    CHECK(full.find("自身") == std::string::npos);
}

TEST_CASE("EffectDescriptionDocument_PreservesAdjacentAttributeActionsAndQualifiers",
          "[battle][effects][description][document]")
{
    const auto eligible = parseRuleText(R"(
時機: 絕招施放
目標: 自身
動作:
  - 屬性修正:
      屬性: 攻擊
      方式: 百分比加算
      數值: 20
      持續幀數: 100
      合併方式: 刷新
  - 屬性修正:
      屬性: 防禦
      方式: 百分比加算
      數值: 30
      持續幀數: 100
      合併方式: 刷新
)");
    const auto detailed = descriptionText(std::span<const EffectRule>{&(eligible), 1}, EffectDescriptionStyle::Detailed, {});
    const auto full = descriptionText(std::span<const EffectRule>{&(eligible), 1}, EffectDescriptionStyle::Full, {});
    const auto compact = descriptionText(std::span<const EffectRule>{&(eligible), 1}, EffectDescriptionStyle::Compact, {});
    CHECK(countOccurrences(detailed, "100幀") >= 2);
    CHECK(countOccurrences(detailed, "刷新") >= 2);
    CHECK(full.find("攻擊+20%") != std::string::npos);
    CHECK(full.find("防禦+30%") != std::string::npos);
    CHECK(countOccurrences(full, "100幀") == 2);
    CHECK(compact.find("攻+20%") != std::string::npos);
    CHECK(compact.find("防+30%") != std::string::npos);
    CHECK(countOccurrences(compact, "100幀") == 2);

    auto independent = eligible;
    for (auto& action : independent.actions)
        std::get<ModifyAttributeAction>(action.value).stack = EffectStackPolicy::Independent;
    CHECK(countOccurrences(
        descriptionText(std::span<const EffectRule>{&(independent), 1}, EffectDescriptionStyle::Compact, {}),
        "100幀") == 2);

    const auto checkDoesNotCoalesce = [](const EffectRule& rule)
    {
        const auto fullText = descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Full, {});
        const auto compactText = descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Compact, {});
        CHECK(countOccurrences(fullText, "持續100幀") == 2);
        CHECK(countOccurrences(compactText, "100幀") == 2);
    };

    auto differentDuration = eligible;
    std::get<ModifyAttributeAction>(differentDuration.actions[1].value).durationFrames = 90;
    const auto differentDurationText = descriptionText(std::span<const EffectRule>{&(differentDuration), 1},
        EffectDescriptionStyle::Compact,
        {});
    CHECK(differentDurationText.find("100幀") != std::string::npos);
    CHECK(differentDurationText.find("90幀") != std::string::npos);

    auto differentOperation = eligible;
    std::get<ModifyAttributeAction>(differentOperation.actions[1].value).operation = AttributeOperation::FlatAdd;
    checkDoesNotCoalesce(differentOperation);

    auto cappedStacks = eligible;
    for (auto& action : cappedStacks.actions)
    {
        auto& modifier = std::get<ModifyAttributeAction>(action.value);
        modifier.stack = EffectStackPolicy::AddStack;
        modifier.stackLimit = 8;
    }
    const auto cappedCompact = descriptionText(std::span<const EffectRule>{&(cappedStacks), 1},
        EffectDescriptionStyle::Compact,
        {});
    CHECK(countOccurrences(cappedCompact, "可疊至8層") == 2);
    CHECK(countOccurrences(cappedCompact, "100幀") == 2);

    auto negative = eligible;
    std::get<ModifyAttributeAction>(negative.actions[0].value).amount.flat = -20;
    std::get<ModifyAttributeAction>(negative.actions[1].value).amount.flat = -30;
    checkDoesNotCoalesce(negative);

    auto boundedNegativeMultiplier = eligible;
    for (auto& action : boundedNegativeMultiplier.actions)
    {
        auto& modifier = std::get<ModifyAttributeAction>(action.value);
        modifier.operation = AttributeOperation::Multiply;
        modifier.amount.flat = 200;
        modifier.amount.maximum = 50;
    }
    checkDoesNotCoalesce(boundedNegativeMultiplier);

    auto formula = eligible;
    std::get<ModifyAttributeAction>(formula.actions[0].value).amount.base = EffectNumberBase::SourceStar;
    std::get<ModifyAttributeAction>(formula.actions[1].value).amount.base = EffectNumberBase::SourceStar;
    checkDoesNotCoalesce(formula);

    auto eventScope = eligible;
    for (auto& action : eventScope.actions)
        std::get<ModifyAttributeAction>(action.value).stackScope = EffectStackScope::EventSource;
    checkDoesNotCoalesce(eventScope);

    for (const auto policy : {
             EffectStackPolicy::Replace,
             EffectStackPolicy::KeepStrongest,
         })
    {
        auto ineligiblePolicy = eligible;
        for (auto& action : ineligiblePolicy.actions)
            std::get<ModifyAttributeAction>(action.value).stack = policy;
        checkDoesNotCoalesce(ineligiblePolicy);
    }

    auto perStack = eligible;
    for (auto& action : perStack.actions)
        std::get<ModifyAttributeAction>(action.value).perStack = true;
    checkDoesNotCoalesce(perStack);

    auto permanent = eligible;
    for (auto& action : permanent.actions)
        std::get<ModifyAttributeAction>(action.value).durationFrames = 0;
    const auto permanentCompact = descriptionText(std::span<const EffectRule>{&(permanent), 1},
        EffectDescriptionStyle::Compact,
        {});
    CHECK(permanentCompact.find("100幀") == std::string::npos);

    auto withInterveningAction = eligible;
    withInterveningAction.actions.insert(
        withInterveningAction.actions.begin() + 1,
        EffectAction{ ChangeResourceAction{
            .resource = BattleResource::Mp,
            .amount = EffectNumber{ .flat = 1 },
            .kind = ResourceChangeKind::Restore,
        } });
    checkDoesNotCoalesce(withInterveningAction);
}

TEST_CASE("ChessBattleEffects_MagicContainerOmitsOnlyMatchingBoundCommitEvent",
          "[battle][effects][description][context]")
{
    auto rule = parseRuleText(R"(
時機: 攻擊提交
目標: 全隊
屬性修正:
  屬性: 防禦
  方式: 固定加算
  數值: 66
  持續幀數: 100
  合併方式: 刷新
)");
    const EffectDescriptionPresentationContext commitContext{
        .enclosingDefaultEvent = EffectEvent::AttackCommitted,
    };
    const auto standalone = descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Compact, {});
    const auto enclosed = descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Compact, commitContext);
    CHECK_FALSE(standalone.starts_with("絕招"));
    CHECK(standalone.find("防+66") != std::string::npos);
    CHECK(standalone.find("100幀") != std::string::npos);
    CHECK_FALSE(enclosed.starts_with("絕招"));
    CHECK(standalone == enclosed);
    CHECK(enclosed.find("防+66") != std::string::npos);
    CHECK(enclosed.find("100幀") != std::string::npos);
    const auto enclosedFull = descriptionText(std::span<const EffectRule>{&(rule), 1},
        EffectDescriptionStyle::Full,
        commitContext);
    CHECK(enclosedFull.find("對全隊防禦+66") != std::string::npos);
    CHECK(enclosedFull.find("持續100幀") != std::string::npos);
    CHECK(descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Detailed, commitContext)
        .starts_with("施放大招"));

    rule.castMatch = EffectCastMatch::OwnerAnyCast;
    const auto anyCast = descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Compact, commitContext);
    CHECK(anyCast.starts_with("出手"));
    CHECK(anyCast.find("任意施放") != std::string::npos);

    rule.castMatch = EffectCastMatch::BoundMagic;
    for (const auto [event, label] : {
             std::pair{ EffectEvent::MainProjectileBeforeDamage, std::string_view("主彈命中") },
             std::pair{ EffectEvent::CastContinuation, std::string_view("施放延續") },
             std::pair{ EffectEvent::CastSettled, std::string_view("施放結算") },
             std::pair{ EffectEvent::UltimateCooldownFinished, std::string_view("絕招冷卻完成") },
         })
    {
        rule.event = event;
        CHECK(descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Compact, commitContext)
            .starts_with(label));
    }
}

TEST_CASE("ChessBattleEffects_PlayerPhrasesUseTypedSignsAndAttributeUnits",
          "[battle][effects][description][phrasing]")
{
    auto incoming = parseRuleText(R"(
時機: 絕招施放
目標: 自身
傷害修正:
  方位: 承受
  階段: 防禦前
  傷害種類: 全部
  方式: 百分比加算
  數值: -6
)");
    CHECK(descriptionText(std::span<const EffectRule>{&(incoming), 1}, EffectDescriptionStyle::Full, {}).find("減傷6%")
        != std::string::npos);
    const auto reviewedIncomingCompact = descriptionText(std::span<const EffectRule>{&(incoming), 1},
        EffectDescriptionStyle::Compact,
        {});
    CHECK(reviewedIncomingCompact.find("減傷6%") != std::string::npos);
    CHECK(reviewedIncomingCompact.find("防前") == std::string::npos);
    auto& incomingModifier = std::get<ModifyDamageAction>(incoming.actions.front().value);
    incomingModifier.amount.flat = 6;
    CHECK(descriptionText(std::span<const EffectRule>{&(incoming), 1}, EffectDescriptionStyle::Full, {}).find("受傷+6%")
        != std::string::npos);
    incomingModifier.amount.flat = 0;
    CHECK(descriptionText(std::span<const EffectRule>{&(incoming), 1}, EffectDescriptionStyle::Full, {}).find("受傷+0%")
        != std::string::npos);
    incomingModifier.amount.base = EffectNumberBase::SourceStar;
    incomingModifier.amount.percent = 100;
    CHECK(descriptionText(std::span<const EffectRule>{&(incoming), 1}, EffectDescriptionStyle::Full, {}).find(
        "承受的所有傷害（防禦結算前）百分比加算星級×1") != std::string::npos);
    const auto formulaIncomingCompact = descriptionText(std::span<const EffectRule>{&(incoming), 1},
        EffectDescriptionStyle::Compact,
        {});
    CHECK(formulaIncomingCompact.find("防前") != std::string::npos);
    CHECK(formulaIncomingCompact.find("防禦結算前") == std::string::npos);

    incomingModifier.perspective = DamageModifierPerspective::Outgoing;
    incomingModifier.channel = DamageChannel::Skill;
    incomingModifier.stage = DamageModifierStage::AfterDefense;
    incomingModifier.amount = EffectNumber{ .flat = 6 };
    CHECK(descriptionText(std::span<const EffectRule>{&(incoming), 1}, EffectDescriptionStyle::Full, {}).find("增傷6%")
        != std::string::npos);
    incomingModifier.amount.flat = -6;
    CHECK(descriptionText(std::span<const EffectRule>{&(incoming), 1}, EffectDescriptionStyle::Full, {}).find("造成傷害-6%")
        != std::string::npos);
    incomingModifier.amount.flat = 0;
    CHECK(descriptionText(std::span<const EffectRule>{&(incoming), 1}, EffectDescriptionStyle::Full, {}).find("造成傷害+0%")
        != std::string::npos);
    incomingModifier.amount.base = EffectNumberBase::SourceStar;
    incomingModifier.amount.percent = 100;
    CHECK(descriptionText(std::span<const EffectRule>{&(incoming), 1}, EffectDescriptionStyle::Full, {}).find(
        "造成的招式傷害（防禦結算後）百分比加算星級×1") != std::string::npos);
    CHECK(descriptionText(std::span<const EffectRule>{&(incoming), 1}, EffectDescriptionStyle::Compact, {}).find("防後")
        != std::string::npos);

    const auto reductionAttribute = parseRuleText(R"(
時機: 開場
目標: 自身
屬性修正:
  屬性: 傷害減免
  方式: 百分比加算
  數值: 6
)");
    CHECK(descriptionText(std::span<const EffectRule>{&(reductionAttribute), 1}, EffectDescriptionStyle::Full, {}) == "減傷+6%");
    auto changedReduction = reductionAttribute;
    auto& reduction = std::get<ModifyAttributeAction>(changedReduction.actions.front().value);
    reduction.amount.flat = -6;
    CHECK(descriptionText(std::span<const EffectRule>{&(changedReduction), 1}, EffectDescriptionStyle::Full, {}) == "減傷-6%");
    reduction.amount.flat = 0;
    CHECK(descriptionText(std::span<const EffectRule>{&(changedReduction), 1}, EffectDescriptionStyle::Full, {}) == "減傷+0%");

    const auto blockChance = parseRuleText(R"(
時機: 開場
目標: 自身
屬性修正:
  屬性: 格擋率
  方式: 固定加算
  數值: 6
)");
    CHECK(descriptionText(std::span<const EffectRule>{&(blockChance), 1}, EffectDescriptionStyle::Full, {}) == "格擋率+6%");
    auto formulaBlockChance = blockChance;
    auto& formulaBlock = std::get<ModifyAttributeAction>(formulaBlockChance.actions.front().value);
    formulaBlock.amount.base = EffectNumberBase::SourceStar;
    formulaBlock.amount.flat = 0;
    formulaBlock.amount.percent = 100;
    CHECK(descriptionText(std::span<const EffectRule>{&(formulaBlockChance), 1}, EffectDescriptionStyle::Full, {})
        == "格擋率增加星級×1%");

    const auto boundedConstant = parseRuleText(R"(
時機: 開場
目標: 自身
屬性修正:
  屬性: 攻擊
  方式: 固定加算
  數值:
    固定: 20
    最大: 5
)");
    CHECK(descriptionText(std::span<const EffectRule>{&(boundedConstant), 1}, EffectDescriptionStyle::Full, {}) == "攻擊+5");
    CHECK(descriptionText(std::span<const EffectRule>{&(boundedConstant), 1}, EffectDescriptionStyle::Compact, {}) == "攻+5");
    const auto boundedDetailed = descriptionText(std::span<const EffectRule>{&(boundedConstant), 1},
        EffectDescriptionStyle::Detailed,
        {});
    CHECK(boundedDetailed.find("攻擊增加20，至多5") != std::string::npos);

    auto boundedPercent = boundedConstant;
    std::get<ModifyAttributeAction>(boundedPercent.actions.front().value).operation
        = AttributeOperation::PercentAdd;
    CHECK(descriptionText(std::span<const EffectRule>{&(boundedPercent), 1}, EffectDescriptionStyle::Full, {}) == "攻擊+5%");
    CHECK(descriptionText(std::span<const EffectRule>{&(boundedPercent), 1}, EffectDescriptionStyle::Detailed, {}).find(
        "攻擊增加20，至多5%") != std::string::npos);

    auto projectilePressure = boundedConstant;
    auto& projectileModifier = std::get<ModifyAttributeAction>(
        projectilePressure.actions.front().value);
    projectileModifier.attribute = BattleAttribute::ProjectilePressureDamage;
    projectileModifier.amount = EffectNumber{ .flat = 10 };
    CHECK_FALSE(battleAttributeUsesPercentagePoints(BattleAttribute::ProjectilePressureDamage));
    CHECK(descriptionText(std::span<const EffectRule>{&(projectilePressure), 1}, EffectDescriptionStyle::Full, {})
        == "彈道壓制傷害+10");
    CHECK(descriptionText(std::span<const EffectRule>{&(projectilePressure), 1}, EffectDescriptionStyle::Compact, {})
        == "彈壓傷+10");

    projectileModifier.operation = AttributeOperation::Multiply;
    projectileModifier.amount.flat = 200;
    CHECK(descriptionText(std::span<const EffectRule>{&(projectilePressure), 1}, EffectDescriptionStyle::Full, {})
        == "彈道壓制傷害乘以200%");
}

TEST_CASE("ChessBattleEffects_RepresentativeThreeTierDescriptionsStayPlayerFacing",
          "[battle][effects][description][golden]")
{
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content != nullptr);

    const auto holyFire = std::ranges::find(
        content->neigong(),
        93,
        &NeigongDef::magicId);
    REQUIRE(holyFire != content->neigong().end());
    REQUIRE(holyFire->rules.size() == 1);
    const auto holyFireDetailed = descriptionText(std::span<const EffectRule>{&(holyFire->rules.front()), 1}, EffectDescriptionStyle::Detailed, {});
    CHECK(holyFireDetailed.starts_with("戰鬥開始"));
    CHECK(holyFireDetailed.find("主詞：") != std::string::npos);
    CHECK(holyFireDetailed.find("攻擊+25") != std::string::npos);
    CHECK(descriptionText(std::span<const EffectRule>{&(holyFire->rules.front()), 1}, EffectDescriptionStyle::Full, {})
        == "攻擊+25");
    CHECK(descriptionText(std::span<const EffectRule>{&(holyFire->rules.front()), 1}, EffectDescriptionStyle::Compact, {})
        == "攻+25");

    const auto ming = std::ranges::find(
        content->combos(),
        std::string{ "明教" },
        &ComboDef::name);
    REQUIRE(ming != content->combos().end());
    const auto mingFollowers = std::ranges::find(
        ming->thresholds,
        2,
        &ComboThreshold::count);
    REQUIRE(mingFollowers != ming->thresholds.end());
    REQUIRE_FALSE(mingFollowers->rules.empty());
    const auto& sameComboDeath = mingFollowers->rules.front();
    const auto comboDetailed = descriptionText(std::span<const EffectRule>{&(sameComboDeath), 1}, EffectDescriptionStyle::Detailed, {});
    CHECK(comboDetailed.starts_with("任一友軍死亡"));
    CHECK(comboDetailed.find("條件：死亡單位屬於此羈絆來源") != std::string::npos);
    CHECK(comboDetailed.find("動作關係：同時") != std::string::npos);
    CHECK(comboDetailed.find("攻擊+50") != std::string::npos);
    CHECK(comboDetailed.find("防禦+50") != std::string::npos);
    const auto sameComboFull = descriptionText(std::span<const EffectRule>{&(sameComboDeath), 1},
        EffectDescriptionStyle::Full,
        {});
    CHECK(sameComboFull.find("同羈絆友軍死亡時") != std::string::npos);
    CHECK(sameComboFull.find("攻擊+50") != std::string::npos);
    CHECK(sameComboFull.find("防禦+50") != std::string::npos);
    const auto sameComboCompact = descriptionText(std::span<const EffectRule>{&(sameComboDeath), 1},
        EffectDescriptionStyle::Compact,
        {});
    CHECK(sameComboCompact.find("同羈絆友軍死亡") != std::string::npos);
    CHECK(sameComboCompact.find("攻+50") != std::string::npos);
    CHECK(sameComboCompact.find("防+50") != std::string::npos);

    const auto crocodileArmor = std::ranges::find(
        content->equipment(),
        61,
        &EquipmentDef::itemId);
    REQUIRE(crocodileArmor != content->equipment().end());
    REQUIRE(crocodileArmor->rules.size() == 2);
    CHECK(descriptionText(std::span<const EffectRule>{&(crocodileArmor->rules[1]), 1}, EffectDescriptionStyle::Full, {})
        == "格擋率+6%");
    CHECK(descriptionText(std::span<const EffectRule>{&(crocodileArmor->rules[1]), 1}, EffectDescriptionStyle::Compact, {})
        == "格擋+6%");

    const auto arhat = std::ranges::find(
        content->neigong(),
        96,
        &NeigongDef::magicId);
    REQUIRE(arhat != content->neigong().end());
    REQUIRE(arhat->rules.size() == 1);
    const auto arhatFull = descriptionText(std::span<const EffectRule>{&(arhat->rules.front()), 1},
        EffectDescriptionStyle::Full,
        {});
    CHECK(arhatFull.find("招式傷害+4%") != std::string::npos);
    CHECK(arhatFull.find("最多8層") != std::string::npos);
    CHECK(arhatFull.find("91幀") != std::string::npos);
    const auto arhatCompact = descriptionText(std::span<const EffectRule>{&(arhat->rules.front()), 1},
        EffectDescriptionStyle::Compact,
        {});
    CHECK(arhatCompact.find("增傷4%×8層") != std::string::npos);
    CHECK(arhatCompact.find("91幀") != std::string::npos);
    const auto arhatDetailed = descriptionText(std::span<const EffectRule>{&(arhat->rules.front()), 1},
        EffectDescriptionStyle::Detailed,
        {});
    CHECK(arhatDetailed.find("防禦結算後") != std::string::npos);
    CHECK(arhatDetailed.find("增加層數") != std::string::npos);
    CHECK(arhatDetailed.find("最多8層") != std::string::npos);
    CHECK(arhatDetailed.find("91幀") != std::string::npos);

    const auto& sandWhip = ruleWithEvent(
        definitionWithId(content->magicEffects(), 78),
        EffectEvent::MainProjectileBeforeDamage);
    const auto sandWhipFull = descriptionText(std::span<const EffectRule>{&(sandWhip), 1},
        EffectDescriptionStyle::Full,
        {});
    CHECK(sandWhipFull.find("在命中位置建立半徑6格的區域") != std::string::npos);
    CHECK(sandWhipFull.find("持續100幀") != std::string::npos);
    CHECK(sandWhipFull.find("區域內敵人速度-25%") != std::string::npos);
    CHECK(sandWhipFull.find("彈道無法追蹤") != std::string::npos);
    CHECK(sandWhipFull.find("彈速及壓制傷害-35%") != std::string::npos);
    const auto sandWhipCompact = descriptionText(std::span<const EffectRule>{&(sandWhip), 1},
        EffectDescriptionStyle::Compact,
        {});
    CHECK(sandWhipCompact.find("區域6格") != std::string::npos);
    CHECK(sandWhipCompact.find("100幀") != std::string::npos);
    CHECK(sandWhipCompact.find("敵速-25%") != std::string::npos);
    CHECK(sandWhipCompact.find("壓制-35%") != std::string::npos);
    CHECK(sandWhipCompact.find("禁追蹤") != std::string::npos);
    CHECK(sandWhipCompact.find('/') == std::string::npos);

    auto mixedRelationArea = sandWhip;
    auto& mixedArea = std::get<CreateAreaAction>(mixedRelationArea.actions.front().value);
    mixedArea.modifiers[1].relation = EffectTeamFilter::Ally;
    const auto mixedAreaCompact = descriptionText(std::span<const EffectRule>{&(mixedRelationArea), 1},
        EffectDescriptionStyle::Compact,
        {});
    CHECK(mixedAreaCompact != sandWhipCompact);
    CHECK(mixedAreaCompact.find("友禁追蹤") != std::string::npos);

    mixedArea.modifiers.front().amount.base = EffectNumberBase::SourceStar;
    mixedArea.modifiers.front().amount.flat = 0;
    mixedArea.modifiers.front().amount.percent = 100;
    CHECK(descriptionText(std::span<const EffectRule>{&(mixedRelationArea), 1}, EffectDescriptionStyle::Full, {}).find(
        "速度增加星級×1%") != std::string::npos);
}

TEST_CASE("ChessBattleEffects_OnlyOrdinaryAcceptedHitsUseTheHitAfterProjection",
          "[battle][effects][description][phrasing]")
{
    auto rule = parseRuleText(R"(
時機: 造成傷害後
目標: 自身
條件:
  - 已接受命中
回復內力: 12
)");
    CHECK(descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Full, {})
        == "每次命中後回復12內力");
    CHECK(descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Compact, {})
        == "命中後回復12內力");

    auto& accepted = std::get<AcceptedHitCondition>(rule.conditions[1]);
    accepted.requirePositiveDamage = true;
    const auto nonOrdinary = descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Full, {});
    CHECK(nonOrdinary.starts_with("傷害結算後"));
    CHECK(nonOrdinary.find("效果持有者造成傷害") != std::string::npos);
    CHECK(nonOrdinary.find("正傷害") != std::string::npos);
}

TEST_CASE("ChessBattleEffects_CoupleBladeCarriesTypedAllyAttackSource",
          "[battle][effects][magic][schema][attack_source]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto& rule = ruleWithEvent(
        definitionWithId(definitions, 62),
        EffectEvent::AttackCommitted);
    REQUIRE(rule.actions.size() == 1);
    const auto conditional = std::get<std::shared_ptr<ConditionalEffectAction>>(
        rule.actions.front().value);
    REQUIRE(conditional);
    REQUIRE(conditional->whenTrue.size() == 1);
    const auto& attack = std::get<ModifyAttackAction>(
        conditional->whenTrue.front().value);
    REQUIRE(attack.source);
    CHECK(attack.source->kind == EffectSelectorKind::Allies);
    CHECK(attack.source->count == 1);
    CHECK(attack.source->excludeOwner);
    CHECK(attack.source->requiredBoundMagic);
    CHECK_FALSE(attack.through);
    CHECK_FALSE(attack.tracking);
    CHECK(attack.strengthPct == 100);
    CHECK(attack.mainProjectile);
    CHECK(attack.targets == AttackTargetPolicy::SameTarget);
    CHECK(attack.addToBaseAttack);
    CHECK(attack.propagation == CastPropagationPolicy::SuppressUltimateRules);
    REQUIRE(conditional->whenFalse.size() == 1);
    const auto& fallback = std::get<ModifyAttackAction>(
        conditional->whenFalse.front().value);
    CHECK(fallback.strengthPct == 50);
    CHECK_FALSE(fallback.mainProjectile);
    CHECK(fallback.targets == AttackTargetPolicy::SameTarget);
    CHECK(fallback.addToBaseAttack);
    CHECK(fallback.propagation == CastPropagationPolicy::SuppressUltimateRules);

    EffectRule explicitFlags;
    REQUIRE(parseEffectRule(
        YAML::Load(R"(
時機: 絕招施放
目標: 原攻擊目標
動作:
  - 修改攻擊:
      貫穿: false
      追蹤: false
)"),
        explicitFlags,
        EffectRuleId{ 1 },
        "攻擊彈道繼承欄位"));
    const auto& explicitAttack = std::get<ModifyAttackAction>(
        explicitFlags.actions.front().value);
    REQUIRE(explicitAttack.through.has_value());
    REQUIRE(explicitAttack.tracking.has_value());
    CHECK_FALSE(*explicitAttack.through);
    CHECK_FALSE(*explicitAttack.tracking);
}

TEST_CASE("ChessBattleEffects_HuFamilyBladeUsesTypedPerCastTargetActivationLimit", "[battle][effects][magic][schema]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto& rule = ruleWithEvent(
        definitionWithId(definitions, 67),
        EffectEvent::MainProjectileBeforeDamage);
    REQUIRE(rule.activationLimit.has_value());
    CHECK(rule.activationLimit->scope == EffectActivationScope::PerCastPerTarget);
    CHECK(rule.activationLimit->maxEvaluations == 1);
    REQUIRE(rule.actions.size() == 1);
    const auto& damage = std::get<DealDamageAction>(rule.actions.front().value);
    CHECK(damage.perCast.perTargetLimit == 0);

    CHECK(descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Full, {}).find(
        "每次施放對同一目標最多判定1次") != std::string::npos);
    CHECK(descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Compact, {}).find(
        "每施放每目標1次") != std::string::npos);
}

TEST_CASE("ChessBattleEffects_ActivationLimitSchemaRejectsInvalidScopeCountAndEvent", "[battle][effects][magic][schema]")
{
    const auto parses = [](std::string_view yaml, std::uint64_t id)
    {
        EffectRule rule;
        return parseEffectRule(
            YAML::Load(std::string(yaml)),
            rule,
            EffectRuleId{ id },
            "觸發限制驗證");
    };

    CHECK_FALSE(parses(R"(
時機: 主彈命中
目標: 命中目標
觸發限制:
  範圍: 未知範圍
  次數: 1
動作:
  - 造成傷害:
      數值: 1
      傷害種類: 純粹
      範圍: 單體
)", 1));
    CHECK_FALSE(parses(R"(
時機: 主彈命中
目標: 命中目標
觸發限制:
  範圍: 每次施放每個目標
  次數: 0
動作:
  - 造成傷害:
      數值: 1
      傷害種類: 純粹
      範圍: 單體
)", 2));
    CHECK_FALSE(parses(R"(
時機: 開場
觸發限制:
  範圍: 每次施放每個目標
  次數: 1
動作:
  - 屬性修正:
      屬性: 攻擊
      方式: 固定加算
      數值: 1
)", 3));
}

TEST_CASE("ChessBattleEffects_OwnerAnyCastRequiresCastProvenance", "[battle][effects][magic][schema][cast_match]")
{
    ModifyAttributeAction modifier;
    modifier.attribute = BattleAttribute::Attack;
    modifier.amount.flat = 1;
    EffectRule rule;
    rule.id = EffectRuleId{ 1 };
    rule.event = EffectEvent::FrameAdvanced;
    rule.castMatch = EffectCastMatch::OwnerAnyCast;
    rule.selector.kind = EffectSelectorKind::Self;
    rule.actions = { EffectAction{ EffectActionValue{ modifier } } };

    std::string error;
    CHECK_FALSE(validateEffectRule(rule, error));
    CHECK(error.find("施放識別") != std::string::npos);
}

TEST_CASE("ChessBattleEffects_AttackDamageDescriptionTracksOverrideAndKindIndependently",
          "[battle][effects][magic][schema][description]")
{
    ModifyAttackAction attack;
    EffectRule rule;
    rule.id = EffectRuleId{ 1 };
    rule.event = EffectEvent::UltimateCommitted;
    rule.selector.kind = EffectSelectorKind::OriginalAttackTarget;
    rule.actions = { EffectAction{ EffectActionValue{ attack } } };

    for (const auto style : {
             EffectDescriptionStyle::Full,
             EffectDescriptionStyle::Compact,
         })
    {
        const auto inherited = descriptionText(std::span<const EffectRule>{&(rule), 1}, style, {});

        auto& configured = std::get<ModifyAttackAction>(rule.actions.front().value);
        configured.damageOverride = EffectNumber{ .flat = 250 };
        const auto overrideOnly = descriptionText(std::span<const EffectRule>{&(rule), 1}, style, {});
        CHECK(overrideOnly.find("250傷害（沿用原傷害種類）") != std::string::npos);
        CHECK(overrideOnly != inherited);

        configured.damageKind = BattleDamageKind::Pure;
        const auto overrideAndKind = descriptionText(std::span<const EffectRule>{&(rule), 1}, style, {});
        CHECK(overrideAndKind.find("250純粹傷害") != std::string::npos);
        CHECK(overrideAndKind != overrideOnly);

        configured.damageOverride.reset();
        const auto kindOnly = descriptionText(std::span<const EffectRule>{&(rule), 1}, style, {});
        CHECK(kindOnly.find("傷害種類改為純粹") != std::string::npos);
        CHECK(kindOnly != overrideAndKind);

        configured.damageKind.reset();
    }
}

TEST_CASE("ChessBattleEffects_ParsesAndDescribesLivingUnitSelectorsWithOwnerExclusion", "[battle][effects][magic][schema][selector]")
{
    const auto node = YAML::Load(R"(
時機: 絕招施放
目標:
  類型: 所有存活單位
  數量: 1
  平手: 戰鬥亂數
  排除效果擁有者: true
動作:
  - 資源變更:
      資源: 內力
      方式: 回復
      數值: 10
)");
    EffectRule rule;
    REQUIRE(parseEffectRule(node, rule, EffectRuleId{ 1 }, "存活單位選擇器"));
    CHECK(rule.selector.kind == EffectSelectorKind::AllLivingUnits);
    CHECK(rule.selector.count == 1);
    CHECK(rule.selector.tieBreak == EffectTieBreak::BattleRandom);
    CHECK(rule.selector.excludeOwner);
    CHECK(descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Full, {}).find("存活單位1人（不含效果持有者）")
          != std::string::npos);
    CHECK(descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Compact, {}).find("存活單位1人（不含效果持有者）")
          != std::string::npos);

    CopyAttackDefinitionAction copy;
    copy.sourceUnits.kind = EffectSelectorKind::UnitsInRadius;
    copy.sourceUnits.radiusTiles = 0;
    copy.filter.conditions = {
        CopiedMagicCondition::HasUltimateAttackDefinition,
        CopiedMagicCondition::ExcludesRecursiveEffects,
    };
    EffectRule invalid;
    invalid.id = { 2 };
    invalid.event = EffectEvent::UltimateCommitted;
    invalid.selector.kind = EffectSelectorKind::Self;
    invalid.actions = { { EffectActionValue{ StateMachineAction{ copy } } } };
    std::string error;
    CHECK_FALSE(validateEffectRule(invalid, error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("ChessBattleEffects_RealSelectorsExcludeSanqingCasterAndLetXiaowuxiangUseAnyOtherLivingUnit", "[battle][effects][magic][schema][selector]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto& sanqing = ruleWithEvent(
        definitionWithId(definitions, 133),
        EffectEvent::AttackCommitted,
        1);
    CHECK(sanqing.selector.kind == EffectSelectorKind::LowestMpAllies);
    CHECK(sanqing.selector.count == 2);
    CHECK(sanqing.selector.excludeOwner);
    CHECK(descriptionText(std::span<const EffectRule>{&(sanqing), 1}, EffectDescriptionStyle::Full, {}).find("不含效果持有者")
          != std::string::npos);

    const auto& xiaowuxiang = ruleWithEvent(
        definitionWithId(definitions, 98),
        EffectEvent::UltimateCommitted);
    REQUIRE(xiaowuxiang.actions.size() == 1);
    const auto& machine = std::get<StateMachineAction>(xiaowuxiang.actions.front().value);
    const auto& copy = std::get<CopyAttackDefinitionAction>(machine);
    CHECK(copy.sourceUnits.kind == EffectSelectorKind::AllLivingUnits);
    CHECK(copy.sourceUnits.count == 0);
    CHECK(copy.copyCount == 1);
    CHECK(copy.sourceUnits.tieBreak == EffectTieBreak::BattleRandom);
    CHECK(copy.sourceUnits.excludeOwner);
    CHECK(copy.filter.conditions == std::vector{
        CopiedMagicCondition::HasUltimateAttackDefinition,
        CopiedMagicCondition::ExcludesRecursiveEffects,
    });
    const auto description = descriptionText(std::span<const EffectRule>{&(xiaowuxiang), 1}, EffectDescriptionStyle::Detailed, {});
    CHECK(description.find("所有存活單位（不含效果持有者）") != std::string::npos);
    CHECK(description.find("選擇：1名") != std::string::npos);
    CHECK(description.find("有絕招攻擊定義") != std::string::npos);
    CHECK(description.find("排除複製與借用遞迴") != std::string::npos);
    CHECK(description.find("不複製該單位的大招規則") != std::string::npos);
}

TEST_CASE("ChessBattleEffects_BorrowRulesRequireCastPlanningAndBorrowedPropagation",
          "[battle][effects][magic][schema][borrow]")
{
    BorrowEffectRulesAction borrow;
    borrow.sourceUnits.kind = EffectSelectorKind::Enemies;
    borrow.sourceCount.flat = 1;
    borrow.filter.allowedActionCategories = {
        BorrowedRuleActionCategory::ResourceChange,
    };
    borrow.propagation = CastPropagationPolicy::BorrowedUltimateRules;

    EffectRule rule;
    rule.id = EffectRuleId{ 1 };
    rule.event = EffectEvent::CastPlanned;
    rule.selector.kind = EffectSelectorKind::Self;
    rule.actions = { { EffectActionValue{ StateMachineAction{ borrow } } } };

    std::string error;
    CHECK(validateEffectRule(rule, error));

    auto missingFilter = rule;
    auto& missingFilterMachine = std::get<StateMachineAction>(
        missingFilter.actions.front().value);
    std::get<BorrowEffectRulesAction>(missingFilterMachine)
        .filter.allowedActionCategories.clear();
    CHECK_FALSE(validateEffectRule(missingFilter, error));
    CHECK(error.find("allow-list") != std::string::npos);

    auto wrongEvent = rule;
    wrongEvent.event = EffectEvent::UltimateCommitted;
    CHECK_FALSE(validateEffectRule(wrongEvent, error));
    CHECK(error.find("施放規劃") != std::string::npos);

    auto wrongPropagation = rule;
    auto& machine = std::get<StateMachineAction>(wrongPropagation.actions.front().value);
    std::get<BorrowEffectRulesAction>(machine).propagation =
        CastPropagationPolicy::SourceRules;
    CHECK_FALSE(validateEffectRule(wrongPropagation, error));
    CHECK(error.find("借用大招規則") != std::string::npos);
}

TEST_CASE("ChessBattleEffects_CopyAttackRequiresUltimateCommitSuppressedPropagationAndOwnerExclusion",
          "[battle][effects][magic][schema][copy]")
{
    CopyAttackDefinitionAction copy;
    copy.sourceUnits.kind = EffectSelectorKind::AllLivingUnits;
    copy.sourceUnits.tieBreak = EffectTieBreak::BattleRandom;
    copy.sourceUnits.excludeOwner = true;
    copy.filter.conditions = {
        CopiedMagicCondition::HasUltimateAttackDefinition,
        CopiedMagicCondition::ExcludesRecursiveEffects,
    };
    copy.copyCount = 1;
    copy.propagation = CastPropagationPolicy::SuppressUltimateRules;

    EffectRule rule;
    rule.id = EffectRuleId{ 1 };
    rule.event = EffectEvent::UltimateCommitted;
    rule.selector.kind = EffectSelectorKind::Self;
    rule.actions = { { EffectActionValue{ StateMachineAction{ copy } } } };

    std::string error;
    CHECK(validateEffectRule(rule, error));

    auto allowsRecursion = rule;
    auto& allowsRecursionMachine = std::get<StateMachineAction>(
        allowsRecursion.actions.front().value);
    std::get<CopyAttackDefinitionAction>(allowsRecursionMachine)
        .filter.conditions = {
            CopiedMagicCondition::HasUltimateAttackDefinition,
        };
    CHECK_FALSE(validateEffectRule(allowsRecursion, error));
    CHECK(error.find("排除複製與借用遞迴") != std::string::npos);

    auto wrongEvent = rule;
    wrongEvent.event = EffectEvent::AttackCommitted;
    CHECK_FALSE(validateEffectRule(wrongEvent, error));
    CHECK(error.find("絕招提交") != std::string::npos);

    auto wrongPropagation = rule;
    auto& wrongPropagationMachine = std::get<StateMachineAction>(
        wrongPropagation.actions.front().value);
    std::get<CopyAttackDefinitionAction>(wrongPropagationMachine).propagation =
        CastPropagationPolicy::SourceRules;
    CHECK_FALSE(validateEffectRule(wrongPropagation, error));
    CHECK(error.find("不傳播大招規則") != std::string::npos);

    auto includesOwner = rule;
    auto& includesOwnerMachine = std::get<StateMachineAction>(
        includesOwner.actions.front().value);
    std::get<CopyAttackDefinitionAction>(includesOwnerMachine).sourceUnits.excludeOwner = false;
    CHECK_FALSE(validateEffectRule(includesOwner, error));
    CHECK(error.find("排除效果擁有者") != std::string::npos);
}

TEST_CASE("ChessBattleEffects_TransferStateMachinesRequireExplicitAllowLists",
          "[battle][effects][magic][schema][filter]")
{
    const auto missingBorrowFilter = YAML::Load(R"(
時機: 施放規劃
目標: 自身
動作:
  - 借用效果規則:
      目標: 敵軍
      來源數量: 1
      傳播政策: 借用大招規則
)");
    const auto missingCopyFilter = YAML::Load(R"(
時機: 絕招施放
目標: 自身
動作:
  - 複製攻擊定義:
      目標:
        類型: 所有存活單位
        排除效果擁有者: true
      來源數量: 1
      傳播政策: 不傳播大招規則
)");
    EffectRule rule;
    CHECK_FALSE(parseEffectRule(
        missingBorrowFilter,
        rule,
        { 91 },
        "缺少借用 allow-list"));
    CHECK_FALSE(parseEffectRule(
        missingCopyFilter,
        rule,
        { 92 },
        "缺少複製 allow-list"));
}

TEST_CASE("ChessBattleEffects_TypedMagicLoaderLeavesSourceBindingToRuntime", "[battle][effects][magic][schema]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto hasInjectedCastCondition = [](const EffectRule& rule)
    {
        return std::ranges::any_of(rule.conditions, [](const EffectCondition& condition)
        {
            return std::holds_alternative<IsUltimateCondition>(condition)
                || std::holds_alternative<CastUsesEffectSourceMagicCondition>(condition);
        });
    };

    const auto& castRule = ruleWithEvent(
        definitionWithId(definitions, 127),
        EffectEvent::AttackCommitted);
    const auto& deathObserver = ruleWithEvent(
        definitionWithId(definitions, 95),
        EffectEvent::UnitDied);
    CHECK_FALSE(hasInjectedCastCondition(castRule));
    CHECK_FALSE(hasInjectedCastCondition(deathObserver));
    REQUIRE(deathObserver.conditions.size() == 1);
    CHECK(std::holds_alternative<SourceHasStateCondition>(deathObserver.conditions.front()));
}

TEST_CASE("ChessBattleEffects_RealSchemaCoversFourVerticalSlices", "[battle][effects][magic][schema]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto& qingnang = definitionWithId(definitions, 127);
    const auto& qingnangRule = ruleWithEvent(qingnang, EffectEvent::AttackCommitted);
    CHECK(qingnang.name == "青囊奇術");
    CHECK(qingnangRule.selector.kind == EffectSelectorKind::LowestHpAllies);
    CHECK(qingnangRule.selector.count == 1);
    REQUIRE(qingnangRule.actions.size() == 1);
    const auto* qingnangHeal = std::get_if<ChangeResourceAction>(&qingnangRule.actions[0].value);
    REQUIRE(qingnangHeal != nullptr);
    CHECK(qingnangHeal->resource == BattleResource::Hp);
    CHECK(qingnangHeal->kind == ResourceChangeKind::Restore);
    CHECK(qingnangHeal->amount.base == EffectNumberBase::TargetMaxHp);
    CHECK(qingnangHeal->amount.percent == 7);

    const auto& shenzhao = definitionWithId(definitions, 94);
    const auto& shenzhaoPlan = ruleWithEvent(shenzhao, EffectEvent::CastPlanned);
    const auto* castChange = std::get_if<ModifyCastAction>(&shenzhaoPlan.actions[0].value);
    REQUIRE(castChange != nullptr);
    REQUIRE(castChange->mpCost.has_value());
    CHECK(castChange->mpCost->flat == 75);
    const auto& shenzhaoCommit = ruleWithEvent(shenzhao, EffectEvent::AttackCommitted);
    const auto* shield = std::get_if<ChangeResourceAction>(&shenzhaoCommit.actions[0].value);
    REQUIRE(shield != nullptr);
    CHECK(shield->resource == BattleResource::Shield);
    CHECK(shield->amount.base == EffectNumberBase::SourceStar);
    CHECK(shield->amount.percent == 10000);

    const auto& witheredBone = definitionWithId(definitions, 11);
    const auto& witheredBoneHit = ruleWithEvent(witheredBone, EffectEvent::MainProjectileBeforeDamage);
    const auto* status = std::get_if<ApplyStatusAction>(&witheredBoneHit.actions[0].value);
    REQUIRE(status != nullptr);
    CHECK(status->status == BattleStatusKind::WitheredBone);
    CHECK(status->durationFrames == 120);
    CHECK(status->potency.flat == 25);
    CHECK(status->secondaryPotency.flat == 75);
    CHECK(status->stack == EffectStackPolicy::Refresh);

    const auto& fiveTiger = definitionWithId(definitions, 59);
    const auto& fiveTigerPlan = ruleWithEvent(fiveTiger, EffectEvent::CastPlanned);
    const auto* attack = std::get_if<ModifyAttackAction>(&fiveTigerPlan.actions[0].value);
    REQUIRE(attack != nullptr);
    CHECK(attack->pattern.kind == AttackPatternKind::Fan);
    CHECK(attack->pattern.projectileCount == 5);
    CHECK(attack->strengthPct == 60);
    CHECK(attack->through);
    CHECK(attack->mainProjectile);
    CHECK(attack->sameTargetHitLimit == 1);
    const auto& fiveTigerDebuff = ruleWithEvent(fiveTiger, EffectEvent::MainProjectileBeforeDamage);
    const auto* defence = std::get_if<ModifyAttributeAction>(&fiveTigerDebuff.actions[0].value);
    REQUIRE(defence != nullptr);
    CHECK(defence->attribute == BattleAttribute::Defence);
    CHECK(defence->amount.flat == -30);
    CHECK(defence->durationFrames == 90);
}

TEST_CASE("ChessBattleEffects_DescriptionsUsePayloadNumbersAndTypedMultiplier", "[battle][effects][magic][description]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto& qingnangRule = ruleWithEvent(definitionWithId(definitions, 127), EffectEvent::AttackCommitted);
    auto mutated = qingnangRule;
    auto& heal = std::get<ChangeResourceAction>(mutated.actions[0].value);
    heal.amount.percent = 9;
    for (const auto style : { EffectDescriptionStyle::Full, EffectDescriptionStyle::Compact })
    {
        const auto originalText = descriptionText(std::span<const EffectRule>{&(qingnangRule), 1}, style, {});
        const auto mutatedText = descriptionText(std::span<const EffectRule>{&(mutated), 1}, style, {});
        CHECK(originalText != mutatedText);
        CHECK(originalText.find("7%") != std::string::npos);
        CHECK(mutatedText.find("9%") != std::string::npos);
    }

    const auto& scissorsRule = ruleWithEvent(definitionWithId(definitions, 75), EffectEvent::MainProjectileBeforeDamage);
    for (const auto style : { EffectDescriptionStyle::Full, EffectDescriptionStyle::Compact })
    {
        const auto text = descriptionText(std::span<const EffectRule>{&(scissorsRule), 1}, style, {});
        CHECK(text.find("目標目前護盾的20%×星級") != std::string::npos);
    }
    CHECK(descriptionText(
        std::span<const EffectRule>{&(scissorsRule), 1},
        EffectDescriptionStyle::Full,
        {}).ends_with("移除目標目前護盾的20%×星級"));

    const auto cooldownRule = parseRuleText(R"(
時機: 每隔
間隔幀數: 120
目標: 自身
動作:
  - 資源變更:
      資源: 目前冷卻
      方式: 移除
      數值:
        基準: 目標目前冷卻
        百分比: 30
        取整: 向上
)");
    const auto cooldownFull = descriptionText(
        std::span<const EffectRule>{&(cooldownRule), 1},
        EffectDescriptionStyle::Full,
        {});
    CHECK(cooldownFull.find("移除目標目前冷卻的30%")
        != std::string::npos);
    CHECK(cooldownFull.find("目標目前冷卻的30%目前冷卻")
        == std::string::npos);

    auto reversedShieldRule = scissorsRule;
    auto& reversedShield = std::get<ChangeResourceAction>(
        reversedShieldRule.actions[0].value);
    reversedShield.amount.base = EffectNumberBase::SourceStar;
    reversedShield.amount.multiplierBase =
        EffectNumberBase::TargetCurrentShield;
    reversedShield.amount.percent = 20;

    const auto reversedCooldownRule = parseRuleText(R"(
時機: 每隔
間隔幀數: 120
目標: 自身
動作:
  - 資源變更:
      資源: 目前冷卻
      方式: 移除
      數值:
        基準: 來源星級
        乘數基準: 目標目前冷卻
        百分比: 30
        取整: 向上
)");
    const auto reversedHpRule = parseRuleText(R"(
時機: 絕招施放
目標: 自身
回復生命:
  數值:
    基準: 來源星級
    乘數基準: 目標最大生命
    百分比: 5
)");
    for (const auto style : {
             EffectDescriptionStyle::Full,
             EffectDescriptionStyle::Compact,
         })
    {
        const auto reversedShieldText = descriptionText(
            std::span<const EffectRule>{&(reversedShieldRule), 1},
            style,
            {});
        CHECK(reversedShieldText.find("目標目前護盾") != std::string::npos);
        CHECK(reversedShieldText.find("目標目前護盾護盾")
            == std::string::npos);

        const auto reversedCooldownText = descriptionText(
            std::span<const EffectRule>{&(reversedCooldownRule), 1},
            style,
            {});
        CHECK(reversedCooldownText.find("目標目前冷卻") != std::string::npos);
        CHECK(reversedCooldownText.find("目標目前冷卻目前冷卻")
            == std::string::npos);

        const auto reversedHpText = descriptionText(
            std::span<const EffectRule>{&(reversedHpRule), 1},
            style,
            {});
        CHECK(reversedHpText.find("目標最大生命") != std::string::npos);
        CHECK(reversedHpText.find("目標最大生命生命")
            == std::string::npos);
    }

    const auto& coupleBlade = ruleWithEvent(
        definitionWithId(definitions, 62),
        EffectEvent::AttackCommitted);
    const auto& silverWhip = ruleWithEvent(
        definitionWithId(definitions, 79),
        EffectEvent::MainProjectileBeforeDamage);
    const auto& sunflower = ruleWithEvent(
        definitionWithId(definitions, 105),
        EffectEvent::AttackSpawned);
    for (const auto style : { EffectDescriptionStyle::Full, EffectDescriptionStyle::Compact })
    {
        const auto coupleBladeText = descriptionText(std::span<const EffectRule>{&(coupleBlade), 1}, style, {});
        CHECK(coupleBladeText.find("同武功友軍") != std::string::npos);
        CHECK(coupleBladeText.find("武功62") == std::string::npos);
        CHECK(coupleBladeText.find("追加至基礎攻擊") == std::string::npos);
        CHECK(coupleBladeText.find("不觸發大招效果") != std::string::npos);

        const auto silverWhipText = descriptionText(std::span<const EffectRule>{&(silverWhip), 1}, style, {});
        CHECK(silverWhipText.find("3格內至多3名敵軍") != std::string::npos);
        CHECK(silverWhipText.find("必含命中目標") != std::string::npos);

        const auto sunflowerText = descriptionText(std::span<const EffectRule>{&(sunflower), 1}, style, {});
        CHECK(sunflowerText.find("最近3名敵人") != std::string::npos);
        CHECK(sunflowerText.find("殘影非主彈×2") != std::string::npos);
        CHECK(sunflowerText.find("50%傷害") != std::string::npos);
        CHECK((sunflowerText.find("效果擁有者任意施放") != std::string::npos
               || sunflowerText.find("任意施放") != std::string::npos));
    }
}

TEST_CASE("ChessBattleEffects_QiankunCarriesTypedAbsorptionSettlement", "[battle][effects][magic][schema][absorption]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto& rule = ruleWithEvent(
        definitionWithId(definitions, 97),
        EffectEvent::AttackCommitted);
    REQUIRE(rule.actions.size() == 1);
    const auto& machine = std::get<StateMachineAction>(rule.actions[0].value);
    const auto& absorption = std::get<StartDamageAbsorptionAction>(machine);
    CHECK(absorption.slot == EffectStateSlot::AbsorbedDamage);
    CHECK(absorption.absorbedPct == 40);
    CHECK(absorption.durationFrames == 80);
    CHECK(absorption.settleOnSourceDeath);
    CHECK(absorption.settlementTarget.kind == EffectSelectorKind::Enemies);
    CHECK(absorption.settlementTarget.count == 1);
    CHECK(absorption.settlementTarget.tieBreak == EffectTieBreak::BattleRandom);
    CHECK(absorption.settlementDamageKind == BattleDamageKind::Pure);
    CHECK(absorption.returnedPct == 100);

    for (const auto style : { EffectDescriptionStyle::Full, EffectDescriptionStyle::Compact })
    {
        const auto description = descriptionText(std::span<const EffectRule>{&(rule), 1}, style, {});
        CHECK(description.find("80幀") != std::string::npos);
        CHECK(description.find("40%") != std::string::npos);
        CHECK(description.find("100%") != std::string::npos);
        CHECK(description.find("純粹傷害") != std::string::npos);
        CHECK(description.find("隨機敵軍1人") != std::string::npos);
        CHECK(description.find("來源死亡時立即結算") != std::string::npos);
    }
}

TEST_CASE("ChessBattleEffects_ParsesPreviouslyIgnoredTypedFields", "[battle][effects][magic][schema]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto& cleanse = ruleWithEvent(definitionWithId(definitions, 134), EffectEvent::AttackCommitted);
    const auto* removal = std::get_if<RemoveStatusAction>(&cleanse.actions[1].value);
    REQUIRE(removal != nullptr);
    CHECK(removal->order == StatusRemovalOrder::LongestRemaining);

    const auto& pull = ruleWithEvent(definitionWithId(definitions, 79), EffectEvent::MainProjectileBeforeDamage);
    REQUIRE(pull.selector.requiredTarget);
    CHECK(*pull.selector.requiredTarget == EffectRequiredTarget::HitTarget);
    const auto* movement = std::get_if<ForceMoveAction>(&pull.actions[0].value);
    REQUIRE(movement != nullptr);
    CHECK(movement->collision == ForceMoveCollision::StopBeforeBlocked);
    CHECK(movement->blocked == ForceMoveBlockedResult::Shorten);

    const auto& sunflower = ruleWithEvent(
        definitionWithId(definitions, 105),
        EffectEvent::AttackSpawned);
    CHECK(sunflower.castMatch == EffectCastMatch::OwnerAnyCast);
    CHECK(std::ranges::any_of(sunflower.conditions, [](const EffectCondition& condition)
    {
        return std::holds_alternative<IsRootAttackCondition>(condition);
    }));
    CHECK(descriptionText(std::span<const EffectRule>{&(sunflower), 1}, EffectDescriptionStyle::Full, {}).find("任意施放")
          != std::string::npos);
    const auto* sunflowerEcho = std::get_if<ModifyAttackAction>(
        &sunflower.actions[0].value);
    REQUIRE(sunflowerEcho != nullptr);
    CHECK(sunflowerEcho->strengthPct == 50);
    CHECK(sunflowerEcho->propagation == CastPropagationPolicy::NoEffectRules);

    const auto& formation = ruleWithEvent(definitionWithId(definitions, 86), EffectEvent::AttackCommitted);
    const auto* area = std::get_if<CreateAreaAction>(&formation.actions[0].value);
    REQUIRE(area != nullptr);
    REQUIRE(area->modifiers.size() == 3);
    CHECK(area->modifiers[2].kind == AreaModifierKind::OutgoingDamage);
    CHECK(area->modifiers[2].damageChannel == DamageChannel::All);

    const auto& poisonExplosion = ruleWithEvent(
        definitionWithId(definitions, 95),
        EffectEvent::UnitDied);
    const auto* poisonExplosionDamage = std::get_if<DealDamageAction>(
        &poisonExplosion.actions[0].value);
    REQUIRE(poisonExplosionDamage != nullptr);
    REQUIRE(poisonExplosion.repetitionCount);
    CHECK(poisonExplosion.repetitionCount->base == EffectNumberBase::SourceStatusStacks);
    CHECK(poisonExplosion.repetitionCount->status == BattleStatusKind::PoisonExplosion);
    CHECK(poisonExplosion.repetitionCount->minimum == 1);
    CHECK(poisonExplosion.selector.kind == EffectSelectorKind::UnitsInRadius);
    CHECK(poisonExplosionDamage->area.kind == DamageAreaKind::SingleTarget);
    CHECK(poisonExplosionDamage->amount.base == EffectNumberBase::SourceStatusPotency);
    CHECK(poisonExplosionDamage->amount.status == BattleStatusKind::PoisonExplosion);
    CHECK_FALSE(poisonExplosionDamage->transactionCount);
    const auto* poisonExplosionStatus = std::get_if<ApplyStatusAction>(
        &poisonExplosion.actions[1].value);
    REQUIRE(poisonExplosionStatus != nullptr);
    CHECK_FALSE(poisonExplosionStatus->applicationCount);
    CHECK(descriptionText(std::span<const EffectRule>{&(poisonExplosion), 1}, EffectDescriptionStyle::Full, {}).find(
        "依序重複毒爆層數的100%（至少1）次") != std::string::npos);

    const auto& sevenStar = ruleWithEvent(
        definitionWithId(definitions, 39),
        EffectEvent::HitBeforeDamage);
    CHECK(sevenStar.observation == EffectObservationScope::OwnerTeamEventSource);
    REQUIRE(sevenStar.conditions.size() == 1);
    CHECK(std::holds_alternative<TargetHasStateFromEffectOwnerCondition>(
        sevenStar.conditions.front()));
    REQUIRE(sevenStar.actions.size() == 2);
    const auto& sevenStarConsume = std::get<ConsumeStatusAction>(
        sevenStar.actions[1].value);
    CHECK(sevenStarConsume.status == BattleStatusKind::SevenStarMark);
    CHECK(sevenStarConsume.source == StatusSourceMatch::EffectOwner);
    REQUIRE(sevenStarConsume.whenDepleted);
    CHECK(sevenStarConsume.whenDepleted->status == BattleStatusKind::Stun);
    CHECK(sevenStarConsume.whenDepleted->durationFrames == 30);

    const auto& heavenDragon = definitionWithId(definitions, 84);
    const auto& heavenDragonCommit = ruleWithEvent(
        heavenDragon,
        EffectEvent::AttackCommitted);
    const auto& heavenDragonProgress = std::get<ChangeStateValueAction>(
        std::get<StateMachineAction>(heavenDragonCommit.actions[0].value));
    CHECK(heavenDragonProgress.slot == EffectStateSlot::PermanentCastProgress);
    CHECK(heavenDragonProgress.delta == 1);
    CHECK(heavenDragonProgress.maximum == 5);
    const auto& heavenDragonHit = ruleWithEvent(
        heavenDragon,
        EffectEvent::MainProjectileBeforeDamage);
    const auto& heavenDragonStun = std::get<ApplyStatusAction>(
        heavenDragonHit.actions[0].value);
    REQUIRE(heavenDragonStun.duration);
    CHECK(heavenDragonStun.duration->base == EffectNumberBase::StoredStateValue);
    CHECK(heavenDragonStun.duration->stateSlot == EffectStateSlot::PermanentCastProgress);
    CHECK(heavenDragonStun.duration->flat == 25);
    CHECK(heavenDragonStun.duration->percent == 2500);
    CHECK(heavenDragonStun.duration->maximum == 150);
    CHECK(heavenDragonStun.stack == EffectStackPolicy::Refresh);

    const auto& starShift = ruleWithEvent(
        definitionWithId(definitions, 43),
        EffectEvent::CastPlanned);
    const auto& starShiftMachine = std::get<StateMachineAction>(
        starShift.actions[0].value);
    const auto& borrowedRules = std::get<BorrowEffectRulesAction>(starShiftMachine);
    CHECK(borrowedRules.sourceCount.base == EffectNumberBase::SourceStar);
    CHECK(borrowedRules.sourceCount.percent == 50);
    CHECK(borrowedRules.sourceCount.rounding == EffectRounding::Ceil);
    CHECK(borrowedRules.filter.allowedActionCategories.size() == 14);
    CHECK(std::ranges::contains(
        borrowedRules.filter.allowedActionCategories,
        BorrowedRuleActionCategory::DamageMemory));
    const auto starShiftDescription = descriptionText(std::span<const EffectRule>{&(starShift), 1},
        EffectDescriptionStyle::Detailed,
        {});
    CHECK(starShiftDescription.find("選擇數量：星級×50%（向上取整），至少1，至多2")
          != std::string::npos);
    CHECK(starShiftDescription.find("允許類別：") != std::string::npos);
    CHECK(starShiftDescription.find("傷害記憶") != std::string::npos);
    CHECK(starShiftDescription.find("不借用複製或借用規則") != std::string::npos);

    const auto& ironPalm = definitionWithId(definitions, 13);
    const auto recordIt = std::ranges::find_if(ironPalm.rules, [](const EffectRule& rule)
    {
        return rule.event == EffectEvent::DamageResolved
            && std::ranges::any_of(rule.actions, [](const EffectAction& action)
            {
                const auto* machine = std::get_if<StateMachineAction>(&action.value);
                return machine && std::holds_alternative<RecordMaximumDamageAction>(*machine);
            });
    });
    REQUIRE(recordIt != ironPalm.rules.end());
    const auto& recordRule = *recordIt;
    const auto& recordMachine = std::get<StateMachineAction>(recordRule.actions[0].value);
    CHECK(std::get<RecordMaximumDamageAction>(recordMachine).slot == EffectStateSlot::CastMaximumHpDamage);
    const auto& consumeRule = ruleWithEvent(ironPalm, EffectEvent::CastSettled);
    const auto& consumeMachine = std::get<StateMachineAction>(consumeRule.actions[0].value);
    CHECK(std::get<ConsumeRecordedMaximumAction>(consumeMachine).slot == EffectStateSlot::CastMaximumHpDamage);
}

TEST_CASE("ChessBattleEffects_StrictSchemaRejectsUnknownFieldsAndIllegalEventActions", "[battle][effects][magic][schema]")
{
    const auto unknownRuleField = YAML::Load(R"(
時機: 絕招施放
未知欄位: 1
動作:
  - 資源變更:
      資源: 內力
      方式: 回復
      數值: 10
)");
    const auto unknownConditionField = YAML::Load(R"(
時機: 絕招施放
條件:
  - 僅限絕招:
      百分比: 50
動作:
  - 資源變更:
      資源: 內力
      方式: 回復
      數值: 10
)");
    const auto removedReflectedConditionField = YAML::Load(R"(
時機: 造成傷害後
條件:
  - 已接受命中:
      排除反彈: true
獲得護盾: 1
)");
    const auto illegalEventAction = YAML::Load(R"(
時機: 絕招施放
動作:
  - 治療交易修正:
      方式: 阻止
      治療種類: [直接治療]
)");
    const auto incompleteForceMove = YAML::Load(R"(
時機: 主彈命中
目標: 命中目標
動作:
  - 強制移動:
      方向: 遠離來源
      距離格數: 4
)");
    const auto illegalDamageCapability = YAML::Load(R"(
時機: 絕招施放
動作:
  - 資源變更:
      資源: 生命
      方式: 回復
      數值:
        基準: 實際生命傷害
        百分比: 50
)");

    EffectRule rule;
    CHECK_FALSE(parseEffectRule(unknownRuleField, rule, { 1 }, "未知規則欄位"));
    CHECK_FALSE(parseEffectRule(unknownConditionField, rule, { 2 }, "未知條件欄位"));
    CHECK_FALSE(parseEffectRule(
        removedReflectedConditionField,
        rule,
        { 6 },
        "已移除的反彈條件欄位"));
    CHECK_FALSE(parseEffectRule(illegalEventAction, rule, { 3 }, "非法事件動作"));
    CHECK_FALSE(parseEffectRule(incompleteForceMove, rule, { 4 }, "不完整強制移動"));
    CHECK_FALSE(parseEffectRule(illegalDamageCapability, rule, { 5 }, "非法事件數值"));
}

TEST_CASE("ChessBattleEffects_StatusApplicationCountRequiresAPositiveTypedFormula",
          "[battle][effects][magic][schema][status]")
{
    const auto parses = [](std::string_view count)
    {
        EffectRule rule;
        return parseEffectRule(
            YAML::Load(std::format(R"(
時機: 單位死亡
目標: 所有敵人
動作:
  - 套用狀態:
      狀態: 中毒
      套用次數: {}
      層數: 4
      持續幀數: 120
      強度: 10
      合併方式: 取代
      層數上限: 4
)", count)),
            rule,
            EffectRuleId{ 1 },
            "狀態套用次數驗證");
    };

    CHECK(parses(R"(
        基準: 來源狀態層數
        狀態: 毒爆
        百分比: 100
        最小: 1)"));
    CHECK_FALSE(parses("0"));
    CHECK_FALSE(parses(R"(
        基準: 來源狀態層數
        狀態: 毒爆
        百分比: 100)"));
}

TEST_CASE("ChessBattleEffects_ResourceChangesRequireNonnegativeAmountsAndOneTransferDestination", "[battle][effects][magic][schema]")
{
    const auto parses = [](std::string_view yaml, std::uint64_t id = 1)
    {
        EffectRule rule;
        return parseEffectRule(
            YAML::Load(std::string(yaml)),
            rule,
            EffectRuleId{ id },
            "資源變更驗證");
    };

    CHECK(parses(R"(
時機: 絕招施放
目標: 全隊
動作:
  - 資源變更:
      資源: 內力
      方式: 回復
      數值: 0
)"));
    CHECK_FALSE(parses(R"(
時機: 絕招施放
動作:
  - 資源變更:
      資源: 內力
      方式: 回復
      數值: -1
)"));
    CHECK_FALSE(parses(R"(
時機: 絕招施放
動作:
  - 資源變更:
      資源: 生命
      方式: 回復
      數值:
        基準: 目標最大生命
        百分比: -1
)"));
    CHECK(parses(R"(
時機: 絕招施放
動作:
  - 資源變更:
      資源: 生命
      方式: 回復
      數值:
        基準: 目標最大生命
        固定: -10
        百分比: 1
        最小: 0
)"));

    CHECK_FALSE(parses(R"(
時機: 絕招施放
動作:
  - 資源變更:
      資源: 內力
      方式: 轉移
      數值: 10
      轉移目標: 全隊
)"));
    CHECK_FALSE(parses(R"(
時機: 絕招施放
動作:
  - 資源變更:
      資源: 內力
      方式: 轉移
      數值: 10
      轉移目標:
        類型: 最低內力友軍
        數量: 2
)"));
    CHECK(parses(R"(
時機: 絕招施放
動作:
  - 資源變更:
      資源: 內力
      方式: 轉移
      數值: 10
      轉移目標:
        類型: 最低內力友軍
        數量: 1
)"));
    CHECK(parses(R"(
時機: 絕招施放
動作:
  - 資源變更:
      資源: 內力
      方式: 轉移
      數值: 10
      轉移目標: 自身
)"));
    CHECK_FALSE(parses(R"(
時機: 絕招施放
動作:
  - 資源變更:
      資源: 內力
      方式: 轉移
      數值: 10
      轉移目標: 命中目標
)"));
}

TEST_CASE("ChessBattleEffects_EventCapabilitiesAgreeForNestedAuthoringContexts",
          "[battle][effects][schema][event_capabilities]")
{
    const auto parses = [](std::string_view yaml)
    {
        EffectRule rule;
        return parseEffectRule(
            YAML::Load(std::string(yaml)),
            rule,
            EffectRuleId{ 1 },
            "事件能力測試");
    };

    CHECK(parses(R"(
時機: 開場
生成分身:
  數量: 1
)"));
    CHECK_FALSE(parses(R"(
時機: 絕招施放
生成分身:
  數量: 1
)"));
    CHECK_FALSE(parses(R"(
時機: 開場
目標: 命中目標
獲得護盾: 1
)"));
    CHECK_FALSE(parses(R"(
時機: 施放規劃
條件:
  - 傷害種類符合: [招式]
獲得護盾: 1
)"));
    CHECK_FALSE(parses(R"(
時機: 絕招施放
條件:
  - 攻擊序號: 1
獲得護盾: 1
)"));
    CHECK(parses(R"(
時機: 命中
目標: 交易目標
獲得護盾: 1
)"));
    CHECK_FALSE(parses(R"(
時機: 造成傷害後
目標: 原攻擊目標
獲得護盾: 1
)"));
    CHECK(parses(R"(
時機: 施放結算完成
條件:
  - 受益者施放前滿內
獲得護盾: 1
)"));
    CHECK_FALSE(parses(R"(
時機: 開場
記錄最大招式生命傷害:
  狀態槽: 最大招式生命傷害
)"));
    CHECK(parses(R"(
時機: 造成傷害後
記錄最大招式生命傷害:
  狀態槽: 最大招式生命傷害
)"));
    CHECK_FALSE(parses(R"(
時機: 開場
強制移動:
  方向: 遠離來源
  距離格數: 1
  碰撞: 阻擋前停止
  受阻結果: 縮短
)"));
    CHECK(parses(R"(
時機: 命中
強制移動:
  方向: 遠離來源
  距離格數: 1
  碰撞: 阻擋前停止
  受阻結果: 縮短
)"));
    CHECK_FALSE(parses(R"(
時機: 治療嘗試
獲得護盾: 1
)"));
    CHECK_FALSE(parses(R"(
時機: 絕招施放
條件分支:
  條件: [僅限絕招]
  成立:
    - 生成分身:
        數量: 1
)"));
}

TEST_CASE("ChessBattleEffects_DamagePerspectiveIsTypedButNotAuthorSpecified", "[battle][effects][magic][schema]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto perspectiveOf = [](const EffectRule& rule)
    {
        const auto it = std::ranges::find_if(rule.conditions, [](const EffectCondition& condition)
        {
            return std::holds_alternative<DamagePerspectiveCondition>(condition);
        });
        REQUIRE(it != rule.conditions.end());
        return std::get<DamagePerspectiveCondition>(*it).perspective;
    };
    for (const int magicId : { 16, 36, 46 })
    {
        const auto& rule = ruleWithEvent(definitionWithId(definitions, magicId), EffectEvent::DamageResolved);
        CHECK(perspectiveOf(rule) == DamagePerspective::Received);
        CHECK(descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Full, {}).find("效果持有者承受傷害") != std::string::npos);
    }
    const auto& lifesteal = ruleWithEvent(definitionWithId(definitions, 63), EffectEvent::DamageResolved);
    CHECK(perspectiveOf(lifesteal) == DamagePerspective::Dealt);
    CHECK(std::ranges::any_of(lifesteal.conditions, [](const EffectCondition& condition)
    {
        return std::holds_alternative<DamageOriginIsAttackCondition>(condition);
    }));
    CHECK(descriptionText(std::span<const EffectRule>{&(lifesteal), 1}, EffectDescriptionStyle::Compact, {}).find("持有者造成傷害") != std::string::npos);

    EffectRule rule;
    CHECK_FALSE(parseEffectRule(YAML::Load(R"(
時機: 絕招施放
條件:
  - 傷害方位: 造成
動作:
  - 資源變更:
      資源: 內力
      方式: 回復
      數值: 1
)"), rule, EffectRuleId{ 100 }, "錯誤傷害方位事件"));
    CHECK_FALSE(parseEffectRule(YAML::Load(R"(
時機: 造成傷害後
條件:
  - 傷害方位: 不明
動作:
  - 資源變更:
      資源: 內力
      方式: 回復
      數值: 1
)"), rule, EffectRuleId{ 101 }, "錯誤傷害方位值"));
}

TEST_CASE("ChessBattleEffects_AnranUsesAttackTimesMissingHpRatio", "[battle][effects][magic][schema][formula]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto& rule = ruleWithEvent(
        definitionWithId(definitions, 25),
        EffectEvent::MainProjectileBeforeDamage);
    REQUIRE(rule.actions.size() == 1);
    const auto& damage = std::get<DealDamageAction>(rule.actions[0].value);
    CHECK(damage.amount.base == EffectNumberBase::SourceAttack);
    REQUIRE(damage.amount.multiplierBase);
    CHECK(*damage.amount.multiplierBase == EffectNumberBase::SourceMissingHpRatio);
    CHECK(damage.amount.percent == 45);
    CHECK(damage.amount.rounding == EffectRounding::TowardZero);
    CHECK(damage.kind == BattleDamageKind::Pure);

    for (const auto style : { EffectDescriptionStyle::Full, EffectDescriptionStyle::Compact })
    {
        const auto description = descriptionText(std::span<const EffectRule>{&(rule), 1}, style, {});
        CHECK(description.find("目前攻擊×已損生命比例×45%") != std::string::npos);
    }
}

TEST_CASE("ChessBattleEffects_JiuyangQiDamageMatchesEveryOwnerCast",
          "[battle][effects][magic][schema][cast_match]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto& rule = ruleWithEvent(
        definitionWithId(definitions, 106),
        EffectEvent::HitBeforeDamage);
    CHECK(rule.castMatch == EffectCastMatch::OwnerAnyCast);
    CHECK(descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Full, {}).find("任意施放")
          != std::string::npos);
}

TEST_CASE("ChessBattleEffects_XiaoyaoDeclaresActionPreservingStaggerRelease", "[battle][effects][magic][schema][control]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto& rule = ruleWithEvent(
        definitionWithId(definitions, 2),
        EffectEvent::AttackCommitted);
    REQUIRE(rule.actions.size() == 3);
    const auto& removal = std::get<RemoveStatusAction>(rule.actions[0].value);
    CHECK(removal.controlOnly);
    CHECK(removal.clearCurrentActionStagger);
    CHECK(removal.statuses.empty());

    for (const auto style : { EffectDescriptionStyle::Full, EffectDescriptionStyle::Compact })
    {
        const auto description = descriptionText(std::span<const EffectRule>{&(rule), 1}, style, {});
        CHECK(description.find("清除全部控制狀態") != std::string::npos);
        CHECK(description.find("解除目前動作僵直") != std::string::npos);
        CHECK(description.find("保留位置與動作") != std::string::npos);
    }
}

TEST_CASE("ChessBattleEffects_AttackRuntimeBehaviorsAreTypedValidatedAndDescribed",
    "[battle][effects][schema][attack_behavior]")
{
    EffectRule rule;
    REQUIRE(parseEffectRule(YAML::Load(R"(
時機: 攻擊提交
動作:
  - 修改攻擊:
      執行行為:
        類型: 彈道彈射
        追加命中次數: 3
        機率: 40
        範圍像素: 300
  - 修改攻擊:
      執行行為:
        類型: 範圍追蹤
        範圍像素: 220
        傷害倍率: 45
  - 修改攻擊:
      執行行為:
        類型: 延遲替代攻擊
        延遲幀數: 7
        傷害倍率: 60
        攻擊者獲得格擋機率: 80
)"), rule, EffectRuleId{ 200 }, "攻擊執行行為"));

    REQUIRE(rule.actions.size() == 3);
    const auto& bounce = std::get<ProjectileBounceAttackBehavior>(
        std::get<ModifyAttackAction>(rule.actions[0].value).runtimeBehavior);
    CHECK(bounce.additionalHits == 3);
    CHECK(bounce.chancePct == 40);
    CHECK(bounce.rangePixels == 300);

    const auto& tracking = std::get<NearbyTrackingAttackBehavior>(
        std::get<ModifyAttackAction>(rule.actions[1].value).runtimeBehavior);
    CHECK(tracking.rangePixels == 220);
    CHECK(tracking.damagePct == 45);

    const auto& delayed = std::get<DelayedAlternateAttackBehavior>(
        std::get<ModifyAttackAction>(rule.actions[2].value).runtimeBehavior);
    CHECK(delayed.delayFrames == 7);
    CHECK(delayed.damagePct == 60);
    CHECK(delayed.attackerBlockGainChancePct == 80);

    EffectRule spiralRule;
    REQUIRE(parseEffectRule(YAML::Load(R"(
時機: 攻擊提交
動作:
  - 修改攻擊:
      執行行為:
        類型: 擴張螺旋
        彈道數量: 3
        流血層數: 1
)"), spiralRule, EffectRuleId{ 202 }, "螺旋攻擊執行行為"));
    REQUIRE(spiralRule.actions.size() == 1);
    const auto& spiral = std::get<ExpandingSpiralAttackBehavior>(
        std::get<ModifyAttackAction>(spiralRule.actions[0].value).runtimeBehavior);
    CHECK(spiral.projectileCount == 3);
    CHECK(spiral.bleedStacks == 1);

    const auto fullDescription = descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Full, {});
    const auto fullSpiralDescription = descriptionText(std::span<const EffectRule>{&(spiralRule), 1},
        EffectDescriptionStyle::Full,
        {});
    CHECK(fullDescription.find("命中時有40%機率在300像素內彈射") != std::string::npos);
    CHECK(fullDescription.find("最多追加命中3次") != std::string::npos);
    CHECK(fullDescription.find("40%") != std::string::npos);
    CHECK(fullDescription.find("300像素") != std::string::npos);
    CHECK(fullDescription.find("命中後在220像素內產生45%傷害追蹤彈") != std::string::npos);
    CHECK(fullDescription.find("7幀後以60%傷害追擊最近的其他敵人") != std::string::npos);
    CHECK(fullDescription.find("附近沒有其他敵人時") != std::string::npos);
    CHECK(fullDescription.find("原本的攻擊目標") != std::string::npos);
    CHECK(fullDescription.find("60%傷害") != std::string::npos);
    CHECK(fullDescription.find("80%機率獲得1次傷害抵擋") != std::string::npos);
    CHECK(fullSpiralDescription.find("擴張螺旋彈×3") != std::string::npos);
    CHECK(fullSpiralDescription.find("流血1層") != std::string::npos);

    const auto compactDescription = descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Compact, {});
    const auto compactSpiralDescription = descriptionText(std::span<const EffectRule>{&(spiralRule), 1},
        EffectDescriptionStyle::Compact,
        {});
    CHECK(compactDescription.find("命中時有40%機率在300像素內彈射") != std::string::npos);
    CHECK(compactDescription.find("最多追加命中3次") != std::string::npos);
    CHECK(compactDescription.find("40%") != std::string::npos);
    CHECK(compactDescription.find("300像素") != std::string::npos);
    CHECK(compactDescription.find("命中後在220像素內產生45%傷害追蹤彈") != std::string::npos);
    CHECK(compactDescription.find("7幀後以60%傷害追擊最近的其他敵人") != std::string::npos);
    CHECK(compactDescription.find("附近無其他敵人則追擊原攻擊目標") != std::string::npos);
    CHECK(compactDescription.find("60%傷害") != std::string::npos);
    CHECK(compactDescription.find("80%機率獲得1次傷害抵擋（最多1次）") != std::string::npos);
    CHECK(compactDescription.find("原彈道") == std::string::npos);
    CHECK(compactDescription.find("替代目標") == std::string::npos);
    CHECK(compactSpiralDescription.find("擴張螺旋彈×3") != std::string::npos);
    CHECK(compactSpiralDescription.find("流血1層") != std::string::npos);

    const auto detailedDescription = descriptionText(
        std::span<const EffectRule>{&(rule), 1},
        EffectDescriptionStyle::Detailed,
        {});
    CHECK(detailedDescription.find(
        "稽核 actions[2].pattern.kind = 0 [schema 預設]") != std::string::npos);
    CHECK(detailedDescription.find(
        "稽核 actions[2].strengthPct = 100 [schema 預設]") != std::string::npos);

    auto delayedOnly = rule;
    delayedOnly.actions = {rule.actions[2]};
    const std::array delayedRules{delayedOnly};
    const auto delayedRows = effectDescriptionTextRows(renderEffectDescription(
        buildEffectDescriptionDocument({
            EffectDescriptionContainerKind::ComboThreshold,
            delayedRules,
        }),
        EffectDescriptionStyle::Compact,
        {}));
    CHECK(delayedRows == std::vector<std::string>{
        "出手：7幀後以60%傷害追擊最近的其他敵人；附近無其他敵人則追擊原攻擊目標",
        "  追擊出手時：80%機率獲得1次傷害抵擋（最多1次）",
    });

    const auto parsesBehavior = [](std::string_view fields)
    {
        std::string yaml = R"(時機: 攻擊提交
動作:
  - 修改攻擊:
      執行行為:
)";
        yaml += fields;
        EffectRule parsed;
        return parseEffectRule(
            YAML::Load(yaml), parsed, EffectRuleId{ 201 }, "錯誤攻擊執行行為");
    };

    CHECK_FALSE(parsesBehavior(R"(      類型: 彈道彈射
      追加命中次數: 0
      機率: 40
      範圍像素: 300
)"));
    CHECK_FALSE(parsesBehavior(R"(      類型: 彈道彈射
      追加命中次數: 1
      機率: 101
      範圍像素: 300
)"));
    CHECK_FALSE(parsesBehavior(R"(      類型: 範圍追蹤
      範圍像素: 0
      傷害倍率: 45
)"));
    CHECK_FALSE(parsesBehavior(R"(      類型: 延遲替代攻擊
      延遲幀數: 7
      傷害倍率: 0
      攻擊者獲得格擋機率: 80
)"));
    CHECK_FALSE(parsesBehavior(R"(      類型: 延遲替代攻擊
      延遲幀數: 7
      傷害倍率: 60
      攻擊者獲得格擋機率: -1
)"));
    CHECK_FALSE(parsesBehavior(R"(      類型: 擴張螺旋
      彈道數量: 3
      流血層數: 0
)"));
    CHECK_FALSE(parsesBehavior(R"(      類型: 擴張螺旋
      彈道數量: 3
      流血層數: 1
      未知欄位: 9
)"));
}

TEST_CASE("ChessBattleEffects_ForceMoveSupportsOneDistanceUnitAndLockFrames",
    "[battle][effects][schema][force_move]")
{
    EffectRule rule;
    REQUIRE(parseEffectRule(YAML::Load(R"(
時機: 命中
目標: 命中目標
動作:
  - 強制移動:
      方向: 遠離來源
      距離像素: 130
      鎖定幀數: 5
      碰撞: 阻擋前停止
      受阻結果: 縮短
)"), rule, EffectRuleId{ 210 }, "強制移動距離"));

    REQUIRE(rule.actions.size() == 1);
    const auto& pixels = std::get<ForceMoveAction>(rule.actions[0].value);
    CHECK(pixels.distanceTiles == 0);
    CHECK(pixels.distancePixels == 130);
    CHECK(pixels.lockFrames == 5);

    EffectRule tileRule;
    REQUIRE(parseEffectRule(YAML::Load(R"(
時機: 命中
目標: 命中目標
動作:
  - 強制移動:
      方向: 接近來源
      距離格數: 4
      碰撞: 佔位前停止
      受阻結果: 停止
)"), tileRule, EffectRuleId{ 212 }, "棋格強制移動距離"));
    REQUIRE(tileRule.actions.size() == 1);
    const auto& tiles = std::get<ForceMoveAction>(tileRule.actions[0].value);
    CHECK(tiles.distanceTiles == 4);
    CHECK(tiles.distancePixels == 0);
    CHECK(tiles.lockFrames == 1);

    const auto fullDescription = descriptionText(
        std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Full, {});
    const auto fullTileDescription = descriptionText(
        std::span<const EffectRule>{&(tileRule), 1}, EffectDescriptionStyle::Full, {});
    CHECK(fullDescription.find("擊退130像素並鎖定5幀") != std::string::npos);
    CHECK(fullTileDescription.find("拉近4格並鎖定1幀") != std::string::npos);

    const auto compactDescription = descriptionText(
        std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Compact, {});
    const auto compactTileDescription = descriptionText(
        std::span<const EffectRule>{&(tileRule), 1}, EffectDescriptionStyle::Compact, {});
    CHECK(compactDescription.find("擊退130像素，鎖5幀") != std::string::npos);
    CHECK(compactTileDescription.find("拉近4格，鎖1幀") != std::string::npos);

    const auto parsesMove = [](std::string_view distanceFields)
    {
        std::string yaml = R"(時機: 命中
目標: 命中目標
動作:
  - 強制移動:
      方向: 遠離來源
)";
        yaml += distanceFields;
        yaml += R"(    碰撞: 阻擋前停止
    受阻結果: 縮短
)";
        EffectRule parsed;
        return parseEffectRule(
            YAML::Load(yaml), parsed, EffectRuleId{ 211 }, "錯誤強制移動距離");
    };

    CHECK_FALSE(parsesMove(""));
    CHECK_FALSE(parsesMove("    距離格數: 2\n    距離像素: 100\n"));
    CHECK_FALSE(parsesMove("    距離像素: -1\n"));
    CHECK_FALSE(parsesMove("    距離像素: 100\n    鎖定幀數: 0\n"));
    CHECK_FALSE(parsesMove("    距離像素: 100\n    未知距離: 1\n"));

    EffectRule unsupportedPointDirection;
    CHECK_FALSE(parseEffectRule(YAML::Load(R"(
時機: 命中
目標: 命中目標
動作:
  - 強制移動:
      方向: 接近指定點
      距離像素: 100
      碰撞: 阻擋前停止
      受阻結果: 縮短
)"), unsupportedPointDirection, EffectRuleId{ 213 }, "未實作的指定點強制移動"));
}

TEST_CASE("ChessBattleEffects_ModifyCastParsesGenericFieldsAndRestrictsSpecialEvents",
    "[battle][effects][schema][modify_cast]")
{
    EffectRule rule;
    REQUIRE(parseEffectRule(YAML::Load(R"(
時機: 施放規劃
動作:
  - 修改施放:
      射程模式: 遠程
      彈道速度百分比: 125
      最小選擇距離: 6
      追加彈道數: 2
      機動政策: 滑步攻擊
  - 修改施放:
      機動政策: 閃擊
)"), rule, EffectRuleId{ 220 }, "修改施放通用欄位"));

    REQUIRE(rule.actions.size() == 2);
    const auto& cast = std::get<ModifyCastAction>(rule.actions[0].value);
    CHECK(cast.rangeMode == CastRangeMode::Ranged);
    CHECK(cast.projectileSpeedPct == 125);
    CHECK(cast.minimumSelectDistance == 6);
    CHECK(cast.additionalProjectiles == 2);
    CHECK(cast.mobility == CastMobilityPolicy::DashAttack);
    CHECK_FALSE(cast.autoUltimate);
    CHECK(std::get<ModifyCastAction>(rule.actions[1].value).mobility
        == CastMobilityPolicy::BlinkAttack);

    EffectRule autoUltimateRule;
    REQUIRE(parseEffectRule(YAML::Load(R"(
時機: 施放規劃
動作:
  - 修改施放:
      自動絕招:
        消耗內力: true
        顯示公告: false
)"), autoUltimateRule, EffectRuleId{ 222 }, "修改施放自動絕招"));
    REQUIRE(autoUltimateRule.actions.size() == 1);
    const auto& autoUltimate = std::get<ModifyCastAction>(
        autoUltimateRule.actions[0].value).autoUltimate;
    REQUIRE(autoUltimate);
    CHECK(autoUltimate->consumeMp);
    CHECK_FALSE(autoUltimate->announce);

    for (const auto style : { EffectDescriptionStyle::Full, EffectDescriptionStyle::Compact })
    {
        const auto description = descriptionText(std::span<const EffectRule>{&(rule), 1}, style, {});
        const auto autoUltimateDescription = descriptionText(std::span<const EffectRule>{&(autoUltimateRule), 1}, style, {});
        CHECK(description.find("武功遠程化") != std::string::npos);
        CHECK(description.find("彈道速度125%") != std::string::npos);
        CHECK(description.find("最小選擇距離6") != std::string::npos);
        CHECK(description.find("追加彈道2枚") != std::string::npos);
        CHECK(description.find("啟用滑步攻擊") != std::string::npos);
        CHECK(description.find("啟用閃擊") != std::string::npos);
        CHECK(autoUltimateDescription.find("消耗內力") != std::string::npos);
        CHECK(autoUltimateDescription.find("不顯示公告") != std::string::npos);
    }

    const auto parsesCast = [](std::string_view fields, std::string_view timing = "施放規劃")
    {
        std::string yaml = "時機: ";
        yaml += timing;
        yaml += R"(
修改施放:
)";
        yaml += fields;
        EffectRule parsed;
        return parseEffectRule(
            YAML::Load(yaml), parsed, EffectRuleId{ 221 }, "錯誤修改施放");
    };

    CHECK_FALSE(parsesCast("  彈道速度百分比: -1\n"));
    CHECK_FALSE(parsesCast("  最小選擇距離: -1\n"));
    CHECK_FALSE(parsesCast("  追加彈道數: -1\n"));
    CHECK_FALSE(parsesCast("  機動政策: 未知機動\n"));
    CHECK_FALSE(parsesCast("  自動絕招: true\n"));
    CHECK_FALSE(parsesCast(R"(  自動絕招:
    消耗內力: false
    顯示公告: true
    未知欄位: false
)"));
    CHECK_FALSE(parsesCast(R"(  追加彈道數: 1
  自動絕招: {}
)"));
    CHECK_FALSE(parsesCast(
        "  免費追加施放: true\n  追加彈道數: 1\n",
        "施放延續"));
    CHECK(parsesCast("  免費追加施放: true\n", "施放延續"));
    CHECK(parsesCast(R"(  自動絕招:
    消耗內力: false
    顯示公告: true
)", "護盾破裂"));
    CHECK_FALSE(parsesCast(R"(  自動絕招:
    消耗內力: false
    顯示公告: true
)", "單位死亡"));
}

TEST_CASE("ChessBattleEffects_PeriodicRuleIntervalIsTypedDescribedAndFrameOnly",
    "[battle][effects][schema][periodic]")
{
    EffectRule rule;
    REQUIRE(parseEffectRule(YAML::Load(R"(
時機: 每隔
間隔幀數: 30
動作:
  - 修改施放:
      自動絕招:
        消耗內力: false
        顯示公告: true
)"), rule, EffectRuleId{ 230 }, "週期自動絕招"));
    CHECK(rule.intervalFrames == 30);
    const auto& request = std::get<ModifyCastAction>(rule.actions[0].value).autoUltimate;
    REQUIRE(request);
    CHECK_FALSE(request->consumeMp);
    CHECK(request->announce);
    CHECK(descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Compact, {}).find("每30幀")
        != std::string::npos);
    CHECK(descriptionText(std::span<const EffectRule>{&(rule), 1}, EffectDescriptionStyle::Full, {}).find("每30幀")
        != std::string::npos);
    for (const auto style : { EffectDescriptionStyle::Full, EffectDescriptionStyle::Compact })
    {
        const auto description = descriptionText(std::span<const EffectRule>{&(rule), 1}, style, {});
        CHECK(description.find("不消耗內力") != std::string::npos);
        CHECK(description.find("顯示公告") != std::string::npos);
    }

    EffectRule invalid;
    CHECK_FALSE(parseEffectRule(YAML::Load(R"(
時機: 每隔
間隔幀數: -1
動作:
  - 修改施放:
      自動絕招: {}
)"), invalid, EffectRuleId{ 231 }, "負週期間隔"));
    CHECK_FALSE(parseEffectRule(YAML::Load(R"(
時機: 每隔
間隔幀數: 0
獲得護盾: 1
)"), invalid, EffectRuleId{ 232 }, "零週期間隔"));
    CHECK_FALSE(parseEffectRule(YAML::Load(R"(
時機: 每隔
獲得護盾: 1
)"), invalid, EffectRuleId{ 233 }, "缺少週期間隔"));
    CHECK_FALSE(parseEffectRule(YAML::Load(R"(
時機: 每幀
間隔幀數: 1
獲得護盾: 1
)"), invalid, EffectRuleId{ 234 }, "每幀不可帶間隔"));
    CHECK_FALSE(parseEffectRule(YAML::Load(R"(
時機: 護盾破裂
間隔幀數: 30
動作:
  - 修改施放:
      自動絕招: {}
)"), invalid, EffectRuleId{ 235 }, "錯誤週期事件"));
}

TEST_CASE("ChessBattleEffects_BattleInitializedSchemaMatchesInitializationRuntimeExactly",
    "[battle][effects][schema][initialization]")
{
    const std::vector<std::string_view> validRules{
        R"(
時機: 開場
目標: 自身
動作:
  - 屬性修正:
      屬性: 攻擊
      方式: 百分比加算
      數值: 20
)",
        R"(
時機: 開場
目標: 自身
動作:
  - 傷害修正:
      方位: 承受
      階段: 最終
      傷害種類: 全部
      方式: 單次承傷上限
      數值: 10
)",
        R"(
時機: 開場
目標: 自身
條件:
  - 自身為最後存活
動作:
  - 生成分身:
      數量: 1
)",
        R"(
時機: 開場
目標: 自身
動作:
  - 條件分支:
      條件:
        - 目標非無敵
      成立:
        - 資源變更:
            資源: 護盾
            方式: 獲得
            數值:
              基準: 目標最大生命
              百分比: 25
)",
    };
    for (std::size_t index = 0; index < validRules.size(); ++index)
    {
        EffectRule rule;
        CAPTURE(index);
        CHECK(parseEffectRule(
            YAML::Load(std::string(validRules[index])),
            rule,
            EffectRuleId{ 300 + index },
            "有效初始化規則"));
    }

    const std::vector<std::string_view> invalidRules{
        R"(
時機: 開場
目標: 自身
機率: 50
動作:
  - 屬性修正:
      屬性: 攻擊
      方式: 固定加算
      數值: 10
)",
        R"(
時機: 開場
目標: 自身
每N次事件: 2
動作:
  - 屬性修正:
      屬性: 攻擊
      方式: 固定加算
      數值: 10
)",
        R"(
時機: 開場
目標:
  類型: 友軍
  平手: 戰鬥亂數
動作:
  - 屬性修正:
      屬性: 攻擊
      方式: 固定加算
      數值: 10
)",
        R"(
時機: 開場
目標:
  類型: 友軍
  武功: 21
動作:
  - 屬性修正:
      屬性: 攻擊
      方式: 固定加算
      數值: 10
)",
        R"(
時機: 開場
目標: 自身
條件:
  - 自身有狀態: 中毒
動作:
  - 屬性修正:
      屬性: 攻擊
      方式: 固定加算
      數值: 10
)",
        R"(
時機: 開場
目標: 自身
動作:
  - 屬性修正:
      屬性: 攻擊
      方式: 取代
      數值: 10
)",
        R"(
時機: 開場
目標: 自身
動作:
  - 屬性修正:
      屬性: 最大生命
      方式: 固定加算
      數值: 10
      持續幀數: 30
)",
        R"(
時機: 開場
目標: 自身
動作:
  - 資源變更:
      資源: 生命
      方式: 回復
      數值: 10
)",
        R"(
時機: 開場
目標: 自身
動作:
  - 資源變更:
      資源: 目前冷卻
      方式: 獲得
      數值: 10
)",
        R"(
時機: 開場
目標: 自身
動作:
  - 資源變更:
      資源: 內力
      方式: 奪取
      數值: 10
)",
        R"(
時機: 開場
目標: 全隊
動作:
  - 生成分身:
      數量: 1
)",
        R"(
時機: 開場
目標: 自身
動作:
  - 變更狀態值:
      狀態槽: 永久施放進展
      增量: 1
)",
        R"(
時機: 開場
目標: 自身
動作:
  - 造成傷害:
      傷害種類: 純粹
      數值: 10
)",
        R"(
時機: 開場
目標: 自身
動作:
  - 屬性修正:
      屬性: 攻擊
      方式: 固定加算
      數值:
        基準: 累計狀態值
        百分比: 100
)",
    };
    for (std::size_t index = 0; index < invalidRules.size(); ++index)
    {
        EffectRule rule;
        CAPTURE(index);
        CHECK_FALSE(parseEffectRule(
            YAML::Load(std::string(invalidRules[index])),
            rule,
            EffectRuleId{ 400 + index },
            "無效初始化規則"));
    }
}

TEST_CASE("ChessBattleEffects_DamageModifierSchemaRejectsRuntimeNoOps",
    "[battle][effects][schema][damage_modifier]")
{
    const std::vector<std::string_view> invalidActions{
        R"(
    方位: 承受
    階段: 防禦前
    傷害種類: 全部
    方式: 乘算
    數值: 80
)",
        R"(
    方位: 承受
    階段: 防禦前
    傷害種類: 全部
    方式: 忽略防禦百分比
    數值: 20
)",
        R"(
    方位: 造成
    階段: 最終
    傷害種類: 全部
    方式: 單次承傷上限
    數值: 20
)",
        R"(
    方位: 承受
    階段: 最終
    傷害種類: 全部
    方式: 低於最大生命百分比時處決
    數值: 20
)",
    };
    for (std::size_t index = 0; index < invalidActions.size(); ++index)
    {
        const auto yaml = std::format(
            "時機: 開場\n目標: 自身\n傷害修正:\n{}",
            invalidActions[index]);
        EffectRule rule;
        CAPTURE(index);
        CHECK_FALSE(parseEffectRule(
            YAML::Load(yaml),
            rule,
            EffectRuleId{ 500 + index },
            "無效傷害修正"));
    }
}

TEST_CASE("ChessBattleEffects_RemovedSchemaVocabularyIsRejected",
    "[battle][effects][schema][removed]")
{
    const std::vector<std::string_view> rules{
        R"(
時機: 每幀
目標: 自身
動作:
  - 資源變更:
      資源: 護盾
      方式: 獲得
      數值: 10
      溢出: 捨棄
)",
        R"(
時機: 命中
目標: 命中目標
動作:
  - 傷害修正:
      方位: 承受
      階段: 最終
      傷害種類: 全部
      方式: 抵擋下一次傷害
      數值: 1
)",
        R"(
時機: 每幀
目標: 自身
動作:
  - 資源變更:
      資源: 護盾
      方式: 獲得
      數值:
        基準: 記錄最大值
        百分比: 100
)",
        R"(
時機: 絕招施放
目標: 所有敵人
動作:
  - 狀態機:
      機制: 結算剩餘狀態傷害
      狀態: 流血
)",
    };
    for (std::size_t index = 0; index < rules.size(); ++index)
    {
        EffectRule rule;
        CAPTURE(index);
        CHECK_FALSE(parseEffectRule(
            YAML::Load(std::string(rules[index])),
            rule,
            EffectRuleId{ 600 + index },
            "已移除語彙"));
    }
}

TEST_CASE("ChessBattleEffects_MagicSchemaRejectsBattleInitializationRules",
    "[battle][effects][magic][schema][initialization]")
{
    const auto root = YAML::Load(R"(
絕招:
  - 武功: 999
    名稱: 錯誤初始化武功
    效果:
      - 時機: 開場
        目標: 自身
        動作:
          - 屬性修正:
              屬性: 攻擊
              方式: 固定加算
              數值: 10
)");
    std::vector<ChessMagicEffectDefinition> definitions;
    CHECK_FALSE(parseMagicEffects(
        root,
        definitions,
        "錯誤初始化武功"));
}

TEST_CASE("ChessBattleEffects_EnabledInitializationInventoryRemainsFullyValidated",
    "[battle][effects][schema][initialization][content]")
{
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content != nullptr);

    std::size_t initializedRuleCount{};
    std::size_t initializedActionCount{};
    const auto countRules = [&](const std::vector<EffectRule>& rules)
    {
        for (const auto& rule : rules)
        {
            if (rule.event != EffectEvent::BattleInitialized) continue;
            std::string error;
            CHECK(validateEffectRule(rule, error));
            CAPTURE(error);
            ++initializedRuleCount;
            initializedActionCount += rule.actions.size();
        }
    };
    for (const auto& combo : content->combos())
        for (const auto& threshold : combo.thresholds) countRules(threshold.rules);
    for (const auto& equipment : content->equipment()) countRules(equipment.rules);
    for (const auto& synergy : content->equipmentSynergies()) countRules(synergy.rules);
    for (const auto& neigong : content->neigong()) countRules(neigong.rules);

    CHECK(initializedRuleCount == 205);
    CHECK(initializedActionCount == 209);
}

TEST_CASE("ChessBattleEffects_CurrentHpBlastPreservesTypedDamagePolicy",
          "[battle][effects][combo][description]")
{
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content != nullptr);
    const auto combo = std::ranges::find(
        content->combos(),
        std::string{ "琴棋書畫" },
        &ComboDef::name);
    REQUIRE(combo != content->combos().end());

    for (const int count : { 2, 4 })
    {
        const auto threshold = std::ranges::find(
            combo->thresholds,
            count,
            &ComboThreshold::count);
        REQUIRE(threshold != combo->thresholds.end());
        const auto rule = std::ranges::find_if(threshold->rules, [](const EffectRule& candidate)
        {
            return candidate.event == EffectEvent::AttackCommitted
                && candidate.actions.size() == 1
                && std::holds_alternative<DealDamageAction>(
                    candidate.actions.front().value);
        });
        REQUIRE(rule != threshold->rules.end());
        const auto& damage = std::get<DealDamageAction>(rule->actions.front().value);
        CHECK(damage.amount.base == EffectNumberBase::TargetCurrentHp);
        CHECK_FALSE(damage.appliesDamageModifiers);
        CHECK_FALSE(damage.triggersHurtInvincibility);
        for (const auto style : { EffectDescriptionStyle::Full, EffectDescriptionStyle::Compact })
        {
            const auto description = descriptionText(std::span<const EffectRule>{&(*rule), 1}, style, {});
            CHECK(description.find("不套用傷害修正") != std::string::npos);
            CHECK(description.find("不觸發受傷無敵") != std::string::npos);
        }
    }
}

TEST_CASE("ChessBattleEffects_DeathBlastPreservesTypedDamagePolicy",
          "[battle][effects][combo][description]")
{
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content != nullptr);
    const auto combo = std::ranges::find(
        content->combos(),
        std::string{ "江湖龍套" },
        &ComboDef::name);
    REQUIRE(combo != content->combos().end());

    for (const int count : { 2, 4, 6 })
    {
        const auto threshold = std::ranges::find(
            combo->thresholds,
            count,
            &ComboThreshold::count);
        REQUIRE(threshold != combo->thresholds.end());
        const auto rule = std::ranges::find_if(threshold->rules, [](const EffectRule& candidate)
        {
            return candidate.event == EffectEvent::UnitDied
                && candidate.actions.size() == 1
                && std::holds_alternative<DealDamageAction>(
                    candidate.actions.front().value);
        });
        REQUIRE(rule != threshold->rules.end());
        const auto* damage = std::get_if<DealDamageAction>(
            &rule->actions.front().value);
        REQUIRE(damage != nullptr);
        REQUIRE(damage->areaProjectiles);
        CHECK_FALSE(damage->appliesDamageModifiers);
        CHECK_FALSE(damage->triggersHurtInvincibility);
    }
}

TEST_CASE("ChessBattleEffects_UltimateDefinitionsCoverStandardHardAndEasyPools", "[battle][effects][magic][schema][content]")
{
    const auto normal = Test::actualContent(Difficulty::Normal);
    const auto hard = Test::actualContent(Difficulty::Hard);
    const auto easy = Test::actualContent(Difficulty::Easy);
    REQUIRE(normal != nullptr);
    REQUIRE(hard != nullptr);
    REQUIRE(easy != nullptr);

    const auto normalUltimates = poolUltimateMagicIds(*normal);
    const auto hardUltimates = poolUltimateMagicIds(*hard);
    const auto easyUltimates = poolUltimateMagicIds(*easy);
    CHECK(normalUltimates.size() == 59);
    CHECK(hardUltimates == normalUltimates);
    CHECK(easyUltimates.size() == 30);
    CHECK(std::ranges::includes(normalUltimates, easyUltimates));

    std::set<int> configuredIds;
    for (const auto& definition : normal->magicEffects())
    {
        configuredIds.insert(definition.magicId);
    }
    CHECK(configuredIds == normalUltimates);
}

TEST_CASE("ChessMagicEffectDisplay_NormalPoolFitsTheNarrowSingleColumnViewport",
          "[chess][effects][magic][layout][content]")
{
    // 244×133 是一般商店面板扣除棋池最寬頭像後的內容區；
    // 196×133 則涵蓋二星升三星欄位使同版型面板進一步變窄的情況。
    constexpr std::array viewports{
        std::pair{244, 133},
        std::pair{196, 133},
    };
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content);
    for (const auto [viewportWidth, viewportHeight] : viewports)
    {
        int minimumEffectFontSize = 100;
        for (const int roleId : content->poolRoleIds())
        {
            const auto* role = content->role(roleId);
            REQUIRE(role);
            for (int star = 1; star <= 3; ++star)
            {
                CAPTURE(viewportWidth, viewportHeight, roleId, role->Name, star);
                const auto selected = chessRoleMagicsForStar(*content, *role, star);
                REQUIRE(selected.size() <= 2);
                std::vector<const MagicSave*> magics;
                for (const auto& [magic, power] : selected)
                {
                    magics.push_back(magic);
                }
                const auto rows = buildChessMagicEffectDisplayRows(
                    magics,
                    content->magicEffects(),
                    selected.empty() ? -1 : selected.back().first->ID);
                const auto layout = layoutChessMagicEffectDisplay(
                    rows,
                    viewportWidth,
                    viewportHeight);
                minimumEffectFontSize = std::min(
                    minimumEffectFontSize,
                    layout.effectFontSize);
                CHECK(layout.effectFontSize >= (viewportWidth == 244 ? 12 : 10));
                CHECK(layout.scrollable == (layout.requiredHeight > viewportHeight));
                int previousBottom = 0;
                for (const auto& line : layout.lines)
                {
                    CHECK(line.x >= 0);
                    CHECK(line.x + line.width <= viewportWidth);
                    CHECK(line.y >= previousBottom);
                    previousBottom = line.y + line.height;
                    if (line.content.kind == ChessMagicEffectDisplayLineKind::Skill)
                    {
                        CHECK(line.x + line.width <= layout.skillValueX);
                        const int valueWidth = displayTextWidth("9999 遠程")
                            * line.fontSize / 2;
                        CHECK(layout.skillValueX + valueWidth <= viewportWidth);
                    }
                }
                CHECK(previousBottom == layout.requiredHeight);
                if (layout.scrollable)
                {
                    REQUIRE_FALSE(layout.scrollStops.empty());
                    CHECK(layout.scrollStops.front() == 0);
                    CHECK(layout.scrollStops.back() == layout.maximumScrollOffset);
                    CHECK(clampChessMagicEffectDisplayScrollOffset(layout, -1) == 0);
                    CHECK(clampChessMagicEffectDisplayScrollOffset(
                        layout,
                        layout.maximumScrollOffset + 1) == layout.maximumScrollOffset);
                    std::vector<bool> reached(layout.lines.size());
                    for (const int offset : layout.scrollStops)
                    {
                        const auto visible = visibleChessMagicEffectDisplayLines(
                            layout,
                            offset);
                        for (const auto& line : visible)
                        {
                            CHECK(line.x >= 0);
                            CHECK(line.x + line.width <= viewportWidth);
                            CHECK(line.y >= 0);
                            CHECK(line.y + line.height
                                <= viewportHeight - layout.scrollIndicatorHeight);
                            for (std::size_t index = 0;
                                 index < layout.lines.size();
                                 ++index)
                            {
                                const auto& source = layout.lines[index];
                                if (source.y == line.y + offset
                                    && source.content.text == line.content.text
                                    && source.content.kind == line.content.kind)
                                {
                                    reached[index] = true;
                                }
                            }
                        }
                    }
                    CHECK(std::ranges::all_of(reached, std::identity{}));
                    int offset{};
                    while (offset < layout.maximumScrollOffset)
                    {
                        const int next = stepChessMagicEffectDisplayScrollOffset(
                            layout,
                            offset,
                            1);
                        REQUIRE(next > offset);
                        offset = next;
                    }
                    CHECK(offset == layout.maximumScrollOffset);
                    while (offset > 0)
                    {
                        const int previous = stepChessMagicEffectDisplayScrollOffset(
                            layout,
                            offset,
                            -1);
                        REQUIRE(previous < offset);
                        offset = previous;
                    }
                    CHECK(offset == 0);
                }
                else
                {
                    CHECK(layout.maximumScrollOffset == 0);
                    const auto visible = visibleChessMagicEffectDisplayLines(layout, 0);
                    REQUIRE(visible.size() == layout.lines.size());
                    for (std::size_t index = 0; index < visible.size(); ++index)
                    {
                        CHECK(visible[index].content.text == layout.lines[index].content.text);
                        CHECK(visible[index].y == layout.lines[index].y);
                    }
                    CHECK(std::ranges::all_of(visible, [viewportHeight](const auto& line)
                    {
                        return line.y >= 0 && line.y + line.height <= viewportHeight;
                    }));
                }
            }
        }
        CHECK(minimumEffectFontSize >= (viewportWidth == 244 ? 12 : 10));
    }
}

TEST_CASE("EffectDescriptionDocument_RendersRepresentativeContainerLifecycles",
          "[battle][effects][description][document][golden]")
{
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content);
    const auto rows = [&](int magicId, EffectDescriptionStyle style)
    {
        const auto& definition = definitionWithId(content->magicEffects(), magicId);
        const auto document = buildEffectDescriptionDocument({
            EffectDescriptionContainerKind::Magic,
            definition.rules,
        });
        return effectDescriptionTextRows(renderEffectDescription(
            document,
            style,
            {}));
    };
    const auto compactRows = [&](int magicId)
    {
        return rows(magicId, EffectDescriptionStyle::Compact);
    };

    CHECK(compactRows(39) == std::vector<std::string>{
        "主彈命中：施加七星7層（150幀）",
        "友軍命中七星目標：破防50%，消耗1層",
        "  最後一層消耗時眩暈30幀",
    });
    const auto sevenStarFull = rows(39, EffectDescriptionStyle::Full);
    CHECK(std::ranges::any_of(sevenStarFull, [](const std::string& row)
    {
        return row.contains("任一友軍命中由效果持有者施加七星的敵人時");
    }));
    CHECK(std::ranges::none_of(sevenStarFull, [](const std::string& row)
    {
        return row.contains("由施法者施加七星");
    }));
    CHECK(compactRows(43) == std::vector<std::string>{
        "準備施放大招：依星級隨機借用1～2名敵人的大招效果",
    });
    CHECK(compactRows(62) == std::vector<std::string>{
        "同武功友軍存活：由該友軍對絕招目標追加100%主彈",
        "  否則：由施法者追加50%副彈（不觸發大招效果）",
    });
    CHECK(compactRows(98) == std::vector<std::string>{
        "施放大招：隨機複製另一名存活單位的絕招攻擊，不複製大招效果",
    });
    CHECK(compactRows(95) == std::vector<std::string>{
        "獲得毒爆1層（強度為星級×60，最多5層）",
        "施法者死亡：逐層引爆",
        "  每層依毒爆強度傷害5格內所有敵軍並施加中毒4層（強度為10，120幀）",
    });
    CHECK(rows(95, EffectDescriptionStyle::Full) == std::vector<std::string>{
        "獲得毒爆1層，強度為星級×60，最多5層",
        "施法者死亡時，逐層引爆毒爆",
        "  每層對5格內所有敵軍造成毒爆強度的100%純粹傷害",
        "  並施加中毒4層（強度為10，持續120幀）",
    });
    CHECK(compactRows(126) == std::vector<std::string>{
        "對生命比例最低的5名友軍施加單次承傷上限，強度為目標最大生命的15%",
        "  取代既有狀態，最多1層",
    });

    const auto& poison = definitionWithId(content->magicEffects(), 95);
    const auto poisonDocument = buildEffectDescriptionDocument({
        EffectDescriptionContainerKind::Magic,
        poison.rules,
    });
    const auto stackBlock = std::ranges::find_if(
        poisonDocument.sections,
        [](const EffectDescriptionSection& section)
        {
            return std::ranges::any_of(section.blocks,
                [](const EffectDescriptionBlock& block)
                {
                    return block.archetype == DescriptionArchetype::StackExplosion;
                });
        });
    REQUIRE(stackBlock != poisonDocument.sections.end());
    const auto producer = std::ranges::find_if(
        stackBlock->blocks,
        [](const EffectDescriptionBlock& block)
        {
            return block.archetype == DescriptionArchetype::StackExplosion
                && block.sourceRuleOrder == 0;
        });
    REQUIRE(producer != stackBlock->blocks.end());
    constexpr DescriptionPlayerProjection bothPlayerStyles{true, true};
    bool foundProjectedPotency{};
    bool foundUnprojectedDefault{};
    for (const auto& entry : producer->coverage.fields)
    {
        if (entry.playerFact != DescriptionPlayerFact::StatusPotency
            && entry.playerFact != DescriptionPlayerFact::StatusSecondaryPotency)
            continue;
        if (entry.disposition == DescriptionFieldDisposition::Visible)
        {
            foundProjectedPotency = true;
            CHECK(entry.projection == bothPlayerStyles);
        }
        if (entry.disposition == DescriptionFieldDisposition::SchemaDefault)
        {
            foundUnprojectedDefault = true;
            CHECK(entry.projection == DescriptionPlayerProjection{});
        }
    }
    CHECK(foundProjectedPotency);
    CHECK(foundUnprojectedDefault);
}

TEST_CASE("EffectDescriptionDocument_GenericFallbackSignatureCoversNestedTypedPayloads",
          "[battle][effects][description][document][coverage][signature]")
{
    const auto makeRule = [](EffectActionValue value)
    {
        EffectRule rule;
        rule.id = {1};
        rule.event = EffectEvent::UltimateCommitted;
        rule.selector.kind = EffectSelectorKind::Self;
        rule.actions.push_back({ std::move(value) });
        return rule;
    };
    const auto signature = [](const EffectRule& rule)
    {
        const std::span rules{ &rule, std::size_t{1} };
        const auto document = buildEffectDescriptionDocument({
            EffectDescriptionContainerKind::Magic,
            rules,
        });
        const auto& coverage = document.sections.front().blocks.front().coverage;
        REQUIRE(coverage.genericFallback);
        REQUIRE(coverage.unmatchedShapeSignatures.size() == 1);
        CHECK(coverage.unmatchedShapeSignatures.front().starts_with("v2:"));
        return coverage.unmatchedShapeSignatures.front();
    };

    auto areaDamage = makeRule(DealDamageAction{
        .amount = EffectNumber{ .flat = 10 },
    });
    const auto areaDamageSignature = signature(areaDamage);
    std::get<DealDamageAction>(areaDamage.actions.front().value).areaProjectiles
        = AreaProjectileDamageDelivery{
            .rangeTiles = 4,
            .maximumTargets = 2,
            .stunFrames = 8,
            .trackEventSource = true,
            .visual = AreaProjectileVisual::ShieldBlast,
        };
    CHECK(signature(areaDamage) != areaDamageSignature);

    auto selectedUnits = makeRule(ChangeResourceAction{
        .resource = BattleResource::Mp,
        .amount = EffectNumber{ .flat = 10 },
        .kind = ResourceChangeKind::Restore,
    });
    selectedUnits.selector = EffectSelector{
        .kind = EffectSelectorKind::Allies,
        .count = 1,
        .team = EffectTeamFilter::Ally,
    };
    const auto selectorSignature = signature(selectedUnits);
    auto changedSelector = selectedUnits;
    changedSelector.selector.count = 2;
    CHECK(signature(changedSelector) != selectorSignature);
    changedSelector = selectedUnits;
    changedSelector.selector.team = EffectTeamFilter::Enemy;
    CHECK(signature(changedSelector) != selectorSignature);
    changedSelector = selectedUnits;
    changedSelector.selector.requiredTarget = EffectRequiredTarget::HitTarget;
    CHECK(signature(changedSelector) != selectorSignature);

    auto attack = makeRule(ModifyAttackAction{});
    const auto attackSignature = signature(attack);
    auto changedAttack = attack;
    std::get<ModifyAttackAction>(changedAttack.actions.front().value).source
        = EffectSelector{ .kind = EffectSelectorKind::SourceUnit };
    CHECK(signature(changedAttack) != attackSignature);
    changedAttack = attack;
    std::get<ModifyAttackAction>(changedAttack.actions.front().value).damageOverride
        = EffectNumber{ .flat = 250 };
    CHECK(signature(changedAttack) != attackSignature);

    auto consume = makeRule(ConsumeStatusAction{
        .status = BattleStatusKind::Poison,
    });
    const auto consumeSignature = signature(consume);
    std::get<ConsumeStatusAction>(consume.actions.front().value).whenDepleted
        = ApplyStatusAction{
            .status = BattleStatusKind::Stun,
            .durationFrames = 10,
        };
    CHECK(signature(consume) != consumeSignature);

    auto transfer = makeRule(ChangeResourceAction{
        .resource = BattleResource::Mp,
        .amount = EffectNumber{ .flat = 10 },
        .kind = ResourceChangeKind::Transfer,
    });
    const auto transferSignature = signature(transfer);
    std::get<ChangeResourceAction>(transfer.actions.front().value).transferDestination
        = EffectSelector{ .kind = EffectSelectorKind::SourceUnit };
    CHECK(signature(transfer) != transferSignature);
}

TEST_CASE("EffectDescriptionDocument_RequiredFirstEnumValuesRemainVisibleCoverage",
          "[battle][effects][description][document][coverage]")
{
    const auto dispositionAt = [](const EffectActionValue& value, std::string_view path)
    {
        EffectRule rule;
        rule.id = {1};
        rule.event = EffectEvent::UltimateCommitted;
        rule.actions.push_back({ value });
        const std::array rules{ rule };
        const auto document = buildEffectDescriptionDocument({
            EffectDescriptionContainerKind::Magic,
            rules,
        });
        const auto& fields = document.sections.front().blocks.front().coverage.fields;
        const auto field = std::ranges::find_if(fields,
            [path](const DescriptionCoverageEntry& entry)
            {
                return entry.source.path == path;
            });
        REQUIRE(field != fields.end());
        return field->disposition;
    };

    CHECK(dispositionAt(
        ApplyStatusAction{ .status = BattleStatusKind::Poison },
        "actions[0].status") == DescriptionFieldDisposition::Visible);
    const ModifyAttributeAction firstAttribute{
        .attribute = BattleAttribute::MaxHp,
        .operation = AttributeOperation::FlatAdd,
    };
    CHECK(dispositionAt(firstAttribute, "actions[0].attribute")
        == DescriptionFieldDisposition::Visible);
    CHECK(dispositionAt(firstAttribute, "actions[0].operation")
        == DescriptionFieldDisposition::Visible);
    const ChangeResourceAction firstResource{
        .resource = BattleResource::Hp,
        .kind = ResourceChangeKind::Restore,
    };
    CHECK(dispositionAt(firstResource, "actions[0].resource")
        == DescriptionFieldDisposition::Visible);
    CHECK(dispositionAt(firstResource, "actions[0].kind")
        == DescriptionFieldDisposition::Visible);

    const CreateAreaAction areaWithRequiredZeroAttribute{
        .durationFrames = 10,
        .modifiers = {
            AreaModifier{
                .kind = AreaModifierKind::Attribute,
                .relation = EffectTeamFilter::Ally,
                .attribute = BattleAttribute::MaxHp,
            },
            AreaModifier{
                .kind = AreaModifierKind::OutgoingDamage,
                .relation = EffectTeamFilter::Enemy,
                .percent = 0,
                .damageChannel = DamageChannel::Skill,
            },
        },
    };
    CHECK(dispositionAt(areaWithRequiredZeroAttribute, "actions[0].modifiers[0].attribute")
        == DescriptionFieldDisposition::Visible);
    CHECK(dispositionAt(areaWithRequiredZeroAttribute, "actions[0].modifiers[1].percent")
        == DescriptionFieldDisposition::Visible);
    CHECK(dispositionAt(areaWithRequiredZeroAttribute, "actions[0].modifiers[1].damageChannel")
        == DescriptionFieldDisposition::Visible);

    const DealDamageAction areaProjectileRequiredZeros{
        .kind = BattleDamageKind::Physical,
        .areaProjectiles = AreaProjectileDamageDelivery{
            .rangeTiles = 0,
            .maximumTargets = 0,
            .stunFrames = 0,
            .visual = AreaProjectileVisual::DeathBlast,
        },
    };
    for (const auto path : {
             "actions[0].areaProjectiles.value.rangeTiles",
             "actions[0].areaProjectiles.value.maximumTargets",
             "actions[0].areaProjectiles.value.stunFrames",
             "actions[0].areaProjectiles.value.visual",
         })
    {
        CHECK(dispositionAt(areaProjectileRequiredZeros, path)
            == DescriptionFieldDisposition::Visible);
    }
    CHECK(dispositionAt(
        areaProjectileRequiredZeros,
        "actions[0].areaProjectiles.value.trackEventSource")
        == DescriptionFieldDisposition::SchemaDefault);
}

TEST_CASE("EffectDescriptionDocument_FormalNonMagicPanelsFitReadableFonts",
          "[battle][effects][description][typography][panel-text]")
{
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content);
    const auto appendRow = [](
        std::vector<PanelTextSourceRow>& rows,
        std::string text,
        int fontSizeDelta = 0,
        int indentUnits = 0,
        int spacingBefore = 0,
        int spacingAfter = 0)
    {
        rows.push_back({
            .text = std::move(text),
            .fontSizeDelta = fontSizeDelta,
            .indentUnits = indentUnits,
            .spacingBefore = spacingBefore,
            .spacingAfter = spacingAfter,
        });
    };
    const auto appendDescription = [](
        std::vector<PanelTextSourceRow>& rows,
        EffectDescriptionContainerKind kind,
        std::span<const EffectRule> rules,
        EffectDescriptionStyle style,
        int fontSizeDelta,
        int extraSpacing,
        int baseIndentUnits = 0)
    {
        const auto document = buildEffectDescriptionDocument({kind, rules});
        const auto rendered = renderEffectDescription(document, style, {});
        auto descriptionRows = panelTextRowsForEffectDescription(
            rendered,
            fontSizeDelta,
            extraSpacing,
            baseIndentUnits);
        rows.insert(
            rows.end(),
            std::make_move_iterator(descriptionRows.begin()),
            std::make_move_iterator(descriptionRows.end()));
    };
    const auto checkCompleteLayout = [](
        std::string_view source,
        std::span<const PanelTextSourceRow> rows,
        const PanelTextLayout& layout,
        int pixelWidth,
        int pixelHeight)
    {
        CAPTURE(source, layout.baseFontSize, layout.height, pixelWidth, pixelHeight);
        REQUIRE_FALSE(rows.empty());
        CHECK(layout.height <= pixelHeight);
        std::vector<bool> represented(rows.size());
        for (const auto& line : layout.lines)
        {
            REQUIRE(line.sourceRow < rows.size());
            represented[line.sourceRow] = true;
            CHECK(line.y + line.fontSize <= pixelHeight);
            CHECK(line.indentPixels
                + displayTextWidth(line.text) * line.fontSize / 2 <= pixelWidth);
        }
        CHECK(std::ranges::all_of(represented, std::identity{}));
    };
    const auto fitComplete = [&](
        std::string_view source,
        std::span<const PanelTextSourceRow> rows,
        int pixelWidth,
        int pixelHeight,
        int preferredFontSize,
        int minimumFontSize)
    {
        const auto minimum = layoutPanelText(rows, pixelWidth, minimumFontSize);
        CAPTURE(source, minimum.height, pixelHeight, minimumFontSize);
        REQUIRE(minimum.height <= pixelHeight);
        const auto fitted = fitPanelText(
            rows,
            pixelWidth,
            pixelHeight,
            preferredFontSize,
            minimumFontSize);
        CHECK(fitted.baseFontSize >= minimumFontSize);
        checkCompleteLayout(source, rows, fitted, pixelWidth, pixelHeight);
        return fitted.baseFontSize;
    };

    constexpr int detailWidth = 520;
    constexpr int detailHeight = 320;
    int smallestEquipmentFont = 26;
    for (const auto& equipment : content->equipment())
    {
        std::vector<PanelTextSourceRow> rows;
        if (!equipment.rules.empty())
        {
            appendRow(rows, "特殊效果:", 2, 0, 0, 2);
            appendDescription(
                rows,
                EffectDescriptionContainerKind::Equipment,
                equipment.rules,
                EffectDescriptionStyle::Full,
                0,
                2);
        }
        const bool hasSynergies = std::ranges::any_of(
            content->equipmentSynergies(),
            [&](const auto& synergy) { return synergy.equipmentId == equipment.itemId; });
        if (hasSynergies)
        {
            appendRow(rows, "裝備羈絆:", 2, 0,
                equipment.rules.empty() ? 0 : 12, 2);
            for (const auto& synergy : content->equipmentSynergies())
            {
                if (synergy.equipmentId != equipment.itemId) continue;
                std::string heading;
                for (std::size_t index = 0; index < synergy.roleIds.size(); ++index)
                {
                    if (index > 0) heading += "、";
                    const auto* role = content->role(synergy.roleIds[index]);
                    REQUIRE(role);
                    heading += role->Name;
                }
                heading += "：";
                const auto comboNames = countsAsComboNames(synergy.managementRules);
                if (!comboNames.empty())
                {
                    heading += "計作";
                    for (std::size_t index = 0; index < comboNames.size(); ++index)
                    {
                        if (index > 0) heading += "、";
                        heading += comboNames[index];
                    }
                }
                appendRow(rows, std::move(heading), 0, 0, 0, 2);
                appendDescription(
                    rows,
                    EffectDescriptionContainerKind::EquipmentSynergy,
                    synergy.rules,
                    EffectDescriptionStyle::Full,
                    0,
                    2,
                    2);
            }
        }
        if (rows.empty()) continue;
        smallestEquipmentFont = std::min(
            smallestEquipmentFont,
            fitComplete(
                std::format("裝備 {}", equipment.itemId),
                rows,
                detailWidth,
                detailHeight,
                26,
                14));
    }
    CHECK(smallestEquipmentFont >= 14);

    int smallestNeigongFont = 24;
    for (const auto& neigong : content->neigong())
    {
        std::vector<PanelTextSourceRow> rows;
        appendRow(rows, "效果:", 2, 0, 0, 2);
        appendDescription(
            rows,
            EffectDescriptionContainerKind::Neigong,
            neigong.rules,
            EffectDescriptionStyle::Full,
            0,
            2,
            2);
        smallestNeigongFont = std::min(
            smallestNeigongFont,
            fitComplete(
                std::format("內功 {}", neigong.magicId),
                rows,
                detailWidth,
                detailHeight,
                24,
                14));
    }
    CHECK(smallestNeigongFont >= 14);

    const std::set<int> poolRoles(
        content->poolRoleIds().begin(),
        content->poolRoleIds().end());
    constexpr int comboHeight = 555;
    constexpr int memberWidth = 270;
    constexpr int thresholdWidth = 260;
    int smallestFullComboFont = 24;
    for (const auto& combo : content->combos())
    {
        std::vector<PanelTextSourceRow> memberRows;
        appendRow(memberRows, "成員:", 0, 0, 0, 2);
        int memberCount{};
        for (const int roleId : combo.memberRoleIds)
        {
            if (!poolRoles.contains(roleId)) continue;
            const auto* role = content->role(roleId);
            REQUIRE(role);
            ++memberCount;
            appendRow(
                memberRows,
                std::format(
                    "  {} ({}費) ✓{}",
                    role->Name,
                    role->Cost,
                    combo.starSynergyBonus ? " ★3" : ""),
                0,
                0,
                0,
                1);
        }
        if (memberCount == 0) continue;

        std::vector<PanelTextSourceRow> thresholdRows;
        appendRow(thresholdRows, combo.isAntiCombo ? "條件:" : "閾值:");
        if (combo.starSynergyBonus)
        {
            appendRow(
                thresholdRows,
                std::format("★ 成員星級計人數，當前+{}人", memberCount * 2),
                -4,
                0,
                4,
                2);
            appendRow(thresholdRows, "每額外★計入1羈絆人數", -6, 2);
        }
        for (const auto& threshold : combo.thresholds)
        {
            appendRow(
                thresholdRows,
                std::format("{}人: {} ✓", threshold.count, threshold.name),
                0,
                0,
                8,
                1);
            appendDescription(
                thresholdRows,
                EffectDescriptionContainerKind::ComboThreshold,
                threshold.rules,
                EffectDescriptionStyle::Full,
                -2,
                2);
        }
        int fittedFont{};
        PanelTextLayout fittedMembers;
        PanelTextLayout fittedThresholds;
        for (int fontSize = 24; fontSize >= 14; --fontSize)
        {
            auto members = layoutPanelText(memberRows, memberWidth, fontSize);
            auto thresholds = layoutPanelText(thresholdRows, thresholdWidth, fontSize);
            if (members.height > comboHeight || thresholds.height > comboHeight) continue;
            fittedFont = fontSize;
            fittedMembers = std::move(members);
            fittedThresholds = std::move(thresholds);
            break;
        }
        CAPTURE(combo.name);
        REQUIRE(fittedFont >= 14);
        checkCompleteLayout(combo.name, memberRows, fittedMembers, memberWidth, comboHeight);
        checkCompleteLayout(combo.name, thresholdRows, fittedThresholds, thresholdWidth, comboHeight);
        smallestFullComboFont = std::min(smallestFullComboFont, fittedFont);
    }
    CHECK(smallestFullComboFont >= 14);

    // 1170px content region - 500px formal menu budget - 20px panel gap
    // - 10px outer inset - 20px text insets.
    constexpr int compactPanelWidth = 620;
    constexpr int compactPanelHeight = 182;
    int smallestCompactComboFont = 19;
    for (const int roleId : content->poolRoleIds())
    {
        std::vector<std::vector<PanelTextSourceRow>> blocks;
        for (const auto& combo : content->combos())
        {
            if (std::ranges::find(combo.memberRoleIds, roleId) == combo.memberRoleIds.end())
                continue;
            REQUIRE_FALSE(combo.thresholds.empty());
            std::vector<PanelTextSourceRow> longestBlock;
            int longestHeight = -1;
            for (const auto& threshold : combo.thresholds)
            {
                std::vector<PanelTextSourceRow> block;
                appendRow(
                    block,
                    std::format("{} (7+14/7 ✓)", combo.name),
                    1,
                    0,
                    0,
                    2);
                appendDescription(
                    block,
                    EffectDescriptionContainerKind::ComboThreshold,
                    threshold.rules,
                    EffectDescriptionStyle::Compact,
                    0,
                    1,
                    2);
                const int height = layoutPanelText(
                    block,
                    compactPanelWidth / 2,
                    12).height;
                if (height > longestHeight)
                {
                    longestHeight = height;
                    longestBlock = std::move(block);
                }
            }
            blocks.push_back(std::move(longestBlock));
        }
        if (blocks.empty()) continue;
        const auto fitted = fitPanelTextBlocks(
            blocks,
            compactPanelWidth,
            compactPanelHeight,
            19,
            12);
        CAPTURE(roleId);
        REQUIRE(fitted);
        REQUIRE(fitted->columns.size() <= 2);
        smallestCompactComboFont = std::min(
            smallestCompactComboFont,
            fitted->baseFontSize);
        std::size_t expectedFirstBlock{};
        for (const auto& column : fitted->columns)
        {
            CHECK(column.firstBlock == expectedFirstBlock);
            REQUIRE(column.lastBlock > column.firstBlock);
            expectedFirstBlock = column.lastBlock;
            std::vector<PanelTextSourceRow> rows;
            for (std::size_t block = column.firstBlock; block < column.lastBlock; ++block)
                rows.insert(rows.end(), blocks[block].begin(), blocks[block].end());
            checkCompleteLayout(
                std::format("角色 {} 快速羈絆", roleId),
                rows,
                column.layout,
                fitted->columnWidth,
                compactPanelHeight);
        }
        CHECK(expectedFirstBlock == blocks.size());
    }
    CHECK(smallestCompactComboFont >= 12);
}

TEST_CASE("EffectDescriptionDocument_FormalContentMeetsCoverageAndRowContracts",
          "[battle][effects][description][document][content]")
{
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content);
    std::set<std::string> fallbackShapes;
    int maximumCompactRowWidth{};
    const auto checkContainer = [&fallbackShapes, &maximumCompactRowWidth](
        EffectDescriptionContainerKind kind,
        std::span<const EffectRule> rules)
    {
        const auto document = buildEffectDescriptionDocument({ kind, rules });
        if (!rules.empty())
            CHECK_FALSE(document.sections.empty());
        std::set<std::tuple<
            std::uint64_t,
            std::size_t,
            DescriptionSourceFieldKind,
            std::string>> covered;
        std::vector<const DescriptionCoverage*> blockCoverages;
        for (const auto& section : document.sections)
        {
            for (const auto& block : section.blocks)
            {
                blockCoverages.push_back(&block.coverage);
                fallbackShapes.insert(
                    block.coverage.unmatchedShapeSignatures.begin(),
                    block.coverage.unmatchedShapeSignatures.end());
                for (const auto& entry : block.coverage.fields)
                {
                    CAPTURE(entry.source.ruleId.value,
                        entry.source.ruleOrder,
                        entry.source.kind,
                        entry.source.path);
                    CHECK(covered.emplace(
                        entry.source.ruleId.value,
                        entry.source.ruleOrder,
                        entry.source.kind,
                        entry.source.path).second);
                    CHECK_FALSE(entry.auditValue.empty());
                    CHECK_FALSE(entry.reason.empty());
                }
            }
        }
        if (!rules.empty()) CHECK_FALSE(covered.empty());

        for (const auto style : {
                 EffectDescriptionStyle::Detailed,
                 EffectDescriptionStyle::Full,
                 EffectDescriptionStyle::Compact,
             })
        {
            const auto rendered = renderEffectDescription(document, style, {});
            if (style == EffectDescriptionStyle::Detailed)
            {
                const auto detailedRows = effectDescriptionTextRows(rendered);
                for (const auto* coverage : blockCoverages)
                {
                    for (const auto& entry : coverage->fields)
                    {
                        const auto auditPrefix = std::format(
                            "稽核 {} = {} [",
                            entry.source.path,
                            entry.auditValue);
                        CHECK(std::ranges::any_of(
                            detailedRows,
                            [&](const std::string& row)
                            {
                                return row.find(auditPrefix) != std::string::npos;
                            }));
                    }
                }
            }
            for (const auto& section : rendered.sections)
            {
                if (section.heading)
                    CHECK_FALSE(section.heading->ends_with("。"));
                for (const auto& block : section.blocks)
                {
                    CHECK_FALSE(block.rows.empty());
                    for (const auto& row : block.rows)
                    {
                        CHECK_FALSE(row.text.empty());
                        CHECK_FALSE(row.text.ends_with("。"));
                        if (style == EffectDescriptionStyle::Detailed) continue;
                        CHECK(row.text.find("→") == std::string::npos);
                        CHECK(row.text.find("／") == std::string::npos);
                        CHECK(row.text.find('/') == std::string::npos);
                        CHECK(row.text.find("·") == std::string::npos);
                        CHECK(row.text.find("替代目標") == std::string::npos);
                        CHECK(row.text.find("強度目標") == std::string::npos);
                        CHECK(row.text.find("取代×") == std::string::npos);
                        CHECK(row.text.find("取強×") == std::string::npos);
                        CHECK(row.text.find("若僅限") == std::string::npos);
                        CHECK(row.text.find("若第") == std::string::npos);
                        CHECK(row.text.find("的傷害且") == std::string::npos);
                        if (style == EffectDescriptionStyle::Compact)
                        {
                            const auto width = displayTextWidth(row.text);
                            maximumCompactRowWidth = std::max(
                                maximumCompactRowWidth,
                                width);
                            CHECK(width <= 72);
                            CHECK_FALSE(row.text.ends_with("。"));
                            CHECK_FALSE(row.text.ends_with("，"));
                            CHECK_FALSE(row.text.ends_with("；"));
                        }
                        else
                        {
                            CHECK(displayTextWidth(row.text) <= 120);
                            CHECK_FALSE(row.text.ends_with("。。"));
                            CHECK(row.text.find("，。") == std::string::npos);
                            CHECK(row.text.find("；。") == std::string::npos);
                            CHECK(row.text.find("條件[") == std::string::npos);
                            CHECK(row.text.find("追加至基礎攻擊") == std::string::npos);
                            CHECK(row.text.find("原樣式") == std::string::npos);
                        }
                    }
                }
            }
        }
    };

    for (const auto& definition : content->magicEffects())
        checkContainer(EffectDescriptionContainerKind::Magic, definition.rules);
    for (const auto& definition : content->equipment())
        checkContainer(EffectDescriptionContainerKind::Equipment, definition.rules);
    for (const auto& definition : content->equipmentSynergies())
        checkContainer(EffectDescriptionContainerKind::EquipmentSynergy,
            definition.rules);
    for (const auto& definition : content->neigong())
        checkContainer(EffectDescriptionContainerKind::Neigong, definition.rules);
    for (const auto& combo : content->combos())
        for (const auto& threshold : combo.thresholds)
            checkContainer(EffectDescriptionContainerKind::ComboThreshold,
                threshold.rules);
    CHECK(maximumCompactRowWidth == 71);
    std::string fallbackShapeCorpus;
    for (const auto& shape : fallbackShapes)
    {
        fallbackShapeCorpus += shape;
        fallbackShapeCorpus += '\n';
    }
    CHECK(fallbackShapes.size() == 347);
    CHECK(chessSha256Hex(chessSha256(fallbackShapeCorpus))
        == "d9eff921d80ffa8aec8f395dc2713c514a199c934374cfe1d11722d5343630b7");
}

TEST_CASE("EffectDescriptionDocument_PresentationContextOnlyOmitsMatchingBoundTrigger",
          "[battle][effects][description][document][context]")
{
    auto bound = parseRuleText(R"(
時機: 絕招施放
目標: 自身
回復內力: 20
)");
    auto anyCast = bound;
    anyCast.id = {2};
    anyCast.castMatch = EffectCastMatch::OwnerAnyCast;
    const std::array rules{ bound, anyCast };
    const auto document = buildEffectDescriptionDocument({
        EffectDescriptionContainerKind::Magic,
        rules,
    });
    const EffectDescriptionPresentationContext context{
        .enclosingDefaultEvent = EffectEvent::UltimateCommitted,
    };
    const auto compact = effectDescriptionTextRows(renderEffectDescription(
        document, EffectDescriptionStyle::Compact, context));
    REQUIRE(compact.size() == 2);
    CHECK_FALSE(compact[0].starts_with("施放大招"));
    CHECK_FALSE(compact[1].empty());
    CHECK(compact[1].find("任意施放") != std::string::npos);

    const auto detailed = effectDescriptionTextRows(renderEffectDescription(
        document, EffectDescriptionStyle::Detailed, context));
    CHECK(std::ranges::contains(detailed, std::string("施放大招")));
    CHECK(std::ranges::any_of(detailed, [](const std::string& row)
    {
        return row.find("施放匹配：本容器綁定武功") != std::string::npos;
    }));
}

TEST_CASE("EffectDescriptionDocument_ArchetypesRejectUndeclaredShapeMutations",
          "[battle][effects][description][document][archetype]")
{
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content);
    const auto archetypes = [](std::span<const EffectRule> rules)
    {
        const auto document = buildEffectDescriptionDocument({
            EffectDescriptionContainerKind::Magic,
            rules,
        });
        std::vector<DescriptionArchetype> result;
        for (const auto& section : document.sections)
            for (const auto& block : section.blocks)
                result.push_back(block.archetype);
        return result;
    };

    auto borrowed = definitionWithId(content->magicEffects(), 43).rules.front();
    auto* borrowedAction = std::get_if<BorrowEffectRulesAction>(
        &std::get<StateMachineAction>(borrowed.actions.front().value));
    REQUIRE(borrowedAction);
    borrowedAction->sourceCount.maximum = 3;
    CHECK_FALSE(std::ranges::contains(
        archetypes(std::span{&borrowed, 1}),
        DescriptionArchetype::BorrowRules));

    auto copied = definitionWithId(content->magicEffects(), 98).rules.front();
    auto* copiedAction = std::get_if<CopyAttackDefinitionAction>(
        &std::get<StateMachineAction>(copied.actions.front().value));
    REQUIRE(copiedAction);
    copiedAction->copyCount = 2;
    CHECK_FALSE(std::ranges::contains(
        archetypes(std::span{&copied, 1}),
        DescriptionArchetype::CopyAttack));

    auto conditional = definitionWithId(content->magicEffects(), 62).rules.front();
    const auto sourceConditional = std::get<std::shared_ptr<ConditionalEffectAction>>(
        conditional.actions.front().value);
    REQUIRE(sourceConditional);
    auto conditionalAction = std::make_shared<ConditionalEffectAction>(
        *sourceConditional);
    conditional.actions.front().value = conditionalAction;
    REQUIRE(conditionalAction);
    auto* allyAttack = std::get_if<ModifyAttackAction>(
        &conditionalAction->whenTrue.front().value);
    REQUIRE(allyAttack);
    allyAttack->sameTargetHitLimit = 1;
    CHECK_FALSE(std::ranges::contains(
        archetypes(std::span{&conditional, 1}),
        DescriptionArchetype::ConditionalAttack));

    auto sevenStarRules = definitionWithId(content->magicEffects(), 39).rules;
    REQUIRE(sevenStarRules.size() == 2);
    sevenStarRules.front().selector = EffectSelector{};
    const auto disconnected = archetypes(sevenStarRules);
    CHECK_FALSE(std::ranges::contains(
        disconnected,
        DescriptionArchetype::StatusLifecycle));

    sevenStarRules = definitionWithId(content->magicEffects(), 39).rules;
    auto duplicateProducer = sevenStarRules.front();
    duplicateProducer.id.value += 1'000'000;
    sevenStarRules.push_back(std::move(duplicateProducer));
    const auto ambiguous = archetypes(sevenStarRules);
    CHECK_FALSE(std::ranges::contains(
        ambiguous,
        DescriptionArchetype::StatusLifecycle));

    auto poisonExplosionRules = definitionWithId(content->magicEffects(), 95).rules;
    REQUIRE(poisonExplosionRules.size() >= 2);
    REQUIRE(poisonExplosionRules[1].repetitionCount);
    poisonExplosionRules[1].repetitionCount->percent = 50;
    const auto nonLayered = archetypes(poisonExplosionRules);
    CHECK_FALSE(std::ranges::contains(
        nonLayered,
        DescriptionArchetype::StackExplosion));

    const auto detailedRows = [](std::span<const EffectRule> rules)
    {
        return effectDescriptionTextRows(renderEffectDescription(
            buildEffectDescriptionDocument({
                EffectDescriptionContainerKind::Magic,
                rules,
            }),
            EffectDescriptionStyle::Detailed,
            {}));
    };
    const auto originalSevenStarRules = definitionWithId(
        content->magicEffects(),
        39).rules;
    const auto originalSevenStarDetailed = detailedRows(originalSevenStarRules);
    const auto playerRows = [](std::span<const EffectRule> rules,
                               EffectDescriptionStyle style)
    {
        return effectDescriptionTextRows(renderEffectDescription(
            buildEffectDescriptionDocument({
                EffectDescriptionContainerKind::Magic,
                rules,
            }),
            style,
            {}));
    };
    const auto originalSevenStarFull = playerRows(
        originalSevenStarRules, EffectDescriptionStyle::Full);
    const auto originalSevenStarCompact = playerRows(
        originalSevenStarRules, EffectDescriptionStyle::Compact);
    for (const bool secondary : {false, true})
    {
        auto mutated = originalSevenStarRules;
        auto& applied = std::get<ApplyStatusAction>(
            mutated.front().actions.front().value);
        (secondary ? applied.secondaryPotency : applied.potency).flat += 1;
        CHECK_FALSE(std::ranges::contains(
            archetypes(mutated),
            DescriptionArchetype::StatusLifecycle));
        CHECK(detailedRows(mutated) != originalSevenStarDetailed);
        CHECK(playerRows(mutated, EffectDescriptionStyle::Full)
            != originalSevenStarFull);
        CHECK(playerRows(mutated, EffectDescriptionStyle::Compact)
            != originalSevenStarCompact);
    }

    const auto originalPoisonExplosionRules = definitionWithId(
        content->magicEffects(),
        95).rules;
    const auto originalPoisonExplosionDetailed = detailedRows(
        originalPoisonExplosionRules);
    const auto originalPoisonExplosionFull = playerRows(
        originalPoisonExplosionRules, EffectDescriptionStyle::Full);
    const auto originalPoisonExplosionCompact = playerRows(
        originalPoisonExplosionRules, EffectDescriptionStyle::Compact);
    for (const bool secondary : {false, true})
    {
        auto mutated = originalPoisonExplosionRules;
        auto& applied = std::get<ApplyStatusAction>(
            mutated.front().actions.front().value);
        (secondary ? applied.secondaryPotency : applied.potency).flat += 1;
        CHECK(std::ranges::contains(
            archetypes(mutated),
            DescriptionArchetype::StackExplosion) == !secondary);
        CHECK(detailedRows(mutated) != originalPoisonExplosionDetailed);
        CHECK(playerRows(mutated, EffectDescriptionStyle::Full)
            != originalPoisonExplosionFull);
        CHECK(playerRows(mutated, EffectDescriptionStyle::Compact)
            != originalPoisonExplosionCompact);
    }
}
