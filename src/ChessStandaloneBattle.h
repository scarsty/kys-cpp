#pragma once

#include "ChessGameSession.h"
#include "Types.h"

#include <cstdint>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace KysChess
{

enum class ChessStandaloneBattleProfile
{
    AutoChess,
    ClassicHades,
};

struct ChessStandaloneBattlePiece
{
    int roleId = -1;
    int star = 1;
    int weaponItemId = -1;
    int armorItemId = -1;
    int chessInstanceId = -1;
    int fightsWon{};
};

struct ChessStandaloneBattleTeam
{
    std::vector<ChessStandaloneBattlePiece> pieces;
    std::vector<int> formationSlots;
    std::set<int> obtainedNeigongIds;
};

struct ChessStandaloneBattleRequest
{
    std::string stableBattleId = "standalone";
    std::uint64_t rootSeed = 1;
    std::optional<int> mapId;
    std::optional<std::uint32_t> battleSeed;
    std::array<ChessStandaloneBattleTeam, 2> teams;
    std::map<int, RoleSave> roleOverrides;
    ChessSessionOptions options;
    ChessStandaloneBattleProfile profile = ChessStandaloneBattleProfile::AutoChess;
    PreparedChessBattleLayout layout = PreparedChessBattleLayout::Standard;
};

struct ChessStandaloneBattleBuild
{
    std::shared_ptr<const ChessGameContent> content;
    PreparedChessBattle preparedBattle;
    ChessSessionOptions options;
    std::uint64_t rootSeed{};

    std::unique_ptr<ChessGameSession> createSession() &&;
};

class ChessStandaloneBattle
{
public:
    static std::optional<ChessStandaloneBattleBuild> prepare(
        std::shared_ptr<const ChessGameContent> content,
        const ChessStandaloneBattleRequest& request,
        std::string& error);
};

}
