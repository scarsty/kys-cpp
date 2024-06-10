#include "Types.h"
#include "GameUtil.h"
#include "Random.h"
#include "Save.h"
#include "filefunc.h"
#include "fmt1.h"
#include "strfunc.h"

//设置人物坐标，若输入值为负，相当于从人物层清除
void Role::setPosition(int x, int y)
{
    if (position_layer_ == nullptr)
    {
        X_ = x;
        Y_ = y;
        return;
    }
    if (X_ >= 0 && Y_ >= 0)
    {
        position_layer_->data(X_, Y_) = nullptr;
    }
    if (x >= 0 && y >= 0)
    {
        position_layer_->data(x, y) = this;
    }
    X_ = x;
    Y_ = y;
}

//显示用的，比内部数组用的多1
int Role::getRoleShowLearnedMagicLevel(int i)
{
    return getRoleMagicLevelIndex(i) + 1;
}

//获取武学等级，返回值是0~9，可以直接用于索引武功的威力等数据
int Role::getRoleMagicLevelIndex(int i)
{
    int l = MagicLevel[i] / 100;
    if (l < 0)
    {
        l = 0;
    }
    if (l > 9)
    {
        l = 9;
    }
    return l;
}

//已学习武学的数量
int Role::getLearnedMagicCount()
{
    int n = 0;
    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        if (MagicID[i] > 0)
        {
            n++;
        }
    }
    return n;
}

//依据武学指针获取等级，-1表示未学得
int Role::getMagicLevelIndex(Magic* magic)
{
    return getMagicLevelIndex(magic->ID);
}

int Role::getMagicLevelIndex(int magic_id)
{
    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        if (MagicID[i] == magic_id)
        {
            return getRoleMagicLevelIndex(i);
        }
    }
    return -1;
}

//武学在角色的栏位编号
int Role::getMagicOfRoleIndex(Magic* magic)
{
    if (magic)
    {
        for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
        {
            if (MagicID[i] == magic->ID)
            {
                return i;
            }
        }
    }
    return -1;
}

int Role::getEquipMagicOfRoleIndex(Magic* magic)
{
    if (magic)
    {
        for (int i = 0; i < 4; i++)
        {
            if (EquipMagic[i] == magic->ID)
            {
                return i;
            }
        }
    }
    return -1;
}

std::vector<Magic*> Role::getLearnedMagics()
{
    std::vector<Magic*> v;
    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        auto m = Save::getInstance()->getMagic(MagicID[i]);
        if (m)
        {
            v.push_back(m);
        }
    }
    return v;
}

//限制人物的属性
void Role::limit()
{
    auto limit2 = [&](int& v, int v1, int v2)
    {
        if (v < v1)
        {
            v = v1;
        }
        if (v > v2)
        {
            v = v2;
        }
    };

    auto r_max = Role::getMaxValue();
    limit2(Level, 0, r_max->Level);

    limit2(Exp, 0, r_max->Exp);
    limit2(ExpForItem, 0, r_max->Exp);
    limit2(ExpForMakeItem, 0, r_max->Exp);

    limit2(Poison, 0, r_max->Poison);

    limit2(MaxHP, 0, r_max->HP);
    limit2(MaxMP, 0, r_max->MP);
    limit2(HP, 0, MaxHP);
    limit2(MP, 0, MaxMP);
    limit2(PhysicalPower, 0, r_max->PhysicalPower);

    limit2(Attack, 0, r_max->Attack);
    limit2(Defence, 0, r_max->Defence);
    limit2(Speed, 0, r_max->Speed);

    limit2(Medicine, 0, r_max->Medicine);
    limit2(UsePoison, 0, r_max->UsePoison);
    limit2(Detoxification, 0, r_max->Detoxification);
    limit2(AntiPoison, 0, r_max->AntiPoison);

    limit2(Fist, 0, r_max->Fist);
    limit2(Sword, 0, r_max->Sword);
    limit2(Knife, 0, r_max->Knife);
    limit2(Unusual, 0, r_max->Unusual);
    limit2(HiddenWeapon, 0, r_max->HiddenWeapon);

    limit2(Knowledge, 0, r_max->Knowledge);
    limit2(Morality, 0, r_max->Morality);
    limit2(AttackWithPoison, 0, r_max->AttackWithPoison);
    limit2(Fame, 0, r_max->Fame);
    limit2(IQ, 0, r_max->IQ);

    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        limit2(MagicLevel[i], 0, MAX_MAGIC_LEVEL);
    }
}

int Role::learnMagic(Magic* magic)
{
    if (magic == nullptr || magic->ID <= 0)
    {
        return -1;
    }    //武学id错误
    return learnMagic(magic->ID);
}

int Role::learnMagic(int magic_id)
{
    if (magic_id <= 0)
    {
        return -1;
    }
    //检查是否已经学得
    int index = -1;
    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        if (MagicID[i] == magic_id)
        {
            if (MagicLevel[i] / 100 < MAX_MAGIC_LEVEL_INDEX)
            {
                MagicLevel[i] += 100;
                return 0;
            }
            else
            {
                return -2;    //满级
            }
        }
        //记录最靠前的空位
        if (MagicID[i] <= 0 && index == -1)
        {
            index = i;
        }
    }

    if (index < 0)
    {
        return -3;    //若进行到此index为负，表示武学栏已满
    }
    else
    {
        //增加武学
        MagicID[index] = magic_id;
        MagicLevel[index] = 0;
        return 0;
    }
}

//某人是否可以使用某物品
//原分类：0剧情，1装备，2秘笈，3药品，4暗器
bool Role::canUseItem(Item* i)
{
    if (this == nullptr)
    {
        return false;
    }
    if (i == nullptr)
    {
        return false;
    }
    if (i->ItemType == 0)
    {
        //剧情类无人可以使用
        return false;
    }
    else if (i->ItemType == 1 || i->ItemType == 2)
    {
        //若有相关武学，满级则为假，未满级为真
        //若已经学满武学，则为假
        //此处注意，如果有可制成物品的秘籍，则武学满级之后不会再制药了，请尽量避免这样的设置
        if (i->MagicID > 0)
        {
            int level = getMagicLevelIndex(i->MagicID);
            if (level >= 0 && level < MAX_MAGIC_LEVEL_INDEX)
            {
                return true;
            }
            if (level < 0 && getLearnedMagicCount() == ROLE_MAGIC_COUNT)
            {
                return false;
            }
            if (level == MAX_MAGIC_LEVEL_INDEX)
            {
                return false;
            }
        }

        if (i->ItemType == 2)
        {
            //内力属性判断
            if ((MPType == 0 || MPType == 1) && (i->NeedMPType == 0 || i->NeedMPType == 1))
            {
                if (MPType != i->NeedMPType)
                {
                    return false;
                }
            }
            //有仅适合人物，直接判断
            if (i->OnlySuitableRole >= 0)
            {
                return i->OnlySuitableRole == ID;
            }
        }

        //判断某个属性是否适合
        auto test = [](int v, int v_need) -> bool
        {
            if (v_need > 0 && v < v_need)
            {
                return false;
            }
            if (v_need < 0 && v > -v_need)
            {
                return false;
            }
            return true;
        };

        //上面的判断未确定则进入下面的判断链
        return test(Attack, i->NeedAttack)
            && test(Speed, i->NeedSpeed)
            && test(Medicine, i->NeedMedicine)
            && test(UsePoison, i->NeedUsePoison)
            && test(Detoxification, i->NeedDetoxification)
            && test(Fist, i->NeedFist)
            && test(Sword, i->NeedSword)
            && test(Knife, i->NeedKnife)
            && test(Unusual, i->NeedUnusual)
            && test(HiddenWeapon, i->NeedHiddenWeapon)
            && test(MP, i->NeedMP)
            && test(IQ, i->NeedIQ);
    }
    else if (i->ItemType == 3)
    {
        //药品类所有人可以使用
        return true;
    }
    else if (i->ItemType == 4)
    {
        return true;
    }
    return false;
}

//使用物品时属性变化
void Role::useItem(Item* i)
{
    if (this == nullptr)
    {
        return;
    }
    if (i == nullptr)
    {
        return;
    }
    PhysicalPower += i->AddPhysicalPower;
    HP += i->AddHP;
    MaxHP += i->AddMaxHP;
    MP += i->AddMP;
    MaxMP += i->AddMaxMP;

    Poison += i->AddPoison;

    Medicine += i->AddMedicine;
    Detoxification += i->AddDetoxification;
    UsePoison += i->AddUsePoison;

    Attack += i->AddAttack;
    Defence += i->AddDefence;
    Speed += i->AddSpeed;

    Fist += i->AddFist;
    Sword += i->AddSword;
    Knife += i->AddKnife;
    Unusual += i->AddUnusual;
    HiddenWeapon += i->AddHiddenWeapon;

    Knowledge += i->AddKnowledge;
    Morality += i->AddMorality;
    AntiPoison += i->AddAntiPoison;
    AttackWithPoison += i->AddAttackWithPoison;

    if (i->ChangeMPType == 2)
    {
        MPType = 2;
    }
    if (i->AddAttackTwice)
    {
        AttackTwice = 1;
    }

    int need_item_exp = getFinishedExpForItem(i);
    if (ExpForItem >= need_item_exp)
    {
        learnMagic(i->MagicID);
        ExpForItem -= need_item_exp;
    }

    limit();
}

//升级的属性变化
void Role::levelUp()
{
    if (this == nullptr)
    {
        return;
    }

    Exp -= level_up_list()[Level - 1];
    Level++;
    RandomDouble rand;
    PhysicalPower = Role::getMaxValue()->PhysicalPower;
    MaxHP += IncLife * 3 + rand.rand_int(6);
    HP = MaxHP;
    MaxMP += 20 + rand.rand_int(6);
    MP = MaxMP;

    Hurt = 0;
    Poison = 0;

    Attack += rand.rand_int(7);
    Speed += rand.rand_int(7);
    Defence += rand.rand_int(7);

    auto check_up = [&](int& value, int limit, int max_inc) -> void
    {
        if (value > limit)
        {
            value += 1 + rand.rand_int(max_inc);
        }
    };

    check_up(Medicine, 0, 3);
    check_up(Detoxification, 0, 3);
    check_up(UsePoison, 0, 3);

    check_up(Fist, 10, 3);
    check_up(Sword, 10, 3);
    check_up(Knife, 10, 3);
    check_up(Unusual, 10, 3);
    check_up(HiddenWeapon, 10, 3);

    limit();
}

//是否可以升级
bool Role::canLevelUp()
{
    if (Level >= 1 && Level <= getMaxValue()->Level)
    {
        if (Exp >= getLevelUpExp(Level))
        {
            return true;
        }
    }
    return false;
}

int Role::getLevelUpExp(int level)
{
    if (level <= 0 || level >= getMaxValue()->Level)
    {
        return INT_MAX;
    }
    return level_up_list()[level - 1];
}

//物品经验值是否足够
bool Role::canFinishedItem()
{
    auto item = Save::getInstance()->getItem(PracticeItem);
    if (ExpForItem >= getFinishedExpForItem(item))
    {
        return true;
    }
    return false;
}

//修炼物品所需经验
int Role::getFinishedExpForItem(Item* i)
{
    //无经验设定物品不可修炼
    if (i == nullptr || i->ItemType != 2 || i->NeedExp < 0)
    {
        return INT_MAX;
    }

    int multiple = 7 - IQ / 15;
    if (multiple <= 0)
    {
        multiple = 1;
    }

    //有关联武学的，如已满级则不可修炼
    if (i->MagicID > 0)
    {
        int magic_level_index = getMagicLevelIndex(i->MagicID);
        if (magic_level_index == MAX_MAGIC_LEVEL_INDEX)
        {
            return INT_MAX;
        }
        //初次修炼和从1级升到2级的是一样的
        if (magic_level_index > 0)
        {
            multiple *= magic_level_index;
        }
    }
    return i->NeedExp * multiple;
}

void Role::equip(Item* i)
{
    if (this == nullptr)
    {
        return;
    }
    if (i == nullptr)
    {
        return;
    }

    auto r0 = Save::getInstance()->getRole(i->User);
    auto book = Save::getInstance()->getItem(PracticeItem);
    auto equip0 = Save::getInstance()->getItem(Equip0);
    auto equip1 = Save::getInstance()->getItem(Equip1);

    i->User = ID;

    if (i->ItemType == 2)
    {
        //秘籍
        if (book)
        {
            book->User = -1;
        }
        PracticeItem = i->ID;
        if (r0)
        {
            r0->PracticeItem = -1;
        }
    }
    if (i->ItemType == 1)
    {
        if (i->EquipType == 0)
        {
            if (equip0)
            {
                equip0->User = -1;
            }
            Equip0 = i->ID;
            if (r0)
            {
                r0->Equip0 = -1;
            }
        }
        if (i->EquipType == 1)
        {
            if (equip1)
            {
                equip1->User = -1;
            }
            Equip1 = i->ID;
            if (r0)
            {
                r0->Equip1 = -1;
            }
        }
    }
}

//医疗的效果
int Role::medicine(Role* r2)
{
    if (this == nullptr || r2 == nullptr)
    {
        return 0;
    }
    auto temp = r2->HP;
    r2->HP += Medicine;
    GameUtil::limit2(r2->HP, 0, r2->MaxHP);
    return r2->HP - temp;
}

//解毒
//注意这个返回值通常应为负
int Role::detoxification(Role* r2)
{
    if (this == nullptr || r2 == nullptr)
    {
        return 0;
    }
    auto temp = r2->Poison;
    r2->Poison -= Detoxification / 3;
    GameUtil::limit2(r2->Poison, 0, Role::getMaxValue()->Poison);
    return r2->Poison - temp;
}

//用毒
int Role::usePoison(Role* r2)
{
    if (this == nullptr || r2 == nullptr)
    {
        return 0;
    }
    auto temp = r2->Poison;
    r2->Poison += UsePoison / 3;
    GameUtil::limit2(r2->Poison, 0, Role::getMaxValue()->Poison);
    return r2->Poison - temp;
}

void Role::setMaxValue()
{
    auto role = getMaxValue();
#define GET_VALUE_INT(v, default_v) \
    do { \
        role->v = GameUtil::getInstance()->getInt("constant", #v, default_v); \
        fmt1::print("{} = {}\n", #v, role->v); \
    } while (0)

    fmt1::print("Max values of roles: \n");

    GET_VALUE_INT(Level, 30);
    GET_VALUE_INT(HP, 999);
    GET_VALUE_INT(MP, 999);
    GET_VALUE_INT(PhysicalPower, 100);

    GET_VALUE_INT(Poison, 100);

    GET_VALUE_INT(Attack, 100);
    GET_VALUE_INT(Defence, 100);
    GET_VALUE_INT(Speed, 100);

    GET_VALUE_INT(Medicine, 100);
    GET_VALUE_INT(UsePoison, 100);
    GET_VALUE_INT(Detoxification, 100);
    GET_VALUE_INT(AntiPoison, 100);

    GET_VALUE_INT(Fist, 100);
    GET_VALUE_INT(Sword, 100);
    GET_VALUE_INT(Knife, 100);
    GET_VALUE_INT(Unusual, 100);
    GET_VALUE_INT(HiddenWeapon, 100);

    GET_VALUE_INT(Knowledge, 100);
    GET_VALUE_INT(Morality, 100);
    GET_VALUE_INT(AttackWithPoison, 100);
    GET_VALUE_INT(Fame, 999);
    GET_VALUE_INT(IQ, 100);

    GET_VALUE_INT(Exp, 99999);

    fmt1::print("\n");

#undef GET_VALUE_INT
}

void Role::setLevelUpList()
{
    auto str = filefunc::readFileToString(GameUtil::PATH() + "list/levelup.txt");
    level_up_list() = strfunc::findNumbers<int>(str);
    if (level_up_list().size() < Role::getMaxValue()->Level)
    {
        level_up_list().resize(Role::getMaxValue()->Level, 60000);
    }
}

//设置某个事件的坐标，在一些MOD里面此语句有错误
void SubMapEvent::setPosition(int x, int y, SubMapInfo* submap_record)
{
    if (x < 0)
    {
        x = X_;
    }
    if (y < 0)
    {
        y = Y_;
    }
    auto index = submap_record->EventIndex(X_, Y_);
    submap_record->EventIndex(X_, Y_) = -1;
    X_ = x;
    Y_ = y;
    submap_record->EventIndex(X_, Y_) = index;
}

int Magic::calMaxLevelIndexByMP(int mp, int max_level)
{
    auto limit = [&](int v, int v1, int v2)
    {
        if (v < v1)
        {
            v = v1;
        }
        if (v > v2)
        {
            v = v2;
        }
        return v;
    };
    max_level = limit(max_level, 0, MAX_MAGIC_LEVEL_INDEX);
    if (NeedMP <= 0)
    {
        return max_level;
    }
    int level = limit(mp / (NeedMP * 2) * 2 - 1, 0, max_level);
    return level;
}

int Item::MoneyItemID = 174;
int Item::CompassItemID = 182;

void Role::resetBattleInfo()
{
    Role r0;
    *(RoleSave*)&r0 = *(RoleSave*)this;
    *this = r0;
}

bool Item::isCompass()
{
    return ID == CompassItemID;
}

void Item::setSpecialItems()
{
#define GET_VALUE_INT(v) \
    do { \
        Item::v = GameUtil::getInstance()->getInt("constant", #v, Item::v); \
        fmt1::print("{} = {}\n", #v, Item::v); \
    } while (0)

    GET_VALUE_INT(MoneyItemID);
    GET_VALUE_INT(CompassItemID);

#undef GET_VALUE_INT
}
