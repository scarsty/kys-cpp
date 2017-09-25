#include "Event.h"
#include "MainMap.h"
#include "SubMap.h"
#include "Menu.h"
#include "Save.h"
#include "PotConv.h"
#include "EventMacro.h"

Event Event::event_;

Event::Event()
{
    initEventData();
}

Event::~Event()
{
}

bool Event::initEventData()
{
    auto talk = Save::getIdxContent("../game/resource/talk.idx", "../game/resource/talk.grp", &offset, &length);
    for (int i = 0; i < offset.back(); i++)
    {
        if (talk[i])
        { talk[i] = talk[i] ^ 0xff; }
    }
    for (int i = 0; i < length.size(); i++)
    {
        talk_.push_back(PotConv::cp950tocp936(talk + offset[i]));
    }
    return false;
}

bool Event::callEvent(int num)
{
    /*
    int p = 0;
    while (p < operation->size())
    {
        int instruct = operation->at(p).num;
        if (instruct < 0)
        { break; }
        string str;
        for (auto i = 1; i < operation->at(p).par.size(); i++)
        {
            str += "[";
            str += to_string(operation->at(p).par[i]);
            str += "]";
        }
        cout << "执行指令" << operation->at(p).num << str << endl;*/
    int i = 0;
    int e[100];

    switch (e[i])
    {
        RUN_INCTRUCT_3(1, e, i, oldTalk);
        RUN_INCTRUCT_2(2, e, i, getItem);
        RUN_INCTRUCT_13(3, e, i, modifyEvent);
    case 4:
        i += instruct_4(e[i + 1], e[i + 2], e[i + 3]);
        i += 4;
        break;
    case 5:
        i += instruct_5(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 6:
        i += instruct_6(e[i + 1], e[i + 2], e[i + 3], e[i + 4]);
        i += 5;
        break;
    case 7: //Break the event.
        i += 1;
        break;
    case 8:
        instruct_8(e[i + 1]);
        i += 2;
        break;
    case 9:
        i += instruct_9(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 10:
        instruct_10(e[i + 1]);
        i += 2;
        break;
    case 11:
        i += instruct_11(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 12:
        instruct_12;
        i += 1;
        break;
    case 13:
        instruct_13;
        i += 1;
        break;
    case 14:
        instruct_14;
        i += 1;
        break;
    case 15:
        instruct_15;
        i += 1;
        break;
    case 16:
        i += instruct_16(e[i + 1], e[i + 2], e[i + 3]);
        i += 4;
        break;
    case 17:
        instruct_17([e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5]]);
        i += 6;
        break;
    case 18:
        i += instruct_18(e[i + 1], e[i + 2], e[i + 3]);
        i += 4;
        break;
    case 19:
        instruct_19(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 20:
        i += instruct_20(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 21:
        instruct_21(e[i + 1]);
        i += 2;
        break;
    case 22:
        instruct_22;
        i += 1;
        break;
    case 23:
        instruct_23(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 24:
        instruct_24;
        i += 1;
        break;
    case 25:
        instruct_25(e[i + 1], e[i + 2], e[i + 3], e[i + 4]);
        i += 5;
        break;
    case 26:
        instruct_26(e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5]);
        i += 6;
        break;
    case 27:
        instruct_27(e[i + 1], e[i + 2], e[i + 3]);
        i += 4;
        break;
    case 28:
        i += instruct_28(e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5]);
        i += 6;
        break;
    case 29:
        i += instruct_29(e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5]);
        i += 6;
        break;
    case 30:
        instruct_30(e[i + 1], e[i + 2], e[i + 3], e[i + 4]);
        i += 5;
        break;
    case 31:
        i += instruct_31(e[i + 1], e[i + 2], e[i + 3]);
        i += 4;
        break;
    case 32:
        instruct_32(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 33:
        instruct_33(e[i + 1], e[i + 2], e[i + 3]);
        i += 4;
        break;
    case 34:
        instruct_34(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 35:
        instruct_35(e[i + 1], e[i + 2], e[i + 3], e[i + 4]);
        i += 5;
        break;
    case 36:
        i += instruct_36(e[i + 1], e[i + 2], e[i + 3]);
        i += 4;
        break;
    case 37:
        instruct_37(e[i + 1]);
        i += 2;
        break;
    case 38:
        instruct_38(e[i + 1], e[i + 2], e[i + 3], e[i + 4]);
        i += 5;
        break;
    case 39:
        instruct_39(e[i + 1]);
        i += 2;
        break;
    case 40:
        instruct_40(e[i + 1]);
        i += 2;
        break;
    case 41:
        instruct_41(e[i + 1], e[i + 2], e[i + 3]);
        i += 4;
        break;
    case 42:
        i += instruct_42(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 43:
        i += instruct_43(e[i + 1], e[i + 2], e[i + 3]);
        i += 4;
        break;
    case 44:
        instruct_44(e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5], e[i + 6]);
        i += 7;
        break;
    case 45:
        instruct_45(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 46:
        instruct_46(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 47:
        instruct_47(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 48:
        instruct_48(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 49:
        instruct_49(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 50:
        p = instruct_50([e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5], e[i + 6], e[i + 7]]);
        i += 8;
        if p < 622592 then
        i += p
            else
        { e[i + ((p + 32768) div 655360) - 1] = p mod 655360; }
        break;
    case 51:
        instruct_51;
        i += 1;
        break;
    case 52:
        instruct_52;
        i += 1;
        break;
    case 53:
        instruct_53;
        i += 1;
        break;
    case 54:
        instruct_54;
        i += 1;
        break;
    case 55:
        i += instruct_55(e[i + 1], e[i + 2], e[i + 3], e[i + 4]);
        i += 5;
        break;
    case 56:
        instruct_56(e[i + 1]);
        i += 2;
        break;
    case 57:
        i += 1;
        break;
    case 58:
        instruct_58;
        i += 1;
        break;
    case 59:
        instruct_59;
        i += 1;
        break;
    case 60:
        i += instruct_60(e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5]);
        i += 6;
        break;
    case 61:
        i += e[i + 1];
        i += 3;
        break;
    case 62:
        instruct_62(e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5], e[i + 6]);
        i += 7;
        break;
    case 63:
        instruct_63(e[i + 1], e[i + 2]);
        i += 3;
        break;
    case 64:
        instruct_64;
        i += 1;
        break;
    case 65:
        i += 1;
        break;
    case 66:
        instruct_66(e[i + 1]);
        i += 2;
        break;
    case 67:
        instruct_67(e[i + 1]);
        i += 2;
        break;
    default:
        //不存在的指令，移动一格
        i += 1;
    }

    return 0;
}


void Event::clear()
{
}


void Event::getItem_2(short tnum, short amount, short rnum)
{
    //getJueSe(&rnum);
    ////Character role;
    //role = Save::getInstance()->roles_[rnum];
    //if (rnum == 0)
    //{
    //    for (auto i : Save::getInstance()->global_data_[0].m_RItemList)
    //    {
    //        if (i.Number == tnum)
    //        {
    //            i.Amount += amount;
    //            break;
    //        }
    //        if (i.Number == -1)
    //        {
    //            i.Number = tnum;
    //            i.Amount = amount;
    //            break;
    //        }
    //    }
    //}
    //else
    //{
    //    for (auto i : role.TakingItem)
    //    {
    //        if (i.Number == tnum)
    //        {
    //            i.Amount += amount;
    //            break;
    //        }
    //        if (i.Number == -1)
    //        {
    //            i.Number = tnum;
    //            i.Amount = amount;
    //            break;
    //        }
    //    }
    //}
}

int Event::judgeItem_4(short inum, short jump1, short jump2)
{
    //if (inum == Head::CurItem)
    //{ return jump1; }
    return jump2;
}
int Event::isFight_5(short jump1, short jump2)
{
    auto menu = new MenuText();
    //menu->setButton("menu", 12, 14, -1, 13, 15, -1);
    //menu->setTitle("是否與之戰鬥？");
    //menu->ini();

    auto a = menu->getResult();
    if (a == 0)
    { return jump1; }
    return jump2;
}
int Event::isAdd_8(short jump1, short jump2)
{
    auto menu = new MenuText();
    //menu->setButton("title", 12, 13, -1, 14, 15, -1);
    //menu->setTitle("是否要求加入？");
    //menu->ini();

    auto a = menu->getResult();
    if (a == 0)
    { return jump1; }
    return jump2;
}


void Event::StudyMagic(int rnum, int magicnum, int newmagicnum, int level, int dismode)
{
    //int max0, tmp;
    //if (dismode == 1)
    //{
    //    //nend new talk
    //}
    //else if (newmagicnum == 0)          //delete magic
    //{
    //    for (int i = 0; i < config::MaxMagicNum - 1; i++)
    //    {
    //        if (Save::getInstance()->roles_[rnum].LMagic[i] == magicnum)
    //        {
    //            for (int n = i; n < config::MaxMagicNum - 2; n++)
    //            {
    //                Save::getInstance()->roles_[rnum].LMagic[n] = Save::getInstance()->roles_[rnum].LMagic[n + 1];
    //                Save::getInstance()->roles_[rnum].MagLevel[n] = Save::getInstance()->roles_[rnum].MagLevel[n + 1];
    //            }
    //            Save::getInstance()->roles_[rnum].LMagic[29] = 0;
    //            Save::getInstance()->roles_[rnum].MagLevel[29] = 0;
    //            break;
    //        }
    //    }

    //}
    //else
    //{
    //    for (int i = 0; i < config::MaxMagicNum - 1; i++)
    //    {
    //        if (Save::getInstance()->roles_[rnum].LMagic[i] == newmagicnum)
    //        {
    //            if (level == -2)
    //            {
    //                level = 0;
    //            }
    //            max0 = 9;
    //            if (Save::getInstance()->magics_[newmagicnum].MagicType == 5)
    //            {
    //                max0 = Save::getInstance()->magics_[newmagicnum].MaxLevel;
    //            }
    //            tmp = Save::getInstance()->roles_[rnum].MagLevel[i];
    //            if (max0 * 100 + 99 > Save::getInstance()->roles_[rnum].MagLevel[i] + level + 100)
    //            {
    //                Save::getInstance()->roles_[rnum].MagLevel[i] = Save::getInstance()->roles_[rnum].MagLevel[i] + level + 100;
    //            }
    //            else
    //            {
    //                Save::getInstance()->roles_[rnum].MagLevel[i] = max0 * 100 + 99;
    //            }
    //            StudyMagic(rnum, magicnum, 0, 0, 0);
    //            break;
    //        }
    //    }
    //    if (magicnum > 0)
    //    {
    //        for (int i = 0; i < config::MaxMagicNum - 1; i++)
    //        {
    //            if (Save::getInstance()->roles_[rnum].LMagic[i] == magicnum || Save::getInstance()->roles_[rnum].LMagic[i] < 0)
    //            {
    //                if (level != -2)
    //                {
    //                    Save::getInstance()->roles_[rnum].MagLevel[i] = level;
    //                }
    //                Save::getInstance()->roles_[rnum].LMagic[i] == newmagicnum;
    //                break;
    //            }
    //        }
    //    }
    //}
}




