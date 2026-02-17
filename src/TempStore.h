#include <map>

#include "Chess.h"
#include "ChessPool.h"

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

    int getCount()
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
    BattleProgress() : stage_(0), subStage_(0) {}

    void advance() {
        subStage_++;
        if (subStage_ >= 5) {
            subStage_ = 0;
            stage_++;
        }
    }

    int getStage() const { return stage_; }
    int getSubStage() const { return subStage_; }

    // Returns true if this is the last substage of a stage (boss fight)
    bool isBossFight() const { return subStage_ == 4; }

    // Max stage is 3 (0, 1, 2)
    bool isGameComplete() const { return stage_ >= 3; }

private:

    // Every 5 sub stage we move to the next stage
    int stage_;
    int subStage_;
};


struct GameData
{
    ChessCollection collection;
    ChessPool chessPool;
    BattleProgress battleProgress;

    static GameData& get() {
        static GameData data;
        return data;
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
        static int expTable[] = { 0, 10, 30, 60, 100, 150, 210, 280, 360, 450, 550 };
        exp_ += e;
        if (exp_ >= expTable[level_ + 1] && level_ < 10)
        {
            level_ += 1;
        }
    }

    int getLevel() const
    {
        return level_;
    }

    // Selected chess pieces for battle (persisted)
    const std::vector<Chess>& getSelectedForBattle() const { return selectedForBattle_; }

    void setSelectedForBattle(const std::vector<Chess>& selected) { selectedForBattle_ = selected; }

    void clearSelectedForBattle() { selectedForBattle_.clear(); }

private:
    int money_ = 0;
    int exp_ = 0;
    int level_ = 0;
    std::vector<Chess> selectedForBattle_;
};

}    // namespace KysChess