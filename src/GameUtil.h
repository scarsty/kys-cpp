#pragma once
#include "Types.h"

//此类中是一些游戏中的公式，例如使用物品的效果，伤害公式等
//通常来说应该全部是静态函数
class GameUtil
{
private:
    GameUtil();
    ~GameUtil();
public:
    static int limit(int current, int min_value, int max_value)
    {
        if (current < min_value) { (current = min_value); }
        if (current > max_value) { (current = max_value); }
        return current;
    }

    static void limitRefrence(int& current, int min_value, int max_value)
    {
        current = limit(current, min_value, max_value);
    }

    static void limitRefrence(int16_t& current, int min_value, int max_value)
    {
        current = limit(current, min_value, max_value);
    }

    static bool canUseItem(Role* r, Item* i) { return false; }
    static bool useItem(Role* r, Item* i) { return false; }
};

