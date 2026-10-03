#include "ChessBattleEffectTestHelpers.h"
#include "ChessGameplayEffect.h"

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>

using namespace KysChess;
using namespace KysChess::Test;

TEST_CASE("Named effect definitions load as ordered runtime contracts", "[chess][effects][named][content]")
{
    const auto fixtures = YAML::LoadFile("tests/data/gameplay-effect-contracts.yaml");
    REQUIRE(fixtures.size() > 0);
    for (const auto& fixture : fixtures)
    {
        INFO(fixture["來源"].as<std::string>());
        const auto configured = fixture["效果"];
        std::vector<GameplayEffect> effects;
        std::vector<EffectRule> rules;
        std::uint64_t id{};
        ChessDiagnosticCollector diagnostics;
        REQUIRE(parseGameplayEffects(configured, effects, rules, id, "固定契約樣本", diagnostics.sink()));
        CHECK_FALSE(diagnostics.hasErrors());
        CHECK(id == rules.size());
        for (const auto& effect : effects)
        {
            CHECK_FALSE(effect->name().empty());
            CHECK_FALSE(effect->describe(EffectDescriptionStyle::Compact).empty());
            CHECK_FALSE(effect->describe(EffectDescriptionStyle::Full).empty());
        }
    }
}

TEST_CASE("Named effect loading rejects malformed parameters atomically", "[chess][effects][named][config]")
{
    const std::string_view invalid[]{
        R"([{類型: 不存在的效果}])",
        R"([{類型: 技能增傷}])",
        R"([{類型: 技能增傷, 百分比: abc}])",
        R"([{類型: 技能增傷, 百分比: 15, 時機: 開場}])",
        R"([{類型: 技能增傷, 百分比: 15, 百分比: 20}])",
        R"([{時機: 開場, 屬性修正: {屬性: 技能傷害, 數值: 15}}])",
        R"([{類型: 機率命中眩暈, 機率百分比: 101, 持續幀數: 20}])",
        R"([{類型: 定時生命回復, 間隔幀數: 0, 生命百分比: 5}])",
        R"([{類型: 出招疊加屬性, 屬性: 格擋率, 每層百分比: 2, 層數上限: 0}])",
        R"([{類型: 命中施毒, 中毒次數: 3, 生命傷害百分比: 7, 中毒間隔幀數: 30}])",
        R"([{類型: 技能增傷, 百分比: 15}, {類型: 不存在的效果}])",
    };
    for (const auto text : invalid)
    {
        INFO(text);
        std::vector<GameplayEffect> effects;
        std::vector<EffectRule> rules;
        std::uint64_t id = 40;
        REQUIRE(parseGameplayEffects(YAML::Load("[{類型: 技能增傷, 百分比: 15}]"),
            effects, rules, id, "有效效果"));
        const auto original = effects.front();
        const auto next = id;
        ChessDiagnosticCollector diagnostics;
        CHECK_FALSE(parseGameplayEffects(YAML::Load(std::string(text)), effects, rules,
            id, "不合法效果", diagnostics.sink()));
        CHECK(diagnostics.hasErrors());
        REQUIRE(effects.size() == 1);
        CHECK(effects.front() == original);
        CHECK(id == next);
    }
}

TEST_CASE("Named poison effects own the canonical interval", "[chess][effects][named][poison]")
{
    const auto configured = YAML::Load(R"(
- 類型: 蓄積死亡毒爆
  每次層數: 1
  層數上限: 5
  半徑格數: 5
  每星每層傷害: 60
  中毒次數: 4
  中毒生命百分比: 10
- 類型: 全體引毒再施毒
  中毒次數: 3
  生命傷害百分比: 7
- 類型: 命中施毒
  中毒次數: 3
  生命傷害百分比: 7
)");
    std::vector<GameplayEffect> effects;
    std::vector<EffectRule> rules;
    std::uint64_t id{};

    REQUIRE(parseGameplayEffects(configured, effects, rules, id, "固定中毒間隔測試"));
    REQUIRE(effects.size() == 3);
    for (const auto& effect : effects)
    {
        CHECK(effect->describe(EffectDescriptionStyle::Full).find("每30幀")
            != std::string::npos);
    }
}

TEST_CASE("Named effects bind parameters to ordered runtime rules", "[chess][effects][named]")
{
    std::vector<GameplayEffect> effects;
    std::vector<EffectRule> rules;
    std::uint64_t id = 200;
    REQUIRE(parseGameplayEffects(YAML::Load(R"(
- 類型: 承傷蓄力加傷
  傷害轉換百分比: 75
- 類型: 命中回血
  生命: 23
)"), effects, rules, id, "可重用效果"));
    REQUIRE(rules.size() == 4);
    CHECK(rules[0].event == EffectEvent::DamageResolved);
    CHECK(rules[1].event == EffectEvent::AttackCommitted);
    CHECK(rules[2].event == EffectEvent::MainProjectileBeforeDamage);
    CHECK(rules[3].event == EffectEvent::DamageResolved);
    for (std::size_t i = 0; i < rules.size(); ++i) CHECK(rules[i].id.value == 200 + i);
    const auto& action = std::get<StateMachineAction>(rules[2].actions.front().value);
    CHECK(std::get<ConsumeRecordedMaximumAction>(action).percent == 75);
    CHECK(std::get<ChangeResourceAction>(rules[3].actions.front().value).amount.flat == 23);
    CHECK(effects[0]->describe(EffectDescriptionStyle::Compact).find("75%") != std::string::npos);
}

TEST_CASE("御劍護身 grants sword allies one refreshing direct-hit guard", "[chess][effects][named][sword-guard]")
{
    std::vector<GameplayEffect> effects;
    std::vector<EffectRule> rules;
    std::uint64_t id{};
    REQUIRE(parseGameplayEffects(
        YAML::Load("[{類型: 御劍護身, 減傷百分比: 40, 持續幀數: 100}]"),
        effects,
        rules,
        id,
        "御劍護身測試"));

    REQUIRE(effects.size() == 1);
    REQUIRE(rules.size() == 1);
    const auto& rule = rules.front();
    CHECK(rule.event == EffectEvent::AttackCommitted);
    CHECK(rule.selector.kind == EffectSelectorKind::AlliesUsingMartialCategory);
    CHECK(rule.selector.requiredMartialCategory == EffectMartialCategory::Sword);
    REQUIRE(rule.actions.size() == 1);
    const auto& application = std::get<ApplyStatusAction>(rule.actions.front().value);
    CHECK(application.status == BattleStatusKind::SwordGuard);
    CHECK(application.durationFrames == 100);
    CHECK(application.reapplication == StatusReapplicationPolicy::RefreshDuration);
    CHECK(std::get<SetStatusTriggerCharges>(application.quantity).count == 1);
    REQUIRE(application.behavior);
    REQUIRE(application.behavior->rules.size() == 1);
    const auto& guard = application.behavior->rules.front();
    CHECK(guard.event == EffectEvent::HitBeforeDamage);
    CHECK(guard.observation == EffectObservationScope::StatusHolderEventTarget);
    CHECK(guard.selector.kind == EffectSelectorKind::StatusHolder);
    REQUIRE(guard.actions.size() == 2);
    const auto& reduction = std::get<ModifyDamageAction>(guard.actions.front().value);
    CHECK(reduction.perspective == DamageModifierPerspective::Incoming);
    CHECK(reduction.stage == DamageModifierStage::Final);
    CHECK(reduction.channel == DamageChannel::All);
    CHECK(reduction.amount.flat == -40);
    CHECK(reduction.operation == DamageModifierOperation::PercentAdd);
    CHECK(std::holds_alternative<ConsumeThisStatusAction>(guard.actions.back().value));
    CHECK(effects.front()->describe(EffectDescriptionStyle::Full).contains("40%"));
    CHECK(effects.front()->describe(EffectDescriptionStyle::Full).contains("非攻擊傷害不會觸發"));
}

TEST_CASE("Magic loader rejects duplicate identities and invalid source attachments", "[chess][effects][named][magic]")
{
    // 測試自有設定樣本，不讀取頂層設定檔。
    const std::string_view fixture = R"(
絕招:
  - 武功: 1
    名稱: 測試武學
    效果:
      - 類型: 命中忽略防禦
        忽略防禦百分比: 100
  - 武功: 2
    名稱: 測試內功
    效果:
      - 類型: 出招減傷
        減傷百分比: 40
        持續幀數: 100
)";
    std::vector<ChessMagicEffectDefinition> definitions;
    auto root = YAML::Load(std::string(fixture));
    REQUIRE(parseMagicEffects(root, definitions, "設定樣本"));
    REQUIRE(definitions.size() == 2);
    root["絕招"].push_back(YAML::Clone(root["絕招"][0]));
    CHECK_FALSE(parseMagicEffects(root, definitions, "重複武功"));
    for (const auto effect : {"[{類型: 技能增傷, 百分比: 15}]",
                             "[{類型: 成員低血全員增攻, 生命門檻百分比: 30, 攻擊百分比: 100, 持續幀數: 200}]"})
    {
        auto invalid = YAML::Load("絕招: [{武功: 1, 名稱: 測試, 效果: []}]");
        invalid["絕招"][0]["效果"] = YAML::Load(effect);
        CHECK_FALSE(parseMagicEffects(invalid, definitions, "不相容的效果來源"));
    }
}

TEST_CASE("Composable effects validate named choices and default omitted formula terms", "[chess][effects][composition]")
{
    std::vector<GameplayEffect> effects;
    std::vector<EffectRule> rules;
    std::uint64_t id{};
    REQUIRE(parseGameplayEffects(YAML::Load("[{類型: 出招護盾, 每星護盾: 70}]"), effects, rules, id, "可選數值"));
    REQUIRE(rules.size() == 1);
    const auto& shield = std::get<ChangeResourceAction>(rules[0].actions[0].value);
    CHECK(shield.amount.flat == 0);
    CHECK(shield.amount.percent == 0);
    REQUIRE(shield.additionalAmount);
    CHECK(shield.additionalAmount->base == EffectNumberBase::SourceStar);
    CHECK(shield.additionalAmount->percent == 7000);
    CHECK(effects[0]->describe(EffectDescriptionStyle::Compact).find("70×星級") != std::string::npos);

    effects.clear();
    rules.clear();
    id = 0;
    REQUIRE(parseGameplayEffects(
        YAML::Load("[{類型: 出招全隊攻擊加成, 每星攻擊: 66, 持續幀數: 100}]"),
        effects,
        rules,
        id,
        "每星全隊攻擊"));
    REQUIRE(rules.size() == 1);
    const auto& attack = std::get<ModifyAttributeAction>(rules[0].actions[0].value);
    CHECK(attack.amount.base == EffectNumberBase::SourceStar);
    CHECK(attack.amount.percent == 6600);
    CHECK(attack.amount.flat == 0);
    CHECK(effects[0]->describe(EffectDescriptionStyle::Compact).find("66×星級") != std::string::npos);

    effects.clear();
    rules.clear();
    id = 0;
    REQUIRE(parseGameplayEffects(
        YAML::Load("[{類型: 出招全隊防禦加成, 每星防禦: 66, 持續幀數: 100}]"),
        effects,
        rules,
        id,
        "每星全隊防禦"));
    REQUIRE(rules.size() == 1);
    const auto& defence = std::get<ModifyAttributeAction>(rules[0].actions[0].value);
    CHECK(rules[0].event == EffectEvent::AttackCommitted);
    CHECK(rules[0].selector.kind == EffectSelectorKind::Allies);
    CHECK(defence.attribute == BattleAttribute::Defence);
    CHECK(defence.amount.base == EffectNumberBase::SourceStar);
    CHECK(defence.amount.percent == 6600);
    CHECK(defence.amount.flat == 0);
    CHECK(defence.durationFrames == 100);
    CHECK(defence.stack == EffectStackPolicy::Refresh);
    CHECK(effects[0]->describe(EffectDescriptionStyle::Compact).find("66×星級") != std::string::npos);

    for (const auto invalid : {
        "[{類型: 出招臨時屬性加成, 屬性: 不存在, 百分比: 25, 持續幀數: 60}]",
        "[{類型: 出招臨時屬性加成, 屬性: 3, 百分比: 25, 持續幀數: 60}]",
        "[{類型: 出招臨時屬性加成, 百分比: 25, 持續幀數: 60}]",
        "[{類型: 出招護盾, 每星護盾: -1}]",
        "[{類型: 出招格擋護盾, 格擋百分比: 25, 持續幀數: 60, 每星護盾: 70}]"})
    {
        CHECK_FALSE(parseGameplayEffects(YAML::Load(invalid), effects, rules, id, "錯誤組合"));
    }
}

TEST_CASE("Authored star-scaled spiral lifetime and target fire area preserve their formulas",
          "[chess][effects][composition][star]")
{
    std::vector<GameplayEffect> effects;
    std::vector<EffectRule> rules;
    std::uint64_t id{};
    REQUIRE(parseGameplayEffects(
        YAML::Load(R"(
- {類型: 機率螺旋流血攻擊, 機率百分比: 10, 彈道數: 3, 流血層數: 1, 基礎幀數: 20, 每星幀數: 15}
- {類型: 持續傷害區域, 半徑格數: 5, 持續幀數: 180, 每星傷害: 25, 間隔幀數: 20}
)"),
        effects,
        rules,
        id,
        "星級效果"));
    REQUIRE(rules.size() == 2);

    const auto& spiral = std::get<ModifyAttackAction>(rules[0].actions[0].value);
    const auto& behavior = std::get<ExpandingSpiralAttackBehavior>(spiral.runtimeBehavior);
    CHECK(behavior.baseFrames == 20);
    CHECK(behavior.framesPerStar == 15);

    CHECK(rules[1].event == EffectEvent::MainProjectileBeforeDamage);
    const auto& area = std::get<CreateAreaAction>(rules[1].actions[0].value);
    CHECK(area.anchor == AreaAnchor::HitPosition);
    CHECK(area.radiusTiles == 5);
    REQUIRE(area.modifiers.size() == 1);
    CHECK(area.modifiers[0].amount.base == EffectNumberBase::SourceStar);
    CHECK(area.modifiers[0].amount.percent == 2500);
}


TEST_CASE("Lore equipment tuning rejects invalid windows and accepts activation on every cast",
          "[chess][effects][lore-equipment]")
{
    for (const auto text : {
        "[{類型: 輪轉護身, 開場層數: 6, 層數上限: 5, 減傷百分比: 30, 回復間隔幀數: 60, 每次回復層數: 1}]",
        "[{類型: 輪轉護身, 開場層數: 5, 層數上限: 5, 減傷百分比: 30, 回復間隔幀數: 0, 每次回復層數: 1}]",
        "[{類型: 化毒養身, 毒傷轉化百分比: 101, 治療上限窗口幀數: 30, 治療上限生命百分比: 2}]",
        "[{類型: 化毒養身, 毒傷轉化百分比: 25, 治療上限窗口幀數: 0, 治療上限生命百分比: 2}]"})
    {
        std::vector<GameplayEffect> effects;
        std::vector<EffectRule> rules;
        std::uint64_t id{};
        CHECK_FALSE(parseGameplayEffects(YAML::Load(text), effects, rules, id, "測試"));
    }
    std::vector<GameplayEffect> effects;
    std::vector<EffectRule> rules;
    std::uint64_t id{};
    REQUIRE(parseGameplayEffects(YAML::Load(R"([
        {類型: 清音護心, 出招次數: 1, 生命護盾百分比: 12},
        {類型: 五輪齊發, 出招次數: 1, 目標數: 5, 傷害百分比: 40}
    ])"), effects, rules, id, "測試"));
}
