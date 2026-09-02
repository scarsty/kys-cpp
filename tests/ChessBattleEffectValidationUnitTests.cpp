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
    CHECK(ruleCount == 79);
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

    CHECK(parses(R"(
        來源狀態數量: 毒爆
        最小: 1)"));
    CHECK_FALSE(parses("0"));
    CHECK_FALSE(parses(R"(
        來源狀態數量: 毒爆)"));
}

TEST_CASE("ChessBattleEffects_SourceStatusQuantityRequiresPublicQuantitySemantics",
          "[battle][effects][status][quantity]")
{
    const auto parses = [](std::string_view status)
    {
        EffectRule rule;
        return parseEffectRule(
            YAML::Load(std::format(R"(
時機: 單位死亡
目標: 所有敵人
重複次數:
  來源狀態數量: {}
  最小: 1
動作:
  - 造成傷害:
      數值: 1
      傷害種類: 純粹
)", status)),
            rule,
            EffectRuleId{ 1 },
            "來源狀態數量目錄驗證");
    };

    CHECK(parses("毒爆"));
    for (const std::string_view status : {
             "眩暈",
             "枯骨",
             "無影",
             "下一次攻擊必定暴擊",
         })
    {
        CAPTURE(status);
        CHECK_FALSE(parses(status));
    }
}

TEST_CASE("ChessBattleEffects_StatusEffectValuesEnforceRuntimeSafeRanges",
          "[battle][effects][status][range]")
{
    std::uint64_t nextRuleId = 700;
    const auto parses = [&](std::string_view action)
    {
        EffectRule rule;
        return parseEffectRule(
            YAML::Load(std::format(R"(
時機: 主彈命中
目標: 命中目標
{}
)", action)),
            rule,
            EffectRuleId{ nextRuleId++ },
            "狀態效果值範圍驗證");
    };

    const auto poison = [&](int value)
    {
        return parses(std::format(R"(施加中毒:
  可觸發次數: 3
  持續幀數: 90
  重複套用: 保留較高傷害
  同事件合併: 合計傷害百分比
  效果:
    每次觸發:
      目前生命傷害百分比: {})", value));
    };
    CHECK(poison(1));
    CHECK_FALSE(poison(0));
    CHECK_FALSE(poison(-1));

    const auto bleed = [&](std::string_view value, int layerLimit = 2)
    {
        return parses(std::format(R"(套用狀態:
  狀態: 流血
  增加層數: 1
  層數上限: {}
  效果:
    每層生效:
      最大生命傷害百分比: {})", layerLimit, value));
    };
    CHECK(bleed("1"));
    CHECK_FALSE(bleed("0"));
    CHECK_FALSE(bleed("-1"));
    CHECK_FALSE(bleed("2147483647", 2));
    CHECK_FALSE(bleed(R"(
        基準: 來源最大生命
        百分比: 1)"));

    const auto coldPoison = [&](int value)
    {
        return parses(std::format(R"(套用狀態:
  狀態: 寒毒
  持續幀數: 90
  重複套用: 刷新持續時間
  效果:
    持續生效:
      禁止受到治療: true
      速度降低百分比: {})", value));
    };
    CHECK(coldPoison(0));
    CHECK_FALSE(coldPoison(-1));

    const auto witheredBone = [&](int damageIncrease, int healingReduction)
    {
        return parses(std::format(R"(套用狀態:
  狀態: 枯骨
  持續幀數: 120
  重複套用: 刷新持續時間
  效果:
    持續生效:
      受到傷害增加百分比: {}
      受到治療減少百分比: {})", damageIncrease, healingReduction));
    };
    CHECK(witheredBone(0, 100));
    CHECK_FALSE(witheredBone(-1, 50));
    CHECK_FALSE(witheredBone(1, -1));
    CHECK_FALSE(witheredBone(1, 101));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 枯骨
  持續幀數: 120
  重複套用: 刷新持續時間
  效果:
    持續生效:
      受到傷害增加百分比: 0
      受到治療減少百分比:
        基準: 來源星級
        百分比: 100
        最小: 101)"));

    const auto singleHitCap = [&](int value)
    {
        return parses(std::format(R"(套用狀態:
  狀態: 下次承傷上限
  可觸發次數: 1
  效果:
    每次觸發:
      傷害上限: {})", value));
    };
    CHECK(singleHitCap(1));
    CHECK_FALSE(singleHitCap(0));
    CHECK(parses(R"(套用狀態:
  狀態: 下次承傷上限
  可觸發次數: 1
  效果:
    每次觸發:
      傷害上限:
        目標最大生命百分比: 15
        取整: 向零
        最小: 1)"));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 下次承傷上限
  可觸發次數: 1
  效果:
    每次觸發:
      傷害上限:
        目標最大生命百分比: 15
        取整: 向零)"));

    const auto battleSpirit = [&](int damageIncrease, int reduction)
    {
        return parses(std::format(R"(套用狀態:
  狀態: 戰意
  增加層數: 1
  層數上限: 10
  效果:
    每層生效:
      招式傷害增加百分比: {}
      傷害減免百分比: {})", damageIncrease, reduction));
    };
    CHECK(battleSpirit(0, 0));
    CHECK_FALSE(battleSpirit(-1, 0));
    CHECK_FALSE(battleSpirit(0, -1));

    const auto trueQi = [&](int value)
    {
        return parses(std::format(R"(套用狀態:
  狀態: 真氣
  增加層數: 1
  層數上限: 10
  效果:
    每層生效:
      命中附加純粹傷害: {})", value));
    };
    CHECK(trueQi(1));
    CHECK_FALSE(trueQi(0));
    CHECK_FALSE(trueQi(-1));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 真氣
  增加層數: 1
  層數上限: 10
  效果:
    每層生效:
      命中附加純粹傷害:
        基準: 來源星級
        百分比: 1)"));
    CHECK_FALSE(parses(R"(套用狀態:
  狀態: 真氣
  增加層數: 1
  層數上限: 10
  效果:
    每層生效:
      命中附加純粹傷害:
        基準: 來源星級
        百分比: 100
        最小: 2147483647)"));

    const auto poisonExplosion = [&](int value)
    {
        return parses(std::format(R"(套用狀態:
  狀態: 毒爆
  增加層數: 1
  層數上限: 5
  效果:
    每層提供數值:
      死亡爆炸純粹傷害: {})", value));
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

TEST_CASE("ChessBattleEffects_ExplicitStatusLifecyclesAreUniqueAndFullyLinked",
          "[battle][effects][status][lifecycle]")
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
    const auto hiddenDepletionProducer = [](
        const ApplyStatusAction& application,
        std::uint64_t id)
    {
        ConsumeStatusAction carrier;
        carrier.status = BattleStatusKind::BattleSpirit;
        carrier.whenDepleted = application;
        EffectRule rule;
        rule.id = EffectRuleId{ id };
        rule.event = EffectEvent::AttackCommitted;
        rule.selector.kind = EffectSelectorKind::Self;
        rule.actions = { EffectAction{ EffectActionValue{ std::move(carrier) } } };
        return rule;
    };
    const auto resourceReferenceRule = [](EffectNumber amount, std::uint64_t id)
    {
        ChangeResourceAction resource;
        resource.resource = BattleResource::Shield;
        resource.kind = ResourceChangeKind::Grant;
        resource.amount = std::move(amount);
        EffectRule rule;
        rule.id = EffectRuleId{ id };
        rule.event = EffectEvent::AttackCommitted;
        rule.selector.kind = EffectSelectorKind::Self;
        rule.actions = { EffectAction{ EffectActionValue{ std::move(resource) } } };
        return rule;
    };

    const auto& sevenStarDefinition = definitionWithId(content->magicEffects(), 39);
    CHECK(validates(sevenStarDefinition.rules));

    auto duplicateSevenStar = sevenStarDefinition.rules;
    auto duplicateSevenStarProducer = producerFor(
        duplicateSevenStar, BattleStatusKind::SevenStarMark);
    duplicateSevenStarProducer.id = EffectRuleId{ 70001 };
    duplicateSevenStar.push_back(std::move(duplicateSevenStarProducer));
    std::string duplicateError;
    CHECK_FALSE(validateEffectRules(duplicateSevenStar, duplicateError));
    CHECK(duplicateError
        == "狀態「七星」的顯式生命週期必須有且只有一個相容 producer；"
           "總數為 2 個，其中相容 2 個");

    auto missingSevenStar = sevenStarDefinition.rules;
    const auto& sevenStarProducer = producerFor(
        missingSevenStar, BattleStatusKind::SevenStarMark);
    missingSevenStar.erase(missingSevenStar.begin()
        + (&sevenStarProducer - missingSevenStar.data()));
    CHECK_FALSE(validates(missingSevenStar));

    auto hiddenSevenStarProducer = sevenStarDefinition.rules;
    const auto& sevenStarApplication = std::get<ApplyStatusAction>(
        producerFor(hiddenSevenStarProducer, BattleStatusKind::SevenStarMark)
            .actions.front().value);
    hiddenSevenStarProducer.push_back(hiddenDepletionProducer(
        sevenStarApplication, 70003));
    CHECK_FALSE(validates(hiddenSevenStarProducer));

    auto wrongSevenStarSource = sevenStarDefinition.rules;
    const auto sevenStarConsumer = std::ranges::find_if(
        wrongSevenStarSource,
        [](const EffectRule& rule)
        {
            return std::ranges::any_of(rule.actions, [](const EffectAction& action)
            {
                const auto* consume = std::get_if<ConsumeStatusAction>(&action.value);
                return consume && consume->status == BattleStatusKind::SevenStarMark;
            });
        });
    REQUIRE(sevenStarConsumer != wrongSevenStarSource.end());
    auto& consume = std::get<ConsumeStatusAction>(sevenStarConsumer->actions[1].value);
    consume.source = StatusSourceMatch::Any;
    std::string incompatibleConsumerError;
    CHECK_FALSE(validateEffectRules(wrongSevenStarSource, incompatibleConsumerError));
    CHECK(incompatibleConsumerError
        == "狀態「七星」的顯式生命週期必須有且只有一個相容 consumer；"
           "總數為 1 個，其中相容 0 個");

    auto missingSevenStarOwnership = sevenStarDefinition.rules;
    const auto sevenStarOwnershipConsumer = std::ranges::find_if(
        missingSevenStarOwnership,
        [](const EffectRule& rule)
        {
            return std::ranges::any_of(rule.conditions, [](const EffectCondition& condition)
            {
                return std::holds_alternative<TargetHasStateFromEffectOwnerCondition>(condition);
            });
        });
    REQUIRE(sevenStarOwnershipConsumer != missingSevenStarOwnership.end());
    sevenStarOwnershipConsumer->conditions.clear();
    CHECK_FALSE(validates(missingSevenStarOwnership));

    auto wrongSevenStarProducerTarget = sevenStarDefinition.rules;
    auto& targetChangedSevenStar = producerFor(
        wrongSevenStarProducerTarget, BattleStatusKind::SevenStarMark);
    targetChangedSevenStar.selector = {};
    targetChangedSevenStar.selector.kind = EffectSelectorKind::Self;
    std::string incompatibleProducerError;
    CHECK_FALSE(validateEffectRules(
        wrongSevenStarProducerTarget, incompatibleProducerError));
    CHECK(incompatibleProducerError
        == "狀態「七星」的顯式生命週期必須有且只有一個相容 producer；"
           "總數為 1 個，其中相容 0 個");

    auto wrongSevenStarProducerEvent = sevenStarDefinition.rules;
    producerFor(wrongSevenStarProducerEvent, BattleStatusKind::SevenStarMark).event
        = EffectEvent::AttackCommitted;
    CHECK_FALSE(validates(wrongSevenStarProducerEvent));

    auto wrongSevenStarProducerObservation = sevenStarDefinition.rules;
    producerFor(wrongSevenStarProducerObservation, BattleStatusKind::SevenStarMark).observation
        = EffectObservationScope::EventTarget;
    CHECK_FALSE(validates(wrongSevenStarProducerObservation));

    auto multipleSevenStarConsumes = sevenStarDefinition.rules;
    const auto multipleConsumeRule = std::ranges::find_if(
        multipleSevenStarConsumes,
        [](const EffectRule& rule)
        {
            return std::ranges::any_of(rule.actions, [](const EffectAction& action)
            {
                const auto* consume = std::get_if<ConsumeStatusAction>(&action.value);
                return consume && consume->status == BattleStatusKind::SevenStarMark;
            });
        });
    REQUIRE(multipleConsumeRule != multipleSevenStarConsumes.end());
    multipleConsumeRule->actions.push_back(multipleConsumeRule->actions.back());
    CHECK_FALSE(validates(multipleSevenStarConsumes));

    const auto& poisonExplosionDefinition = definitionWithId(content->magicEffects(), 95);
    CHECK(validates(poisonExplosionDefinition.rules));

    auto duplicatePoisonExplosion = poisonExplosionDefinition.rules;
    auto duplicatePoisonExplosionProducer = producerFor(
        duplicatePoisonExplosion, BattleStatusKind::PoisonExplosion);
    duplicatePoisonExplosionProducer.id = EffectRuleId{ 70002 };
    duplicatePoisonExplosion.push_back(std::move(duplicatePoisonExplosionProducer));
    CHECK_FALSE(validates(duplicatePoisonExplosion));

    auto missingPoisonExplosion = poisonExplosionDefinition.rules;
    const auto& poisonExplosionProducer = producerFor(
        missingPoisonExplosion, BattleStatusKind::PoisonExplosion);
    missingPoisonExplosion.erase(missingPoisonExplosion.begin()
        + (&poisonExplosionProducer - missingPoisonExplosion.data()));
    CHECK_FALSE(validates(missingPoisonExplosion));

    auto hiddenPoisonExplosionProducer = poisonExplosionDefinition.rules;
    const auto& poisonExplosionApplication = std::get<ApplyStatusAction>(
        producerFor(hiddenPoisonExplosionProducer, BattleStatusKind::PoisonExplosion)
            .actions.front().value);
    hiddenPoisonExplosionProducer.push_back(hiddenDepletionProducer(
        poisonExplosionApplication, 70004));
    CHECK_FALSE(validates(hiddenPoisonExplosionProducer));

    auto missingPoisonExplosionQuantity = poisonExplosionDefinition.rules;
    const auto poisonExplosionConsumer = std::ranges::find_if(
        missingPoisonExplosionQuantity,
        [](const EffectRule& rule)
        {
            return rule.event == EffectEvent::UnitDied;
        });
    REQUIRE(poisonExplosionConsumer != missingPoisonExplosionQuantity.end());
    poisonExplosionConsumer->repetitionCount.reset();
    CHECK_FALSE(validates(missingPoisonExplosionQuantity));

    auto missingPoisonExplosionValue = poisonExplosionDefinition.rules;
    const auto poisonExplosionValueConsumer = std::ranges::find_if(
        missingPoisonExplosionValue,
        [](const EffectRule& rule)
        {
            return rule.event == EffectEvent::UnitDied;
        });
    REQUIRE(poisonExplosionValueConsumer != missingPoisonExplosionValue.end());
    auto& explosionDamage = std::get<DealDamageAction>(
        poisonExplosionValueConsumer->actions[0].value);
    explosionDamage.amount = {};
    explosionDamage.amount.base = EffectNumberBase::SourceStar;
    explosionDamage.amount.percent = 100;
    CHECK_FALSE(validates(missingPoisonExplosionValue));

    EffectNumber poisonExplosionQuantity;
    poisonExplosionQuantity.base = EffectNumberBase::SourceStatusQuantity;
    poisonExplosionQuantity.status = BattleStatusKind::PoisonExplosion;
    poisonExplosionQuantity.percent = 100;
    const auto quantityReader = resourceReferenceRule(
        poisonExplosionQuantity, 70005);
    std::string readerError;
    const bool quantityReaderValid = validateEffectRule(quantityReader, readerError);
    INFO(readerError);
    REQUIRE(quantityReaderValid);
    auto extraPoisonExplosionQuantityReader = poisonExplosionDefinition.rules;
    extraPoisonExplosionQuantityReader.push_back(quantityReader);
    CHECK_FALSE(validates(extraPoisonExplosionQuantityReader));

    EffectNumber poisonExplosionValue;
    poisonExplosionValue.base = EffectNumberBase::SourceStatusEffectValue;
    poisonExplosionValue.status = BattleStatusKind::PoisonExplosion;
    poisonExplosionValue.statusEffect
        = StatusEffectValueKind::PoisonExplosionDeathPureDamage;
    poisonExplosionValue.percent = 100;
    const auto valueReader = resourceReferenceRule(poisonExplosionValue, 70006);
    readerError.clear();
    const bool valueReaderValid = validateEffectRule(valueReader, readerError);
    INFO(readerError);
    REQUIRE(valueReaderValid);
    auto extraPoisonExplosionValueReader = poisonExplosionDefinition.rules;
    extraPoisonExplosionValueReader.push_back(valueReader);
    CHECK_FALSE(validates(extraPoisonExplosionValueReader));

    auto wrongPoisonExplosionProducerTarget = poisonExplosionDefinition.rules;
    auto& targetChangedPoisonExplosion = producerFor(
        wrongPoisonExplosionProducerTarget, BattleStatusKind::PoisonExplosion);
    targetChangedPoisonExplosion.selector = {};
    targetChangedPoisonExplosion.selector.kind = EffectSelectorKind::HitTarget;
    CHECK_FALSE(validates(wrongPoisonExplosionProducerTarget));

    auto wrongPoisonExplosionProducerEvent = poisonExplosionDefinition.rules;
    producerFor(wrongPoisonExplosionProducerEvent, BattleStatusKind::PoisonExplosion).event
        = EffectEvent::HitBeforeDamage;
    CHECK_FALSE(validates(wrongPoisonExplosionProducerEvent));

    auto wrongPoisonExplosionProducerObservation = poisonExplosionDefinition.rules;
    producerFor(
        wrongPoisonExplosionProducerObservation,
        BattleStatusKind::PoisonExplosion).observation = EffectObservationScope::EventTarget;
    CHECK_FALSE(validates(wrongPoisonExplosionProducerObservation));

    auto missingPoisonExplosionGuard = poisonExplosionDefinition.rules;
    const auto missingGuardConsumer = std::ranges::find_if(
        missingPoisonExplosionGuard,
        [](const EffectRule& rule)
        {
            return rule.event == EffectEvent::UnitDied;
        });
    REQUIRE(missingGuardConsumer != missingPoisonExplosionGuard.end());
    missingGuardConsumer->conditions.clear();
    CHECK_FALSE(validates(missingPoisonExplosionGuard));

    auto wrongPoisonExplosionConsumerEvent = poisonExplosionDefinition.rules;
    const auto wrongEventConsumer = std::ranges::find_if(
        wrongPoisonExplosionConsumerEvent,
        [](const EffectRule& rule)
        {
            return rule.event == EffectEvent::UnitDied;
        });
    REQUIRE(wrongEventConsumer != wrongPoisonExplosionConsumerEvent.end());
    wrongEventConsumer->event = EffectEvent::AttackCommitted;
    CHECK_FALSE(validates(wrongPoisonExplosionConsumerEvent));

    auto wrongPoisonExplosionConsumerObservation = poisonExplosionDefinition.rules;
    const auto wrongObservationConsumer = std::ranges::find_if(
        wrongPoisonExplosionConsumerObservation,
        [](const EffectRule& rule)
        {
            return rule.event == EffectEvent::UnitDied;
        });
    REQUIRE(wrongObservationConsumer != wrongPoisonExplosionConsumerObservation.end());
    wrongObservationConsumer->observation = EffectObservationScope::EventTarget;
    CHECK_FALSE(validates(wrongPoisonExplosionConsumerObservation));
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
