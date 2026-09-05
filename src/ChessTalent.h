#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace KysChess
{
enum class ChessTalentId : std::uint8_t { DivineArms, LateBloomer, Gambler, Backbone };
inline constexpr std::array kChessTalentIds{
    ChessTalentId::DivineArms, ChessTalentId::LateBloomer,
    ChessTalentId::Gambler, ChessTalentId::Backbone};

inline const char* chessTalentName(ChessTalentId id)
{
    switch (id)
    {
    case ChessTalentId::DivineArms: return "神兵";
    case ChessTalentId::LateBloomer: return "晚成";
    case ChessTalentId::Gambler: return "賭徒";
    case ChessTalentId::Backbone: return "中堅";
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
    int amplifiedGrowthPercent{};
    int openingBans{};
    int banMinTier{};
    int banMaxTier{};
    int luckLastFight{};
    int luckMinTier{};
    int luckMaxTier{};
    int luckPerRefresh{};
    int luckChancePerStack{};
    int luckChanceCap{};
    int luckSurvivalHp{};
    int luckInvincibleFrames{};
    int targetTier{};
    int mpPerExtraStar{};
    int extraStarCap{};
    int guaranteeStar{};
    int guaranteeCount{};

    int luckChance(int stacks) const
    {
        return static_cast<int>((std::min)(static_cast<std::int64_t>(stacks) * luckChancePerStack,
            static_cast<std::int64_t>(luckChanceCap)));
    }
    std::string details(ChessTalentId id) const
    {
        switch (id)
        {
        case ChessTalentId::DivineArms: return description;
        case ChessTalentId::LateBloomer:
            return std::format("{}\n完整勝場成長的 {}% 先加入基礎屬性，再接受星級倍率。", description, amplifiedGrowthPercent);
        case ChessTalentId::Gambler:
            return std::format("{}\n開局額外禁棋 {} 次，限 {}～{} 費，可放棄。前 {} 關付費刷新隨機令一枚 {}～{} 費棋子增加 {} 層賭運。每層 {}%，上限 {}%；每場一次判定，成功保留 {} 生命、無敵 {} 幀，立即施放絕招且不消耗內力。", description,
                openingBans, banMinTier, banMaxTier, luckLastFight, luckMinTier, luckMaxTier, luckPerRefresh,
                luckChancePerStack, luckChanceCap, luckSurvivalHp, luckInvincibleFrames);
        case ChessTalentId::Backbone:
            return std::format("{}\n{} 費棋子從其他出戰友軍每顆額外星獲得 {} 開場內力，最多 {} 顆。升至 {} 星後，下次刷新保證 {} 枚同名棋子，按原價購買；同名三星會取消待用保證。", description,
                targetTier, mpPerExtraStar, extraStarCap, guaranteeStar, guaranteeCount);
        }
        std::unreachable();
    }
};
}
