#pragma once

#include <map>

#include "Chess.h"
#include "GameDataStore.h"

namespace KysChess
{

struct ChessStoredItemStats {
    int totalCount{};
    int equippedCount{};
    ItemInstanceID availableInstanceId{-1};
};

class ChessEquipmentInventory
{
public:
    ChessEquipmentInventory() = default;
    explicit ChessEquipmentInventory(const GameDataStore& store);

    void exportTo(GameDataStore& store) const;

    void storeItem(int itemId);
    ChessStoredItemStats getItemStats(int itemId) const;
    std::vector<std::pair<ItemInstanceID, ChessInstanceID>> getInstancesForItem(int itemId) const;

    bool contains(ItemInstanceID id) const;
    InstancedItem getInstance(ItemInstanceID id) const;
    void assignToChess(ItemInstanceID id, ChessInstanceID chessInstanceId);
    void clearAssignment(ItemInstanceID id);

private:
    struct AttachableItem {
        InstancedItem instance{};
        ChessInstanceID chessInstanceId{};
    };

    int nextEquipInstanceId_ = 1;
    std::map<ItemInstanceID, AttachableItem> equipments_;
};

}