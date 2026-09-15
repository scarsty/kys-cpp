#include "ChessNonBattleRules.h"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <array>
#include <format>
#include <type_traits>
#include <utility>

namespace KysChess
{
namespace
{

bool rejectUnknownFields(
    const YAML::Node& node,
    std::span<const std::string_view> allowed,
    std::string_view context,
    const ChessDiagnosticSink& diagnostics)
{
    for (const auto& field : node)
    {
        const auto key = field.first.as<std::string>();
        if (std::ranges::find(allowed, key) != allowed.end())
        {
            continue;
        }
        emitChessDiagnostic(
            diagnostics,
            ChessDiagnosticSeverity::Error,
            "非戰鬥規則",
            std::format("{}含有未知欄位「{}」", context, key));
        return false;
    }
    return true;
}

void appendGrowthPart(std::vector<std::string>& parts, std::string_view name, int value, bool valueFirst)
{
    if (value != 0)
    {
        parts.push_back(valueFirst ? std::format("{:+}{}", value, name) : std::format("{}{:+}", name, value));
    }
}

std::string joinParts(const std::vector<std::string>& parts, std::string_view separator)
{
    std::string result;
    for (const auto& part : parts)
    {
        if (!result.empty())
        {
            result += separator;
        }
        result += part;
    }
    return result;
}

}

ChessNonBattleRuleKind chessNonBattleRuleKind(const ChessNonBattleRule& rule)
{
    return std::visit(
        [](const auto& value) -> ChessNonBattleRuleKind
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, CountsAsComboRule>) return ChessNonBattleRuleKind::CountsAsCombo;
            if constexpr (std::is_same_v<T, VictoryGoldRule>) return ChessNonBattleRuleKind::VictoryGold;
            if constexpr (std::is_same_v<T, FreeShopRefreshRule>) return ChessNonBattleRuleKind::FreeShopRefresh;
            if constexpr (std::is_same_v<T, BattleMapChoiceRule>) return ChessNonBattleRuleKind::BattleMapChoice;
            if constexpr (std::is_same_v<T, FightWinGrowthRule>) return ChessNonBattleRuleKind::FightWinGrowth;
        },
        rule);
}

std::optional<ChessNonBattleRule> parseChessNonBattleRule(
    const YAML::Node& node,
    const ChessTextConverter& toTraditional,
    std::string_view context,
    const ChessDiagnosticSink& diagnostics)
{
    if (!node.IsMap() || !node["類型"])
    {
        emitChessDiagnostic(
            diagnostics,
            ChessDiagnosticSeverity::Error,
            "非戰鬥規則",
            std::format("{}必須是含有「類型」的物件", context));
        return std::nullopt;
    }

    try
    {
        const auto type = node["類型"].as<std::string>();
        if (type == "計作羈絆")
        {
            static constexpr std::array allowed{std::string_view{"類型"}, std::string_view{"羈絆"}};
            if (!rejectUnknownFields(node, allowed, context, diagnostics))
            {
                return std::nullopt;
            }
            if (!node["羈絆"])
            {
                emitChessDiagnostic(
                    diagnostics,
                    ChessDiagnosticSeverity::Error,
                    "非戰鬥規則",
                    std::format("{}的「計作羈絆」缺少「羈絆」", context));
                return std::nullopt;
            }
            auto comboName = toTraditional(node["羈絆"].as<std::string>());
            if (comboName.empty())
            {
                emitChessDiagnostic(
                    diagnostics,
                    ChessDiagnosticSeverity::Error,
                    "非戰鬥規則",
                    std::format("{}的「羈絆」不可為空", context));
                return std::nullopt;
            }
            return CountsAsComboRule{std::move(comboName)};
        }
        if (type == "勝利金幣")
        {
            static constexpr std::array allowed{
                std::string_view{"類型"},
                std::string_view{"每最高存活星級"},
            };
            if (!rejectUnknownFields(node, allowed, context, diagnostics))
            {
                return std::nullopt;
            }
            if (!node["每最高存活星級"])
            {
                emitChessDiagnostic(
                    diagnostics,
                    ChessDiagnosticSeverity::Error,
                    "非戰鬥規則",
                    std::format("{}的「勝利金幣」缺少「每最高存活星級」", context));
                return std::nullopt;
            }
            const int value = node["每最高存活星級"].as<int>();
            if (value <= 0)
            {
                emitChessDiagnostic(
                    diagnostics,
                    ChessDiagnosticSeverity::Error,
                    "非戰鬥規則",
                    std::format("{}的「每最高存活星級」必須大於0", context));
                return std::nullopt;
            }
            return VictoryGoldRule{value};
        }
        if (type == "免費刷新")
        {
            static constexpr std::array allowed{std::string_view{"類型"}};
            if (!rejectUnknownFields(node, allowed, context, diagnostics))
            {
                return std::nullopt;
            }
            return FreeShopRefreshRule{};
        }
        if (type == "戰場選擇")
        {
            static constexpr std::array allowed{std::string_view{"類型"}};
            if (!rejectUnknownFields(node, allowed, context, diagnostics))
            {
                return std::nullopt;
            }
            return BattleMapChoiceRule{};
        }
        if (type == "勝場成長")
        {
            static constexpr std::array allowed{
                std::string_view{"類型"},
                std::string_view{"生命"},
                std::string_view{"攻擊"},
                std::string_view{"防禦"},
            };
            if (!rejectUnknownFields(node, allowed, context, diagnostics))
            {
                return std::nullopt;
            }
            FightWinGrowthRule result{
                node["生命"] ? node["生命"].as<int>() : 0,
                node["攻擊"] ? node["攻擊"].as<int>() : 0,
                node["防禦"] ? node["防禦"].as<int>() : 0,
            };
            if (result.maxHp == 0 && result.attack == 0 && result.defence == 0)
            {
                emitChessDiagnostic(
                    diagnostics,
                    ChessDiagnosticSeverity::Error,
                    "非戰鬥規則",
                    std::format("{}的「勝場成長」必須設定至少一項非零屬性", context));
                return std::nullopt;
            }
            return result;
        }

        emitChessDiagnostic(
            diagnostics,
            ChessDiagnosticSeverity::Error,
            "非戰鬥規則",
            std::format("{}使用未知類型「{}」", context, type));
        return std::nullopt;
    }
    catch (const YAML::Exception& error)
    {
        emitChessDiagnostic(
            diagnostics,
            ChessDiagnosticSeverity::Error,
            "非戰鬥規則",
            std::format("{}欄位格式錯誤：{}", context, error.what()));
        return std::nullopt;
    }
}

std::string chessNonBattleRuleDescription(const ChessNonBattleRule& rule, bool compact)
{
    return std::visit(
        [compact](const auto& value) -> std::string
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, CountsAsComboRule>)
            {
                return compact
                    ? std::format("計作{}", value.comboName)
                    : std::format("計作「{}」羈絆的一名成員", value.comboName);
            }
            if constexpr (std::is_same_v<T, VictoryGoldRule>)
            {
                return compact
                    ? std::format("勝利：+{}×最高存活星級金幣", value.perHighestSurvivorStar)
                    : std::format(
                        "勝利時，若至少一名本羈絆成員存活，獲得最高存活棋子星級×{}金幣",
                        value.perHighestSurvivorStar);
            }
            if constexpr (std::is_same_v<T, FreeShopRefreshRule>)
            {
                return compact ? "勝利：免費刷新1次" : "勝利後獲得一次免費商店刷新；尚未使用時不重複累積";
            }
            if constexpr (std::is_same_v<T, BattleMapChoiceRule>)
            {
                return compact ? "戰前：可選戰場" : "戰鬥開始前可從合適的戰場中選擇一個";
            }
            if constexpr (std::is_same_v<T, FightWinGrowthRule>)
            {
                std::vector<std::string> parts;
                appendGrowthPart(parts, compact ? "血上限" : "生命", value.maxHp, false);
                appendGrowthPart(parts, compact ? "攻" : "攻擊", value.attack, compact);
                appendGrowthPart(parts, compact ? "防" : "防禦", value.defence, compact);
                return compact
                    ? std::format("每勝：{}", joinParts(parts, "、"))
                    : std::format("本羈絆成員每個勝場使{}", joinParts(parts, "、"));
            }
        },
        rule);
}

std::vector<std::string> countsAsComboNames(std::span<const ChessNonBattleRule> rules)
{
    std::vector<std::string> result;
    for (const auto& rule : rules)
    {
        if (const auto* countsAs = std::get_if<CountsAsComboRule>(&rule))
        {
            result.push_back(countsAs->comboName);
        }
    }
    return result;
}

FightWinGrowthRule sumFightWinGrowth(std::span<const ChessNonBattleRule> rules)
{
    FightWinGrowthRule result;
    for (const auto& rule : rules)
    {
        if (const auto* growth = std::get_if<FightWinGrowthRule>(&rule))
        {
            result.maxHp += growth->maxHp;
            result.attack += growth->attack;
            result.defence += growth->defence;
        }
    }
    return result;
}

}  // namespace KysChess
