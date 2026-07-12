#pragma once

#include "ChessGameContent.h"
#include "ChessRunRandom.h"
#include "ChessSessionTypes.h"

namespace KysChess
{

class ChessManagementRules
{
public:
    static void initializeShop(
        ChessSessionState& state,
        const ChessGameContent& content,
        ChessRunRandom& random);
    static void refreshShop(
        ChessSessionState& state,
        const ChessGameContent& content,
        ChessRunRandom& random);

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

    static int maximumDeployment(const ChessSessionState& state, const ChessGameContent& content);
    static int experienceForNextLevel(const ChessSessionState& state, const ChessGameContent& content);
    static bool gainExperience(
        ChessSessionState& state,
        const ChessGameContent& content,
        int amount);
    static int maximumDeploymentAtLevel(const ChessGameContent& content, int level);
    static int maximumBanCount(const ChessSessionState& state, const ChessGameContent& content);
    static int pieceValue(const ChessGameContent& content, int roleId, int star);
    static bool wouldGrantPieceMerge(const ChessSessionState& state, int roleId);
    static bool canGrantPiece(
        const ChessSessionState& state,
        const ChessGameContent& content,
        int roleId);
    static int grantPiece(
        ChessSessionState& state,
        const ChessGameContent& content,
        int roleId,
        std::vector<ChessSemanticEvent>& events,
        int eventValue = 0);
    static int grantEquipment(
        ChessSessionState& state,
        int itemId,
        std::vector<ChessSemanticEvent>& events);
    static void upgradePiece(
        ChessSessionState& state,
        const ChessGameContent& content,
        int chessInstanceId,
        int newStar,
        std::vector<ChessSemanticEvent>& events);
};

}
