#pragma once

#include "ChessCatalogQueries.h"
#include "PreparedChessBattle.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace KysChess
{

struct ChessPreparedBattleMapIdentity
{
    int mapId = -1;
    std::string name;
};

struct ChessPreparedBattleIdentity
{
    PreparedChessBattleKind kind = PreparedChessBattleKind::Campaign;
    std::string stableBattleId;
    std::vector<ChessPreparedBattleMapIdentity> mapCandidates;
    int chosenMapId = -1;
    std::string chosenMapName;
    std::uint32_t battleSeed{};
};

struct ChessPreparedBattleUnitAnalysis
{
    int unitId = -1;
    int chessInstanceId = -1;
    int roleId = -1;
    std::string name;
    int team = -1;
    int star = 1;
    int fightsWon{};
    int x{};
    int y{};
    int headId = -1;
    std::string skillNames;
    int weaponItemId = -1;
    std::string weaponName;
    int armorItemId = -1;
    std::string armorName;
    ChessCalculatedStats baselineStats;
    std::optional<ChessCalculatedStats> initializedCombatStats;
    std::optional<ChessCalculatedStats> initializedStatDelta;
    std::vector<ChessAbilityMetadata> abilities;
};

struct ChessPreparedBattleAssetIds
{
    std::vector<int> attackSoundIds;
    std::vector<int> effectIds;
};

struct ChessPreparedBattleAnalysis
{
    ChessPreparedBattleIdentity identity;
    std::vector<ChessPreparedBattleUnitAnalysis> units;
    std::vector<ChessComboMetadata> allySynergies;
    std::vector<ChessComboMetadata> enemySynergies;
    ChessPreparedBattleAssetIds presentationAssets;
    bool combatInitialized{};
    std::string baselineStatsNote;
    std::string initializedStatsNote;
};

ChessPreparedBattleAnalysis projectPreparedChessBattle(
    const PreparedChessBattle& prepared,
    const ChessGameContent& content);
ChessPreparedBattleAnalysis analyzePreparedChessBattle(
    const PreparedChessBattle& prepared,
    const ChessGameContent& content,
    const std::set<int>& obtainedNeigongIds,
    int maximumFrames);

}  // namespace KysChess
