#pragma once

#include "ChessDiagnostics.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace YAML { class Node; }

namespace KysChess
{

struct CountsAsComboRule
{
    std::string comboName;

    bool operator==(const CountsAsComboRule&) const = default;
};

struct VictoryGoldRule
{
    int perHighestSurvivorStar{};

    bool operator==(const VictoryGoldRule&) const = default;
};

struct FreeShopRefreshRule
{
    bool operator==(const FreeShopRefreshRule&) const = default;
};

struct BattleMapChoiceRule
{
    bool operator==(const BattleMapChoiceRule&) const = default;
};

struct DeploymentCapacityRule
{
    bool operator==(const DeploymentCapacityRule&) const = default;
};

struct FightWinGrowthRule
{
    int maxHp{};
    int attack{};
    int defence{};

    bool operator==(const FightWinGrowthRule&) const = default;
};

struct HighTierNeigongDoublingRule
{
    bool operator==(const HighTierNeigongDoublingRule&) const = default;
};

using ChessNonBattleRule = std::variant<
    CountsAsComboRule,
    VictoryGoldRule,
    FreeShopRefreshRule,
    BattleMapChoiceRule,
    DeploymentCapacityRule,
    FightWinGrowthRule,
    HighTierNeigongDoublingRule>;

enum class ChessNonBattleRuleKind
{
    CountsAsCombo,
    VictoryGold,
    FreeShopRefresh,
    BattleMapChoice,
    DeploymentCapacity,
    FightWinGrowth,
    HighTierNeigongDoubling,
};

ChessNonBattleRuleKind chessNonBattleRuleKind(const ChessNonBattleRule& rule);

std::optional<ChessNonBattleRule> parseChessNonBattleRule(
    const YAML::Node& node,
    const ChessTextConverter& toTraditional,
    std::string_view context,
    const ChessDiagnosticSink& diagnostics);

std::string chessNonBattleRuleDescription(const ChessNonBattleRule& rule, bool compact = false);

std::vector<std::string> countsAsComboNames(std::span<const ChessNonBattleRule> rules);

FightWinGrowthRule sumFightWinGrowth(std::span<const ChessNonBattleRule> rules);

bool doublesHighTierNeigong(std::span<const ChessNonBattleRule> rules);

}  // namespace KysChess
