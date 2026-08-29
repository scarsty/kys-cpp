#include "ChessBattleEffectTestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
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
    CHECK(ruleCount == 80);
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
