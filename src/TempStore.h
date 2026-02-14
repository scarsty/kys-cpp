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

struct GameData
{
    ChessCollection collection;
    ChessPool chessPool;

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

private:
    int money_ = 0;
    int exp_ = 0;
    int level_ = 0;
};

}    // namespace KysChess