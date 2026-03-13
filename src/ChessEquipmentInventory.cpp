#include "ChessEquipmentInventory.h"

namespace KysChess
{

ChessEquipmentInventory::ChessEquipmentInventory(const GameDataStore& store)
{
    nextEquipInstanceId_ = store.nextEquipInstanceId;

    for (const auto& entry : store.equipmentInventory)
    {
        ItemInstanceID id{entry.equipInstanceId};
        InstancedItem instance{.id = id, .itemId = entry.itemId};
        equipments_[id] = {instance, k_nonExistentChess};
    }
}

void ChessEquipmentInventory::exportTo(GameDataStore& store) const
{
    store.nextEquipInstanceId = nextEquipInstanceId_;
    store.equipmentInventory.clear();
    for (const auto& [id, attachableItem] : equipments_)
    {
        store.equipmentInventory.push_back({id.value, attachableItem.instance.itemId});
    }
}

void ChessEquipmentInventory::storeItem(int itemId)
{
    AttachableItem item{
        .instance = InstancedItem{.id = nextEquipInstanceId_, .itemId = itemId},
        .chessInstanceId = k_nonExistentChess};
    equipments_[ItemInstanceID{nextEquipInstanceId_}] = item;
    nextEquipInstanceId_ += 1;
}

ChessStoredItemStats ChessEquipmentInventory::getItemStats(int itemId) const
{
    ChessStoredItemStats stats;
    for (const auto& [id, attachableItem] : equipments_)
    {
        if (attachableItem.instance.itemId != itemId)
        {
            continue;
        }

        stats.totalCount += 1;
        if (attachableItem.chessInstanceId != k_nonExistentChess)
        {
            stats.equippedCount += 1;
            if (stats.availableInstanceId == k_nonExistentItem)
            {
                stats.availableInstanceId = id;
            }
        }
        else
        {
            stats.availableInstanceId = id;
        }
    }
    return stats;
}

std::vector<std::pair<ItemInstanceID, ChessInstanceID>> ChessEquipmentInventory::getInstancesForItem(int itemId) const
{
    std::vector<std::pair<ItemInstanceID, ChessInstanceID>> result;
    for (const auto& [id, attachableItem] : equipments_)
    {
        if (attachableItem.instance.itemId == itemId)
        {
            result.push_back({id, attachableItem.chessInstanceId});
        }
    }
    return result;
}

bool ChessEquipmentInventory::contains(ItemInstanceID id) const
{
    return equipments_.contains(id);
}

InstancedItem ChessEquipmentInventory::getInstance(ItemInstanceID id) const
{
    return equipments_.at(id).instance;
}

void ChessEquipmentInventory::assignToChess(ItemInstanceID id, ChessInstanceID chessInstanceId)
{
    equipments_.at(id).chessInstanceId = chessInstanceId;
}

void ChessEquipmentInventory::clearAssignment(ItemInstanceID id)
{
    equipments_.at(id).chessInstanceId = k_nonExistentChess;
}

}