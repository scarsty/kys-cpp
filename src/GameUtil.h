#pragma once
#include "Types.h"
#include <climits>
#include <cmath>
#include "INIReader.h"

//此类中是一些游戏中的公式，例如使用物品的效果，伤害公式等
//通常来说应该全部是静态函数
class GameUtil : public INIReaderNormal
{
private:
    GameUtil();
    ~GameUtil();
    std::vector<int> level_up_list_;
    //std::vector<int> level_up_list_;
public:
    static GameUtil* getInstance()
    {
        static GameUtil gu;
        return &gu;
    }

    static const std::string& VERSION()
    {
        static std::string v = "";
        return v;
    }
    
    static int sign(int v)
    {
        if (v > 0)
        {
            return 1;
        }
        if (v < 0)
        {
            return -1;
        }
        return 0;
    }

    //返回限制值
    static int limit(int current, int min_value, int max_value)
    {
        if (current < min_value)
        {
            current = min_value;
        }
        if (current > max_value)
        {
            current = max_value;
        }
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

    static void limit2(uint16_t& current, int min_value, int max_value)
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

    static bool canUseItem(Role* r, Item* i);
    static void useItem(Role* r, Item* i);
    static void levelUp(Role* r);
    static bool canLevelUp(Role* r);
    static int getLevelUpExp(int level);
    static bool canFinishedItem(Role* r);
    static int getFinishedExpForItem(Role* r, Item* i);

    static void equip(Role* r, Item* i);

    //以下3个函数的返回值为需要显示的数值
    static int medicine(Role* r1, Role* r2);
    static int detoxification(Role* r1, Role* r2);
    static int usePoison(Role* r1, Role* r2);

    void setRoleMaxValue(Role* role);
    void setSpecialItems();
};
