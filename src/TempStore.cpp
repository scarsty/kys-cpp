#include "TempStore.h"

namespace KysChess {

void ChessCollection::addChess(Chess c)
{
    countTotal_ += 1;
    decltype(countMap_)::iterator iter;
    if (iter = countMap_.find(c); iter != countMap_.end())
    {
        iter->second += 1;
    }
    else
    {
        auto [it, inserted] = countMap_.emplace(c, 1);
        iter = it;
    }
    if (c.star <= 1 && iter->second == 3)
    {
        countMap_.erase(iter);
        countTotal_ -= 3;
        c.star += 1;
        addChess(c);
    }
}

bool ChessCollection::removeChess(Chess c)
{
    if (auto it = countMap_.find(c); it != countMap_.end())
    {
        it->second -= 1;
        if (it->second == 0)
        {
            countMap_.erase(c);
        }
        countTotal_ -= 1;
        return true;
    }
    return false;
}

} // namespace KysChess