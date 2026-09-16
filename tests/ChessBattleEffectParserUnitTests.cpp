#include "ChessBattleEffectTestHelpers.h"
#include "ChessGameplayEffect.h"

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>

using namespace KysChess;
using namespace KysChess::Test;

TEST_CASE("Named effect definitions preserve the ordered runtime contracts", "[chess][effects][named][content]")
{
    const auto fixtures = YAML::LoadFile("tests/data/gameplay-effect-contracts.yaml");
    REQUIRE(fixtures.size() == 229);
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
        ChessGameContentData data;
        ChessMagicEffectDefinition definition;
        definition.rules = rules;
        data.magicEffects.push_back(std::move(definition));
        CHECK(chessSha256Hex(ChessGameContent(std::move(data)).contentFingerprint())
            == fixture["指紋"].as<std::string>());
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

TEST_CASE("Magic loader rejects duplicate identities and invalid source attachments", "[chess][effects][named][magic]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    REQUIRE(loadMagicEffectsFile("config/chess_magic_effects.yaml", definitions));
    REQUIRE(definitions.size() == 59);
    auto root = YAML::LoadFile("config/chess_magic_effects.yaml");
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
