#pragma once

#include "ChessActionOffers.h"
#include "ChessCatalogQueries.h"
#include "ChessGameContent.h"
#include "ChessRunRandom.h"

#include <optional>
#include <string>
#include <vector>

namespace KysChess
{

struct ChessShopTierOddsAnalysis
{
    int tier{};
    int configuredWeight{};
    double probability{};
    std::vector<int> availableRoleIds;
};

struct ChessShopOddsAnalysis
{
    int level{};
    int totalEffectiveWeight{};
    std::vector<ChessShopTierOddsAnalysis> tiers;
    std::string poolNote;
};

struct ChessShopPurchaseSynergyAnalysis
{
    int comboId = -1;
    std::string name;
    int current{};
    int afterPurchase{};
};

enum class ChessProjectedPurchaseResult
{
    BenchFull,
    AddOneStar,
    Merge,
};

struct ChessShopSlotAnalysis
{
    struct CopiesByStar
    {
        int star{};
        int count{};
    };

    int slot{};
    bool occupied{};
    int roleId = -1;
    int cost{};
    int shopTier{};
    int ownedCopies{};
    std::vector<CopiesByStar> copiesByStar;
    ChessRuleErrorCode purchaseError = ChessRuleErrorCode::None;
    int goldCost{};
    int projectedGoldAfter{};
    ChessProjectedPurchaseResult projectedResult = ChessProjectedPurchaseResult::BenchFull;
    int projectedResultStar = 1;
    std::vector<ChessShopPurchaseSynergyAnalysis> synergies;
    double levelTierProbability{};
};

struct ChessShopAnalysis
{
    int money{};
    int level{};
    std::vector<ChessShopSlotAnalysis> slots;
    ChessShopOddsAnalysis odds;
};

struct ChessInstanceAnalysis
{
    ChessSessionPiece piece;
    ChessCalculatedStats currentStats;
    int oneStarEquivalentCopies{};
    int sameStarCopies{};
    int copiesRequiredForNextStar{};
    std::vector<ChessEquipmentInstance> equipment;
    std::vector<ChessComboMetadata> synergyContributions;
};

struct ChessBanTierGroup
{
    int cost{};
    std::vector<int> roleIds;
};

struct ChessBanAnalysis
{
    int currentBanCount{};
    int maximumBanCount{};
    int remainingBanCapacity{};
    std::vector<ChessBanTierGroup> currentBansByCost;
    std::vector<ChessBanTierGroup> eligibleBansByCost;
    std::string effectTiming;
    std::string forcedPhaseNote;
};

ChessGameplayObservation queryChessGameplayObservation(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const ChessRunRandom& random);
ChessShopOddsAnalysis queryChessShopOdds(
    const ChessSessionState& state,
    const ChessGameContent& content,
    int level);
ChessShopSlotAnalysis queryChessShopSlot(
    const ChessSessionState& state,
    const ChessGameContent& content,
    int slot,
    const ChessActionValidator& validate);
ChessShopAnalysis queryChessShop(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const ChessActionValidator& validate);
ChessInstanceAnalysis queryChessInstance(
    const ChessSessionState& state,
    const ChessGameContent& content,
    int chessInstanceId);
ChessBanAnalysis queryChessBans(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const std::vector<ChessActionOffer>& offers);

}  // namespace KysChess
