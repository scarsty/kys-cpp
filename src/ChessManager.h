#pragma once
#include "ChessContext.h"
#include "Types.h"
#include "Chess.h"
#include "ChessEquipment.h"
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
    explicit ChessManager(IChessGameState& gameState) : gameState_(gameState) {}

    static int calculateCost(int tier, int star, int count = 1);
    PurchaseResult purchaseChess(Role* role, int tier);
    SellResult sellChess(ChessInstanceID chessInstanceId);
    GrantResult grantChess(Role* role);

    // Core merge operation
    Chess handleMerge(Chess newChess);

    // Query helpers
    int getBenchCount() const;
    int getSelectedCount() const;
    bool isBenchFull() const;
    std::vector<Chess> getSelectedForBattle() const;

    // Returns chess if instance exists in collection, nullopt otherwise.
    // Instance ID may not exist for: enemies, shop items, test-injected chess.
    std::optional<Chess> tryFindChessByInstanceId(ChessInstanceID id) const;

    // Reward application (pure logic, no UI)
    bool applyGoldReward(int amount);
    GrantResult applyPieceReward(int maxTier);
    bool applyStarUpReward(int fromStar, int maxTier) const;
    bool applyEquipmentReward(int maxTier, int specificId = -1);

    void setSelectedForBattle(ChessInstanceID chessInstanceId, bool selected);
    void upgradeChess(ChessInstanceID chessInstanceId, int newStar);
    void equipItem(ChessInstanceID chessInstanceId, const EquipmentDef& equipment, ItemInstanceID itemInstanceId);

private:
    IChessGameState& gameState_;
};

}
