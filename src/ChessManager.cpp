#include "ChessManager.h"
#include "ChessEquipment.h"
#include "ChessPool.h"
#include "GameState.h"


namespace KysChess
{

namespace
{

bool addChessWithAutoMerge(Role* role)
{
    auto& gd = GameState::get();

    Chess newChess = gd.createChess(role, 1);
    bool willMerge = gd.wouldMerge(role, 1);
    gd.updateToCollection(newChess);

    for (int level = 0; level < 2 && newChess.star <= 2; level++)
    {
        if (!gd.canMerge(newChess.role, newChess.star)) break;

        bool keepSelected = false;
        int mergeCandidateCount = 0;
        for (auto& [instanceId, piece] : gd.getCollection())
        {
            if (piece.role != newChess.role || piece.star != newChess.star) continue;
            if (piece.selectedForBattle)
                keepSelected = true;
            if (++mergeCandidateCount == 3)
                break;
        }

        newChess = ChessManager::handleMerge(newChess);
        if (keepSelected) {
            newChess.selectedForBattle = true;
        }
        gd.updateToCollection(newChess);
    }

    return willMerge;
}

}  // namespace

Chess ChessManager::handleMerge(Chess newChess)
{
    auto& gd = GameState::get();
    std::vector<Chess> mergedChess;

    // Explicitly consume exactly three pieces even if future bugs create more.
    for (auto& [instanceId, p] : gd.getCollection())
        if (p.role == newChess.role && p.star == newChess.star)
        {
            mergedChess.push_back(p);
            if (mergedChess.size() == 3) break;
        }

    // Find best equipment before deletion
    InstancedItem bestWeapon{}; 
    int bestWeaponTier = 0;
    InstancedItem bestArmor{}; 
    int bestArmorTier = 0;

    for (const auto& chess : mergedChess) {
        int w = chess.weaponInstance.itemId;
        if (w >= 0) {
            auto* eq = ChessEquipment::getById(w);
            if (eq && eq->tier > bestWeaponTier) {
                bestWeaponTier = eq->tier;
                bestWeapon = chess.weaponInstance;
            }
        }
        int a = chess.armorInstance.itemId;
        if (a >= 0) {
            auto* eq = ChessEquipment::getById(a);
            if (eq && eq->tier > bestArmorTier) {
                bestArmorTier = eq->tier;
                bestArmor = chess.armorInstance;
            }
        }
    }

    // Delete old pieces (equipment returns to inventory)
    for (const auto& chess : mergedChess)
        gd.removeChess(chess.id);

    // Create upgraded piece
    Chess upgraded = gd.createChess(newChess.role, newChess.star + 1);

    // Equip best items from inventory
    if (bestWeapon.id != k_nonExistentItem)
        upgraded.weaponInstance = bestWeapon;
    if (bestArmor.id != k_nonExistentItem)
        upgraded.armorInstance = bestArmor;

    return upgraded;
}

int ChessManager::calculateCost(int tier, int star, int count)
{
    auto& cfg = ChessBalance::config();
    return cfg.tierPrices[tier - 1] * static_cast<int>(std::pow(cfg.starCostMult, star - 1)) * count;
}

PurchaseResult ChessManager::purchaseChess(Role* role, int tier)
{
    auto& gd = GameState::get();
    int cost = calculateCost(tier, 1, 1);

    if (gd.isBenchFull() && !gd.wouldMerge(role, 1))
        return {false, false, cost};

    if (!gd.spend(cost))
        return {false, false, cost};

    bool willMerge = addChessWithAutoMerge(role);

    return {true, willMerge, cost};
}

SellResult ChessManager::sellChess(ChessInstanceID chessInstanceId)
{
    auto& gd = GameState::get();

    auto chess = gd.findChessByInstanceId(chessInstanceId);

    int tier = ChessPool::GetChessTier(chess.role->ID);
    int sellPrice = calculateCost(tier, chess.star, 1);

    gd.removeChess(chessInstanceId);
    gd.make(sellPrice);

    return SellResult{sellPrice, chess.role, chess.star};
}

GrantResult ChessManager::grantChess(Role* role)
{
    auto& gd = GameState::get();

    if (gd.isBenchFull() && !gd.wouldMerge(role, 1))
        return {false, false};

    return {true, addChessWithAutoMerge(role)};
}

}
