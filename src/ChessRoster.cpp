#include "ChessRoster.h"

#include <cassert>

#include "ChessEquipmentInventory.h"
#include "ChessPool.h"
#include "ChessRoleSave.h"

namespace KysChess
{

ChessRoster::ChessRoster(ChessRoleSave& roleSave, ChessEquipmentInventory& equipmentInventory)
    : roleSave_(roleSave)
    , equipmentInventory_(equipmentInventory)
{
}

ChessRoster::ChessRoster(ChessRoleSave& roleSave, ChessEquipmentInventory& equipmentInventory, const GameDataStore& store)
    : ChessRoster(roleSave, equipmentInventory)
{
    nextChessInstanceId_ = store.nextChessInstanceId;

    for (const auto& storedChess : store.storedCollection)
    {
        Chess chess{};
        chess.role = roleSave_.getRole(storedChess.roleId);
        chess.star = storedChess.star;
        chess.id = ChessInstanceID{storedChess.chessInstanceId};
        chess.selectedForBattle = storedChess.selectedForBattle;

        auto restoreItemInstance = [&](int storedItemInstanceId, InstancedItem& slot) {
            if (storedItemInstanceId == k_nonExistentItem.value)
            {
                return;
            }
            ItemInstanceID itemInstanceId{storedItemInstanceId};
            assert(equipmentInventory_.contains(itemInstanceId));
            slot = equipmentInventory_.getInstance(itemInstanceId);
        };

        restoreItemInstance(storedChess.weaponInstanceId, chess.weaponInstance);
        restoreItemInstance(storedChess.armorInstanceId, chess.armorInstance);

        collection_.insert_or_assign(chess.id, chess);
    }

    for (auto& [id, chess] : collection_)
    {
        if (chess.weaponInstance.id != k_nonExistentItem)
        {
            equipmentInventory_.assignToChess(chess.weaponInstance.id, id);
        }
        if (chess.armorInstance.id != k_nonExistentItem)
        {
            equipmentInventory_.assignToChess(chess.armorInstance.id, id);
        }
    }
}

void ChessRoster::exportTo(GameDataStore& store) const
{
    store.nextChessInstanceId = nextChessInstanceId_;
    store.storedCollection.clear();
    for (auto [instanceId, chess] : collection_)
    {
        store.storedCollection.emplace_back(
            chess.role->ID,
            chess.star,
            chess.id.value,
            chess.selectedForBattle,
            chess.weaponInstance.id.value,
            chess.armorInstance.id.value);
    }
}

std::vector<Chess> ChessRoster::getSelectedForBattle() const
{
    std::vector<Chess> selected;
    for (const auto& [id, chess] : collection_)
    {
        if (chess.selectedForBattle)
        {
            selected.push_back(chess);
        }
    }
    return selected;
}

void ChessRoster::setSelectedInstanceIds(const std::vector<ChessInstanceID>& selected)
{
    for (auto& [id, chess] : collection_)
    {
        chess.selectedForBattle = false;
    }
    for (auto id : selected)
    {
        collection_.at(id).selectedForBattle = true;
    }
}

void ChessRoster::update(Chess chess)
{
    assert(chess.id != k_nonExistentChess);

    if (collection_.contains(chess.id))
    {
        remove(chess.id);
    }

    auto assignItemInstance = [&](InstancedItem instance) {
        if (instance.id != k_nonExistentItem)
        {
            assert(equipmentInventory_.contains(instance.id));

            // Clear item from any other chess piece that has it
            for (auto& [id, otherChess] : collection_)
            {
                if (otherChess.weaponInstance.id == instance.id)
                {
                    otherChess.weaponInstance = {};
                }
                if (otherChess.armorInstance.id == instance.id)
                {
                    otherChess.armorInstance = {};
                }
            }

            equipmentInventory_.assignToChess(instance.id, chess.id);
        }
    };

    assignItemInstance(chess.weaponInstance);
    assignItemInstance(chess.armorInstance);

    collection_.insert({chess.id, chess});
}

Chess ChessRoster::create(Role* role, int star)
{
    Chess chess = {role, star, nextChessInstanceId_++};
    collection_.insert_or_assign(chess.id, chess);
    return chess;
}

bool ChessRoster::wouldMerge(Role* role, int star) const
{
    return hasAtLeastMatchingPieces(role, star, 2);
}

bool ChessRoster::canMerge(Role* role, int star) const
{
    return hasAtLeastMatchingPieces(role, star, 3);
}

const std::map<ChessInstanceID, Chess>& ChessRoster::items() const
{
    return collection_;
}

std::map<RoleAndStar, int> ChessRoster::getChessCountMap() const
{
    std::map<RoleAndStar, int> result;
    for (const auto& [instanceId, chess] : collection_)
    {
        result[RoleAndStar{chess.role, chess.star}]++;
    }
    return result;
}

void ChessRoster::remove(ChessInstanceID chessInstanceId)
{
    const auto& chess = collection_.at(chessInstanceId);

    auto clearItemAssignment = [&](ItemInstanceID id) {
        if (id != k_nonExistentItem)
        {
            equipmentInventory_.clearAssignment(id);
        }
    };

    clearItemAssignment(chess.weaponInstance.id);
    clearItemAssignment(chess.armorInstance.id);
    collection_.erase(chessInstanceId);
}

std::vector<Chess> ChessRoster::getByStarAndTier(int star, int maxTier) const
{
    std::vector<Chess> result;
    for (const auto& [instanceId, chess] : collection_)
    {
        if (chess.star == star)
        {
            int tier = ChessPool::GetChessTier(chess.role->ID);
            if (tier >= 0 && tier <= maxTier)
            {
                result.push_back(chess);
            }
        }
    }
    return result;
}

Chess ChessRoster::find(ChessInstanceID instanceId) const
{
    return collection_.at(instanceId);
}

bool ChessRoster::hasAtLeastMatchingPieces(Role* role, int star, int count) const
{
    if (star > 2)
    {
        return false;
    }

    int matched = 0;
    for (const auto& [instanceId, piece] : collection_)
    {
        if (piece.role == role && piece.star == star)
        {
            if (++matched >= count)
            {
                return true;
            }
        }
    }

    return false;
}

}