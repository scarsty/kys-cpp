#include "GameUtil.h"

GameUtil::GameUtil()
{
}

GameUtil::~GameUtil()
{
}

//原分类：0剧情，1装备，2秘笈，3药品，4暗器
bool GameUtil::canUseItem(Role* r, Item* i)
{
    if (r == nullptr || i == nullptr) { return false; }
    if (i->ItemType == 0)
    {
        //剧情类无人可以使用
        return false;
    }
    else if (i->ItemType == 1 || i->ItemType == 2)
    {
        return true;
    }
    else if (i->ItemType == 3)
    {
        //药品类所有人可以使用
        return true;
    }
    else if (i->ItemType == 4)
    {
        //暗器类不可以使用
        return false;
    }
    return false;
}
