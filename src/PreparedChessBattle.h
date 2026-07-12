#pragma once

#include "ChessRunRandom.h"

#include <cstdint>
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

    auto operator<=>(const PreparedChessBattleUnit&) const = default;
};

struct PreparedChessBattle
{
    PreparedChessBattleKind kind = PreparedChessBattleKind::Campaign;
    std::string stableBattleId;
    std::vector<PreparedChessBattleUnit> units;
    std::vector<int> mapCandidates;
    int chosenMapId = -1;
    std::vector<std::pair<int, int>> formationSwaps;
    std::uint32_t battleSeed{};
    ChessRunRandomCheckpoint preparationCheckpoint;

    auto operator<=>(const PreparedChessBattle&) const = default;
};

}
