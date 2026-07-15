#pragma once

#include "ChessGameContent.h"
#include "ChessSessionTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace KysChess
{

namespace Battle
{
struct BattleInitializationRoleDelta;
}

enum class ChessStatCalculationScope
{
    BaseRole,
    StarAndFightWins,
    EquipmentBaseStats,
    InitializedCombat,
};

struct ChessCalculatedStats
{
    int maxHp{};
    int maxMp{};
    int attack{};
    int defence{};
    int speed{};
    int fist{};
    int sword{};
    int knife{};
    int unusual{};
    int hiddenWeapon{};

    auto operator<=>(const ChessCalculatedStats&) const = default;
};

struct ChessAbilityStarPower
{
    int star{};
    int power{};

    auto operator<=>(const ChessAbilityStarPower&) const = default;
};

struct ChessAbilityMetadata
{
    int magicId = -1;
    std::string name;
    std::vector<ChessAbilityStarPower> powerByStar;
    int mpCost{};
    int selectDistance{};
    int areaDistance{};
    std::string geometry;
    std::vector<std::string> effects;
    std::string effectNote;
};

struct ChessRoleMetadata
{
    int roleId = -1;
    std::string name;
    int cost{};
    ChessCalculatedStats baseStats;
    std::vector<ChessAbilityMetadata> abilities;
    std::vector<std::string> combos;
};

struct ChessEquipmentCharacterBonusMetadata
{
    std::vector<std::string> roles;
    std::vector<std::string> effects;
    std::vector<std::string> countsAsCombos;
};

struct ChessEquipmentMetadata
{
    int itemId = -1;
    std::string name;
    int tier{};
    int equipType{};
    std::vector<std::string> baseStatEffects;
    std::vector<std::string> specialEffects;
    std::vector<std::string> countsAsCombos;
    std::vector<ChessEquipmentCharacterBonusMetadata> characterBonuses;
    std::string comboCountingNote;
};

struct ChessNamedId
{
    int id{};
    std::string name;
};

struct ChessComboThresholdMetadata
{
    int requiredCount{};
    std::string name;
    std::vector<std::string> effects;
    bool active{};
};

struct ChessComboContributionMetadata
{
    int roleId = -1;
    std::string roleName;
    std::vector<int> unitIds;
    int countedStar = 1;
    int physicalPoints{};
    int starBonusPoints{};
    int effectivePoints{};
    bool naturalMember{};
    std::vector<ChessNamedId> equipmentSources;
    std::string explanation;
};

struct ChessComboMetadata
{
    int comboId = -1;
    std::string name;
    int physicalCount{};
    int effectiveCount{};
    int activeThresholdIndex = -1;
    int nextThresholdIndex = -1;
    bool active{};
    std::string progressCount;
    std::vector<std::string> members;
    std::vector<ChessComboThresholdMetadata> thresholds;
    std::vector<ChessComboContributionMetadata> contributions;
    std::string countExplanation;
};

struct ChessChallengeEnemyMetadata
{
    int roleId = -1;
    std::string name;
    int star = 1;
    std::optional<ChessEquipmentMetadata> weapon;
    std::optional<ChessEquipmentMetadata> armor;
};

struct ChessChallengeMetadata
{
    std::string name;
    std::string description;
    std::string summaryDescription;
    std::vector<ChessChallengeEnemyMetadata> enemies;
    std::vector<std::string> rewards;
};

std::string chessBattleMapDisplayName(const ChessGameContent& content, int mapId);
std::string chessItemDisplayName(const ChessGameContent& content, int itemId);

ChessCalculatedStats chessBaseRoleStats(const ChessRoleDefinition& role);
ChessCalculatedStats chessRoleStats(
    const ChessRoleDefinition& role,
    const BalanceConfig& balance,
    int star,
    int fightsWon);
void applyChessItemBaseStats(ChessCalculatedStats& stats, const ChessItemDefinition* item);
ChessCalculatedStats chessPieceStats(
    const ChessGameContent& content,
    const ChessSessionPiece& piece,
    const std::vector<ChessEquipmentInstance>& equipmentInventory);
ChessCalculatedStats chessPreparedUnitBaselineStats(
    const ChessGameContent& content,
    const PreparedChessBattleUnit& unit);
ChessCalculatedStats chessInitializedCombatStats(
    const Battle::BattleInitializationRoleDelta& initialized);
ChessCalculatedStats chessStatDelta(
    const ChessCalculatedStats& initialized,
    const ChessCalculatedStats& baseline);

ChessAbilityMetadata chessAbilityMetadata(
    const ChessGameContent& content,
    const ChessMagicDefinition& magic,
    std::vector<ChessAbilityStarPower> powerByStar);
std::vector<ChessAbilityMetadata> chessAbilitiesForRoleStar(
    const ChessGameContent& content,
    const ChessRoleDefinition& role,
    int star);
ChessRoleMetadata chessRoleMetadata(const ChessGameContent& content, int roleId);
ChessEquipmentMetadata chessEquipmentMetadata(const ChessGameContent& content, int itemId);
std::vector<std::string> chessEquipmentSynergyDetailLines(
    const ChessGameContent& content,
    int itemId);
ChessComboMetadata chessComboMetadata(
    const ChessGameContent& content,
    const ComboDef& definition,
    int physicalCount,
    int effectiveCount,
    int activeThresholdIndex,
    int nextThresholdIndex,
    const std::vector<ResolvedChessComboContribution>& contributions);
ChessChallengeMetadata chessChallengeMetadata(
    const ChessGameContent& content,
    const BalanceConfig::ChallengeDef& challenge);

}  // namespace KysChess
