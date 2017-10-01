#include "Types.h"
#include "Save.h"

Magic* Role::getLearnedMagic(int i)
{
    if (i < 0 || i >= ROLE_MAGIC_COUNT) { return nullptr; }
    return Save::getInstance()->getMagic(MagicID[i]);
}

int Role::getLearnedMagicLevel(int i)
{
    return MagicLevel[i] / 100 + 1;
}

void Role::limit()
{

}

void SubMapEvent::setPosition(int x, int y, SubMapInfo* submap_record)
{
    if (x < 0) { x = X_; }
    if (y < 0) { y = Y_; }
    auto index = submap_record->EventIndex(X_, Y_);
    submap_record->EventIndex(X_, Y_) = -1;
    X_ = x;
    Y_ = y;
    submap_record->EventIndex(X_, Y_) = index;
}
