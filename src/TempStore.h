#include <map>
#include <set>

#include "Chess.h"
#include "ChessBalance.h"
#include "ChessPool.h"
#include "Random.h"

namespace KysChess
{

class ChessCollection
{
private:
    std::map<Chess, int> countMap_;

    int countTotal_ = 0;

public:
    void addChess(Chess c);

    bool removeChess(Chess c);

    bool wouldMerge(Chess c) const
    {
        if (c.star > 2) return false;
        auto it = countMap_.find(c);
        return it != countMap_.end() && it->second >= 2;
    }

    int getCount() const
    {
        return countTotal_;
    }

    const std::map<Chess, int>& getChess() const
    {
        return countMap_;
    }
};

class BattleProgress {

public:
    BattleProgress() : fight_(0) {}

    void advance() { fight_++; }

    int getFight() const { return fight_; }
    void setFight(int f) { fight_ = f; }

    bool isBossFight() const { return (fight_ + 1) % ChessBalance::config().bossInterval == 0; }
    bool isGameComplete() const { return fight_ >= ChessBalance::config().totalFights; }

private:
    int fight_;
};


struct GameData
{
    ChessCollection collection;
    ChessPool chessPool;
    BattleProgress battleProgress;

    // Two independent random streams for save/load determinism
    // Shop: pool selection + shop rolls; Enemy: enemy selection + battle variation
    unsigned int shopSeed_ = 0, enemySeed_ = 0;
    unsigned int shopCallCount_ = 0, enemyCallCount_ = 0;
    Random<> shopRand_, enemyRand_;

    void initRand()
    {
        std::random_device rd;
        shopSeed_ = rd();
        enemySeed_ = rd();
        shopCallCount_ = enemyCallCount_ = 0;
        shopRand_.set_seed(shopSeed_);
        enemyRand_.set_seed(enemySeed_);
    }

    void restoreRand()
    {
        shopRand_.set_seed(shopSeed_);
        shopRand_.get_generator().discard(shopCallCount_);
        enemyRand_.set_seed(enemySeed_);
        enemyRand_.get_generator().discard(enemyCallCount_);
    }

    int shopRandInt(int n)
    {
        shopCallCount_++;
        return shopRand_.rand_int(n);
    }

    int enemyRandInt(int n)
    {
        enemyCallCount_++;
        return enemyRand_.rand_int(n);
    }

    static GameData& get() {
        static GameData data;
        return data;
    }

    void reset()
    {
        collection = ChessCollection{};
        chessPool = ChessPool{};
        battleProgress = BattleProgress{};
        selectedForBattle_.clear();
        obtainedNeigong_.clear();
        completedChallenges_.clear();
        money_ = 0;
        exp_ = 0;
        level_ = 0;
        shopLocked_ = false;
    }

    bool spend(int m)
    {
        if (money_ < m)
        {
            return false;
        }

        money_ -= m;

        return true;
    }

    void make(int m)
    {
        money_ += m;
    }

    void increaseExp(int e)
    {
        auto& cfg = ChessBalance::config();
        exp_ += e;
        if (level_ < cfg.maxLevel && level_ < (int)cfg.expTable.size()
            && exp_ >= cfg.expTable[level_])
        {
            exp_ -= cfg.expTable[level_];
            level_ += 1;
        }
    }

    int getExpForNextLevel() const
    {
        auto& cfg = ChessBalance::config();
        if (level_ < cfg.maxLevel && level_ < (int)cfg.expTable.size())
            return cfg.expTable[level_];
        return exp_;
    }

    int getMaxDeploy() const { return std::max(level_ + 1, ChessBalance::config().minBattleSize); }
    int getLevel() const { return level_; }
    int getMoney() const { return money_; }
    int getExp() const { return exp_; }
    void setLevel(int v) { level_ = v; }
    void setMoney(int v) { money_ = v; }
    void setExp(int v) { exp_ = v; }

    int getBenchCount() const { return collection.getCount() - (int)selectedForBattle_.size(); }
    bool isBenchFull() const { return getBenchCount() >= ChessBalance::config().benchSize; }

    const std::vector<Chess>& getSelectedForBattle() const { return selectedForBattle_; }

    void setSelectedForBattle(const std::vector<Chess>& selected) { selectedForBattle_ = selected; }

    void clearSelectedForBattle() { selectedForBattle_.clear(); }

    // Add chess to collection; if merge occurs, auto-upgrade selected pieces
    void addChessAndFixSelection(Chess c)
    {
        collection.addChess(c);
        // Walk up the star chain to find what it merged into
        Chess upgraded = c;
        while (upgraded.star <= 3 && collection.getChess().find(upgraded) == collection.getChess().end())
            upgraded.star++;
        if (upgraded.star == c.star) return;  // no merge
        // Upgrade first matching selected piece; remove extras consumed by merge
        bool kept = false;
        for (auto it = selectedForBattle_.begin(); it != selectedForBattle_.end();)
        {
            if (it->role == c.role && it->star < upgraded.star)
            {
                if (!kept) { it->star = upgraded.star; kept = true; ++it; }
                else it = selectedForBattle_.erase(it);
            }
            else ++it;
        }
    }

    bool isShopLocked() const { return shopLocked_; }
    void setShopLocked(bool v) { shopLocked_ = v; }

    const std::vector<int>& getObtainedNeigong() const { return obtainedNeigong_; }
    void addNeigong(int magicId) { obtainedNeigong_.push_back(magicId); }
    void setObtainedNeigong(std::vector<int> v) { obtainedNeigong_ = std::move(v); }

    bool isChallengeCompleted(int idx) const { return completedChallenges_.count(idx); }
    void completeChallenge(int idx) { completedChallenges_.insert(idx); }
    const std::set<int>& getCompletedChallenges() const { return completedChallenges_; }
    void setCompletedChallenges(std::set<int> v) { completedChallenges_ = std::move(v); }

private:
    int money_ = 0;
    int exp_ = 0;
    int level_ = 0;
    bool shopLocked_ = false;
    std::vector<Chess> selectedForBattle_;
    std::vector<int> obtainedNeigong_;
    std::set<int> completedChallenges_;
};

}    // namespace KysChess