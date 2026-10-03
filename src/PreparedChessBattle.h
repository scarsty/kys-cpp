#pragma once

#include "ChessRunRandom.h"
#include "battle/BattleLethalRecovery.h"
#include "battle/BattleStrengthening.h"
#include <optional>

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace KysChess
{

enum class PreparedChessBattleKind : std::uint8_t
{
    Campaign,
    Challenge,
    Standalone,
};

enum class PreparedChessBattleLayout : std::uint8_t
{
    Standard,
    PvpArena,
};

struct PreparedChessBattleUnit
{
    int unitId = -1;
    int chessInstanceId = -1;
    int roleId = -1;
    int team = -1;
    int star = 1;
    int weaponItemId = -1;
    int armorItemId = -1;
    int fightsWon{};
    int x{};
    int y{};
    int formationSlot = -1;
    int amplifiedGrowthPercent{};
    int openingMp{};
    Battle::BattleStrengthening openingStrengthening{};
    std::optional<Battle::BattleLethalRecovery> lethalRecovery;

    auto operator<=>(const PreparedChessBattleUnit&) const = default;
};

struct PreparedChessBattle
{
    PreparedChessBattleKind kind = PreparedChessBattleKind::Campaign;
    PreparedChessBattleLayout layout = PreparedChessBattleLayout::Standard;
    std::string stableBattleId;
    std::vector<PreparedChessBattleUnit> units;
    std::vector<int> mapCandidates;
    int chosenMapId = -1;
    std::vector<std::pair<int, int>> formationSwaps;
    std::uint32_t battleSeed{};
    std::uint32_t talentBattleSeed{};
    ChessRunRandomCheckpoint preparationCheckpoint;
    std::array<std::set<int>, 2> obtainedNeigongIdsByTeam;

    auto operator<=>(const PreparedChessBattle&) const = default;
};

}
