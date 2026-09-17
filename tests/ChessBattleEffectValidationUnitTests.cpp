#include "ChessBattleEffectTestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace KysChess;
using namespace KysChess::Test;

TEST_CASE("ChessBattleEffects_FullMpHealingRequiresAnHpRestoreAction",
          "[battle][effects][schema][healing]")
{
    auto rule = ruleWithEvent(contractMagicDefinition(133), EffectEvent::AttackCommitted);
    auto& heal = std::get<ChangeResourceAction>(rule.actions.at(1).value);
    CHECK(heal.healRequiresFullMp);
    std::string error;
    REQUIRE(validateEffectRule(rule, error));
    heal.resource = BattleResource::Mp;
    CHECK_FALSE(validateEffectRule(rule, error));
    heal.resource = BattleResource::Hp;
    heal.kind = ResourceChangeKind::Remove;
    CHECK_FALSE(validateEffectRule(rule, error));
}

TEST_CASE("ChessBattleEffects_ProjectileSweepRequiresAtLeastHitRadius",
          "[battle][effects][schema][projectile_sweep]")
{
    auto rule = ruleWithEvent(contractMagicDefinition(75), EffectEvent::AttackCommitted);
    auto& action = std::get<ModifyAttackAction>(rule.actions.front().value);
    for (const int radius : { -1, 1, 99, 0, 100, 150, 200 })
    {
        CAPTURE(radius);
        action.projectileClearRadiusPct = radius;
        std::string error;
        CHECK(validateEffectRule(rule, error) == (radius == 0 || radius >= 100));
    }
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

TEST_CASE("ChessBattleEffects_DamageDescendantEventsExposeOptionalCastProvenance",
          "[battle][effects][magic][schema][damage][provenance]")
{
    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 1;

    for (const auto event : {
             EffectEvent::DamageResolved,
             EffectEvent::ShieldBroken,
             EffectEvent::UnitDied,
             EffectEvent::AllyDied,
         })
    {
        EffectRule rule;
        rule.id = EffectRuleId{ 1 };
        rule.event = event;
        rule.selector.kind = EffectSelectorKind::Self;
        rule.conditions = { IsUltimateCondition{} };
        rule.actions = { EffectAction{ EffectActionValue{ shield } } };

        std::string error;
        CAPTURE(event, error);
        CHECK(validateEffectRule(rule, error));
    }
}

TEST_CASE("ChessBattleEffects_SourceFilteredGenericNegativeCleanseIsAuthorable",
          "[battle][effects][magic][schema][status][cleanse]")
{
    RemoveStatusAction cleanse;
    cleanse.negativeOnly = true;
    cleanse.source = StatusSourceMatch::EffectOwner;
    cleanse.count = 1;

    EffectRule rule;
    rule.id = EffectRuleId{ 1 };
    rule.event = EffectEvent::UltimateCommitted;
    rule.selector.kind = EffectSelectorKind::Self;
    rule.actions = { EffectAction{ EffectActionValue{ cleanse } } };

    std::string error;
    CHECK(validateEffectRule(rule, error));

    cleanse.negativeOnly = false;
    rule.actions = { EffectAction{ EffectActionValue{ cleanse } } };
    CHECK_FALSE(validateEffectRule(rule, error));
    CHECK(error == "移除狀態需要狀態列表或篩選條件");
}

TEST_CASE("ChessBattleEffects_IntrinsicRuleIdsRequireTheTrustedRuntimeContext",
          "[battle][effects][status][validation][intrinsic]")
{
    EffectRule rule;
    rule.id = EffectRuleId{ IntrinsicEffectRuleIdMask | 1 };
    rule.event = EffectEvent::FrameAdvanced;
    rule.observation = EffectObservationScope::StatusHolderEventSource;
    rule.selector.kind = EffectSelectorKind::StatusHolder;
    DealDamageAction damage;
    damage.amount.flat = 1;
    damage.kind = BattleDamageKind::Bleed;
    rule.actions = { EffectAction{ damage } };

    std::string error;
    CHECK_FALSE(validateEffectRule(
        rule,
        error,
        EffectRuleAuthoringContext::Configured));
    CHECK(error == "作者規則 ID 不可使用保留的內建狀態識別空間");
    CHECK_FALSE(validateEffectRule(
        rule,
        error,
        EffectRuleAuthoringContext::StatusBehavior));
    CHECK(error == "作者規則 ID 不可使用保留的內建狀態識別空間");
    CHECK(validateEffectRule(
        rule,
        error,
        EffectRuleAuthoringContext::RuntimeIntrinsicStatusBehavior));

    rule.id = EffectRuleId{ 1 };
    CHECK_FALSE(validateEffectRule(
        rule,
        error,
        EffectRuleAuthoringContext::RuntimeIntrinsicStatusBehavior));
    CHECK(error == "內建狀態效果驗證只接受保留的內建規則 ID");
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
TEST_CASE("ChessBattleEffects_TypedStatusDiagnosticsNameTheStatusAndQuantity",
          "[battle][effects][status][diagnostic]")
{
    ApplyStatusAction sevenStar;
    sevenStar.status = BattleStatusKind::SevenStarMark;
    sevenStar.durationFrames = 150;
    sevenStar.quantity = AddStatusLayers{ 7, 7 };

    EffectRule rule;
    rule.id = EffectRuleId{ 7300 };
    rule.event = EffectEvent::MainProjectileBeforeDamage;
    rule.selector.kind = EffectSelectorKind::HitTarget;
    rule.actions = { EffectAction{ sevenStar } };

    std::string error;
    CHECK_FALSE(validateEffectRule(rule, error));
    CHECK(error == "狀態「七星」必須使用「設定印記層數」");
}
TEST_CASE("ChessBattleEffects_StatusInterceptorsHaveClosedEventsAndCannotHideSiblingActions",
    "[battle][effects][status][interceptor][validation]")
{
    const auto validates = [](EffectEvent event, std::vector<EffectAction> actions)
    {
        EffectRule rule;
        rule.id = EffectRuleId{ 2900 };
        rule.event = event;
        rule.observation = event == EffectEvent::HealAttempted
                || event == EffectEvent::HealApplied
            ? EffectObservationScope::StatusHolderEventTarget
            : EffectObservationScope::StatusHolderEventSource;
        rule.selector.kind = EffectSelectorKind::StatusHolder;
        rule.actions = std::move(actions);
        std::string error;
        const bool valid = validateEffectRule(
            rule,
            error,
            EffectRuleAuthoringContext::StatusBehavior);
        return std::pair{ valid, error };
    };

    CHECK(validates(
        EffectEvent::HitBeforeDamage,
        { EffectAction{ SuppressCurrentCastContactsAction{} } }).first);
    CHECK_FALSE(validates(
        EffectEvent::HealApplied,
        { EffectAction{ SuppressCurrentCastContactsAction{} } }).first);
    CHECK_FALSE(validates(
        EffectEvent::DamageResolved,
        { EffectAction{ MakeIncomingAttackMissAction{} } }).first);
    CHECK(validates(
        EffectEvent::StatusPersistent,
        { EffectAction{ BlockPositiveDamageAction{} } }).first);
    CHECK_FALSE(validates(
        EffectEvent::HitBeforeDamage,
        { EffectAction{ BlockPositiveDamageAction{} } }).first);
    CHECK(validates(
        EffectEvent::HealAttempted,
        { EffectAction{ ConsumeThisStatusAction{} } }).first);

    auto invalidPhaseConditional = std::make_shared<ConditionalEffectAction>();
    invalidPhaseConditional->conditions = { TargetIsStatusHolderCondition{} };
    invalidPhaseConditional->whenTrue = {
        EffectAction{ MakeIncomingAttackMissAction{} },
    };
    CHECK_FALSE(validates(
        EffectEvent::HealApplied,
        { EffectAction{ invalidPhaseConditional } }).first);

    const auto mixed = validates(
        EffectEvent::HitBeforeDamage,
        {
            EffectAction{ SuppressCurrentCastContactsAction{} },
            EffectAction{ ConsumeThisStatusAction{} },
        });
    CHECK_FALSE(mixed.first);
    CHECK(mixed.second
        == "攻擊落空攔截動作必須是規則中唯一且直接指定的動作；成功後效果請寫在攔截動作內");

    CHECK_FALSE(validates(
        EffectEvent::HitBeforeDamage,
        {
            EffectAction{ SuppressCurrentCastContactsAction{} },
            EffectAction{ MakeIncomingAttackMissAction{} },
        }).first);

    auto conditionalInterceptor = std::make_shared<ConditionalEffectAction>();
    conditionalInterceptor->conditions = { TargetIsStatusHolderCondition{} };
    conditionalInterceptor->whenTrue = {
        EffectAction{ MakeIncomingAttackMissAction{} },
    };
    CHECK_FALSE(validates(
        EffectEvent::HitBeforeDamage,
        { EffectAction{ conditionalInterceptor } }).first);

    auto mixedConditional = std::make_shared<ConditionalEffectAction>();
    mixedConditional->conditions = { TargetIsStatusHolderCondition{} };
    mixedConditional->whenTrue = {
        EffectAction{ MakeIncomingAttackMissAction{} },
    };
    mixedConditional->whenFalse = {
        EffectAction{ ConsumeThisStatusAction{} },
    };
    CHECK_FALSE(validates(
        EffectEvent::HitBeforeDamage,
        { EffectAction{ mixedConditional } }).first);
}

TEST_CASE("ChessBattleEffects_PersistentStatusRulesAreClosedQueryDefinitions",
          "[battle][effects][status][persistent][validation]")
{
    EffectRule valid;
    valid.id = EffectRuleId{ 2910 };
    valid.event = EffectEvent::StatusPersistent;
    valid.observation = EffectObservationScope::StatusHolderEventSource;
    valid.selector.kind = EffectSelectorKind::StatusHolder;
    valid.actions = { EffectAction{ BlockPositiveDamageAction{} } };
    std::string error;
    REQUIRE(validateEffectRule(
        valid,
        error,
        EffectRuleAuthoringContext::StatusBehavior));

    const auto rejects = [&](EffectRule rule)
    {
        error.clear();
        CHECK_FALSE(validateEffectRule(
            rule,
            error,
            EffectRuleAuthoringContext::StatusBehavior));
        CHECK_FALSE(error.empty());
    };

    auto chance = valid;
    chance.chancePct = 50;
    rejects(std::move(chance));

    auto condition = valid;
    condition.conditions = { TargetIsStatusHolderCondition{} };
    rejects(std::move(condition));

    auto selector = valid;
    selector.selector.kind = EffectSelectorKind::Self;
    rejects(std::move(selector));

    auto observation = valid;
    observation.observation = EffectObservationScope::StatusHolderEventTarget;
    rejects(std::move(observation));

    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 1;
    auto unsupportedAction = valid;
    unsupportedAction.actions = { EffectAction{ shield } };
    rejects(std::move(unsupportedAction));

    ModifyAttributeAction eventLive;
    eventLive.attribute = BattleAttribute::Speed;
    eventLive.operation = AttributeOperation::PercentAdd;
    eventLive.amount.base = EffectNumberBase::TargetMaxHp;
    eventLive.amount.percent = 1;
    auto eventLiveBase = valid;
    eventLiveBase.actions = { EffectAction{ eventLive } };
    rejects(std::move(eventLiveBase));

    eventLive.amount.base = EffectNumberBase::ApplicationTargetMaxHp;
    eventLive.amount.multiplierBase = EffectNumberBase::TargetCurrentHp;
    auto eventLiveMultiplier = valid;
    eventLiveMultiplier.actions = { EffectAction{ eventLive } };
    rejects(std::move(eventLiveMultiplier));

    eventLive.amount.multiplierBase.reset();
    auto applicationBound = valid;
    applicationBound.actions = { EffectAction{ eventLive } };
    CHECK(validateEffectRule(
        applicationBound,
        error,
        EffectRuleAuthoringContext::StatusBehavior));

    ModifyAttributeAction attack;
    attack.attribute = BattleAttribute::Attack;
    attack.operation = AttributeOperation::PercentAdd;
    attack.amount.flat = 10;
    auto unsupportedAttribute = valid;
    unsupportedAttribute.actions = { EffectAction{ attack } };
    rejects(std::move(unsupportedAttribute));

    ModifyAttributeAction localLifetime;
    localLifetime.attribute = BattleAttribute::Speed;
    localLifetime.operation = AttributeOperation::PercentAdd;
    localLifetime.amount.flat = -10;
    localLifetime.durationFrames = 30;
    auto lifetime = valid;
    lifetime.actions = { EffectAction{ localLifetime } };
    rejects(std::move(lifetime));

    ModifyDamageAction unsupportedDamage;
    unsupportedDamage.perspective = DamageModifierPerspective::Outgoing;
    unsupportedDamage.stage = DamageModifierStage::Final;
    unsupportedDamage.channel = DamageChannel::All;
    unsupportedDamage.operation = DamageModifierOperation::PercentAdd;
    unsupportedDamage.amount.flat = 10;
    auto damage = valid;
    damage.actions = { EffectAction{ unsupportedDamage } };
    rejects(std::move(damage));
}

TEST_CASE("ChessBattleEffects_StatusOnlyNumbersCannotEscapeTheirContributionContext",
          "[battle][effects][status][number][validation]")
{
    EffectRule configured;
    configured.id = EffectRuleId{ 2911 };
    configured.event = EffectEvent::HitBeforeDamage;
    configured.selector.kind = EffectSelectorKind::HitTarget;

    DealDamageAction damage;
    damage.kind = BattleDamageKind::Pure;
    damage.amount.flat = 9;
    damage.amount.statusScale = StatusNumberScale::PerContributionLayer;
    configured.actions = { EffectAction{ damage } };

    std::string error;
    CHECK_FALSE(validateEffectRule(configured, error));
    CHECK(error == "「每層數值」只能用於層數狀態的效果規則");

    damage.amount = {};
    damage.amount.base = EffectNumberBase::CurrentContributionQuantity;
    damage.amount.percent = 100;
    configured.actions = { EffectAction{ damage } };
    CHECK_FALSE(validateEffectRule(configured, error));
    CHECK(error == "數值基準「此狀態貢獻數量」只能用於狀態效果規則");

    damage.amount.base = EffectNumberBase::ApplicationTargetMaxHp;
    damage.amount.percent = 3;
    configured.actions = { EffectAction{ damage } };
    CHECK_FALSE(validateEffectRule(configured, error));
    CHECK(error == "數值基準「套用目標最大生命」只能用於狀態效果規則");
}