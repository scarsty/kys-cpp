#include <unordered_map>

#include "Save.h"

namespace KysChess {

struct Chess
{
    Role* role = nullptr;
    int star = 0;
    friend auto operator<=>(const Chess&, const Chess&) = default;
};

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

    bool spend(int m)
    {
        if (money < m) {
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
        exp += e;
    }

};


} // namespace KysChess
