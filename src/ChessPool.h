#pragma once

#include "Save.h"

#include <unordered_set>

namespace KysChess
{

class ChessPool {

public:

    static int GetChessTier(int roleId);

    // Returns a list of pairs of Role* and its star (0-4)
    std::vector<std::pair<Role*, int>> getChessFromPool(int level);

    // Remove a chess from the current selection;
    void removeChessAt(int idx);

    void refresh();

    // Restore shop state from save data
    void restoreShop(std::vector<std::pair<Role*, int>> shop);

    const std::vector<std::pair<Role*, int>>& getCurrentShop() const { return current_; }

    // Select a random enemy role from a specific tier (no rejection logic)
    static Role* selectEnemyFromPool(int tier);

    static const std::vector<int>& getRolesOfTier(int tier);

private:
    Role* selectFromPool(int tier);


    bool getNewChess_ = true;
    std::vector<std::pair<Role*, int>> current_;
    std::unordered_set<Role*> rejected_;

};




}    // namespace KysChess