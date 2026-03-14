#include "ChessManager.h"
#include "ChessEquipment.h"
#include "ChessPool.h"


namespace KysChess
{

namespace
{

bool tryAutoMerge(ChessRoster& roster, ChessManager& manager, Chess& chess)
{
    if (chess.star > 2 || !roster.canMerge(chess.role, chess.star))
        return false;

    bool keepSelected = false;
    for (auto& [instanceId, piece] : roster.items())
    {
        if (piece.role == chess.role && piece.star == chess.star && piece.selectedForBattle)
        {
            keepSelected = true;
            break;
        }
    }

    chess = manager.handleMerge(chess);
    if (keepSelected)
        chess.selectedForBattle = true;
    roster.update(chess);
    return true;
}

bool addChessWithAutoMerge(ChessRoster& roster, ChessManager& manager, Role* role)
{
    Chess newChess = roster.create(role, 1);
    roster.update(newChess);

    bool didMerge = false;
    for (int level = 0; level < 2; level++)
    {
        if (!tryAutoMerge(roster, manager, newChess))
            break;
        didMerge = true;
    }

    return didMerge;
}

}  // namespace

Chess ChessManager::handleMerge(Chess newChess)
{
    std::vector<Chess> mergedChess;

    // Explicitly consume exactly three pieces even if future bugs create more.
    for (auto& [instanceId, p] : roster_.items())
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
        roster_.remove(chess.id);

    // Create upgraded piece
    Chess upgraded = roster_.create(newChess.role, newChess.star + 1);

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

    if (isBenchFull() && !roster_.wouldMerge(role, 1))
        return {false, false, cost};

    if (!economy_.spend(cost))
        return {false, false, cost};

    bool willMerge = addChessWithAutoMerge(roster_, *this, role);

    return {true, willMerge, cost};
}

SellResult ChessManager::sellChess(ChessInstanceID chessInstanceId)
{
    auto chess = roster_.find(chessInstanceId);

    int tier = chess.role ? chess.role->Cost : -1;
    int sellPrice = calculateCost(tier, chess.star, 1);

    roster_.remove(chessInstanceId);
    economy_.make(sellPrice);

    return SellResult{sellPrice, chess.role, chess.star};
}

GrantResult ChessManager::grantChess(Role* role)
{
    if (isBenchFull() && !roster_.wouldMerge(role, 1))
        return {false, false};

    return {true, addChessWithAutoMerge(roster_, *this, role)};
}

int ChessManager::getBenchCount() const
{
    return static_cast<int>(roster_.items().size()) - getSelectedCount();
}

int ChessManager::getSelectedCount() const
{
    int count = 0;
    for (const auto& [id, chess] : roster_.items())
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
    return roster_.getSelectedForBattle();
}

std::optional<Chess> ChessManager::tryFindChessByInstanceId(ChessInstanceID id) const
{
    if (id == k_nonExistentChess)
        return std::nullopt;
    auto& collection = roster_.items();
    auto it = collection.find(id);
    return (it != collection.end()) ? std::optional{it->second} : std::nullopt;
}

bool ChessManager::applyGoldReward(int amount)
{
    economy_.make(amount);
    return true;
}

bool ChessManager::applyStarUpReward(int fromStar, int maxTier) const
{
    auto chesses = roster_.getByStarAndTier(fromStar, maxTier);
    return !chesses.empty();
}

bool ChessManager::applyEquipmentReward(int maxTier, int specificId)
{
    if (specificId >= 0)
    {
        auto* eq = ChessEquipment::getById(specificId);
        if (eq)
        {
            equipmentInventory_.storeItem(eq->itemId);
            return true;
        }
        return false;
    }
    return true;  // UI selection required, handled by ChessSelector
}

void ChessManager::upgradeChess(ChessInstanceID chessInstanceId, int newStar)
{
    auto chess = roster_.find(chessInstanceId);
    chess.star = newStar;
    roster_.update(chess);
    tryAutoMerge(roster_, *this, chess);
}

void ChessManager::equipItem(ChessInstanceID chessInstanceId, const EquipmentDef& equipment, ItemInstanceID itemInstanceId)
{
    auto chess = roster_.find(chessInstanceId);
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
    roster_.update(chess);
}

}
