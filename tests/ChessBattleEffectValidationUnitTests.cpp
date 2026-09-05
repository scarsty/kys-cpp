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

TEST_CASE("ChessBattleEffects_DescriptorOwnedProbesReachEveryAuthorVariantAndField",
          "[battle][effects][schema][descriptor][probe]")
{
    std::string error;
    const bool valid = validateEffectAuthoringDescriptorProbes(error);
    INFO(error);
    CHECK(valid);
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
    CHECK(ruleCount == 76);
    CHECK(attackCommitRuleCount == 41);
    CHECK(ultimateCommitRuleCount == 1);
    CHECK(ruleWithEvent(
        definitionWithId(definitions, 98),
        EffectEvent::UltimateCommitted).actions.size() == 1);
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

TEST_CASE("ChessBattleEffects_RepetitionCountRequiresAPositiveTypedFormula",
          "[battle][effects][magic][schema][repetition]")
{
    const auto parses = [](std::string_view count)
    {
        EffectRule rule;
        return parseEffectRule(
            YAML::Load(std::format(R"(
時機: 單位死亡
目標: 所有敵人
重複次數: {}
動作:
  - 造成傷害:
      數值: 1
      傷害種類: 純粹
)", count)),
            rule,
            EffectRuleId{ 1 },
            "規則重複次數驗證");
    };

    CHECK(parses("2"));
    CHECK(parses("{固定: 2}"));
    CHECK_FALSE(parses("0"));
    CHECK_FALSE(parses("-1"));
}

TEST_CASE("ChessBattleEffects_StatusQuantityReferencesRemainCanonicalWhileNamedEffectValuesAreRetired",
          "[battle][effects][status][behavior]")
{
    const auto parses = [](std::string_view number)
    {
        EffectRule rule;
        return parseEffectRule(
            YAML::Load(std::format(R"(
時機: 單位死亡
目標: 所有敵人
動作:
  - 造成傷害:
      數值: {}
      傷害種類: 純粹
)", number)),
            rule,
            EffectRuleId{ 1 },
            "已移除的名稱式狀態數值參照");
    };

    CHECK(parses("{來源狀態數量: 毒爆}"));
    CHECK_FALSE(parses(
        R"({來源狀態效果值: {狀態: 毒爆, 名稱: 死亡爆炸純粹傷害}})"));
}

TEST_CASE("ChessBattleEffects_StatusBehaviorProfilesEnforceRuntimeSafeRanges",
          "[battle][effects][status][range]")
{
    std::uint64_t nextRuleId = 700;
    const auto parses = [&](std::string_view action)
    {
        EffectRule rule;
        return parseEffectRule(
            YAML::Load(std::format(R"(
時機: 命中
目標: 命中目標
{}
)", action)),
            rule,
            EffectRuleId{ nextRuleId++ },
            "狀態效果值範圍驗證");
    };

    const auto poison = [&](int value, int minimum = 1)
    {
        return parses(std::format(R"(施加中毒:
  可觸發次數: 3
  持續幀數: 90
  重複套用: 保留較高傷害
  同事件合併: 合計傷害百分比
  效果:
    - 時機: 每隔
      間隔幀數: 30
      目標: 狀態持有者
      動作:
        - 造成傷害:
            數值:
              目標目前生命百分比: {}
              取整: 向零
              最小: {}
            傷害種類: 中毒
        - 消耗此狀態:
            消耗數量: 1)", value, minimum));
    };
    CHECK(poison(1));
    CHECK_FALSE(poison(0, 0));
    CHECK_FALSE(poison(-1, 0));
    CHECK_FALSE(parses(R"(施加中毒:
  可觸發次數: 3
  持續幀數: 90
  重複套用: 保留較高傷害
  同事件合併: 合計傷害百分比
  效果:
    - 時機: 每隔
      間隔幀數: 30
      目標: 狀態持有者
      動作:
        - 造成傷害:
            數值:
              基準: 目標目前生命
              固定: 100
              百分比: 1
              最小: 1
            傷害種類: 中毒
        - 消耗此狀態:
            消耗數量: 1)"));
    CHECK_FALSE(parses(R"(施加中毒:
  可觸發次數: 3
  持續幀數: 90
  重複套用: 保留較高傷害
  同事件合併: 合計傷害百分比
  效果:
    - 時機: 每隔
      間隔幀數: 30
      目標: 狀態持有者
      動作:
        - 造成傷害:
            數值: {目標目前生命百分比: 1, 最小: 1}
            傷害種類: 中毒
        - 消耗此狀態: {消耗數量: 1}
        - 造成傷害:
            數值: {目標目前生命百分比: 2, 最小: 1}
            傷害種類: 中毒
        - 消耗此狀態: {消耗數量: 1})"));
    CHECK_FALSE(parses(R"(施加中毒:
  可觸發次數: 3
  持續幀數: 90
  重複套用: 保留較高傷害
  同事件合併: 合計傷害百分比
  效果:
    - 時機: 每隔
      間隔幀數: 15
      目標: 狀態持有者
      動作:
        - 造成傷害:
            數值: {目標目前生命百分比: 1, 最小: 1}
            傷害種類: 中毒
        - 消耗此狀態: {消耗數量: 1})"));

    CHECK(parses(R"(套用狀態:
  狀態: 流血
  增加層數: 1
  目標總層數上限: 3)"));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 流血
  增加層數: 1
  目標總層數上限: 0)"));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 流血
  增加層數: 1
  目標總層數上限: 2
  效果:
    - 時機: 每隔
      間隔幀數: 10
      目標: 狀態持有者
      造成傷害:
        每層數值:
          基準: 目標最大生命
          百分比: 1
        傷害種類: 流血)"));

    CHECK(parses(R"(套用狀態:
  狀態: 寒毒
  持續幀數: 90)"));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 寒毒
  持續幀數: 90
  重複套用: 刷新持續時間)"));
    CHECK(parses(R"(套用狀態:
  狀態: 枯骨
  持續幀數: 120)"));

    const auto singleHitCap = [&](int value)
    {
        return parses(std::format(R"(套用狀態:
  狀態: 下次承傷上限
  可觸發次數: 1
  效果:
    - 時機: 持續
      目標: 狀態持有者
      傷害修正:
        方位: 承受
        階段: 最終
        傷害種類: 全部
        方式: 單次承傷上限
        數值: {})", value));
    };
    CHECK(singleHitCap(1));
    CHECK_FALSE(singleHitCap(0));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 下次承傷上限
  可觸發次數: 1
  效果:
    - 時機: 持續
      目標: 狀態持有者
      傷害修正:
        方位: 承受
        階段: 最終
        傷害種類: 全部
        方式: 單次承傷上限
        數值:
          目標最大生命百分比: 15
          取整: 向零
          最小: 1)"));
    CHECK(parses(R"(套用狀態:
  狀態: 下次承傷上限
  可觸發次數: 1
  效果:
    - 時機: 持續
      目標: 狀態持有者
      傷害修正:
        方位: 承受
        階段: 最終
        傷害種類: 全部
        方式: 單次承傷上限
        數值:
          基準: 套用目標最大生命
          百分比: 15
          取整: 向零
          最小: 1)"));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 下次承傷上限
  可觸發次數: 1
  效果:
    - 時機: 持續
      目標: 狀態持有者
      傷害修正:
        方位: 承受
        階段: 最終
        傷害種類: 全部
        方式: 單次承傷上限
        數值:
          目標最大生命百分比: 15
          取整: 向零)"));

    const auto battleSpirit = [&](int damageIncrease, int reduction)
    {
        return parses(std::format(R"(套用狀態:
  狀態: 戰意
  增加層數: 1
  層數上限: 10
  效果:
    - 時機: 持續
      目標: 狀態持有者
      動作:
        - 傷害修正:
            方位: 造成
            階段: 防禦前
            傷害種類: 招式
            方式: 百分比加算
            每層數值: {}
        - 傷害修正:
            方位: 承受
            階段: 防禦前
            傷害種類: 全部
            方式: 百分比加算
            每層數值: {})", damageIncrease, reduction));
    };
    CHECK(battleSpirit(5, -1));
    CHECK_FALSE(battleSpirit(-1, 0));
    CHECK_FALSE(battleSpirit(5, 1));

    const auto trueQi = [&](int value)
    {
        return parses(std::format(R"(套用狀態:
  狀態: 真氣
  增加層數: 1
  層數上限: 10
  效果:
    - 時機: 命中
      目標: 命中目標
      造成傷害:
        每層數值: {}
        傷害種類: 純粹)", value));
    };
    CHECK(trueQi(1));
    CHECK_FALSE(trueQi(0));
    CHECK_FALSE(trueQi(-1));
    CHECK_FALSE(trueQi(std::numeric_limits<int>::max()));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 真氣
  增加層數: 1
  層數上限: 10
  效果:
    - 時機: 命中
      目標: 命中目標
      造成傷害:
        每層數值: 9
        傷害種類: 純粹
    - 時機: 命中
      目標: 狀態持有者
      強制移動:
        方向: 遠離來源
        距離像素: 40
        碰撞: 阻擋前停止
        受阻結果: 縮短)"));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 下一次受到攻擊必定落空
  持續幀數: 120
  可觸發次數: 1
  效果:
    - 時機: 命中
      觀察範圍: 狀態持有者事件目標
      目標: 狀態持有者
      機率: 1
      使本次受到攻擊落空: {})"));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 戰意
  增加層數: 1
  層數上限: 10
  效果:
    - 時機: 持續
      目標: 狀態持有者
      動作:
        - 傷害修正:
            方位: 造成
            階段: 防禦前
            傷害種類: 招式
            方式: 百分比加算
            每層數值: 5
        - 傷害修正:
            方位: 承受
            階段: 最終
            傷害種類: 全部
            方式: 百分比加算
            每層數值: -1)"));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 真氣
  增加層數: 1
  層數上限: 10
  效果:
    - 時機: 命中
      目標: 命中目標
      造成傷害:
        每層數值:
          基準: 來源星級
          百分比: 1
        傷害種類: 純粹)"));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 真氣
  增加層數: 1
  層數上限: 10
  效果:
    - 時機: 命中
      目標: 命中目標
      造成傷害:
        每層數值:
          基準: 來源星級
          百分比: 100
          最小: 2147483647
        傷害種類: 純粹)"));

    const auto poisonExplosion = [&](int value)
    {
        return parses(std::format(R"(套用狀態:
  狀態: 毒爆
  增加層數: 1
  層數上限: 5
  效果:
    - 時機: 單位死亡
      觀察範圍: 狀態持有者事件目標
      目標:
        類型: 半徑內單位
        半徑格數: 5
        隊伍: 敵方
      動作:
        - 造成傷害:
            每層數值:
              每星級: {}
            傷害種類: 純粹
        - 施加中毒:
            可觸發次數: 4
            持續幀數: 120
            重複套用: 取代現有中毒
            效果:
              - 時機: 每隔
                間隔幀數: 30
                目標: 狀態持有者
                動作:
                  - 造成傷害:
                      數值:
                        目標目前生命百分比: 10
                        取整: 向零
                        最小: 1
                      傷害種類: 中毒
                  - 消耗此狀態:
                      消耗數量: 1)", value));
    };
    CHECK(poisonExplosion(1));
    CHECK_FALSE(poisonExplosion(0));
    CHECK_FALSE(poisonExplosion(-1));

    const auto persistentCap = [&](int value)
    {
        EffectRule rule;
        return parseEffectRule(
            YAML::Load(std::format(R"(
時機: 開場
目標: 自身
傷害修正:
  方位: 承受
  階段: 最終
  傷害種類: 全部
  方式: 每次承傷不超過最大生命百分比
  數值: {}
)", value)),
            rule,
            EffectRuleId{ nextRuleId++ },
            "常駐單次承傷上限範圍驗證");
    };
    CHECK(persistentCap(1));
    CHECK_FALSE(persistentCap(0));
    CHECK_FALSE(persistentCap(-1));
}

TEST_CASE("ChessBattleEffects_PoisonSameEventMergeIsADeterministicTopLevelHitPreflight",
          "[battle][effects][status][poison][aggregation][validation]")
{
    EffectRule valid;
    REQUIRE(parseEffectRule(
        YAML::Load(R"(
時機: 命中
目標: 命中目標
施加中毒:
  可觸發次數: 3
  持續幀數: 90
  重複套用: 保留較高傷害
  同事件合併: 合計傷害百分比
  效果:
    - 時機: 每隔
      間隔幀數: 30
      目標: 狀態持有者
      動作:
        - 造成傷害:
            數值: {目標目前生命百分比: 7, 最小: 1}
            傷害種類: 中毒
        - 消耗此狀態: {消耗數量: 1}
)"), valid, EffectRuleId{ 750 }, "中毒同事件合併前置規則"));

    const auto rejection = [](
        EffectRule rule,
        EffectRuleAuthoringContext context = EffectRuleAuthoringContext::Configured)
    {
        std::string error;
        CHECK_FALSE(validateEffectRule(rule, error, context));
        return error;
    };

    CHECK(rejection(valid, EffectRuleAuthoringContext::StatusBehavior)
        == "中毒「同事件合併」只能寫在頂層設定規則");

    auto mixed = valid;
    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 1;
    mixed.actions.push_back(EffectAction{ EffectActionValue{ shield } });
    CHECK(rejection(mixed)
        == "中毒「同事件合併」必須是規則中唯一且直接指定的動作");

    auto conditional = valid;
    auto branch = std::make_shared<ConditionalEffectAction>();
    branch->conditions.push_back(IsMainProjectileCondition{});
    branch->whenTrue.push_back(valid.actions.front());
    conditional.actions = {
        EffectAction{ EffectActionValue{ std::move(branch) } },
    };
    CHECK(rejection(conditional)
        == "中毒「同事件合併」必須是規則中唯一且直接指定的動作");

    auto wrongEvent = valid;
    wrongEvent.event = EffectEvent::UltimateCommitted;
    wrongEvent.selector.kind = EffectSelectorKind::Self;
    CHECK(rejection(wrongEvent)
        == "中毒「同事件合併」只支援「命中」時機");

    auto wrongSelector = valid;
    wrongSelector.selector.kind = EffectSelectorKind::Self;
    CHECK(rejection(wrongSelector)
        == "中毒「同事件合併」必須使用「命中目標」");

    const auto rejectsAccounting = [&](const auto& mutate)
    {
        auto rule = valid;
        mutate(rule);
        CHECK_FALSE(rejection(std::move(rule)).empty());
    };
    rejectsAccounting([](EffectRule& rule)
    {
        rule.conditions.push_back(AcceptedHitCondition{});
    });
    rejectsAccounting([](EffectRule& rule) { rule.chancePct = 50; });
    rejectsAccounting([](EffectRule& rule) { rule.maxActivations = 1; });
    rejectsAccounting([](EffectRule& rule) { rule.sharedCooldownFrames = 30; });
    rejectsAccounting([](EffectRule& rule) { rule.intervalFrames = 30; });
    rejectsAccounting([](EffectRule& rule) { rule.everyNthEvent = 2; });
    rejectsAccounting([](EffectRule& rule)
    {
        rule.activationLimit = EffectActivationLimit{
            EffectActivationScope::PerCastPerTarget,
            1,
        };
    });
    rejectsAccounting([](EffectRule& rule)
    {
        rule.repetitionCount = EffectNumber{ .flat = 2 };
    });
}

TEST_CASE("ChessBattleEffects_AuthoredRulesCannotUseIntrinsicStatusRuleIds",
          "[battle][effects][status][identity]")
{
    EffectRule rule;
    CHECK_FALSE(parseEffectRule(
        YAML::Load(R"(
時機: 攻擊提交
目標: 自身
回復內力: 1
)"),
        rule,
        EffectRuleId{ IntrinsicEffectRuleIdMask | 17 },
        "保留內建狀態 ID 驗證"));
}

TEST_CASE("ChessBattleEffects_CatalogOwnedProfilesAreCompleteAndCannotBeOverridden",
          "[battle][effects][status][profile][catalog]")
{
    EffectRule first;
    REQUIRE(parseEffectRule(YAML::Load(R"(
時機: 主彈命中
目標: 命中目標
套用狀態:
  狀態: 寒毒
  持續幀數: 90
)"), first, EffectRuleId{ 7400 }, "寒毒目錄行為"));
    const auto& firstApplication = std::get<ApplyStatusAction>(
        first.actions.front().value);
    REQUIRE(firstApplication.behavior);
    CHECK(statusBehaviorsEquivalent(
        firstApplication.behavior,
        makeCatalogOwnedStatusBehavior(firstApplication)));

    EffectRule second;
    REQUIRE(parseEffectRule(YAML::Load(R"(
時機: 主彈命中
目標: 命中目標
套用狀態:
  狀態: 寒毒
  持續幀數: 60
)"), second, EffectRuleId{ 7401 }, "第二個寒毒生產者"));
    const auto& secondApplication = std::get<ApplyStatusAction>(
        second.actions.front().value);
    CHECK(statusBehaviorsEquivalent(
        firstApplication.behavior,
        secondApplication.behavior));

    ChessDiagnosticCollector diagnostics;
    EffectRule overridden;
    CHECK_FALSE(parseEffectRule(YAML::Load(R"(
時機: 主彈命中
目標: 命中目標
套用狀態:
  狀態: 寒毒
  持續幀數: 90
  效果:
    - 時機: 持續
      目標: 狀態持有者
      屬性修正:
        屬性: 速度
        方式: 百分比加算
        數值: -25
)"), overridden, EffectRuleId{ 7402 }, "不可覆寫寒毒行為",
        diagnostics.sink()));
    REQUIRE_FALSE(diagnostics.diagnostics().empty());
    CHECK(diagnostics.diagnostics().front().message.find(
        "狀態「寒毒」的行為由目錄決定") != std::string::npos);
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

TEST_CASE("ChessBattleEffects_EffectNumberTraversalReachesNestedStatusBehavior",
          "[battle][effects][status][numbers]")
{
    EffectRule rule;
    REQUIRE(parseEffectRule(YAML::Load(R"(
時機: 主彈命中
目標: 命中目標
套用狀態:
  狀態: 化勁
  可觸發次數: 1
  化解後護盾:
    每星級: 100
)"), rule, EffectRuleId{ 7401 }, "巢狀狀態數值走訪"));

    std::vector<EffectNumber> numbers;
    const EffectRule& constRule = rule;
    forEachEffectNumber(constRule, [&](const EffectNumber& number)
    {
        numbers.push_back(number);
    });
    REQUIRE(numbers.size() == 1);
    CHECK(numbers.front().base == EffectNumberBase::SourceStar);
    CHECK(numbers.front().percent == 10000);
}

TEST_CASE("ChessBattleEffects_OpenMarkersAndStatusOnlyActionsAreValidatedRecursively",
          "[battle][effects][status][validation]")
{
    const auto parse = [](std::string_view yaml, std::string* diagnostic = nullptr)
    {
        EffectRule rule;
        ChessDiagnosticCollector diagnostics;
        const bool parsed = parseEffectRule(
            YAML::Load(std::string(yaml)),
            rule,
            EffectRuleId{ 7405 },
            "狀態專用動作遞迴驗證",
            diagnostics.sink());
        if (diagnostic)
        {
            *diagnostic = diagnostics.diagnostics().empty()
                ? std::string{}
                : diagnostics.diagnostics().front().message;
        }
        return parsed;
    };

    std::string diagnostic;
    CHECK_FALSE(parse(R"(
時機: 開場
目標: 自身
套用狀態:
  狀態: 無影
  持續幀數: 90
  重複套用: 刷新持續時間
)", &diagnostic));
    CHECK(diagnostic.find("狀態「無影」需要「效果」") != std::string::npos);

    CHECK(parse(R"(
時機: 開場
目標: 自身
套用狀態:
  狀態: 無影
  持續幀數: 90
  重複套用: 刷新持續時間
  效果:
    - 時機: 命中
      目標: 狀態持有者
      消耗此狀態: {消耗數量: 1}
)"));

    diagnostic.clear();
    CHECK_FALSE(parse(R"(
時機: 主彈命中
目標: 命中目標
套用狀態:
  狀態: 眩暈
  持續幀數: 30
  重複套用: 延長持續時間
  效果:
    - 時機: 命中
      目標: 狀態持有者
      獲得護盾: 1
)", &diagnostic));
    CHECK(diagnostic.find("狀態「眩暈」由內建群組語意決定行為")
        != std::string::npos);

    diagnostic.clear();
    CHECK_FALSE(parse(R"(
時機: 開場
目標: 自身
套用狀態:
  狀態: 無影
  持續幀數: 90
  重複套用: 刷新持續時間
  效果:
    - 時機: 命中
      目標: 狀態持有者
      消耗此狀態: {消耗數量: 0}
)", &diagnostic));
    CHECK(diagnostic.find("消耗此狀態數量必須為正數") != std::string::npos);

    diagnostic.clear();
    CHECK_FALSE(parse(R"(
時機: 開場
目標: 自身
套用狀態:
  狀態: 無影
  持續幀數: 90
  重複套用: 刷新持續時間
  效果:
    - 時機: 命中
      目標: 狀態持有者
      消耗此狀態:
        消耗數量: 1
        最後一次:
          套用狀態:
            狀態: 眩暈
            持續幀數: 0
            重複套用: 延長持續時間
)", &diagnostic));
    CHECK(diagnostic.find("狀態「眩暈」") != std::string::npos);

    diagnostic.clear();
    CHECK_FALSE(parse(R"(
時機: 開場
目標: 自身
套用狀態:
  狀態: 化勁
  可觸發次數: 1
  化解後護盾: 0
)", &diagnostic));
    CHECK(diagnostic.find("狀態「化勁」的「化解後護盾」必須保證為正數")
        != std::string::npos);
}

TEST_CASE("ChessBattleEffects_OuterValidationDoesNotRebindNestedStatusBehaviorNumbers",
          "[battle][effects][status][numbers][binding]")
{
    EffectRule opening;
    CHECK(parseEffectRule(YAML::Load(R"(
時機: 開場
目標: 自身
套用狀態:
  狀態: 無影
  持續幀數: 90
  重複套用: 刷新持續時間
  效果:
    - 時機: 命中
      目標: 命中目標
      獲得護盾:
        基準: 目標目前護盾
        百分比: 50
)"), opening, EffectRuleId{ 7402 }, "開場套用的事件即時狀態效果"));

    EffectRule nested;
    CHECK(parseEffectRule(YAML::Load(R"(
時機: 主彈命中
目標: 命中目標
套用狀態:
  狀態: 無影
  持續幀數: 90
  重複套用: 刷新持續時間
  效果:
    - 時機: 命中
      目標: 命中目標
      套用狀態:
        狀態: 化勁
        可觸發次數: 1
        化解後護盾:
          每星級: 100
)"), nested, EffectRuleId{ 7403 }, "外層狀態行為中的巢狀具名參數"));
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
      方式: 每次承傷不超過最大生命百分比
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
