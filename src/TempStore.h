#include <unordered_map>

#include "Chess.h"

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

    int money = 0;
    int exp = 0;
    int level = 0;

    bool spend(int m)

    {
        if (money < m)
        {
            return false;
        }

        money -= m;

        return true;
    }

    void make(int m)

    {
        money += m;
    }

    void increaseExp(int e)

    {
        static int expTable[] = { 0, 10, 30, 60, 100, 150, 210, 280, 360, 450, 550 };
        exp += e;
        if (exp >= expTable[level + 1] && level < 10)
        {
            level += 1;
        }
    }

    int getLevel() const
    {
        return level;
    }
};

}    // namespace KysChess