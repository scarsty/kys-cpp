#include "Types.h"
#include "Save.h"

Magic* Role::getLearnedMagic(int i)
{
    return Save::getInstance()->getMagic(MagicID[i]);
}

int16_t& SubMapRecord::Earth(int x, int y)
{
    return Save::getInstance()->submap_data_[ID].data[0][x + y * MAX_SUBMAP_COORD];
}

int16_t& SubMapRecord::Building(int x, int y)
{
    return Save::getInstance()->submap_data_[ID].data[1][x + y * MAX_SUBMAP_COORD];
}

int16_t& SubMapRecord::Decoration(int x, int y)
{
    return Save::getInstance()->submap_data_[ID].data[2][x + y * MAX_SUBMAP_COORD];
}

int16_t& SubMapRecord::EventIndex(int x, int y)
{
    return Save::getInstance()->submap_data_[ID].data[3][x + y * MAX_SUBMAP_COORD];
}

int16_t& SubMapRecord::BuildingHeight(int x, int y)
{
    return Save::getInstance()->submap_data_[ID].data[4][x + y * MAX_SUBMAP_COORD];
}

int16_t& SubMapRecord::EventHeight(int x, int y)
{
    return Save::getInstance()->submap_data_[ID].data[5][x + y * MAX_SUBMAP_COORD];
}

SubMapEvent* SubMapRecord::Event(int x, int y)
{
    int i = EventIndex(x, y);
    return Event(i);
}

SubMapEvent* SubMapRecord::Event(int i)
{
    if (i < 0 || i >= MAX_SUBMAP_EVENT)
    {
        return nullptr;
    }
    return &(Save::getInstance()->submap_event_[MAX_SUBMAP_EVENT * ID + i]);
}


