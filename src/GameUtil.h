#pragma once
#include "Types.h"
#include <cmath>

//此类中是一些游戏中的公式，例如使用物品的效果，伤害公式等
//通常来说应该全部是静态函数
class GameUtil
{
private:
    GameUtil();
    ~GameUtil();
public:
    //返回限制值
    static int limit(int current, int min_value, int max_value)
    {
        if (current < min_value) { current = min_value; }
        if (current > max_value) { current = max_value; }
        return current;
    }

    //limit2是直接修改引用值，有两个重载
    static void limit2(int& current, int min_value, int max_value)
    {
        current = limit(current, min_value, max_value);
    }

    static void limit2(int16_t& current, int min_value, int max_value)
    {
        current = limit(current, min_value, max_value);
    }

    //计算某个数值的位数
    static int digit(int x)
    {
        int n = floor(log10(0.5 + abs(x)));
        if (x >= 0)
        {
            return n;
        }
        else
        {
            return n + 1;
        }
    }

    static bool canUseItem(Role* r, Item* i) { return false; }
    static bool useItem(Role* r, Item* i) { return false; }
};

