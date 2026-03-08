#pragma once

#include <map>
#include <vector>
#include "Chess.h"
#include "ChessPool.h"
#include "GameState.h"

namespace KysChess
{

struct ChessStoredItemStats {
    int totalCount{};
    int equippedCount{};
    ItemInstanceID availableInstanceId{-1};
};

class IChessGameState
{
public:
    virtual ~IChessGameState() = default;

    virtual int getMoney() const = 0;
    virtual void make(int amount) = 0;
    virtual bool spend(int amount) = 0;

    virtual int getExp() const = 0;
    virtual int getExpForNextLevel() const = 0;
    virtual int getLevel() const = 0;
    virtual void increaseExp(int amount) = 0;
    virtual int getMaxDeploy() const = 0;

    virtual bool wouldMerge(Role* role, int star) const = 0;
    virtual bool canMerge(Role* role, int star) const = 0;
    virtual const std::map<ChessInstanceID, Chess>& getCollection() const = 0;
    virtual std::map<RoleAndStar, int> getChessCountMap() const = 0;
    virtual std::vector<Chess> getSelectedForBattle() const = 0;
    virtual void setSelectedInstanceIds(const std::vector<ChessInstanceID>& selected) = 0;

    virtual Chess createChess(Role* role, int star) = 0;
    virtual void updateToCollection(Chess chess) = 0;
    virtual void removeChess(ChessInstanceID chessInstanceId) = 0;
    virtual Chess upgradeChessInstance(ChessInstanceID instanceId, int newStar) = 0;
    virtual std::vector<Chess> getChessByStarAndTier(int star, int maxTier) const = 0;
    virtual Chess findChessByInstanceId(ChessInstanceID chessInstanceId) const = 0;

    virtual bool isShopLocked() const = 0;
    virtual void setShopLocked(bool value) = 0;
    virtual ChessPool& chessPool() = 0;
    virtual const ChessPool& chessPool() const = 0;
    virtual BattleProgress& battleProgress() = 0;
    virtual const BattleProgress& battleProgress() const = 0;

    virtual const std::vector<int>& getObtainedNeigong() const = 0;
    virtual void addNeigong(int magicId) = 0;

    virtual bool isChallengeCompleted(int idx) const = 0;
    virtual void completeChallenge(int idx) = 0;

    virtual bool isPositionSwapEnabled() const = 0;
    virtual void setPositionSwapEnabled(bool enabled) = 0;

    virtual void storeEquipmentItem(int itemId) = 0;
    virtual ChessStoredItemStats getStoredItemStats(int itemId) const = 0;

    virtual unsigned int getEnemyCallCount() const = 0;
    virtual void setEnemyCallCount(unsigned int count) = 0;
    virtual void restoreRand() = 0;
    virtual int shopRandInt(int n) = 0;
    virtual int enemyRandInt(int n) = 0;
};

class IChessCatalog
{
public:
    virtual ~IChessCatalog() = default;

    virtual Role* getRole(int roleId) const = 0;
    virtual Item* getItem(int itemId) const = 0;
    virtual const std::vector<Role*>& getRoles() const = 0;
};

class IChessFeedback
{
public:
    virtual ~IChessFeedback() = default;

    virtual void showMessage(const std::string& text, int fontSize = 32) = 0;
    virtual void playUpgradeSound() = 0;
};

struct ChessContext {
    IChessGameState& gameState;
    IChessCatalog& catalog;
    IChessFeedback& feedback;
};

}