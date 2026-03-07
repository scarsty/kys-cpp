#pragma once
#include "Types.h"
#include "Chess.h"

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
};

}
