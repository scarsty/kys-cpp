#pragma once
#include <unordered_map>
#include <set>
#include <utility>
#include "Chess.h"
#include "ChessBalance.h"
#include "ChessEquipment.h"
#include "ChessPool.h"
#include "Random.h"
#include "GameDataStore.h"

namespace KysChess
{

class BattleProgress {
public:
    BattleProgress() : fight_(0) {}

    void advance() { fight_++; }
    int getFight() const { return fight_; }
    void setFight(int f) { fight_ = f; }
    bool isBossFight() const;
    bool isGameComplete() const;

private:
    int fight_;
};

// State management layer - business logic and caching
class GameState : private GameDataStore
{
public:

public:
    static GameState& get() {
        static GameState state;
        return state;
    }

    // Random Ops
    void initRand();
    void restoreRand();
    int shopRandInt(int n);
    int enemyRandInt(int n);
    unsigned int getEnemyCallCount() const { return enemyCallCount; }
    void setEnemyCallCount(unsigned int count) { enemyCallCount = count; }

    // Lifecycle - some of those are questionable
    void reset();
    GameDataStore exportStore() const;
    void importStore(const GameDataStore& store);

    // Economy
    bool spend(int m);
    void make(int m) { money += m; }
    int getMoney() const { return money; }
    void setMoney(int v) { money = v; }

    // Experience
    void increaseExp(int e);
    int getExp() const { return exp; }
    int getExpForNextLevel() const;
    int getLevel() const { return level; }
    void setExp(int v) { exp = v; }
    void setLevel(int v) { level = v; }
    int getMaxDeploy() const;

    // TODO: move getSelectedForBattle to ChessManager
    std::vector<Chess> getSelectedForBattle() const;

    // This is a neccessary call - we will assert if any selected
    // if not a instance id we own
    void setSelectedInstanceIds(const std::vector<ChessInstanceID>& selected);
    void resetChess(Chess chess);

    // Collection operation
    
    // When you addToCollection - we are not responsible for assigning
    // instance id. This is simply an overwrite into the collection.
    // asserts if the instanceId is missing. Note this is also how
    // you would update equipments. Given a chess if the equipment ids
    // are set to -1, internally the equipment is unequipped.
    // Will also assert if incorrect item instance ids are assigned.
    void updateToCollection(Chess c);

    // The chess is created and added into the collection 
    // and returned with an instance ID. Caller needs to setup
    // selected or equipments.
    Chess createChess(Role* role, int star);

    bool wouldMerge(Role* role, int star) const;
    bool canMerge(Role* role, int star) const;
    const std::map<ChessInstanceID, Chess>& getCollection() const;
    std::map<RoleAndStar, int> getChessCountMap() const;

    // Shop
    bool isShopLocked() const { return shopLocked; }
    void setShopLocked(bool v) { shopLocked = v; }

    // Features
    bool isPositionSwapEnabled() const { return positionSwapEnabled; }
    void setPositionSwapEnabled(bool v) { positionSwapEnabled = v; }

    // Neigong
    const std::vector<int>& getObtainedNeigong() const { return obtainedNeigong; }
    void addNeigong(int magicId) { obtainedNeigong.push_back(magicId); }
    void setObtainedNeigong(std::vector<int> v) { obtainedNeigong = std::move(v); }

    // Challenges
    bool isChallengeCompleted(int idx) const { return completedChallenges.count(idx); }
    void completeChallenge(int idx) { completedChallenges.insert(idx); }

    // Equipment system
    void storeEquipmentItem(int itemId);

    struct StoredItemStats {
        int totalCount{};
        int equippedCount{};
        ItemInstanceID availableInstanceId{-1};
    };
    StoredItemStats getStoredItemStats(int itemId) const;

    // Instance management
    void removeChess(ChessInstanceID chessInstanceId);
    Chess upgradeChessInstance(ChessInstanceID instanceId, int newStar);

    // Queries
    
    std::vector<Chess> getChessByStarAndTier(int star, int maxTier) const;
    Chess findChessByInstanceId(ChessInstanceID instanceId) const;

private:
    bool hasAtLeastMatchingPieces(Role* role, int star, int count) const;


public:
    // Core states that are on top of GameDataStore
    // Note those needs explicit conversion against GameDataStore
    // Not ever sync'd against GameDataStore.
    // We will however export and import those correctly.
    // We can later further decouple those
    ChessPool chessPool;            // handles shopping
    BattleProgress battleProgress;  // simple abstraction for fight level

private:
    // current set of chess pieces
    // map from chessInstandId to the Chess itself
    // we cannot add a chess without an instance ID
    // and the instance ID facilitates quick look up of any data
    std::map<ChessInstanceID, Chess> collection_; 

    // Your inventory, the raw data store is simply a map
    // from equipment instance ID to item ID and chess instance ID. 
    // To keep track of the equipped, if chess instance ID is not set (eg. -1)
    // then it means its unequipped. 
    struct AttachableItem {
        InstancedItem instance{};
        ChessInstanceID chessInstanceId{};
    };

    std::map<ItemInstanceID, AttachableItem> equipments_;
    
    // Random devices that are not copyable.
    // Those states needs explicit setting on import.
    Random<> shopRand_;  // shop has to be deterministic
    Random<> enemyRand_; // enemy encounter has to be deterministic    
};

}
