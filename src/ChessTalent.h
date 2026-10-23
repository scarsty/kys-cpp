#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace KysChess
{
inline constexpr int kChessMaximumGrowthAmplificationPercent = 1000;

enum class ChessTalentId : std::uint8_t { DivineArms, LateBloomer, Gambler, Backbone, Alliance };
inline constexpr std::array kChessTalentIds{
    ChessTalentId::DivineArms, ChessTalentId::LateBloomer,
    ChessTalentId::Gambler, ChessTalentId::Backbone, ChessTalentId::Alliance};

inline const char* chessTalentName(ChessTalentId id)
{
    switch (id)
    {
    case ChessTalentId::DivineArms: return "神兵";
    case ChessTalentId::LateBloomer: return "晚成";
    case ChessTalentId::Gambler: return "賭徒";
    case ChessTalentId::Backbone: return "中堅";
    case ChessTalentId::Alliance: return "合縱";
    }
    std::unreachable();
}
inline const char* chessTalentId(ChessTalentId id)
{
    switch (id)
    {
    case ChessTalentId::DivineArms: return "divine_arms";
    case ChessTalentId::LateBloomer: return "late_bloomer";
    case ChessTalentId::Gambler: return "gambler";
    case ChessTalentId::Backbone: return "backbone";
    case ChessTalentId::Alliance: return "alliance";
    }
    std::unreachable();
}
inline std::optional<ChessTalentId> parseChessTalent(std::string_view text)
{
    for (const auto id : kChessTalentIds)
        if (text == chessTalentId(id) || text == chessTalentName(id)) return id;
    return std::nullopt;
}

struct ChessTalentDefinition
{
    std::string description;
    bool legendaryShop = false;
    int evolutionWins{};
    int evolutionWarningBattles{};
    std::array<int, 5> evolutionStatusShields{};
    int openingBans{};
    int banMinTier{};
    int banMaxTier{};
    int luckLastFight{};
    int luckMinTier{};
    int luckMaxTier{};
    int luckPerRefresh{};
    int luckChancePerStack{};
    int luckStackCap{};
    int luckSurvivalHp{};
    int luckInvincibleFrames{};
    int targetTier{};
    int mpPerExtraStar{};
    int strengtheningChargesPerExtraStar{};
    int strengtheningDamagePercent{};
    int extraStarCap{};
    int guaranteeStar{};
    int guaranteeCount{};
    int selectedComboBonus{};
    int reducedThresholdMinimum{};
    int thresholdReduction{};

    int luckChance(int stacks) const
    {
        return static_cast<int>((std::min)(static_cast<std::int64_t>((std::min)(stacks, luckStackCap)) * luckChancePerStack,
            std::int64_t{100}));
    }
};
}
