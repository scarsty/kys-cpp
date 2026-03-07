#pragma once
#include <unordered_map>
#include <set>
#include <vector>
#include "Chess.h"
#include "ChessBalance.h"

class SQLite3Wrapper;

namespace KysChess
{

// Pure storage structures - no game logic
struct StoredChess {
    int roleId;
    int star;
    int chessInstanceId;
    bool selectedForBattle;
    int weaponInstanceId;
    int armorInstanceId;
};

struct StoredEquipment {
    int weapon = -1;
    int armor = -1;
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
    std::vector<std::pair<int, int>> currentShop; // roleId, tier
    std::vector<int> obtainedNeigong;
    std::set<int> completedChallenges;

    // from equipment instance id to item id
    // equipment id here is unique, we can have multiple item id 
    std::unordered_map<int, int> equipmentInventory;

    void save(SQLite3Wrapper& db) const;
    void load(SQLite3Wrapper& db);
    void reset();
};

}
