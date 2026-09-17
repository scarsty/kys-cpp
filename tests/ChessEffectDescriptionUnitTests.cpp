#include "ChessBattleEffectTestHelpers.h"
#include "ChessGameplayEffect.h"
#include "battle/BattleEffectSystem.h"

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <regex>

using namespace KysChess;
using namespace KysChess::Test;

namespace
{
std::vector<GameplayEffect> configured(std::string_view yaml)
{
    std::vector<GameplayEffect> effects;
    std::vector<EffectRule> rules;
    std::uint64_t id{};
    REQUIRE(parseGameplayEffects(YAML::Load(std::string(yaml)), effects, rules, id, "描述測試"));
    return effects;
}
}

TEST_CASE("Every registered compact effect follows the same wording conventions", "[chess][effects][description][catalog]")
{
    const std::regex periodicPrefix("^每[0-9]+幀[^：]");
    for (const auto& entry : gameplayEffectCatalog())
    {
        std::vector<int> values;
        for (const auto& parameter : entry.parameters)
            values.push_back(parameter.choices.empty() ? std::clamp(20, parameter.minimum, parameter.maximum) : 0);

        const auto checkDescription = [&] {
            const auto effect = entry.create(values);
            const auto text = joinEffectDescriptionRows(describeGameplayEffects(
                std::span<const GameplayEffect>(&effect, 1), EffectDescriptionStyle::Compact));
            INFO(entry.name << " | " << text);
            CHECK_FALSE(text.empty());
            for (const auto trigger : {"出招", "命中", "技能命中", "主彈命中", "受擊", "擊殺", "開場", "破盾", "陣亡", "絕招"})
            {
                if (text.starts_with(trigger) && !text.starts_with("出招結束：") && !text.starts_with("絕招就緒："))
                    CHECK(text.starts_with(std::string(trigger) + "："));
            }
            CHECK_FALSE(std::regex_search(text, periodicPrefix));
            for (const auto detail : {"。", "至少1", "最低1", "每次最低", "重複施加", "刷新時間", "此來源", "保留當前", "不影響自身", "遇障礙停止", "，持續"})
                CHECK_FALSE(text.contains(detail));
        };
        checkDescription();
        for (std::size_t index = 0; index < entry.parameters.size(); ++index)
        {
            const auto& parameter = entry.parameters[index];
            for (int choice = 1; choice < static_cast<int>(parameter.choices.size()); ++choice)
            {
                values[index] = choice;
                checkDescription();
            }
            if (!parameter.choices.empty()) values[index] = 0;
        }
    }
}

TEST_CASE("Descriptions belong to reusable effects and follow their parameters", "[chess][effects][description]")
{
    auto first = configured("[{類型: 技能增傷, 百分比: 15}]");
    auto second = configured("[{類型: 技能增傷, 百分比: 25}]");
    CHECK(first.front()->describe(EffectDescriptionStyle::Compact) == "技能傷害+15%。");
    CHECK(second.front()->describe(EffectDescriptionStyle::Compact) == "技能傷害+25%。");
    CHECK(joinEffectDescriptionRows(describeGameplayEffects(first, EffectDescriptionStyle::Compact))
        == "技能傷害+15%");
    CHECK(first.front()->describe(EffectDescriptionStyle::Full) == "技能傷害+15%。");
}

TEST_CASE("Descriptions distinguish damage channels and explain debuff consequences", "[chess][effects][description]")
{
    auto effects = configured(R"(
- {類型: 固定傷害加成, 點數: 15}
- {類型: 目標周圍純粹傷害, 每星傷害: 100, 方形邊長: 5}
- {類型: 命中禁療減速, 持續幀數: 90}
- {類型: 命中易傷減療, 持續幀數: 120}
)");
    const auto text = joinEffectDescriptionRows(describeGameplayEffects(effects, EffectDescriptionStyle::Full));
    CHECK(text.find("傷害+15點") != std::string::npos);
    CHECK(text.find("每星100純粹傷害") != std::string::npos);
    CHECK(text.find("無法恢復生命且速度降低25%") != std::string::npos);
    CHECK(text.find("承受傷害增加25%、受到治療減少75%") != std::string::npos);
    CHECK(text.find("寒毒") == std::string::npos);
    CHECK(text.find("枯骨") == std::string::npos);
}

TEST_CASE("Compact cards omit terminal punctuation and retain mechanical qualifiers", "[chess][effects][description]")
{
    auto effects = configured(R"(
- {類型: 固定承傷修正, 點數: -15}
- {類型: 命中禁療減速, 持續幀數: 90}
- {類型: 保護低血友軍, 友軍數: 5, 承傷上限百分比: 15}
)");
    const auto rows = effectDescriptionTextRows(describeGameplayEffects(effects, EffectDescriptionStyle::Compact));
    REQUIRE(rows.size() == 3);
    for (const auto& row : rows) CHECK_FALSE(row.ends_with("。"));
    CHECK(rows[0] == "固定減傷15點");
    CHECK(rows[1] == "命中：禁療、速度-25%，90幀");
    CHECK(rows[2] == "出招：血比最低5名友軍，下次承傷≤血上限15%");
    const auto full = joinEffectDescriptionRows(describeGameplayEffects(effects, EffectDescriptionStyle::Full));
    CHECK(full.find("在計算防禦前生效。") != std::string::npos);
    CHECK(full.find("無法恢復生命且速度降低25%") != std::string::npos);
    CHECK(full.find("每人抵擋一次傷害") != std::string::npos);
}

TEST_CASE("Combo threshold descriptions append management rules after battle effects", "[chess][effects][description]")
{
    auto effects = configured("[{類型: 技能增傷, 百分比: 15}]");
    const std::vector<ChessNonBattleRule> rules{FreeShopRefreshRule{}, BattleMapChoiceRule{}};

    const auto full = effectDescriptionTextRows(describeGameplayEffectsAndManagementRules(
        effects, rules, EffectDescriptionStyle::Full));
    REQUIRE(full.size() == 3);
    CHECK(full[0] == "技能傷害+15%。");
    CHECK(full[1] == "勝利後獲得一次免費商店刷新；尚未使用時不重複累積");
    CHECK(full[2] == "戰鬥開始前可從合適的戰場中選擇一個");

    const auto compact = effectDescriptionTextRows(describeGameplayEffectsAndManagementRules(
        effects, rules, EffectDescriptionStyle::Compact));
    REQUIRE(compact.size() == 3);
    CHECK(compact[0] == "技能傷害+15%");
    CHECK(compact[1] == "勝利：免費刷新1次");
    CHECK(compact[2] == "戰前：可選戰場");

    const std::vector<ChessNonBattleRule> goldRules{VictoryGoldRule{2}};
    const auto managementOnly = effectDescriptionTextRows(describeGameplayEffectsAndManagementRules(
        {}, goldRules, EffectDescriptionStyle::Compact));
    REQUIRE(managementOnly.size() == 1);
    CHECK(managementOnly[0] == "勝利：+2×最高存活星級金幣");
}

TEST_CASE("Compound effects describe their complete lifecycle without runtime reconstruction", "[chess][effects][description]")
{
    auto effects = configured("[{類型: 承傷蓄力加傷, 傷害轉換百分比: 75}]");
    const auto compact = effects.front()->describe(EffectDescriptionStyle::Compact);
    const auto full = effects.front()->describe(EffectDescriptionStyle::Full);
    CHECK(compact.find("最大單次") != std::string::npos);
    CHECK(compact.find("75%") != std::string::npos);
    CHECK(full.find("首次命中後消耗") != std::string::npos);
    CHECK(full.find("各來源獨立記錄") != std::string::npos);
    CHECK(full.find("狀態槽") == std::string::npos);
    CHECK(full.find("太極") == std::string::npos);
}

TEST_CASE("All description rows retain complete prose for UI wrapping", "[chess][effects][description]")
{
    auto effects = configured(R"(
- {類型: 命中禁療減速, 持續幀數: 90}
- {類型: 命中回血, 生命: 20}
)");
    const auto rendered = describeGameplayEffects(effects, EffectDescriptionStyle::Full);
    REQUIRE(rendered.sections.size() == 1);
    REQUIRE(rendered.sections.front().blocks.size() == 2);
    CHECK(rendered.sections.front().blocks[0].rows[0].wrapping == DisplayTextWrapping::Prose);
    CHECK(rendered.sections.front().blocks[1].rows[0].breakBefore == EffectDescriptionSemanticBreak::Block);
    CHECK(effectDescriptionTextRows(rendered).size() == 2);
    CHECK(describeGameplayEffects({}, EffectDescriptionStyle::Full).sections.empty());
}

TEST_CASE("Ratio descriptions reflect the actual integer combat result", "[chess][effects][description][number]")
{
    using namespace KysChess::Battle;
    EffectUnitSnapshot owner;
    owner.maxHp = 100;
    owner.maxMp = 100;
    EffectEventData context;
    context.header.owner = &owner;

    struct Sample
    {
        std::string_view yaml;
        std::string_view compactFragment;
        std::string_view fullFragment;
        int emptyResult{};
        int halfResult{};
        int fullResult{};
    };
    const Sample samples[]{
        {"[{類型: 失血固定承傷修正, 點數: -20}]", "最多減少20點", "×-20點", 0, -10, -20},
        {"[{類型: 失血百分比承傷修正, 百分比: -14}]", "最多減少14%", "×-14", 0, -7, -14},
        {"[{類型: 失血百分比承傷修正, 百分比: -28}]", "最多減少28%", "×-28", 0, -14, -28},
        {"[{類型: 內力比例技能增傷, 基礎傷害百分比: 100, 滿內增傷百分比: 35}]",
            "100%至135%", "×35個百分點", 100, 117, 135},
        {"[{類型: 失血固定承傷修正, 點數: 1}]", "最多增加1點", "×1點", 0, 0, 1},
        {"[{類型: 失血百分比承傷修正, 百分比: 3}]", "最多增加3%", "×3", 0, 1, 3},
    };
    for (const auto& sample : samples)
    {
        INFO(sample.yaml);
        const auto effect = configured(sample.yaml).front();
        CHECK(effect->describe(EffectDescriptionStyle::Compact).find(sample.compactFragment) != std::string::npos);
        CHECK(effect->describe(EffectDescriptionStyle::Full).find(sample.fullFragment) != std::string::npos);
        const auto rules = effect->buildRules();
        const auto& amount = std::get<ModifyDamageAction>(rules.front().actions.front().value).amount;
        owner.hp = 100;
        owner.mp = 0;
        CHECK(BattleEffectSystem::evaluateNumber(amount, context, owner) == sample.emptyResult);
        owner.hp = 50;
        owner.mp = 50;
        CHECK(BattleEffectSystem::evaluateNumber(amount, context, owner) == sample.halfResult);
        owner.hp = 0;
        owner.mp = 100;
        CHECK(BattleEffectSystem::evaluateNumber(amount, context, owner) == sample.fullResult);
    }
}

TEST_CASE("Short compact descriptions preserve full explanations and area qualifiers", "[chess][effects][description]")
{
    auto effects = configured(R"(
- {類型: 出招全隊回內, 回復內力: 30}
- {類型: 目標周圍純粹傷害, 每星傷害: 100, 方形邊長: 5}
)");
    const auto rows = effectDescriptionTextRows(describeGameplayEffects(effects, EffectDescriptionStyle::Compact));
    REQUIRE(rows.size() == 2);
    CHECK(rows[0] == "出招：全隊回30內");
    CHECK(rows[1] == "出招：目標周圍方形邊長5格，每星100純粹傷害");
    CHECK(effects[0]->describe(EffectDescriptionStyle::Full) == "出招時全隊回復30內力。");
    CHECK(effects[1]->describe(EffectDescriptionStyle::Full).find("同次出招對每名目標最多生效一次") != std::string::npos);
}
