#include "Types.h"
#include "GameUtil.h"

void Role::setPosition(int x, int y)
{
    if (position_layer_ == nullptr || x < 0 || y < 0)
    {
        return;
    }
    if (X_ > 0 && Y_ > 0)
    {
        position_layer_->data(X_, Y_) = -1;
    }
    if (x > 0 && y > 0)
    {
        position_layer_->data(x, y) = ID;
    }
    X_ = x;
    Y_ = y;
}

//显示用的，比内部数组用的多1
int Role::getShowLearnedMagicLevel(int i)
{
    return MagicLevel[i] / 100 + 1;
}

void Role::limit()
{
    GameUtil::limitRefrence(Level, 0, MAX_LEVEL);

    GameUtil::limitRefrence(MaxHP, 0, MAX_HP);
    GameUtil::limitRefrence(MaxMP, 0, MAX_MP);
    GameUtil::limitRefrence(PhysicalPower, 0, MAX_PHYSICAL_POWER);

    GameUtil::limitRefrence(Attack, 0, MAX_ATTACK);
    GameUtil::limitRefrence(Defence, 0, MAX_DEFENCE);
    GameUtil::limitRefrence(Speed, 0, MAX_SPEED);

    GameUtil::limitRefrence(Medcine, 0, MAX_MEDCINE);
    GameUtil::limitRefrence(UsePoison, 0, MAX_USE_POISON);
    GameUtil::limitRefrence(Detoxification, 0, MAX_DETOXIFICATION);
    GameUtil::limitRefrence(AntiPoison, 0, MAX_ANTI_POISON);

    GameUtil::limitRefrence(Fist, 0, MAX_FIST);
    GameUtil::limitRefrence(Sword, 0, MAX_SWORD);
    GameUtil::limitRefrence(Knife, 0, MAX_KNIFE);
    GameUtil::limitRefrence(Unusual, 0, MAX_UNUSUAL);
    GameUtil::limitRefrence(HiddenWeapon, 0, MAX_HIDDEN_WEAPON);

    GameUtil::limitRefrence(Knowledge, 0, MAX_KNOWLEDGE);
    GameUtil::limitRefrence(Morality, 0, MAX_MORALITY);
    GameUtil::limitRefrence(AttackWithPoison, 0, MAX_ATTACK_WITH_POISON);
    GameUtil::limitRefrence(Fame, 0, MAX_FAME);
    GameUtil::limitRefrence(IQ, 0, MAX_IQ);
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
