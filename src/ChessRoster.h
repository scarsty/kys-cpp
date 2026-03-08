#pragma once

#include <map>
#include <vector>

#include "Chess.h"
#include "GameDataStore.h"

namespace KysChess
{

class ChessEquipmentInventory;
class ChessRoleSave;

class ChessRoster
{
public:
    ChessRoster(ChessRoleSave& roleSave, ChessEquipmentInventory& equipmentInventory);
    ChessRoster(ChessRoleSave& roleSave, ChessEquipmentInventory& equipmentInventory, const GameDataStore& store);

    void exportTo(GameDataStore& store) const;

    std::vector<Chess> getSelectedForBattle() const;
    void setSelectedInstanceIds(const std::vector<ChessInstanceID>& selected);

    void update(Chess chess);
    Chess create(Role* role, int star);
    bool wouldMerge(Role* role, int star) const;
    bool canMerge(Role* role, int star) const;
    const std::map<ChessInstanceID, Chess>& items() const;
    std::map<RoleAndStar, int> getChessCountMap() const;

    void remove(ChessInstanceID chessInstanceId);
    Chess upgrade(ChessInstanceID instanceId, int newStar);

    std::vector<Chess> getByStarAndTier(int star, int maxTier) const;
    Chess find(ChessInstanceID instanceId) const;

private:
    bool hasAtLeastMatchingPieces(Role* role, int star, int count) const;

    ChessRoleSave& roleSave_;
    ChessEquipmentInventory& equipmentInventory_;
    int nextChessInstanceId_ = 1;
    std::map<ChessInstanceID, Chess> collection_;
};

}