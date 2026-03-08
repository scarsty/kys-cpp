#include "ChessManager.h"
#include "ChessEquipment.h"
#include "ChessPool.h"
#include "GameState.h"


namespace KysChess
{

namespace
{

bool addChessWithAutoMerge(IChessGameState& gameState, ChessManager& manager, Role* role)
{
    Chess newChess = gameState.createChess(role, 1);
    bool willMerge = gameState.wouldMerge(role, 1);
    gameState.updateToCollection(newChess);

    for (int level = 0; level < 2 && newChess.star <= 2; level++)
    {
        if (!gameState.canMerge(newChess.role, newChess.star)) break;

        bool keepSelected = false;
        int mergeCandidateCount = 0;
        for (auto& [instanceId, piece] : gameState.getCollection())
        {
            if (piece.role != newChess.role || piece.star != newChess.star) continue;
            if (piece.selectedForBattle)
                keepSelected = true;
            if (++mergeCandidateCount == 3)
                break;
        }

        newChess = manager.handleMerge(newChess);
        if (keepSelected) {
            newChess.selectedForBattle = true;
        }
        gameState.updateToCollection(newChess);
    }

    return willMerge;
}

}  // namespace

Chess ChessManager::handleMerge(Chess newChess)
{
    std::vector<Chess> mergedChess;

    // Explicitly consume exactly three pieces even if future bugs create more.
    for (auto& [instanceId, p] : gameState_.getCollection())
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
        gameState_.removeChess(chess.id);

    // Create upgraded piece
    Chess upgraded = gameState_.createChess(newChess.role, newChess.star + 1);

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
    int cost = calculateCost(tier, 1, 1);

    if (isBenchFull() && !gameState_.wouldMerge(role, 1))
        return {false, false, cost};

    if (!gameState_.spend(cost))
        return {false, false, cost};

    bool willMerge = addChessWithAutoMerge(gameState_, *this, role);

    return {true, willMerge, cost};
}

SellResult ChessManager::sellChess(ChessInstanceID chessInstanceId)
{
    auto chess = gameState_.findChessByInstanceId(chessInstanceId);

    int tier = ChessPool::GetChessTier(chess.role->ID);
    int sellPrice = calculateCost(tier, chess.star, 1);

    gameState_.removeChess(chessInstanceId);
    gameState_.make(sellPrice);

    return SellResult{sellPrice, chess.role, chess.star};
}

GrantResult ChessManager::grantChess(Role* role)
{
    if (isBenchFull() && !gameState_.wouldMerge(role, 1))
        return {false, false};

    return {true, addChessWithAutoMerge(gameState_, *this, role)};
}

int ChessManager::getBenchCount() const
{
    return static_cast<int>(gameState_.getCollection().size()) - getSelectedCount();
}

int ChessManager::getSelectedCount() const
{
    int count = 0;
    for (const auto& [id, chess] : gameState_.getCollection())
        if (chess.selectedForBattle)
            count++;
    return count;
}

bool ChessManager::isBenchFull() const
{
    return getBenchCount() >= ChessBalance::config().benchSize;
}

std::vector<Chess> ChessManager::getSelectedForBattle() const
{
    return gameState_.getSelectedForBattle();
}

std::optional<Chess> ChessManager::tryFindChessByInstanceId(ChessInstanceID id) const
{
    if (id == k_nonExistentChess)
        return std::nullopt;
    auto& collection = gameState_.getCollection();
    auto it = collection.find(id);
    return (it != collection.end()) ? std::optional{it->second} : std::nullopt;
}

bool ChessManager::applyGoldReward(int amount)
{
    gameState_.make(amount);
    return true;
}

GrantResult ChessManager::applyPieceReward(int maxTier)
{
    return {false, false};  // UI selection required, handled by ChessSelector
}

bool ChessManager::applyStarUpReward(int fromStar, int maxTier) const
{
    auto chesses = gameState_.getChessByStarAndTier(fromStar, maxTier);
    return !chesses.empty();
}

bool ChessManager::applyEquipmentReward(int maxTier, int specificId)
{
    if (specificId >= 0)
    {
        auto* eq = ChessEquipment::getById(specificId);
        if (eq)
        {
            gameState_.storeEquipmentItem(eq->itemId);
            return true;
        }
        return false;
    }
    return true;  // UI selection required, handled by ChessSelector
}

void ChessManager::setSelectedForBattle(ChessInstanceID chessInstanceId, bool selected)
{
    auto chess = gameState_.findChessByInstanceId(chessInstanceId);
    chess.selectedForBattle = selected;
    gameState_.updateToCollection(chess);
}

void ChessManager::upgradeChess(ChessInstanceID chessInstanceId, int newStar)
{
    gameState_.upgradeChessInstance(chessInstanceId, newStar);
}

void ChessManager::equipItem(ChessInstanceID chessInstanceId, const EquipmentDef& equipment, ItemInstanceID itemInstanceId)
{
    auto chess = gameState_.findChessByInstanceId(chessInstanceId);
    if (equipment.equipType == 0)
    {
        chess.weaponInstance.id = itemInstanceId;
        chess.weaponInstance.itemId = equipment.itemId;
    }
    else
    {
        chess.armorInstance.id = itemInstanceId;
        chess.armorInstance.itemId = equipment.itemId;
    }
    gameState_.updateToCollection(chess);
}

}
