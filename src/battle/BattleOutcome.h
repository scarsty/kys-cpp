#pragma once

#include <cstdint>

namespace KysChess::Battle
{

enum class BattleOutcome : std::uint8_t
{
    InProgress,
    PlayerVictory,
    PlayerDefeat,
    Timeout,
};

inline constexpr bool hasDefeatProgressionSemantics(BattleOutcome outcome)
{
    return outcome == BattleOutcome::PlayerDefeat || outcome == BattleOutcome::Timeout;
}

}
