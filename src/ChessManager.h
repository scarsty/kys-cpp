#pragma once
#include "ChessEconomy.h"
#include "ChessEquipmentInventory.h"
#include "ChessRoster.h"
#include "Types.h"
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
    ChessManager(ChessRoster& roster, ChessEquipmentInventory& equipmentInventory, ChessEconomy& economy)
        : roster_(roster), equipmentInventory_(equipmentInventory), economy_(economy) {}

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
    bool applyStarUpReward(int fromStar, int maxTier) const;
    bool applyEquipmentReward(int maxTier, int specificId = -1);
    void incrementFightsWon(ChessInstanceID chessInstanceId, int amount = 1);

    void upgradeChess(ChessInstanceID chessInstanceId, int newStar);
    void equipItem(ChessInstanceID chessInstanceId, const EquipmentDef& equipment, ItemInstanceID itemInstanceId);

private:
    ChessRoster& roster_;
    ChessEquipmentInventory& equipmentInventory_;
    ChessEconomy& economy_;
};

}
