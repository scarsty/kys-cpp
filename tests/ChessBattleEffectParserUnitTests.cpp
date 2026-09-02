#include "ChessBattleEffectTestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace KysChess;
using namespace KysChess::Test;

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
      疊加範圍: 事件來源
  - 屬性修正:
      屬性: 防禦
      方式: 固定加算
      數值: 30
      持續幀數: 90
      合併方式: 增加層數
      層數上限: 3
      疊加範圍: 事件來源
  - 屬性修正:
      屬性: 速度
      方式: 百分比加算
      數值: 15
      持續幀數: 90
      合併方式: 增加層數
      層數上限: 3
      疊加範圍: 事件來源
  - 屬性修正:
      屬性: 暴擊率
      方式: 百分點加算
      數值: 10
      持續幀數: 90
      合併方式: 增加層數
      層數上限: 3
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
  疊加範圍: 事件來源
)");
    checkRulesEqual(attributeCanonical, attributeShorthand);

    const auto poisonRule = parseRuleText(R"(
時機: 命中
目標: 命中目標
施加中毒:
  可觸發次數: 3
  持續幀數: 90
  重複套用: 保留較高傷害
  同事件合併: 合計傷害百分比
  效果:
    每次觸發:
      目前生命傷害百分比: 7
)");
    const auto& standardPoison = std::get<ApplyStatusAction>(
        poisonRule.actions.front().value);
    CHECK(standardPoison.quantity == StatusQuantityOperation{
        SetStatusTriggerCharges{ 3 }});
    CHECK(standardPoison.reapplication
        == StatusReapplicationPolicy::KeepHigherDamage);
    const auto& standardPoisonEffects = std::get<PoisonStatusEffects>(
        standardPoison.effects);
    CHECK(standardPoisonEffects.currentHpDamagePercent.flat == 7);
    CHECK(standardPoisonEffects.sameEventMerge
        == PoisonSameEventMerge::SumDamagePercent);

    const auto resetPoisonRule = parseRuleText(R"(
時機: 絕招施放
目標: 所有敵人
施加中毒:
  可觸發次數: 5
  持續幀數: 150
  重複套用: 取代並重設
  效果:
    每次觸發:
      目前生命傷害百分比: 10
)");
    const auto& resetPoison = std::get<ApplyStatusAction>(
        resetPoisonRule.actions.front().value);
    CHECK(resetPoison.quantity == StatusQuantityOperation{
        SetStatusTriggerCharges{ 5 }});
    CHECK(resetPoison.reapplication
        == StatusReapplicationPolicy::ReplaceAndReset);

    EffectRule removedPoisonRule;
    CHECK_FALSE(parseEffectRule(YAML::Load(R"(
時機: 命中
目標: 命中目標
施毒: {持續幀數: 90, 層數: 3, 強度: 7}
)"), removedPoisonRule, EffectRuleId{ 2 }, "已移除的施毒簡式"));

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
      重複套用: 刷新持續時間
      效果:
        持續生效:
          禁止受到治療: true
          速度降低百分比: {基準: 來源最大生命, 百分比: 3}
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
      重複套用: 刷新持續時間
      效果:
        持續生效:
          禁止受到治療: true
          速度降低百分比: {來源最大生命百分比: 3}
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
      方式: 每次承傷不超過最大生命百分比
      數值: 20
)", R"(
時機: 開場
目標: 自身
傷害修正:
  方位: 承受
  階段: 最終
  傷害種類: 全部
  方式: 每次承傷不超過最大生命百分比
  數值: 20
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
    CHECK(status->reapplication == StatusReapplicationPolicy::RefreshDuration);
    const auto& witheredEffects = std::get<WitheredBoneStatusEffects>(
        status->effects);
    CHECK(witheredEffects.damageTakenIncreasePercent.flat == 25);
    CHECK(witheredEffects.healingReductionPercent.flat == 75);

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
    CHECK(poisonExplosion.repetitionCount->base == EffectNumberBase::SourceStatusQuantity);
    CHECK(poisonExplosion.repetitionCount->status == BattleStatusKind::PoisonExplosion);
    CHECK(poisonExplosion.repetitionCount->minimum == 1);
    CHECK(poisonExplosion.selector.kind == EffectSelectorKind::UnitsInRadius);
    CHECK(poisonExplosionDamage->area.kind == DamageAreaKind::SingleTarget);
    CHECK(poisonExplosionDamage->amount.base == EffectNumberBase::SourceStatusEffectValue);
    CHECK(poisonExplosionDamage->amount.status == BattleStatusKind::PoisonExplosion);
    CHECK(poisonExplosionDamage->amount.statusEffect
        == StatusEffectValueKind::PoisonExplosionDeathPureDamage);
    CHECK_FALSE(poisonExplosionDamage->transactionCount);
    const auto* poisonExplosionStatus = std::get_if<ApplyStatusAction>(
        &poisonExplosion.actions[1].value);
    REQUIRE(poisonExplosionStatus != nullptr);
    CHECK(std::holds_alternative<SetStatusTriggerCharges>(
        poisonExplosionStatus->quantity));
    const auto poisonExplosionDescription = descriptionText(
        std::span<const EffectRule>{&(poisonExplosion), 1},
        EffectDescriptionStyle::Full,
        {});
    CHECK(poisonExplosionDescription.find("100%") == std::string::npos);
    CHECK(poisonExplosionDescription.find("強度") == std::string::npos);

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
    CHECK(heavenDragonStun.reapplication
        == StatusReapplicationPolicy::KeepLongerDuration);

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

TEST_CASE("ChessBattleEffects_JiuyangAuthorsTrueQiAsStatusOwnedHitDamage",
          "[battle][effects][magic][schema][status]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));

    const auto& jiuyang = definitionWithId(definitions, 106);
    REQUIRE(jiuyang.rules.size() == 1);
    const auto& producer = ruleWithEvent(jiuyang, EffectEvent::AttackCommitted);
    REQUIRE(producer.actions.size() == 2);
    const auto* trueQi = std::get_if<ApplyStatusAction>(&producer.actions[1].value);
    REQUIRE(trueQi != nullptr);
    CHECK(trueQi->status == BattleStatusKind::TrueQi);
    CHECK(trueQi->quantity == StatusQuantityOperation{ AddStatusLayers{ 1, 10 } });
    const auto* effects = std::get_if<TrueQiStatusEffects>(&trueQi->effects);
    REQUIRE(effects != nullptr);
    CHECK(effects->pureDamagePerHit.flat == 9);
    CHECK(std::ranges::none_of(jiuyang.rules, [](const EffectRule& rule)
    {
        return rule.event == EffectEvent::HitBeforeDamage;
    }));
}

TEST_CASE("ChessBattleEffects_StatusDiagnosticsNameTheMissingCanonicalField",
          "[battle][effects][schema][status][diagnostic]")
{
    const auto diagnosticFor = [](std::string_view yaml)
    {
        EffectRule rule;
        ChessDiagnosticCollector diagnostics;
        CHECK_FALSE(parseEffectRule(
            YAML::Load(std::string(yaml)),
            rule,
            EffectRuleId{ 7200 },
            "狀態診斷測試",
            diagnostics.sink()));
        REQUIRE(diagnostics.diagnostics().size() == 1);
        return diagnostics.diagnostics().front().message;
    };

    CHECK(diagnosticFor(R"(
時機: 命中
套用狀態:
  狀態: 眩暈
  持續幀數: 30
)").find("狀態「眩暈」需要「重複套用」") != std::string::npos);

    CHECK(diagnosticFor(R"(
時機: 主彈命中
套用狀態:
  狀態: 七星
  增加層數: 7
  層數上限: 7
  持續幀數: 150
)").find("狀態「七星」必須使用「設定印記層數」") != std::string::npos);

    const auto missingTrueQiValue = diagnosticFor(R"(
時機: 攻擊提交
套用狀態:
  狀態: 真氣
  增加層數: 1
  層數上限: 10
  效果:
    每層生效: {}
)");
    CHECK(missingTrueQiValue.ends_with(
        "狀態「真氣」的效果缺少「命中附加純粹傷害」"));

    const auto wrongStatusValue = diagnosticFor(R"(
時機: 攻擊提交
套用狀態:
  狀態: 真氣
  增加層數: 1
  層數上限: 10
  效果:
    每層生效:
      速度降低百分比: 9
)");
    CHECK(wrongStatusValue.ends_with(
        "狀態「真氣」的效果不允許欄位「速度降低百分比」；此欄位屬於狀態「寒毒」"));

    const auto sharedWrongStatusValue = diagnosticFor(R"(
時機: 攻擊提交
套用狀態:
  狀態: 真氣
  增加層數: 1
  層數上限: 10
  效果:
    每層生效:
      阻止本次施放: true
)");
    CHECK(sharedWrongStatusValue.ends_with(
        "狀態「真氣」的效果不允許欄位「阻止本次施放」；"
        "此欄位屬於狀態「化勁」、「刺目」"));

    const auto malformedTrueQiValue = diagnosticFor(R"(
時機: 攻擊提交
套用狀態:
  狀態: 真氣
  增加層數: 1
  層數上限: 10
  效果:
    每層生效:
      命中附加純粹傷害: 不是整數
)");
    CHECK(malformedTrueQiValue.find(
        "狀態「真氣」的效果「命中附加純粹傷害」不是有效數值：數值不是有效整數")
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

TEST_CASE("ChessBattleEffects_ShippedPersistentHitCapsPreserveAllThreePercentages",
          "[battle][effects][damage-modifier][content]")
{
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content != nullptr);

    std::vector<int> capPercentages;
    const auto inspectRules = [&](std::span<const EffectRule> rules)
    {
        for (const auto& rule : rules)
        {
            for (const auto& action : rule.actions)
            {
                const auto* modifier = std::get_if<ModifyDamageAction>(
                    &action.value);
                if (!modifier
                    || modifier->operation
                        != DamageModifierOperation::CapSingleHitAtMaxHpPercent)
                {
                    continue;
                }
                CHECK(rule.event == EffectEvent::BattleInitialized);
                CHECK(modifier->perspective
                    == DamageModifierPerspective::Incoming);
                CHECK(modifier->stage == DamageModifierStage::Final);
                CHECK(modifier->channel == DamageChannel::All);
                CHECK(modifier->durationFrames == 0);
                CHECK(modifier->stack == EffectStackPolicy::Independent);
                CHECK(modifier->amount.base == EffectNumberBase::Constant);
                CHECK_FALSE(modifier->amount.multiplierBase);
                CHECK_FALSE(modifier->amount.status);
                CHECK_FALSE(modifier->amount.statusEffect);
                CHECK_FALSE(modifier->amount.stateSlot);
                CHECK(modifier->amount.percent == 0);
                capPercentages.push_back(modifier->amount.flat);
            }
        }
    };

    for (const auto& combo : content->combos())
        for (const auto& threshold : combo.thresholds)
            inspectRules(threshold.rules);
    for (const auto& equipment : content->equipment())
        inspectRules(equipment.rules);
    for (const auto& synergy : content->equipmentSynergies())
        inspectRules(synergy.rules);
    for (const auto& neigong : content->neigong())
        inspectRules(neigong.rules);

    std::ranges::sort(capPercentages);
    CHECK(capPercentages == std::vector<int>{ 8, 8, 12 });
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
