#pragma once

#include "ChessSessionTypes.h"

#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace KysChess
{

class ChessGameContent;

struct ChessActionEconomicConsequence
{
    int currentGold{};
    int goldCost{};
    int goldGain{};
    int projectedGoldAfter{};
    std::optional<int> experienceGained;
    std::optional<int> projectedExperienceAfter;
    std::optional<int> projectedLevelAfter;
    std::optional<int> affectedOptionCount;
    std::optional<bool> consumesFreeShopRefresh;
};

struct ChessShopSlotOfferCandidate
{
    int slot{};
    int roleId{};
    int cost{};
    int projectedGoldAfter{};
};

struct ChessPieceOfferCandidate
{
    int chessInstanceId{};
    int roleId{};
    int star = 1;
    bool deployed{};
    std::optional<int> goldGain;
    std::optional<int> projectedGoldAfter;
};

struct ChessRoleOfferCandidate
{
    int roleId{};
};

struct ChessEquipmentOfferCandidate
{
    int equipmentInstanceId{};
    int itemId{};
    int assignedChessInstanceId = -1;
};

struct ChessEquipmentTargetOfferCandidate
{
    int chessInstanceId{};
    int roleId{};
    int star = 1;
    bool deployed{};
};

struct ChessItemOfferCandidate
{
    int itemId{};
    int cost{};
    int projectedGoldAfter{};
};

struct ChessMapOfferCandidate
{
    int mapId{};
};

struct ChessBattleUnitOfferCandidate
{
    int unitId{};
    int roleId{};
    int x{};
    int y{};
};

struct ChessRewardOfferCandidate
{
    std::string rewardId;
    ChessRewardKind kind{};
    int value{};
    int value2{};
};

struct ChessChallengeOfferCandidate
{
    std::string challengeName;
};

struct ChessNoSelectionOffer
{
};

struct ChessToggleOffer
{
    bool currentValue{};
};

struct ChessShopSlotSelectionOffer
{
    std::vector<ChessShopSlotOfferCandidate> candidates;
};

struct ChessPieceSelectionOffer
{
    std::vector<ChessPieceOfferCandidate> candidates;
};

struct ChessRoleSelectionOffer
{
    std::vector<ChessRoleOfferCandidate> candidates;
};

struct ChessEquipmentAssignmentOffer
{
    std::vector<ChessEquipmentOfferCandidate> equipment;
    std::vector<ChessEquipmentTargetOfferCandidate> targets;
};

struct ChessItemSelectionOffer
{
    std::vector<ChessItemOfferCandidate> candidates;
};

struct ChessMapSelectionOffer
{
    std::vector<ChessMapOfferCandidate> candidates;
};

struct ChessPositionSwapOffer
{
    std::vector<ChessBattleUnitOfferCandidate> candidates;
};

struct ChessRewardSelectionOffer
{
    std::vector<ChessRewardOfferCandidate> candidates;
};

struct ChessChallengeSelectionOffer
{
    std::vector<ChessChallengeOfferCandidate> candidates;
};

using ChessActionOfferPayload = std::variant<
    ChessNoSelectionOffer,
    ChessToggleOffer,
    ChessShopSlotSelectionOffer,
    ChessPieceSelectionOffer,
    ChessRoleSelectionOffer,
    ChessEquipmentAssignmentOffer,
    ChessItemSelectionOffer,
    ChessMapSelectionOffer,
    ChessPositionSwapOffer,
    ChessRewardSelectionOffer,
    ChessChallengeSelectionOffer>;

struct ChessActionOffer
{
    ChessActionType type{};
    int minimumSelection{};
    int maximumSelection{};
    ChessActionOfferPayload payload;
    std::optional<ChessActionEconomicConsequence> economicConsequence;
};

template<typename Detail>
const Detail& chessActionOfferDetail(const ChessActionOffer& offer)
{
    return std::get<Detail>(offer.payload);
}

using ChessActionValidator = std::function<ChessRuleErrorCode(const ChessAction&)>;

std::vector<ChessActionOffer> buildChessActionOffers(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const ChessActionValidator& validate);

}  // namespace KysChess
