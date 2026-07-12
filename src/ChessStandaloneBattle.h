#pragma once

#include "ChessGameSession.h"
#include "Types.h"

#include <cstdint>
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

struct ChessStandaloneBattleRequest
{
    std::string stableBattleId = "standalone";
    std::uint64_t rootSeed = 1;
    std::optional<int> mapId;
    std::optional<std::uint32_t> battleSeed;
    std::vector<ChessStandaloneBattlePiece> allies;
    std::vector<ChessStandaloneBattlePiece> enemies;
    std::set<int> obtainedNeigongIds;
    std::map<int, RoleSave> roleOverrides;
    ChessSessionOptions options;
    ChessStandaloneBattleProfile profile = ChessStandaloneBattleProfile::AutoChess;
};

struct ChessStandaloneBattleBuild
{
    std::shared_ptr<const ChessGameContent> content;
    PreparedChessBattle preparedBattle;
    std::set<int> obtainedNeigongIds;
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
