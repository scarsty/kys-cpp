#pragma once

#include "Chess.h"
#include "Engine.h"
#include "GameDataStore.h"

#include <unordered_set>

namespace KysChess
{

class ChessRandom;
class ChessRoleSave;

class ChessPool {

public:
    ChessPool(ChessRandom& random, ChessRoleSave& roleSave);
    ChessPool(ChessRandom& random, ChessRoleSave& roleSave, const std::vector<StoredShopEntry>& shop);

    static int GetChessTier(int roleId);
    static Color GetTierColor(int tier);

    // Returns a list of pairs of Role* and its star (0-4)
    std::vector<std::pair<Role*, int>> getChessFromPool(int level);

    // Remove a chess from the current selection;
    void removeChessAt(int idx);

    void refresh();

    const std::vector<std::pair<Role*, int>>& getCurrentShop() const { return current_; }

    // Select a random enemy role from a specific tier (no rejection logic)
    Role* selectEnemyFromPool(int tier);

    static const std::vector<int>& getRolesOfTier(int tier);

private:
    Role* selectFromPool(int tier);

    ChessRandom& random_;
    ChessRoleSave& roleSave_;
    bool getNewChess_ = true;
    std::vector<std::pair<Role*, int>> current_;
    std::unordered_set<Role*> rejected_;

};




}    // namespace KysChess