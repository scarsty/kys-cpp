#include "GameState.h"
#include "GameUtil.h"
#include "Save.h"
#include <cassert>
#include <random>
#include <ranges>
#include <algorithm>

namespace KysChess
{

bool BattleProgress::isBossFight() const
{
    return (fight_ + 1) % ChessBalance::config().bossInterval == 0;
}

bool BattleProgress::isGameComplete() const
{
    return fight_ >= ChessBalance::config().totalFights;
}

// ------------ Random Ops -------------------------
void GameState::initRand()
{
    std::random_device rd;
    shopSeed = rd();
    enemySeed = rd();
    shopCallCount = 0;
    enemyCallCount = 0;
    shopRand_.set_seed(shopSeed);
    enemyRand_.set_seed(enemySeed);
}

void GameState::restoreRand()
{
    shopRand_.set_seed(shopSeed);
    shopRand_.get_generator().discard(shopCallCount);
    enemyRand_.set_seed(enemySeed);
    enemyRand_.get_generator().discard(enemyCallCount);
}

int GameState::shopRandInt(int n)
{
    shopCallCount++;
    return shopRand_.rand_int(n);
}

int GameState::enemyRandInt(int n)
{
    enemyCallCount++;
    return enemyRand_.rand_int(n);
}


// --------------- Lifecycle ---------------------

void GameState::reset()
{
    // The random class is not copyable
    GameDataStore::operator=(GameDataStore{});
    
    // So lets manually reset the states
    // Likely this function can be removed. Will review later.
    initRand();
    chessPool = {};
    battleProgress = {};
    collection_.clear();
    equipments_.clear();
    ChessBalance::setDifficulty(difficulty);
}

GameDataStore GameState::exportStore() const
{
    GameDataStore exported = *this;

    exported.currentShop.clear();
    auto shop = chessPool.getCurrentShop();
    for (auto [role, star] : shop) {
        exported.currentShop.push_back({role->ID, star});
    }

    exported.fight = battleProgress.getFight();

    exported.storedCollection.clear();
    for (auto [instanceId, c] : collection_)
    {
        exported.storedCollection.emplace_back(
            c.role->ID, c.star, c.id.value, c.selectedForBattle, 
            c.weaponInstance.id.value, c.armorInstance.id.value);
    }

    exported.equipmentInventory.clear();
    for (const auto& [id, attachableItem] : equipments_) {
        exported.equipmentInventory.push_back({id.value, attachableItem.instance.itemId});
    }
    
    return exported;
}

void GameState::importStore(const GameDataStore& store)
{
    GameDataStore::operator=(store);
    ChessBalance::setDifficulty(difficulty);

    collection_.clear();
    equipments_.clear();

    std::vector<std::pair<Role*, int>> shopRoles;
    for (const auto& shopEntry : currentShop) {
        shopRoles.emplace_back(Save::getInstance()->getRole(shopEntry.roleId), shopEntry.tier);
    }
    chessPool.restoreShop(shopRoles);

    battleProgress.setFight(fight);

    // Rebuild equipment instances first so collection entries can restore both
    // instance ids and item ids consistently.
    for (const auto& entry : equipmentInventory) {
        ItemInstanceID id{entry.equipInstanceId};
        InstancedItem instance{.id=id, .itemId=entry.itemId};
        equipments_[id] = {instance, k_nonExistentChess};
    }

    for (const auto& storedChess : storedCollection) {
        Chess chess{};
        chess.role = Save::getInstance()->getRole(storedChess.roleId);
        chess.star = storedChess.star;
        chess.id = ChessInstanceID{storedChess.chessInstanceId};
        chess.selectedForBattle = storedChess.selectedForBattle;

        auto restoreItemInstance = [&](int storedItemInstanceId, InstancedItem& slot) {
            assert(storedItemInstanceId != k_nonExistentItem.value);
            ItemInstanceID itemInstanceId{storedItemInstanceId};
            assert(equipments_.contains(itemInstanceId));
            slot = equipments_.at(itemInstanceId).instance;
        };

        restoreItemInstance(storedChess.weaponInstanceId, chess.weaponInstance);
        restoreItemInstance(storedChess.armorInstanceId, chess.armorInstance);

        collection_.insert_or_assign(chess.id, chess);
    }

    for (auto& [id, chess] : collection_) {
        if (chess.weaponInstance.id.value != -1) {
            assert(equipments_.contains(chess.weaponInstance.id));
            equipments_[chess.weaponInstance.id].chessInstanceId = id;
        }
        if (chess.armorInstance.id.value != -1) {
            assert(equipments_.contains(chess.armorInstance.id));
            equipments_[chess.armorInstance.id].chessInstanceId = id;
        }
    }
    

    restoreRand();
}

// ------------------------ Economy ----------------------

bool GameState::spend(int m)
{
    if (money < m) return false;
    money -= m;
    return true;
}

// ------------------------- Experience -------------------

void GameState::increaseExp(int e)
{
    auto& cfg = ChessBalance::config();
    exp += e;
    if (level < cfg.maxLevel && level < (int)cfg.expTable.size() && exp >= cfg.expTable[level])
    {
        exp -= cfg.expTable[level];
        level += 1;
    }
}

int GameState::getExpForNextLevel() const
{
    auto& cfg = ChessBalance::config();
    if (level < cfg.maxLevel && level < (int)cfg.expTable.size())
        return cfg.expTable[level];
    return exp;
}

int GameState::getMaxDeploy() const
{
    return std::max(level + 1, ChessBalance::config().minBattleSize);
}


// ----------------- Battle Selection -------------------------------

std::vector<Chess> GameState::getSelectedForBattle() const
{
    std::vector<Chess> selected;
    for (const auto& [id, chess] : collection_)
        if (chess.selectedForBattle)
            selected.push_back(chess);
    return selected;
}

void GameState::setSelectedInstanceIds(const std::vector<ChessInstanceID>& selected)
{
    for (auto& [id, chess] : collection_)
        chess.selectedForBattle = false;
    for (auto id : selected)
        collection_.at(id).selectedForBattle = true;
}


// ---------------- Collection Operation ---------------------------

void GameState::updateToCollection(Chess c) {
    assert(c.id != k_nonExistentChess);

    // Lets first see if this chess was already in collection
    // if yes, just remove it which returns its existing items
    if (collection_.contains(c.id)) {
        removeChess(c.id);
    }

    // Now add it to collection.
    collection_.insert({c.id, c});
    
    // Finally, equip the items to itself
    auto equipItemInstance = [&](InstancedItem instance) {
        if (instance.id != k_nonExistentItem) {
            assert(equipments_.contains(instance.id));
            equipments_[instance.id].chessInstanceId = c.id;
        }
    };

    equipItemInstance(c.weaponInstance);
    equipItemInstance(c.armorInstance);
}

Chess GameState::createChess(Role* role, int star)
{
    Chess chess = {role, star, nextChessInstanceId++};
    collection_.insert_or_assign(chess.id, chess);
    return chess;
}

bool GameState::wouldMerge(Role* role, int star) const
{
    return hasAtLeastMatchingPieces(role, star, 2);
}

bool GameState::canMerge(Role* role, int star) const
{
    return hasAtLeastMatchingPieces(role, star, 3);
}

const std::map<ChessInstanceID, Chess>& GameState::getCollection() const 
{
    return collection_;
}

std::map<RoleAndStar, int> GameState::getChessCountMap() const
{
    std::map<RoleAndStar, int> result;
    for (auto& [instance, chess] : collection_)
        result[RoleAndStar{chess.role, chess.star}]++;
    return result;
}

void GameState::storeEquipmentItem(int itemId)
{
    AttachableItem item{
        .instance = InstancedItem{
            .id = nextEquipInstanceId, .itemId = itemId },
        .chessInstanceId = k_nonExistentChess
    };
    equipments_[ItemInstanceID{ nextEquipInstanceId }] = item;
    nextEquipInstanceId += 1;
}

GameState::StoredItemStats GameState::getStoredItemStats(int itemId) const
{
    // The available instance id should prefer an unequipped one if possible
    // otherwise we steal the instance.
    StoredItemStats stats;
    for (auto&& [id, attachableItem] : equipments_) {
        if (attachableItem.instance.itemId == itemId) {
            stats.totalCount += 1;
            if (attachableItem.chessInstanceId != k_nonExistentChess) {
                stats.equippedCount += 1;
                if (stats.availableInstanceId == k_nonExistentItem) {
                    stats.availableInstanceId = id;
                }
            } else {
                stats.availableInstanceId = id;
            }
        }
    }
    return stats;
}

void GameState::removeChess(ChessInstanceID chessInstanceId)
{
    const auto& chess = collection_.at(chessInstanceId);

    auto setToNoneIfEquipped = [&](ItemInstanceID id) {
        if (id != k_nonExistentItem) {
            equipments_[id].chessInstanceId = k_nonExistentChess;
        }
    };

    setToNoneIfEquipped(chess.weaponInstance.id);
    setToNoneIfEquipped(chess.armorInstance.id);
    collection_.erase(chessInstanceId);
}

Chess GameState::upgradeChessInstance(ChessInstanceID instanceId, int newStar)
{
    Chess& chess = collection_[instanceId];
    chess.star = newStar;
    return chess;
}

std::vector<Chess> GameState::getChessByStarAndTier(int star, int maxTier) const
{
    std::vector<Chess> result;
    for (auto& [instanceId, chess] : collection_)
    {
        if (chess.star == star)
        {
            int tier = ChessPool::GetChessTier(chess.role->ID);
            if (tier >= 0 && tier <= maxTier)
                result.push_back(chess);
        }
    }
    return result;
}

Chess GameState::findChessByInstanceId(ChessInstanceID instanceId) const
{
    return collection_.at(instanceId);
}

bool GameState::hasAtLeastMatchingPieces(Role* role, int star, int count) const
{
    if (star > 2) return false;

    int matched = 0;
    for (auto& [instanceId, p] : collection_)
        if (p.role == role && p.star == star)
            if (++matched >= count) return true;

    return false;
}

}
