#pragma once

#include "ChessBalance.h"
#include "Chess.h"
#include "Engine.h"
#include "GameDataStore.h"

#include <array>
#include <set>
#include <unordered_set>

namespace YAML
{
class Node;
}

namespace KysChess
{

class ChessRandom;
class ChessRoleSave;

class ChessPool {

public:
    ChessPool(ChessRandom& random, ChessRoleSave& roleSave);
    ChessPool(ChessRandom& random, ChessRoleSave& roleSave, const std::vector<StoredShopEntry>& shop);

    static Color GetTierColor(int tier);

    // Returns a list of pairs of Role* and its star (0-4)
    std::vector<std::pair<Role*, int>> getChessFromPool(int level);

    // Remove a chess from the current selection;
    void removeChessAt(int idx);

    void refresh();

    const std::vector<std::pair<Role*, int>>& getCurrentShop() const { return current_; }

    // Select a random enemy role from a specific tier (no rejection logic)
    Role* selectEnemyFromPool(int tier);

    bool isRoleInPool(int roleId);
    const std::vector<int>& getRolesOfTier(int tier);
    void setBannedRoleIds(const std::set<int>& banned);

private:
    void ensurePoolLoaded();
    void reloadPool();
    void loadPoolNode(const YAML::Node& root);
    Role* selectFromPool(int tier);
    int getRoleTier(int roleId) const;

    ChessRandom& random_;
    ChessRoleSave& roleSave_;
    bool getNewChess_ = true;
    std::vector<std::pair<Role*, int>> current_;
    std::unordered_set<Role*> rejected_;
    std::set<int> banned_;
    std::array<std::vector<int>, 5> rolesByTier_;
    std::unordered_set<int> roleIdsInPool_;
    bool poolLoaded_ = false;
    Difficulty loadedDifficulty_ = Difficulty::Easy;

};




}    // namespace KysChess
