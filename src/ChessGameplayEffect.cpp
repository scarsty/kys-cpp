#include "ChessBattleEffectValidation.h"
#include "ChessGameplayEffectInternal.h"
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <set>

namespace KysChess
{

std::span<const GameplayEffectRegistration> gameplayEffectCatalog()
{
    static const auto entries = []
    {
        std::vector<GameplayEffectRegistration> result;
        GameplayEffects::appendAttributeEffects(result);
        GameplayEffects::appendAttackEffects(result);
        GameplayEffects::appendStatusEffects(result);
        GameplayEffects::appendRecoveryEffects(result);
        GameplayEffects::appendTacticalEffects(result);
        std::ranges::sort(result, {}, &GameplayEffectRegistration::name);
        assert(std::ranges::adjacent_find(result, {}, &GameplayEffectRegistration::name) == result.end());
        return result;
    }();
    return entries;
}

bool parseGameplayEffects(const YAML::Node& node, std::vector<GameplayEffect>& effects, std::vector<EffectRule>& rules,
                          std::uint64_t& nextRuleId, const std::string& context, const ChessDiagnosticSink& diagnostics)
{
    const auto fail = [&](std::string message)
    {
        emitChessDiagnostic(
            diagnostics, ChessDiagnosticSeverity::Error, "玩法效果", std::format("{}：{}", context, message));
        return false;
    };
    if (!node || !node.IsSequence()) { return fail("效果必須是列表"); }
    std::vector<GameplayEffect> parsedEffects;
    std::vector<EffectRule> parsedRules;
    auto id = nextRuleId;
    try
    {
        for (const auto& entry : node)
        {
            if (!entry.IsMap() || !entry["類型"]) { return fail("效果需要「類型」"); }
            const auto name = entry["類型"].as<std::string>();
            const auto catalog = gameplayEffectCatalog();
            const auto found = std::ranges::find(catalog, name, &GameplayEffectRegistration::name);
            if (found == catalog.end()) { return fail(std::format("未知效果「{}」", name)); }
            std::set<std::string> keys;
            for (const auto& field : entry)
            {
                const auto key = field.first.as<std::string>();
                if (!keys.insert(key).second) { return fail(std::format("「{}」重複欄位「{}」", name, key)); }
                if (key != "類型"
                    && std::ranges::find(found->parameters, key, &GameplayEffectParameter::name)
                        == found->parameters.end())
                {
                    return fail(std::format("「{}」未知參數「{}」", name, key));
                }
            }
            std::vector<int> values;
            for (const auto& parameter : found->parameters)
            {
                const auto valueNode = entry[std::string(parameter.name)];
                if (!valueNode && !parameter.defaultValue)
                {
                    return fail(std::format("「{}」缺少參數「{}」", name, parameter.name));
                }
                int value{ };
                if (!valueNode)
                {
                    value = *parameter.defaultValue;
                }
                else if (parameter.choices.empty())
                {
                    value = valueNode.as<int>();
                }
                else
                {
                    const auto choice = valueNode.as<std::string>();
                    const auto selected = std::ranges::find(parameter.choices, choice);
                    if (selected == parameter.choices.end())
                    {
                        return fail(std::format("「{}」參數「{}」不支援「{}」", name, parameter.name, choice));
                    }
                    value = static_cast<int>(selected - parameter.choices.begin());
                }
                if (value < parameter.minimum || value > parameter.maximum)
                {
                    return fail(std::format("「{}」參數「{}」必須介於{}與{}之間",
                                            name,
                                            parameter.name,
                                            parameter.minimum,
                                            parameter.maximum));
                }
                values.push_back(value);
            }
            auto effect = found->create(values);
            auto compiled = effect->buildRules();
            assert(!compiled.empty());
            for (auto& rule : compiled) { rule.id = EffectRuleId{id++}; }
            parsedRules.insert(
                parsedRules.end(), std::make_move_iterator(compiled.begin()), std::make_move_iterator(compiled.end()));
            parsedEffects.push_back(std::move(effect));
        }
    } catch (const YAML::Exception& ex) { return fail(std::format("參數格式錯誤：{}", ex.what())); }
    std::string error;
    if (!validateEffectRules(parsedRules, error)) { return fail(error); }
    effects = std::move(parsedEffects);
    rules = std::move(parsedRules);
    nextRuleId = id;
    return true;
}

}    // namespace KysChess
