#pragma once

#include "ChessBattleEffectTypes.h"
#include "ChessDiagnostics.h"

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace YAML
{
class Node;
}

namespace KysChess
{

enum class EffectDescriptionStyle
{
    Full,
    Compact
};

// 不可變的玩法定義。編譯後的規則交給既有戰鬥排程；描述只讀取此定義。
class GameplayEffectDefinition
{
public:
    virtual ~GameplayEffectDefinition() = default;
    virtual std::string_view name() const = 0;
    virtual std::vector<EffectRule> buildRules() const = 0;
    virtual std::string describe(EffectDescriptionStyle style) const = 0;
};

struct GameplayEffectParameter
{
    std::string_view name;
    int minimum{};
    int maximum{};
    std::span<const std::string_view> choices{};
    std::optional<int> defaultValue;
};

struct GameplayEffectRegistration
{
    std::string_view name;
    std::span<const GameplayEffectParameter> parameters;
    GameplayEffect (*create)(std::span<const int> values);
};

std::span<const GameplayEffectRegistration> gameplayEffectCatalog();

bool parseGameplayEffects(const YAML::Node& node, std::vector<GameplayEffect>& effects, std::vector<EffectRule>& rules,
                          std::uint64_t& nextRuleId, const std::string& context,
                          const ChessDiagnosticSink& diagnostics = {});

}    // namespace KysChess
