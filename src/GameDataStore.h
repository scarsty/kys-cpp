#pragma once
#include <set>
#include <vector>
#include "Chess.h"
#include "ChessBalance.h"

class SQLite3Wrapper;

namespace KysChess
{

// Pure storage structures - no game logic
struct StoredChess {
    int roleId = 0;
    int star = 0;
    int chessInstanceId = 0;
    bool selectedForBattle = false;
    int weaponInstanceId = -1;
    int armorInstanceId = -1;
};

struct StoredShopEntry {
    int roleId = 0;
    int tier = 0;
};

struct StoredEquipmentInventoryEntry {
    int equipInstanceId = -1;
    int itemId = -1;
};

struct GameDataStore {
    // Scalar state
    int money = 0;
    int exp = 0;
    int level = 0;
    int fight = 0;
    unsigned int shopSeed = 0;
    unsigned int shopCallCount = 0;
    unsigned int enemySeed = 0;
    unsigned int enemyCallCount = 0;
    bool shopLocked = false;
    Difficulty difficulty = Difficulty::Easy;
    bool positionSwapEnabled = false;
    int nextChessInstanceId = 1;
    int nextEquipInstanceId = 1;

    // Collections
    std::vector<StoredChess> storedCollection;
    std::vector<StoredShopEntry> currentShop;
    std::vector<int> obtainedNeigong;
    std::set<int> completedChallenges;
    std::set<int> seenRoleIds;
    std::set<int> bannedRoleIds;
    std::vector<StoredEquipmentInventoryEntry> equipmentInventory;

    void save(SQLite3Wrapper& db) const;
    void load(SQLite3Wrapper& db);
    void reset();
};

}
