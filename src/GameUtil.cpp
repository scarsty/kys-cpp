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
    auto test = [](SAVE_INT v, SAVE_INT v_need)->bool
    {
        if (v_need > 0 && v < v_need) { return false; }
        if (v_need < 0 && v > -v_need) { return false; }
        return true;
    };

    if (r == nullptr || i == nullptr) { return false; }
    if (i->ItemType == 0)
    {
        //剧情类无人可以使用
        return false;
    }
    else if (i->ItemType == 1 || i->ItemType == 2)
    {
        if (i->ItemType == 2)
        {
            //内力属性判断
            if ((r->MPType == 0 || r->MPType == 1) && (i->NeedMPType == 0 || i->NeedMPType == 1))
            {
                if (r->MPType != i->NeedMPType)
                {
                    return false;
                }
            }
            //有仅适合人物，直接判断
            if (i->OnlySuitableRole >= 0)
            {
                return i->OnlySuitableRole == r->ID;
            }
        }

        //若有相关武学，满级则为假，未满级为真
        if (i->MagicID > 0)
        {
            int level = r->getMagicLevelIndex(i->MagicID);
            if (level >= 0 && level < MAX_MAGIC_LEVEL_INDEX) { return true; }
            if (level == MAX_MAGIC_LEVEL_INDEX) { return false; }
        }

        //上面的判断未确定则进入下面的判断链
        return test(r->Attack, i->NeedAttack) && test(r->Speed, i->NeedSpeed)
            && test(r->Medcine, i->NeedMedcine)
            && test(r->UsePoison, i->NeedUsePoison) && test(r->Detoxification, i->NeedDetoxification)
            && test(r->Fist, i->NeedFist) && test(r->Sword, i->NeedSword)
            && test(r->Knife, i->NeedKnife) && test(r->Unusual, i->NeedUnusual)
            && test(r->HiddenWeapon, i->NeedHiddenWeapon)
            && test(r->MP, i->NeedMP)
            && test(r->IQ, i->NeedIQ);
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
