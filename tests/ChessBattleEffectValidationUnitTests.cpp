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
    std::vector<ChessMagicEffectDefinition> definitions;
    REQUIRE(loadMagicEffectsFile("config/chess_magic_effects.yaml", definitions));
    auto rule = ruleWithEvent(definitionWithId(definitions, 133), EffectEvent::AttackCommitted);
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
    std::vector<ChessMagicEffectDefinition> definitions;
    REQUIRE(loadMagicEffectsFile("config/chess_magic_effects.yaml", definitions));
    auto rule = ruleWithEvent(definitionWithId(definitions, 75), EffectEvent::AttackCommitted);
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

TEST_CASE("ChessBattleEffects_EveryAuthorableStatusHasAValidFixtureAndEveryProfileHasANegativeFixture",
          "[battle][effects][status][profile][catalog][coverage]")
{
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content);

    std::vector<std::vector<EffectRule>> authoredRuleSets;
    for (const auto& definition : content->magicEffects())
        authoredRuleSets.push_back(definition.rules);
    for (const auto& combo : content->combos())
        for (const auto& threshold : combo.thresholds)
            authoredRuleSets.push_back(threshold.rules);
    for (const auto& equipment : content->equipment())
        authoredRuleSets.push_back(equipment.rules);
    for (const auto& synergy : content->equipmentSynergies())
        authoredRuleSets.push_back(synergy.rules);
    for (const auto& neigong : content->neigong())
        authoredRuleSets.push_back(neigong.rules);

    const auto findInActions = [](
        const auto& self,
        std::vector<EffectAction>& actions,
        BattleStatusKind status) -> ApplyStatusAction*
    {
        for (auto& effectAction : actions)
        {
            if (auto* application = std::get_if<ApplyStatusAction>(
                    &effectAction.value);
                application && application->status == status)
            {
                return application;
            }
            auto* conditional = std::get_if<std::shared_ptr<ConditionalEffectAction>>(
                &effectAction.value);
            if (!conditional) continue;
            REQUIRE(*conditional);
            if (auto* found = self(self, (*conditional)->whenTrue, status))
                return found;
            if (auto* found = self(self, (*conditional)->whenFalse, status))
                return found;
        }
        return nullptr;
    };
    const auto findApplication = [&](
        std::vector<EffectRule>& rules,
        BattleStatusKind status) -> ApplyStatusAction*
    {
        for (auto& rule : rules)
        {
            if (auto* found = findInActions(findInActions, rule.actions, status))
                return found;
        }
        return nullptr;
    };

    std::size_t expectedProfileCount{};
    std::size_t expectedAuthorableCount{};
    std::set<BattleStatusKind> covered;
    const auto coldPoisonFixture = std::ranges::find_if(
        authoredRuleSets,
        [&](auto& rules)
        {
            return findApplication(rules, BattleStatusKind::ColdPoison) != nullptr;
        });
    REQUIRE(coldPoisonFixture != authoredRuleSets.end());
    const auto* coldPoison = findApplication(
        *coldPoisonFixture, BattleStatusKind::ColdPoison);
    REQUIRE(coldPoison);
    REQUIRE(coldPoison->behavior);
    REQUIRE_FALSE(coldPoison->behavior->rules.empty());
    EffectRule unrelatedBehavior = coldPoison->behavior->rules.front();
    std::erase_if(unrelatedBehavior.actions, [](const EffectAction& action)
    {
        return !std::holds_alternative<ModifyAttributeAction>(action.value);
    });
    REQUIRE_FALSE(unrelatedBehavior.actions.empty());
    for (const auto& catalog : statusCatalogEntries())
    {
        if (!catalog.authorable) continue;
        ++expectedAuthorableCount;
        CAPTURE(catalog.label);

        const auto fixture = std::ranges::find_if(
            authoredRuleSets,
            [&](auto& rules)
            {
                return findApplication(rules, catalog.status) != nullptr;
            });
        REQUIRE(fixture != authoredRuleSets.end());
        std::string error;
        CHECK(validateEffectRules(*fixture, error));
        INFO(error);
        CHECK(covered.insert(catalog.status).second);

        if (catalog.behaviorClassification
            != StatusBehaviorClassification::Profiled)
        {
            continue;
        }
        ++expectedProfileCount;

        auto incomplete = *fixture;
        auto* application = findApplication(incomplete, catalog.status);
        REQUIRE(application);
        REQUIRE(application->behavior);
        auto missingCapability = std::make_shared<StatusBehaviorDefinition>(
            *application->behavior);
        missingCapability->rules = { unrelatedBehavior };
        application->behavior = std::move(missingCapability);
        error.clear();
        CHECK_FALSE(validateEffectRules(incomplete, error));
        CHECK(error.find("缺少必要能力") != std::string::npos);
    }

    CHECK(expectedProfileCount == static_cast<std::size_t>(std::ranges::count_if(
        statusCatalogEntries(),
        [](const StatusCatalogEntry& entry)
        {
            return entry.behaviorClassification
                == StatusBehaviorClassification::Profiled;
        })));
    CHECK(expectedAuthorableCount == static_cast<std::size_t>(std::ranges::count_if(
        statusCatalogEntries(),
        [](const StatusCatalogEntry& entry)
        {
            return entry.authorable;
        })));
    CHECK(covered.size() == expectedAuthorableCount);
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

TEST_CASE("ChessBattleEffects_StatusBehaviorProfilesAreContributionLocal",
          "[battle][effects][status][behavior][profile]")
{
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content);
    const auto validates = [](const std::vector<EffectRule>& rules)
    {
        std::string error;
        const bool valid = validateEffectRules(rules, error);
        INFO(error);
        return valid;
    };
    const auto producerFor = [](std::vector<EffectRule>& rules, BattleStatusKind status)
        -> EffectRule&
    {
        const auto found = std::ranges::find_if(rules, [&](const EffectRule& rule)
        {
            return std::ranges::any_of(rule.actions, [&](const EffectAction& action)
            {
                const auto* apply = std::get_if<ApplyStatusAction>(&action.value);
                return apply && apply->status == status;
            });
        });
        REQUIRE(found != rules.end());
        return *found;
    };
    const auto applicationFor = [&](std::vector<EffectRule>& rules, BattleStatusKind status)
        -> ApplyStatusAction&
    {
        auto& producer = producerFor(rules, status);
        const auto found = std::ranges::find_if(
            producer.actions,
            [&](const EffectAction& action)
            {
                const auto* applied = std::get_if<ApplyStatusAction>(&action.value);
                return applied && applied->status == status;
            });
        REQUIRE(found != producer.actions.end());
        return std::get<ApplyStatusAction>(found->value);
    };
    const auto mutateBehavior = [](
        ApplyStatusAction& application,
        const auto& mutation)
    {
        REQUIRE(application.behavior);
        auto behavior = std::make_shared<StatusBehaviorDefinition>(*application.behavior);
        mutation(behavior->rules);
        application.behavior = std::move(behavior);
    };

    const auto& sevenStarDefinition = definitionWithId(content->magicEffects(), 39);
    const auto& poisonExplosionDefinition = definitionWithId(content->magicEffects(), 95);
    const auto& neutralizeForceDefinition = definitionWithId(content->magicEffects(), 7);
    const auto& blindedDefinition = definitionWithId(content->magicEffects(), 48);
    CHECK(validates(sevenStarDefinition.rules));
    CHECK(validates(poisonExplosionDefinition.rules));
    CHECK(validates(neutralizeForceDefinition.rules));
    CHECK(validates(blindedDefinition.rules));

    auto wrongNeutralizeForceObservation = neutralizeForceDefinition.rules;
    mutateBehavior(
        applicationFor(
            wrongNeutralizeForceObservation,
            BattleStatusKind::NeutralizeForce),
        [](std::vector<EffectRule>& behaviorRules)
        {
            REQUIRE(behaviorRules.size() == 1);
            behaviorRules.front().observation
                = EffectObservationScope::StatusHolderEventTarget;
        });
    CHECK_FALSE(validates(wrongNeutralizeForceObservation));

    auto wrongBlindedObservation = blindedDefinition.rules;
    mutateBehavior(
        applicationFor(wrongBlindedObservation, BattleStatusKind::Blinded),
        [](std::vector<EffectRule>& behaviorRules)
        {
            REQUIRE(behaviorRules.size() == 1);
            behaviorRules.front().observation
                = EffectObservationScope::StatusHolderEventTarget;
        });
    CHECK_FALSE(validates(wrongBlindedObservation));

    auto duplicateSevenStar = sevenStarDefinition.rules;
    auto duplicateSevenStarProducer = producerFor(
        duplicateSevenStar, BattleStatusKind::SevenStarMark);
    duplicateSevenStarProducer.id = EffectRuleId{ 70001 };
    duplicateSevenStar.push_back(std::move(duplicateSevenStarProducer));
    CHECK(validates(duplicateSevenStar));

    auto missingSevenStarBehavior = sevenStarDefinition.rules;
    applicationFor(
        missingSevenStarBehavior,
        BattleStatusKind::SevenStarMark).behavior.reset();
    CHECK_FALSE(validates(missingSevenStarBehavior));

    auto wrongSevenStarProfile = sevenStarDefinition.rules;
    auto poisonProfileSource = poisonExplosionDefinition.rules;
    applicationFor(
        wrongSevenStarProfile,
        BattleStatusKind::SevenStarMark).behavior = applicationFor(
            poisonProfileSource,
            BattleStatusKind::PoisonExplosion).behavior;
    CHECK_FALSE(validates(wrongSevenStarProfile));

    auto missingSevenStarHolderConstraint = sevenStarDefinition.rules;
    mutateBehavior(
        applicationFor(missingSevenStarHolderConstraint, BattleStatusKind::SevenStarMark),
        [](std::vector<EffectRule>& behaviorRules)
        {
            REQUIRE(behaviorRules.size() == 1);
            behaviorRules.front().conditions.clear();
        });
    CHECK_FALSE(validates(missingSevenStarHolderConstraint));

    auto wrongSevenStarObservation = sevenStarDefinition.rules;
    mutateBehavior(
        applicationFor(wrongSevenStarObservation, BattleStatusKind::SevenStarMark),
        [](std::vector<EffectRule>& behaviorRules)
        {
            REQUIRE(behaviorRules.size() == 1);
            behaviorRules.front().observation
                = EffectObservationScope::StatusHolderEventSource;
        });
    CHECK_FALSE(validates(wrongSevenStarObservation));

    auto missingSevenStarDefenseModification = sevenStarDefinition.rules;
    mutateBehavior(
        applicationFor(
            missingSevenStarDefenseModification,
            BattleStatusKind::SevenStarMark),
        [](std::vector<EffectRule>& behaviorRules)
        {
            REQUIRE(behaviorRules.front().actions.size() == 2);
            behaviorRules.front().actions.erase(behaviorRules.front().actions.begin());
        });
    CHECK_FALSE(validates(missingSevenStarDefenseModification));

    auto wrongSevenStarConsumption = sevenStarDefinition.rules;
    mutateBehavior(
        applicationFor(wrongSevenStarConsumption, BattleStatusKind::SevenStarMark),
        [](std::vector<EffectRule>& behaviorRules)
        {
            auto& consume = std::get<ConsumeThisStatusAction>(
                behaviorRules.front().actions.back().value);
            consume.quantity = 2;
        });
    CHECK_FALSE(validates(wrongSevenStarConsumption));

    auto missingSevenStarDepletionStun = sevenStarDefinition.rules;
    mutateBehavior(
        applicationFor(
            missingSevenStarDepletionStun,
            BattleStatusKind::SevenStarMark),
        [](std::vector<EffectRule>& behaviorRules)
        {
            auto& consume = std::get<ConsumeThisStatusAction>(
                behaviorRules.front().actions.back().value);
            consume.whenDepleted.reset();
        });
    CHECK_FALSE(validates(missingSevenStarDepletionStun));

    auto duplicatePoisonExplosion = poisonExplosionDefinition.rules;
    auto duplicatePoisonExplosionProducer = producerFor(
        duplicatePoisonExplosion, BattleStatusKind::PoisonExplosion);
    duplicatePoisonExplosionProducer.id = EffectRuleId{ 70002 };
    duplicatePoisonExplosion.push_back(std::move(duplicatePoisonExplosionProducer));
    CHECK(validates(duplicatePoisonExplosion));

    auto missingPoisonExplosionBehavior = poisonExplosionDefinition.rules;
    applicationFor(
        missingPoisonExplosionBehavior,
        BattleStatusKind::PoisonExplosion).behavior.reset();
    CHECK_FALSE(validates(missingPoisonExplosionBehavior));

    auto wrongPoisonExplosionProfile = poisonExplosionDefinition.rules;
    auto sevenStarProfileSource = sevenStarDefinition.rules;
    applicationFor(
        wrongPoisonExplosionProfile,
        BattleStatusKind::PoisonExplosion).behavior = applicationFor(
            sevenStarProfileSource,
            BattleStatusKind::SevenStarMark).behavior;
    CHECK_FALSE(validates(wrongPoisonExplosionProfile));

    auto wrongPoisonExplosionHolderDeath = poisonExplosionDefinition.rules;
    mutateBehavior(
        applicationFor(
            wrongPoisonExplosionHolderDeath,
            BattleStatusKind::PoisonExplosion),
        [](std::vector<EffectRule>& behaviorRules)
        {
            REQUIRE(behaviorRules.size() == 1);
            behaviorRules.front().observation
                = EffectObservationScope::StatusHolderEventSource;
        });
    CHECK_FALSE(validates(wrongPoisonExplosionHolderDeath));

    auto wrongPoisonExplosionSelector = poisonExplosionDefinition.rules;
    mutateBehavior(
        applicationFor(
            wrongPoisonExplosionSelector,
            BattleStatusKind::PoisonExplosion),
        [](std::vector<EffectRule>& behaviorRules)
        {
            behaviorRules.front().selector.kind = EffectSelectorKind::StatusHolder;
            behaviorRules.front().selector.radiusTiles = 0;
            behaviorRules.front().selector.team = EffectTeamFilter::Any;
        });
    CHECK_FALSE(validates(wrongPoisonExplosionSelector));

    auto wrongPoisonExplosionTeam = poisonExplosionDefinition.rules;
    mutateBehavior(
        applicationFor(
            wrongPoisonExplosionTeam,
            BattleStatusKind::PoisonExplosion),
        [](std::vector<EffectRule>& behaviorRules)
        {
            behaviorRules.front().selector.team = EffectTeamFilter::Ally;
        });
    CHECK_FALSE(validates(wrongPoisonExplosionTeam));

    auto missingPoisonExplosionLayerScale = poisonExplosionDefinition.rules;
    mutateBehavior(
        applicationFor(
            missingPoisonExplosionLayerScale,
            BattleStatusKind::PoisonExplosion),
        [](std::vector<EffectRule>& behaviorRules)
        {
            auto& damage = std::get<DealDamageAction>(
                behaviorRules.front().actions.front().value);
            damage.amount.statusScale = StatusNumberScale::Once;
        });
    CHECK_FALSE(validates(missingPoisonExplosionLayerScale));

    auto missingPoisonExplosionPoison = poisonExplosionDefinition.rules;
    mutateBehavior(
        applicationFor(
            missingPoisonExplosionPoison,
            BattleStatusKind::PoisonExplosion),
        [](std::vector<EffectRule>& behaviorRules)
        {
            REQUIRE(behaviorRules.front().actions.size() == 2);
            behaviorRules.front().actions.pop_back();
        });
    CHECK_FALSE(validates(missingPoisonExplosionPoison));

    auto poisonExplosionWithExtraBehavior = poisonExplosionDefinition.rules;
    mutateBehavior(
        applicationFor(
            poisonExplosionWithExtraBehavior,
            BattleStatusKind::PoisonExplosion),
        [](std::vector<EffectRule>& behaviorRules)
        {
            auto extraRule = behaviorRules.front();
            extraRule.id = EffectRuleId{ 70003 };
            behaviorRules.push_back(std::move(extraRule));
        });
    CHECK(validates(poisonExplosionWithExtraBehavior));

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
