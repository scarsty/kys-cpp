#pragma once

#include "ChessGameContent.h"
#include "ChessRunRandom.h"
#include "ChessSessionTypes.h"

namespace KysChess
{

class ChessRewardRules
{
public:
    static void enqueueCampaignRewards(
        ChessSessionState& state,
        const ChessGameContent& content,
        ChessRunRandom& random,
        int completedFight,
        std::vector<ChessSemanticEvent>& events);
    static void enqueueChallengeReward(
        ChessSessionState& state,
        const ChessGameContent& content,
        const BalanceConfig::ChallengeDef& challenge,
        std::vector<ChessSemanticEvent>& events);
    static void enqueueForcedBan(
        ChessSessionState& state,
        const ChessGameContent& content,
        int slots,
        int maximumTier,
        std::vector<ChessSemanticEvent>& events);
    static void completePendingReward(
        ChessSessionState& state,
        std::vector<ChessSemanticEvent>& events);

    static ChessRuleErrorCode validate(
        const ChessSessionState& state,
        const ChessGameContent& content,
        const ChessAction& action);
    static void apply(
        ChessSessionState& state,
        const ChessGameContent& content,
        ChessRunRandom& random,
        const ChessAction& action,
        std::vector<ChessSemanticEvent>& events);
};

}
