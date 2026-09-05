#include "ChessGameContent.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace KysChess
{
namespace
{

auto roleContentView(const ChessRoleDefinition& role)
{
    std::array<int, ROLE_MAGIC_COUNT> magicIds{};
    std::array<int, ROLE_MAGIC_COUNT> magicPower{};
    std::ranges::copy(role.MagicID, magicIds.begin());
    std::ranges::copy(role.MagicPower, magicPower.begin());
    return std::tuple{
        role.ID,
        role.HeadID,
        role.Cost,
        role.Sexual,
        role.MaxHP,
        role.MPType,
        role.MaxMP,
        role.Attack,
        role.Speed,
        role.Defence,
        role.Medicine,
        role.UsePoison,
        role.Detoxification,
        role.AntiPoison,
        role.Fist,
        role.Sword,
        role.Knife,
        role.Unusual,
        role.HiddenWeapon,
        role.Knowledge,
        role.Morality,
        role.AttackWithPoison,
        role.AttackTwice,
        role.Fame,
        role.IQ,
        std::move(magicIds),
        std::move(magicPower),
        role.Name,
        role.Nick,
    };
}

auto magicContentView(const ChessMagicDefinition& magic)
{
    return std::tuple{
        magic.ID,
        magic.SoundID,
        magic.MagicType,
        magic.EffectID,
        magic.HurtType,
        magic.AttackAreaType,
        magic.NeedMP,
        magic.WithPoison,
        magic.SelectDistance,
        magic.AttackDistance,
        magic.AddMP,
        magic.HurtMP,
        magic.Name,
    };
}

auto roleContentViews(const std::map<int, ChessRoleDefinition>& roles)
{
    using View = decltype(roleContentView(std::declval<const ChessRoleDefinition&>()));
    std::vector<std::pair<int, View>> result;
    result.reserve(roles.size());
    for (const auto& [id, role] : roles)
    {
        result.emplace_back(id, roleContentView(role));
    }
    return result;
}

auto magicContentViews(const std::map<int, ChessMagicDefinition>& magics)
{
    using View = decltype(magicContentView(std::declval<const ChessMagicDefinition&>()));
    std::vector<std::pair<int, View>> result;
    result.reserve(magics.size());
    for (const auto& [id, magic] : magics)
    {
        result.emplace_back(id, magicContentView(magic));
    }
    return result;
}

template<typename Value>
std::optional<ChessSha256> optionalContentHash(
    std::string_view domain,
    const std::optional<Value>& value)
{
    return value ? std::optional{ chessBeveSha256(domain, *value) } : std::nullopt;
}

ChessSha256 effectNumberContentHash(const EffectNumber& number)
{
    return chessBeveSha256(
        "KYS_EFFECT_NUMBER",
        static_cast<int>(number.base),
        number.multiplierBase,
        number.status,
        static_cast<int>(number.statusSource),
        number.stateSlot,
        number.flat,
        number.percent,
        static_cast<int>(number.rounding),
        number.minimum,
        number.maximum,
        static_cast<int>(number.statusScale),
        number.boundNumerator,
        number.boundDenominator);
}

ChessSha256 selectorContentHash(const EffectSelector& selector)
{
    return chessBeveSha256(
        "KYS_EFFECT_SELECTOR",
        static_cast<int>(selector.kind),
        selector.count,
        selector.radiusTiles,
        selector.squareSideTiles,
        static_cast<int>(selector.team),
        static_cast<int>(selector.tieBreak),
        selector.excludeOwner,
        selector.requiredBoundMagic,
        static_cast<int>(selector.requiredMartialCategory),
        selector.requiredTarget);
}

ChessSha256 attackPatternContentHash(const AttackPattern& pattern)
{
    return chessBeveSha256(
        "KYS_EFFECT_ATTACK_PATTERN",
        static_cast<int>(pattern.kind),
        pattern.projectileCount,
        pattern.spreadDegrees,
        pattern.intervalFrames);
}

ChessSha256 attackRuntimeBehaviorContentHash(const AttackRuntimeBehavior& behavior)
{
    return std::visit(
        [&](const auto& typed)
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return chessBeveSha256("KYS_EFFECT_ATTACK_RUNTIME", behavior.index());
            else if constexpr (std::is_same_v<T, ProjectileBounceAttackBehavior>)
                return chessBeveSha256("KYS_EFFECT_ATTACK_RUNTIME", behavior.index(),
                    typed.additionalHits, typed.chancePct, typed.rangePixels);
            else if constexpr (std::is_same_v<T, NearbyTrackingAttackBehavior>)
                return chessBeveSha256("KYS_EFFECT_ATTACK_RUNTIME", behavior.index(),
                    typed.rangePixels, typed.damagePct);
            else if constexpr (std::is_same_v<T, DelayedAlternateAttackBehavior>)
                return chessBeveSha256("KYS_EFFECT_ATTACK_RUNTIME", behavior.index(),
                    typed.delayFrames, typed.damagePct, typed.attackerBlockGainChancePct);
            else
                return chessBeveSha256("KYS_EFFECT_ATTACK_RUNTIME", behavior.index(),
                    typed.projectileCount, typed.bleedStacks);
        },
        behavior);
}

ChessSha256 conditionContentHash(const EffectCondition& condition)
{
    return std::visit(
        [&](const auto& typed)
        {
            using T = std::decay_t<decltype(typed)>;
            constexpr auto empty = std::is_empty_v<T>;
            if constexpr (empty)
                return chessBeveSha256("KYS_EFFECT_CONDITION", condition.index());
            else if constexpr (std::is_same_v<T, SourceHpRatioAtMostCondition>
                || std::is_same_v<T, SourceHpRatioBelowCondition>
                || std::is_same_v<T, TargetHpRatioAtMostCondition>)
                return chessBeveSha256("KYS_EFFECT_CONDITION", condition.index(), typed.percent);
            else if constexpr (std::is_same_v<T, SourceHasStateCondition>
                || std::is_same_v<T, TargetHasStateCondition>
                || std::is_same_v<T, TargetHasStateFromEffectOwnerCondition>)
                return chessBeveSha256("KYS_EFFECT_CONDITION", condition.index(), static_cast<int>(typed.state));
            else if constexpr (std::is_same_v<T, SourceStackAtLeastCondition>)
                return chessBeveSha256("KYS_EFFECT_CONDITION", condition.index(),
                    static_cast<int>(typed.stack), typed.count);
            else if constexpr (std::is_same_v<T, CastDistinctTargetCountAtLeastCondition>)
                return chessBeveSha256("KYS_EFFECT_CONDITION", condition.index(), typed.count);
            else if constexpr (std::is_same_v<T, AttackOrdinalEqualsCondition>)
                return chessBeveSha256("KYS_EFFECT_CONDITION", condition.index(), typed.ordinal);
            else if constexpr (std::is_same_v<T, HealKindInCondition>
                || std::is_same_v<T, DamageKindInCondition>)
                return chessBeveSha256("KYS_EFFECT_CONDITION", condition.index(), typed.kinds);
            else if constexpr (std::is_same_v<T, AcceptedHitCondition>)
                return chessBeveSha256("KYS_EFFECT_CONDITION", condition.index(), typed.requirePositiveDamage);
            else if constexpr (std::is_same_v<T, DamagePerspectiveCondition>)
                return chessBeveSha256("KYS_EFFECT_CONDITION", condition.index(), static_cast<int>(typed.perspective));
            else
                static_assert(false, "Unhandled condition content hash");
        },
        condition);
}

std::vector<ChessSha256> conditionContentHashes(
    const std::vector<EffectCondition>& conditions)
{
    std::vector<ChessSha256> result;
    result.reserve(conditions.size());
    std::ranges::transform(conditions, std::back_inserter(result), conditionContentHash);
    return result;
}

ChessSha256 statusBehaviorContentHash(const StatusBehaviorDefinition& behavior);

ChessSha256 applyStatusContentHash(const ApplyStatusAction& action)
{
    const auto quantityHash = std::visit([&](const auto& quantity)
    {
        using T = std::decay_t<decltype(quantity)>;
        if constexpr (std::is_same_v<T, NoStatusQuantity>)
            return chessBeveSha256("KYS_EFFECT_STATUS_QUANTITY", action.quantity.index());
        else if constexpr (std::is_same_v<T, AddStatusLayers>
            || std::is_same_v<T, AddDamageBlockCharges>)
            return chessBeveSha256(
                "KYS_EFFECT_STATUS_QUANTITY", action.quantity.index(),
                quantity.count, quantity.limit);
        else
            return chessBeveSha256(
                "KYS_EFFECT_STATUS_QUANTITY", action.quantity.index(), quantity.count);
    }, action.quantity);
    return chessBeveSha256(
        "KYS_EFFECT_APPLY_STATUS",
        static_cast<int>(action.status),
        action.durationFrames,
        action.duration ? std::optional{ effectNumberContentHash(*action.duration) } : std::nullopt,
        quantityHash,
        static_cast<int>(action.reapplication),
        static_cast<int>(action.poisonSameEventMerge),
        action.behavior
            ? std::optional{ statusBehaviorContentHash(*action.behavior) }
            : std::nullopt);
}

ChessSha256 areaModifierContentHash(const AreaModifier& modifier)
{
    return chessBeveSha256(
        "KYS_EFFECT_AREA_MODIFIER",
        static_cast<int>(modifier.kind),
        static_cast<int>(modifier.relation),
        static_cast<int>(modifier.attribute),
        effectNumberContentHash(modifier.amount),
        modifier.percent,
        static_cast<int>(modifier.damageChannel),
        modifier.tracking,
        modifier.speedPct,
        modifier.projectilePressurePct,
        modifier.blockedDirection,
        static_cast<int>(modifier.overlap),
        modifier.trackingOverlap,
        modifier.speedOverlap,
        modifier.projectilePressureOverlap);
}

std::vector<ChessSha256> areaModifierContentHashes(
    const std::vector<AreaModifier>& modifiers)
{
    std::vector<ChessSha256> result;
    result.reserve(modifiers.size());
    std::ranges::transform(modifiers, std::back_inserter(result), areaModifierContentHash);
    return result;
}

ChessSha256 actionContentHash(const EffectAction& action);

std::vector<ChessSha256> actionContentHashes(const std::vector<EffectAction>& actions)
{
    std::vector<ChessSha256> result;
    result.reserve(actions.size());
    std::ranges::transform(actions, std::back_inserter(result), actionContentHash);
    return result;
}

ChessSha256 stateMachineContentHash(const StateMachineAction& machine)
{
    return std::visit(
        [&](const auto& typed)
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, ChangeStateValueAction>)
                return chessBeveSha256("KYS_EFFECT_STATE_MACHINE", machine.index(),
                    static_cast<int>(typed.slot), typed.delta, typed.minimum, typed.maximum);
            else if constexpr (std::is_same_v<T, TransferStateValueAction>)
                return chessBeveSha256("KYS_EFFECT_STATE_MACHINE", machine.index(),
                    static_cast<int>(typed.sourceSlot), static_cast<int>(typed.destinationSlot));
            else if constexpr (std::is_same_v<T, RecordMaximumDamageAction>)
                return chessBeveSha256("KYS_EFFECT_STATE_MACHINE", machine.index(),
                    static_cast<int>(typed.slot), static_cast<int>(typed.channel));
            else if constexpr (std::is_same_v<T, ConsumeRecordedMaximumAction>)
                return chessBeveSha256("KYS_EFFECT_STATE_MACHINE", machine.index(),
                    static_cast<int>(typed.slot), static_cast<int>(typed.destination),
                    typed.percent, typed.clearAfterConsume);
            else if constexpr (std::is_same_v<T, StartDamageAbsorptionAction>)
                return chessBeveSha256("KYS_EFFECT_STATE_MACHINE", machine.index(),
                    static_cast<int>(typed.slot), typed.absorbedPct, typed.durationFrames,
                    typed.settleOnSourceDeath, selectorContentHash(typed.settlementTarget),
                    static_cast<int>(typed.settlementDamageKind), typed.returnedPct);
            else if constexpr (std::is_same_v<T, SettleDamageAbsorptionAction>)
                return chessBeveSha256("KYS_EFFECT_STATE_MACHINE", machine.index(),
                    static_cast<int>(typed.slot), selectorContentHash(typed.target),
                    static_cast<int>(typed.damageKind), typed.returnedPct, typed.clearAfterSettle);
            else if constexpr (std::is_same_v<T, BorrowEffectRulesAction>)
                return chessBeveSha256("KYS_EFFECT_STATE_MACHINE", machine.index(),
                    selectorContentHash(typed.sourceUnits), effectNumberContentHash(typed.sourceCount),
                    typed.filter.allowedActionCategories, static_cast<int>(typed.propagation));
            else if constexpr (std::is_same_v<T, CopyAttackDefinitionAction>)
                return chessBeveSha256("KYS_EFFECT_STATE_MACHINE", machine.index(),
                    selectorContentHash(typed.sourceUnits), typed.filter.conditions,
                    typed.copyCount, static_cast<int>(typed.propagation));
            else if constexpr (std::is_same_v<T, SettleRemainingStatusDamageAction>)
                return chessBeveSha256("KYS_EFFECT_STATE_MACHINE", machine.index(), static_cast<int>(typed.status));
            else if constexpr (std::is_same_v<T, GenerateClonesAction>)
                return chessBeveSha256("KYS_EFFECT_STATE_MACHINE", machine.index(), typed.count);
            else if constexpr (std::is_same_v<T, PreventDeathAction>)
                return chessBeveSha256("KYS_EFFECT_STATE_MACHINE", machine.index(), typed.invincibilityFrames);
            else
                return chessBeveSha256("KYS_EFFECT_STATE_MACHINE", machine.index(),
                    static_cast<int>(typed.mode), typed.activations);
        },
        machine);
}

ChessSha256 actionContentHash(const EffectAction& action)
{
    return std::visit(
        [&](const auto& typed)
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, ModifyAttributeAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    static_cast<int>(typed.attribute), effectNumberContentHash(typed.amount),
                    static_cast<int>(typed.operation), typed.durationFrames,
                    static_cast<int>(typed.stack), typed.stackLimit,
                    static_cast<int>(typed.stackScope));
            else if constexpr (std::is_same_v<T, ModifyDamageAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    static_cast<int>(typed.perspective), static_cast<int>(typed.stage),
                    static_cast<int>(typed.channel), effectNumberContentHash(typed.amount),
                    static_cast<int>(typed.operation), typed.durationFrames,
                    static_cast<int>(typed.stack), typed.stackLimit,
                    static_cast<int>(typed.stackScope));
            else if constexpr (std::is_same_v<T, ChangeResourceAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    static_cast<int>(typed.resource), effectNumberContentHash(typed.amount),
                    static_cast<int>(typed.kind),
                    typed.transferDestination ? std::optional{ selectorContentHash(*typed.transferDestination) } : std::nullopt,
                    static_cast<int>(typed.healKind), static_cast<int>(typed.healSourcePolicy));
            else if constexpr (std::is_same_v<T, ModifyHealTransactionAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    static_cast<int>(typed.operation), typed.kinds, typed.percent);
            else if constexpr (std::is_same_v<T, ApplyStatusAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(), applyStatusContentHash(typed));
            else if constexpr (std::is_same_v<T, ConsumeStatusAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    static_cast<int>(typed.status), typed.quantity, static_cast<int>(typed.source),
                    typed.whenDepleted ? std::optional{ applyStatusContentHash(*typed.whenDepleted) } : std::nullopt);
            else if constexpr (std::is_same_v<T, ConsumeThisStatusAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    typed.quantity,
                    typed.whenDepleted ? std::optional{ applyStatusContentHash(*typed.whenDepleted) } : std::nullopt);
            else if constexpr (std::is_same_v<T, RemoveStatusAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    typed.statuses, static_cast<int>(typed.source),
                    typed.negativeOnly, typed.controlOnly,
                    typed.clearCurrentActionStagger, typed.count, static_cast<int>(typed.order));
            else if constexpr (std::is_same_v<T, SuppressCurrentCastContactsAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    typed.originalTargetShield
                        ? std::optional{ effectNumberContentHash(*typed.originalTargetShield) }
                        : std::nullopt);
            else if constexpr (std::is_same_v<T, MakeIncomingAttackMissAction>
                || std::is_same_v<T, BlockPositiveDamageAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index());
            else if constexpr (std::is_same_v<T, DealDamageAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    effectNumberContentHash(typed.amount),
                    typed.transactionCount ? std::optional{ effectNumberContentHash(*typed.transactionCount) } : std::nullopt,
                    static_cast<int>(typed.kind), typed.appliesDamageModifiers,
                    typed.triggersHurtInvincibility, static_cast<int>(typed.area.kind),
                    typed.area.radiusTiles, typed.area.squareSideTiles, typed.perCast.perTargetLimit,
                    typed.areaProjectiles ? std::optional{ chessBeveSha256("KYS_EFFECT_AREA_PROJECTILES",
                        typed.areaProjectiles->rangeTiles, typed.areaProjectiles->maximumTargets,
                        typed.areaProjectiles->stunFrames, typed.areaProjectiles->trackEventSource,
                        static_cast<int>(typed.areaProjectiles->visual)) } : std::nullopt);
            else if constexpr (std::is_same_v<T, ModifyAttackAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    attackPatternContentHash(typed.pattern), typed.strengthPct,
                    typed.through, typed.tracking, typed.mainProjectile,
                    typed.sameTargetHitLimit, static_cast<int>(typed.targets),
                    static_cast<int>(typed.propagation), typed.addToBaseAttack,
                    typed.source ? std::optional{ selectorContentHash(*typed.source) } : std::nullopt,
                    typed.damageOverride ? std::optional{ effectNumberContentHash(*typed.damageOverride) } : std::nullopt,
                    typed.damageKind, attackRuntimeBehaviorContentHash(typed.runtimeBehavior));
            else if constexpr (std::is_same_v<T, ForceMoveAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    static_cast<int>(typed.direction), typed.distanceTiles, typed.distancePixels,
                    typed.lockFrames, static_cast<int>(typed.collision), static_cast<int>(typed.blocked));
            else if constexpr (std::is_same_v<T, CreateAreaAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    static_cast<int>(typed.shape), typed.radiusTiles, typed.squareSideTiles,
                    static_cast<int>(typed.anchor), typed.durationFrames,
                    static_cast<int>(typed.sourceDeath), static_cast<int>(typed.merge),
                    areaModifierContentHashes(typed.modifiers));
            else if constexpr (std::is_same_v<T, ModifyCastAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    typed.mpCost ? std::optional{ effectNumberContentHash(*typed.mpCost) } : std::nullopt,
                    typed.rangeMode, typed.projectileSpeedPct, typed.minimumSelectDistance,
                    typed.additionalProjectiles, static_cast<int>(typed.mobility),
                    typed.autoUltimate ? std::optional{ chessBeveSha256("KYS_EFFECT_AUTO_CAST",
                        typed.autoUltimate->consumeMp, typed.autoUltimate->announce) } : std::nullopt,
                    typed.replacementPattern ? std::optional{ attackPatternContentHash(*typed.replacementPattern) } : std::nullopt,
                    typed.freeAdditionalCast, static_cast<int>(typed.propagation));
            else if constexpr (std::is_same_v<T, StateMachineAction>)
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(), stateMachineContentHash(typed));
            else
            {
                assert(typed);
                return chessBeveSha256("KYS_EFFECT_ACTION", action.value.index(),
                    conditionContentHashes(typed->conditions),
                    actionContentHashes(typed->whenTrue),
                    actionContentHashes(typed->whenFalse));
            }
        },
        action.value);
}

using RuleContentView = std::tuple<
    std::uint64_t,
    int,
    int,
    int,
    ChessSha256,
    std::vector<ChessSha256>,
    int,
    int,
    int,
    int,
    int,
    std::optional<std::pair<int, int>>,
    std::optional<ChessSha256>,
    std::vector<ChessSha256>>;
using NonBattleRuleContentView = std::tuple<int, std::string, int, int, int>;

std::vector<RuleContentView> effectRuleContentViews(
    const std::vector<EffectRule>& rules)
{
    std::vector<RuleContentView> result;
    result.reserve(rules.size());
    for (const auto& rule : rules)
    {
        result.emplace_back(
            rule.id.value,
            static_cast<int>(rule.event),
            static_cast<int>(rule.observation),
            static_cast<int>(rule.castMatch),
            selectorContentHash(rule.selector),
            conditionContentHashes(rule.conditions),
            rule.chancePct,
            rule.maxActivations,
            rule.sharedCooldownFrames,
            rule.intervalFrames,
            rule.everyNthEvent,
            rule.activationLimit
                ? std::optional{ std::pair{
                    static_cast<int>(rule.activationLimit->scope),
                    rule.activationLimit->maxEvaluations } }
                : std::nullopt,
            rule.repetitionCount
                ? std::optional{ effectNumberContentHash(*rule.repetitionCount) }
                : std::nullopt,
            actionContentHashes(rule.actions));
    }
    return result;
}

ChessSha256 statusBehaviorContentHash(const StatusBehaviorDefinition& behavior)
{
    return chessBeveSha256(
        "KYS_STATUS_BEHAVIOR",
        effectRuleContentViews(behavior.rules));
}

NonBattleRuleContentView nonBattleRuleContentView(const ChessNonBattleRule& rule)
{
    return std::visit(
        [](const auto& typed) -> NonBattleRuleContentView
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, CountsAsComboRule>)
            {
                return {
                    static_cast<int>(ChessNonBattleRuleKind::CountsAsCombo),
                    typed.comboName,
                    0,
                    0,
                    0,
                };
            }
            else if constexpr (std::is_same_v<T, VictoryGoldRule>)
            {
                return {
                    static_cast<int>(ChessNonBattleRuleKind::VictoryGold),
                    {},
                    typed.perHighestSurvivorStar,
                    0,
                    0,
                };
            }
            else if constexpr (std::is_same_v<T, FreeShopRefreshRule>)
            {
                return {
                    static_cast<int>(ChessNonBattleRuleKind::FreeShopRefresh),
                    {},
                    0,
                    0,
                    0,
                };
            }
            else if constexpr (std::is_same_v<T, BattleMapChoiceRule>)
            {
                return {
                    static_cast<int>(ChessNonBattleRuleKind::BattleMapChoice),
                    {},
                    0,
                    0,
                    0,
                };
            }
            else
            {
                static_assert(std::is_same_v<T, FightWinGrowthRule>);
                return {
                    static_cast<int>(ChessNonBattleRuleKind::FightWinGrowth),
                    {},
                    typed.maxHp,
                    typed.attack,
                    typed.defence,
                };
            }
        },
        rule);
}

std::vector<NonBattleRuleContentView> nonBattleRuleContentViews(
    const std::vector<ChessNonBattleRule>& rules)
{
    std::vector<NonBattleRuleContentView> result;
    result.reserve(rules.size());
    std::ranges::transform(rules, std::back_inserter(result), nonBattleRuleContentView);
    return result;
}

auto comboContentViews(const std::vector<ComboDef>& definitions)
{
    using ThresholdView = std::tuple<
        int,
        std::string,
        std::vector<RuleContentView>,
        std::vector<NonBattleRuleContentView>>;
    using DefinitionView = std::tuple<
        int,
        std::string,
        std::vector<int>,
        std::vector<ThresholdView>,
        bool,
        bool>;
    std::vector<DefinitionView> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions)
    {
        std::vector<ThresholdView> thresholds;
        thresholds.reserve(definition.thresholds.size());
        for (const auto& threshold : definition.thresholds)
        {
            thresholds.emplace_back(
                threshold.count,
                threshold.name,
                effectRuleContentViews(threshold.rules),
                nonBattleRuleContentViews(threshold.managementRules));
        }
        result.emplace_back(
            definition.id,
            definition.name,
            definition.memberRoleIds,
            std::move(thresholds),
            definition.isAntiCombo,
            definition.starSynergyBonus);
    }
    return result;
}

auto equipmentContentViews(const std::vector<EquipmentDef>& definitions)
{
    using DefinitionView = std::tuple<
        int,
        int,
        int,
        std::vector<RuleContentView>,
        std::vector<NonBattleRuleContentView>>;
    std::vector<DefinitionView> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions)
    {
        result.emplace_back(
            definition.itemId,
            definition.tier,
            definition.equipType,
            effectRuleContentViews(definition.rules),
            nonBattleRuleContentViews(definition.managementRules));
    }
    return result;
}

auto equipmentSynergyContentViews(
    const std::vector<EquipmentSynergyDef>& definitions)
{
    using DefinitionView = std::tuple<
        std::vector<int>,
        int,
        std::vector<RuleContentView>,
        std::vector<NonBattleRuleContentView>>;
    std::vector<DefinitionView> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions)
    {
        result.emplace_back(
            definition.roleIds,
            definition.equipmentId,
            effectRuleContentViews(definition.rules),
            nonBattleRuleContentViews(definition.managementRules));
    }
    return result;
}

auto neigongContentViews(const std::vector<NeigongDef>& definitions)
{
    using DefinitionView = std::tuple<
        int,
        int,
        int,
        std::string,
        std::vector<RuleContentView>>;
    std::vector<DefinitionView> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions)
    {
        result.emplace_back(
            definition.itemId,
            definition.magicId,
            definition.tier,
            definition.name,
            effectRuleContentViews(definition.rules));
    }
    return result;
}

auto magicEffectContentViews(const std::vector<ChessMagicEffectDefinition>& definitions)
{
    using DefinitionView = std::tuple<
        int,
        std::vector<RuleContentView>>;
    std::vector<DefinitionView> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions)
    {
        result.emplace_back(
            definition.magicId,
            effectRuleContentViews(definition.rules));
    }
    return result;
}

ChessSha256 chessContentFingerprint(const ChessGameContentData& data)
{
    const auto roles = roleContentViews(data.roles);
    const auto magics = magicContentViews(data.magics);
    const auto combos = comboContentViews(data.combos);
    const auto equipment = equipmentContentViews(data.equipment);
    const auto equipmentSynergies = equipmentSynergyContentViews(
        data.equipmentSynergies);
    const auto neigong = neigongContentViews(data.neigong);
    const auto magicEffects = magicEffectContentViews(data.magicEffects);
    return chessBeveSha256(
        "KYS_CHESS_CONTENT",
        data.difficulty,
        data.balance,
        roles,
        magics,
        data.items,
        data.poolRoleIds,
        combos,
        equipment,
        equipmentSynergies,
        data.neigongConfig,
        neigong,
        magicEffects,
        data.battleMaps,
        data.battlefields);
}

}

ChessSha256 statusBehaviorContentFingerprint(
    const StatusBehaviorDefinition& behavior)
{
    return statusBehaviorContentHash(behavior);
}

bool statusBehaviorsEquivalent(
    const std::shared_ptr<const StatusBehaviorDefinition>& lhs,
    const std::shared_ptr<const StatusBehaviorDefinition>& rhs)
{
    if (!lhs || !rhs) return lhs == rhs;
    return statusBehaviorContentHash(*lhs) == statusBehaviorContentHash(*rhs);
}

ChessGameContent::ChessGameContent(ChessGameContentData data, std::string gameVersion)
    : data_(std::make_shared<const ChessGameContentData>(std::move(data))),
      contentFingerprint_(chessContentFingerprint(*data_)),
      gameVersion_(std::move(gameVersion))
{
}

std::shared_ptr<const ChessGameContent> ChessGameContent::withGameVersion(
    std::string gameVersion) const
{
    auto result = std::make_shared<ChessGameContent>(*this);
    result->gameVersion_ = std::move(gameVersion);
    return result;
}

const ChessRoleDefinition* ChessGameContent::role(int roleId) const
{
    const auto found = data_->roles.find(roleId);
    return found == data_->roles.end() ? nullptr : &found->second;
}

const ChessMagicDefinition* ChessGameContent::magic(int magicId) const
{
    const auto found = data_->magics.find(magicId);
    return found == data_->magics.end() ? nullptr : &found->second;
}

const ChessItemDefinition* ChessGameContent::item(int itemId) const
{
    const auto found = data_->items.find(itemId);
    return found == data_->items.end() ? nullptr : &found->second;
}

std::vector<std::pair<const ChessMagicDefinition*, int>> chessRoleMagicsForStar(
    const ChessGameContent& content,
    const ChessRoleDefinition& role,
    int star)
{
    std::vector<std::pair<const ChessMagicDefinition*, int>> result;
    for (int index = RoleSave::getMagicSlotStart(star);
         index < RoleSave::getMagicSlotEnd(star);
         ++index)
    {
        if (const auto* magic = content.magic(role.MagicID[index]))
        {
            result.emplace_back(magic, role.MagicPower[index]);
        }
    }
    std::ranges::sort(result, [](const auto& lhs, const auto& rhs) {
        return std::tuple{lhs.second, lhs.first->ID}
            < std::tuple{rhs.second, rhs.first->ID};
    });
    return result;
}

}
