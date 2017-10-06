#include "Types.h"
#include "GameUtil.h"

//设置人物坐标，若输入值为负，相当于从人物层清除
void Role::setPosition(int x, int y)
{
    if (position_layer_ == nullptr)
    {
        return;
    }
    if (X_ >= 0 && Y_ >= 0)
    {
        position_layer_->data(X_, Y_) = -1;
    }
    if (x >= 0 && y >= 0)
    {
        position_layer_->data(x, y) = ID;
    }
    X_ = x;
    Y_ = y;
}

//显示用的，比内部数组用的多1
int Role::getShowLearnedMagicLevel(int i)
{
    return getMagicLevelIndex(i) + 1;
}

//获取武学等级，返回值是0~9，可以直接用于索引武功的威力等数据
int Role::getMagicLevelIndex(int i)
{
    int l = MagicLevel[i] / 100;
    if (l < 0) { l = 0; }
    if (l > 9) { l = 9; }
    return l;
}

//限制人物的属性
void Role::limit()
{
    GameUtil::limit2(Level, 0, MAX_LEVEL);

    GameUtil::limit2(MaxHP, 0, MAX_HP);
    GameUtil::limit2(MaxMP, 0, MAX_MP);
    GameUtil::limit2(PhysicalPower, 0, MAX_PHYSICAL_POWER);

    GameUtil::limit2(Attack, 0, MAX_ATTACK);
    GameUtil::limit2(Defence, 0, MAX_DEFENCE);
    GameUtil::limit2(Speed, 0, MAX_SPEED);

    GameUtil::limit2(Medcine, 0, MAX_MEDCINE);
    GameUtil::limit2(UsePoison, 0, MAX_USE_POISON);
    GameUtil::limit2(Detoxification, 0, MAX_DETOXIFICATION);
    GameUtil::limit2(AntiPoison, 0, MAX_ANTI_POISON);

    GameUtil::limit2(Fist, 0, MAX_FIST);
    GameUtil::limit2(Sword, 0, MAX_SWORD);
    GameUtil::limit2(Knife, 0, MAX_KNIFE);
    GameUtil::limit2(Unusual, 0, MAX_UNUSUAL);
    GameUtil::limit2(HiddenWeapon, 0, MAX_HIDDEN_WEAPON);

    GameUtil::limit2(Knowledge, 0, MAX_KNOWLEDGE);
    GameUtil::limit2(Morality, 0, MAX_MORALITY);
    GameUtil::limit2(AttackWithPoison, 0, MAX_ATTACK_WITH_POISON);
    GameUtil::limit2(Fame, 0, MAX_FAME);
    GameUtil::limit2(IQ, 0, MAX_IQ);

    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        GameUtil::limit2(MagicLevel[i], 0, MAX_MAGIC_LEVEL);
    }
}

//已学习武学的数量
int Role::getLearnedMagicCount()
{
    int n = 0;
    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        if (MagicID[i] > 0) { n++; }
    }
    return n;
}

//设置某个事件的坐标，在一些MOD里面此语句有错误
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
