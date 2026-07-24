#pragma once

#include "ChessSessionTypes.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace KysChess
{

struct ChessCliActionCommand
{
    ChessActionType type{};
    std::string_view verb;
    std::string_view arguments;
};

inline constexpr std::array<ChessCliActionCommand, 20> kChessCliActionCommands{{
    {ChessActionType::BuyShopSlot, "buy", "<商店欄位>"},
    {ChessActionType::RefreshShop, "refresh", {}},
    {ChessActionType::SetShopLocked, "lock", "<on|off>"},
    {ChessActionType::SellChess, "sell", "<棋子 ID>"},
    {ChessActionType::SetDeployment, "deploy", "[棋子 ID,...]"},
    {ChessActionType::BuyExp, "exp", {}},
    {ChessActionType::AddBan, "ban", "<角色 ID>"},
    {ChessActionType::SkipForcedBans, "skip_bans", {}},
    {ChessActionType::Equip, "equip", "<裝備 ID> <棋子 ID>"},
    {ChessActionType::BuyLegendaryEquipment, "legendary", "<裝備項目 ID>"},
    {ChessActionType::SetPositionSwapEnabled, "position_swap", "<on|off>"},
    {ChessActionType::RerollEnemySeed, "reroll_enemy", {}},
    {ChessActionType::PrepareBattle, "prepare", {}},
    {ChessActionType::ChooseMap, "map", "<地圖 ID>"},
    {ChessActionType::SwapPositions, "swap", "<單位 ID> <單位 ID>"},
    {ChessActionType::StartBattle, "start", {}},
    {ChessActionType::ChooseReward, "reward", "<獎勵 ID>"},
    {ChessActionType::StartChallenge, "challenge", "<遠征名稱>"},
    {ChessActionType::FinishRun, "finish", {}},
    {ChessActionType::SetFormation, "formation", "<十格棋子 ID,...>"},
}};

inline constexpr const ChessCliActionCommand* chessCliActionCommand(
    ChessActionType type)
{
    for (const auto& command : kChessCliActionCommands)
    {
        if (command.type == type)
        {
            return &command;
        }
    }
    return nullptr;
}

inline constexpr const ChessCliActionCommand* chessCliActionCommand(
    std::string_view verb)
{
    for (const auto& command : kChessCliActionCommands)
    {
        if (command.verb == verb)
        {
            return &command;
        }
    }
    return nullptr;
}

inline std::string chessCliActionSyntax(const ChessCliActionCommand& command)
{
    std::string result(command.verb);
    if (!command.arguments.empty())
    {
        result += ' ';
        result += command.arguments;
    }
    return result;
}

inline std::optional<Difficulty> parseChessCliDifficulty(std::string_view text)
{
    if (text == "easy") return Difficulty::Easy;
    if (text == "normal") return Difficulty::Normal;
    if (text == "hard") return Difficulty::Hard;
    return std::nullopt;
}

inline std::optional<std::uint64_t> parseChessCliSeed(std::string_view text)
{
    std::uint64_t value{};
    int base = 10;
    if (text.starts_with("0x"))
    {
        text.remove_prefix(2);
        base = 16;
    }
    if (text.empty())
    {
        return std::nullopt;
    }
    const auto result = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value,
        base);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size()
        ? std::optional(value)
        : std::nullopt;
}

}
