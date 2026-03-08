#pragma once
#include "Types.h"
#include "Chess.h"
#include <optional>

namespace KysChess
{

struct PurchaseResult {
    bool success;
    bool merged;
    int cost;
};

struct SellResult {
    int price;
    Role* role;
    int star;
};

struct GrantResult {
    bool success;
    bool merged;
};

struct UpgradeResult {
    bool success;
};

// Some merging logic are here
// We may add more in the future and decouple some logic we have in ChessSelector
class ChessManager
{
public:
    static int calculateCost(int tier, int star, int count = 1);
    static PurchaseResult purchaseChess(Role* role, int tier);
    static SellResult sellChess(ChessInstanceID chessInstanceId);
    static GrantResult grantChess(Role* role);

    // Core merge operation
    static Chess handleMerge(Chess newChess);

    // Query helpers
    static int getBenchCount();
    static int getSelectedCount();
    static bool isBenchFull();
    static std::vector<Chess> getSelectedForBattle();

    // Returns chess if instance exists in collection, nullopt otherwise.
    // Instance ID may not exist for: enemies, shop items, test-injected chess.
    static std::optional<Chess> tryFindChessByInstanceId(ChessInstanceID id);

    // Reward application (pure logic, no UI)
    static bool applyGoldReward(int amount);
    static GrantResult applyPieceReward(int maxTier);
    static bool applyStarUpReward(int fromStar, int maxTier);
    static bool applyEquipmentReward(int maxTier, int specificId = -1);
};

}
