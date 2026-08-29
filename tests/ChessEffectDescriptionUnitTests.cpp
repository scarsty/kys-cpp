#include "ChessBattleEffectTestHelpers.h"
#include "ChessNonBattleRules.h"
#include "ChessUiCommon.h"
#include "DisplayText.h"
#include "Types.h"

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace KysChess;
using namespace KysChess::Test;

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
