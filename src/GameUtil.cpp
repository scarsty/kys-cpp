#include "GameUtil.h"
#include "Random.h"
#include "Save.h"
#include "libconvert.h"

GameUtil::GameUtil()
{
    loadFile("../game/config/kysmod.ini");
    auto str = convert::readStringFromFile("../game/list/levelup.txt");
    level_up_list_ = convert::findNumbers<int>(str);
    if (level_up_list_.size() < Role::getMaxValue()->Level)
    {
        level_up_list_.resize(Role::getMaxValue()->Level, 60000);
    }
    setRoleMaxValue(Role::getMaxValue());
    setSpecialItems();
}

GameUtil::~GameUtil()
{
}

//某人是否可以使用某物品
//原分类：0剧情，1装备，2秘笈，3药品，4暗器
bool GameUtil::canUseItem(Role* r, Item* i)
{
    if (r == nullptr)
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
        //若已经学满武学，则为假
        //此处注意，如果有可制成物品的秘籍，则武学满级之后不会再制药了，请尽量避免这样的设置
        if (i->MagicID > 0)
        {
            int level = r->getMagicLevelIndex(i->MagicID);
            if (level >= 0 && level < MAX_MAGIC_LEVEL_INDEX)
            {
                return true;
            }
            if (level < 0 && r->getLearnedMagicCount() == ROLE_MAGIC_COUNT)
            {
                return false;
            }
            if (level == MAX_MAGIC_LEVEL_INDEX)
            {
                return false;
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
        return test(r->Attack, i->NeedAttack)
            && test(r->Speed, i->NeedSpeed)
            && test(r->Medicine, i->NeedMedicine)
            && test(r->UsePoison, i->NeedUsePoison)
            && test(r->Detoxification, i->NeedDetoxification)
            && test(r->Fist, i->NeedFist)
            && test(r->Sword, i->NeedSword)
            && test(r->Knife, i->NeedKnife)
            && test(r->Unusual, i->NeedUnusual)
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

//使用物品时属性变化
void GameUtil::useItem(Role* r, Item* i)
{
    if (r == nullptr)
    {
        return;
    }
    if (i == nullptr)
    {
        return;
    }
    r->PhysicalPower += i->AddPhysicalPower;
    r->HP += i->AddHP;
    r->MaxHP += i->AddMaxHP;
    r->MP += i->AddMP;
    r->MaxMP += i->AddMaxMP;

    r->Poison += i->AddPoison;

    r->Medicine += i->AddMedicine;
    r->Detoxification += i->AddDetoxification;
    r->UsePoison += i->AddUsePoison;

    r->Attack += i->AddAttack;
    r->Defence += i->AddDefence;
    r->Speed += i->AddSpeed;

    r->Fist += i->AddFist;
    r->Sword += i->AddSword;
    r->Knife += i->AddKnife;
    r->Unusual += i->AddUnusual;
    r->HiddenWeapon += i->AddHiddenWeapon;

    r->Knowledge += i->AddKnowledge;
    r->Morality += i->AddMorality;
    r->AntiPoison += i->AddAntiPoison;
    r->AttackWithPoison += i->AddAttackWithPoison;

    if (i->ChangeMPType == 2)
    {
        r->MPType = 2;
    }
    if (i->AddAttackTwice)
    {
        r->AttackTwice = 1;
    }

    int need_item_exp = getFinishedExpForItem(r, i);
    if (r->ExpForItem >= need_item_exp)
    {
        r->learnMagic(i->MagicID);
        r->ExpForItem -= need_item_exp;
    }

    r->limit();
}

//升级的属性变化
void GameUtil::levelUp(Role* r)
{
    if (r == nullptr)
    {
        return;
    }

    r->Exp -= getInstance()->level_up_list_[r->Level - 1];
    r->Level++;
    RandomDouble rand;
    r->PhysicalPower = Role::getMaxValue()->PhysicalPower;
    r->MaxHP += r->IncLife * 3 + rand.rand_int(6);
    r->HP = r->MaxHP;
    r->MaxMP += 20 + rand.rand_int(6);
    r->MP = r->MaxMP;

    r->Hurt = 0;
    r->Poison = 0;

    r->Attack += rand.rand_int(7);
    r->Speed += rand.rand_int(7);
    r->Defence += rand.rand_int(7);

    auto check_up = [&](int& value, int limit, int max_inc) -> void
    {
        if (value > limit)
        {
            value += 1 + rand.rand_int(max_inc);
        }
    };

    check_up(r->Medicine, 0, 3);
    check_up(r->Detoxification, 0, 3);
    check_up(r->UsePoison, 0, 3);

    check_up(r->Fist, 10, 3);
    check_up(r->Sword, 10, 3);
    check_up(r->Knife, 10, 3);
    check_up(r->Unusual, 10, 3);
    check_up(r->HiddenWeapon, 10, 3);

    r->limit();
}

//是否可以升级
bool GameUtil::canLevelUp(Role* r)
{
    if (r->Level >= 1 && r->Level <= Role::getMaxValue()->Level)
    {
        if (r->Exp >= getLevelUpExp(r->Level))
        {
            return true;
        }
    }
    return false;
}

int GameUtil::getLevelUpExp(int level)
{
    if (level <= 0 || level >= Role::getMaxValue()->Level)
    {
        return INT_MAX;
    }
    return getInstance()->level_up_list_[level - 1];
}

//物品经验值是否足够
bool GameUtil::canFinishedItem(Role* r)
{
    auto item = Save::getInstance()->getItem(r->PracticeItem);
    if (r->ExpForItem >= getFinishedExpForItem(r, item))
    {
        return true;
    }
    return false;
}

//修炼物品所需经验
int GameUtil::getFinishedExpForItem(Role* r, Item* i)
{
    //无经验设定物品不可修炼
    if (i == nullptr || i->ItemType != 2 || i->NeedExp < 0)
    {
        return INT_MAX;
    }

    int multiple = 7 - r->IQ / 15;
    if (multiple <= 0)
    {
        multiple = 1;
    }

    //有关联武学的，如已满级则不可修炼
    if (i->MagicID > 0)
    {
        int magic_level_index = r->getMagicLevelIndex(i->MagicID);
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

void GameUtil::equip(Role* r, Item* i)
{
    if (r == nullptr)
    {
        return;
    }
    if (i == nullptr)
    {
        return;
    }

    auto r0 = Save::getInstance()->getRole(i->User);
    auto book = Save::getInstance()->getItem(r->PracticeItem);
    auto equip0 = Save::getInstance()->getItem(r->Equip0);
    auto equip1 = Save::getInstance()->getItem(r->Equip1);

    i->User = r->ID;

    if (i->ItemType == 2)
    {
        //秘籍
        if (book)
        {
            book->User = -1;
        }
        r->PracticeItem = i->ID;
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
            r->Equip0 = i->ID;
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
            r->Equip1 = i->ID;
            if (r0)
            {
                r0->Equip1 = -1;
            }
        }
    }
}

//医疗的效果
int GameUtil::medicine(Role* r1, Role* r2)
{
    if (r1 == nullptr || r2 == nullptr)
    {
        return 0;
    }
    auto temp = r2->HP;
    r2->HP += r1->Medicine;
    GameUtil::limit2(r2->HP, 0, r2->MaxHP);
    return r2->HP - temp;
}

//解毒
//注意这个返回值通常应为负
int GameUtil::detoxification(Role* r1, Role* r2)
{
    if (r1 == nullptr || r2 == nullptr)
    {
        return 0;
    }
    auto temp = r2->Poison;
    r2->Poison -= r1->Detoxification / 3;
    GameUtil::limit2(r2->Poison, 0, Role::getMaxValue()->Poison);
    return r2->Poison - temp;
}

//用毒
int GameUtil::usePoison(Role* r1, Role* r2)
{
    if (r1 == nullptr || r2 == nullptr)
    {
        return 0;
    }
    auto temp = r2->Poison;
    r2->Poison += r1->UsePoison / 3;
    GameUtil::limit2(r2->Poison, 0, Role::getMaxValue()->Poison);
    return r2->Poison - temp;
}

void GameUtil::setRoleMaxValue(Role* role)
{
#define GET_VALUE_INT(v, default_v)                  \
    do                                               \
    {                                                \
        role->v = getInt("constant", #v, default_v); \
        printf("%s = %d\n", #v, role->v);            \
    } while (0)

    printf("Max values of roles: \n");

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

    printf("\n");

#undef GET_VALUE_INT
}

void GameUtil::setSpecialItems()
{
#define GET_VALUE_INT(v)                           \
    do                                             \
    {                                              \
        Item::v = getInt("constant", #v, Item::v); \
        printf("%s = %d\n", #v, Item::v);          \
    } while (0)

    GET_VALUE_INT(MoneyItemID);
    GET_VALUE_INT(CompassItemID);

#undef GET_VALUE_INT
}
