#include "Types.h"
#include "Save.h"

Magic* Role::getLearnedMagic(int i)
{
    if (i < 0 || i >= ROLE_MAGIC_COUNT) { return nullptr; }
    return Save::getInstance()->getMagic(MagicID[i]);
}

int Role::getLearnedMagicLevel(int i)
{
    return MagicLevel[i] / 100;
}

void Role::limit()
{

}

int16_t& SubMapRecord::Earth(int x, int y)
{
    return Save::getInstance()->submap_data_[ID].data[0][x + y * SUBMAP_COORD_COUNT];
}

int16_t& SubMapRecord::Building(int x, int y)
{
    return Save::getInstance()->submap_data_[ID].data[1][x + y * SUBMAP_COORD_COUNT];
}

int16_t& SubMapRecord::Decoration(int x, int y)
{
    return Save::getInstance()->submap_data_[ID].data[2][x + y * SUBMAP_COORD_COUNT];
}

int16_t& SubMapRecord::EventIndex(int x, int y)
{
    return Save::getInstance()->submap_data_[ID].data[3][x + y * SUBMAP_COORD_COUNT];
}

int16_t& SubMapRecord::BuildingHeight(int x, int y)
{
    return Save::getInstance()->submap_data_[ID].data[4][x + y * SUBMAP_COORD_COUNT];
}

int16_t& SubMapRecord::EventHeight(int x, int y)
{
    return Save::getInstance()->submap_data_[ID].data[5][x + y * SUBMAP_COORD_COUNT];
}

SubMapEvent* SubMapRecord::Event(int x, int y)
{
    int i = EventIndex(x, y);
    return Event(i);
}

SubMapEvent* SubMapRecord::Event(int i)
{
    if (i < 0 || i >= SUBMAP_EVENT_COUNT)
    {
        return nullptr;
    }
    return &(Save::getInstance()->submap_event_[SUBMAP_EVENT_COUNT * ID + i]);
}

void SubMapEvent::setPosition(int x, int y, SubMapRecord* submap_record)
{
    if (x < 0) { x = X_; }
    if (y < 0) { y = Y_; }
    auto index = submap_record->EventIndex(X_, Y_);
    submap_record->EventIndex(X_, Y_) = -1;
    X_ = x;
    Y_ = y;
    submap_record->EventIndex(X_, Y_) = index;
}
