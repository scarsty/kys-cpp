#pragma once

#include "ChessBattleEffectParser.h"
#include "ChessBattleEffectSemantics.h"
#include "ChessBattleEffectValidation.h"
#include "ChessEffectDescription.h"
#include "ChessGameContent.h"
#include "ChessGameSessionTestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace KysChess::Test
{

inline std::string descriptionText(
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

inline const ChessMagicEffectDefinition& definitionWithId(
    const std::vector<ChessMagicEffectDefinition>& definitions,
    int magicId)
{
    const auto it = std::ranges::find(definitions, magicId, &ChessMagicEffectDefinition::magicId);
    REQUIRE(it != definitions.end());
    return *it;
}

inline const EffectRule& ruleWithEvent(
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

inline EffectRule parseRuleText(std::string_view yaml, std::uint64_t id = 1)
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

inline std::size_t countOccurrences(std::string_view text, std::string_view fragment)
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

inline void checkEffectNumberEqual(const EffectNumber& lhs, const EffectNumber& rhs)
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

inline void checkOptionalEffectNumberEqual(
    const std::optional<EffectNumber>& lhs,
    const std::optional<EffectNumber>& rhs)
{
    REQUIRE(lhs.has_value() == rhs.has_value());
    if (lhs) checkEffectNumberEqual(*lhs, *rhs);
}

inline void checkSelectorEqual(const EffectSelector& lhs, const EffectSelector& rhs)
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

inline void checkOptionalSelectorEqual(
    const std::optional<EffectSelector>& lhs,
    const std::optional<EffectSelector>& rhs)
{
    REQUIRE(lhs.has_value() == rhs.has_value());
    if (lhs) checkSelectorEqual(*lhs, *rhs);
}

inline void checkConditionEqual(const EffectCondition& lhs, const EffectCondition& rhs)
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

inline void checkApplyStatusEqual(const ApplyStatusAction& lhs, const ApplyStatusAction& rhs)
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

inline void checkAttackPatternEqual(const AttackPattern& lhs, const AttackPattern& rhs)
{
    CHECK(lhs.kind == rhs.kind);
    CHECK(lhs.projectileCount == rhs.projectileCount);
    CHECK(lhs.spreadDegrees == rhs.spreadDegrees);
    CHECK(lhs.intervalFrames == rhs.intervalFrames);
}

inline void checkAttackRuntimeBehaviorEqual(
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

inline void checkAreaModifierEqual(const AreaModifier& lhs, const AreaModifier& rhs)
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

inline void checkStateMachineEqual(const StateMachineAction& lhs, const StateMachineAction& rhs)
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

inline void checkActionEqual(const EffectAction& lhs, const EffectAction& rhs);

inline void checkActionsEqual(
    const std::vector<EffectAction>& lhs,
    const std::vector<EffectAction>& rhs)
{
    REQUIRE(lhs.size() == rhs.size());
    for (std::size_t index = 0; index < lhs.size(); ++index)
        checkActionEqual(lhs[index], rhs[index]);
}

inline void checkActionEqual(const EffectAction& lhs, const EffectAction& rhs)
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

inline void checkRulesEqual(const EffectRule& lhs, const EffectRule& rhs)
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

inline std::set<int> poolUltimateMagicIds(const ChessGameContent& content)
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

}  // namespace KysChess::Test
