#include "Event.h"
#include "MainMap.h"
#include "SubMap.h"
#include "Menu.h"
#include "Save.h"
#include "PotConv.h"

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
        cout << "执行指令" << operation->at(p).num << str << endl;
        switch (instruct)
        {
        case -1:
        {
            break;
        }
        case 0:
        {
            clear();
            break;
        }
        case 1:
        {
            break;
        }
        case 2:
        {
            getItem_2(operation->at(p).par[1], operation->at(p).par[2], operation->at(p).par[3]);
            break;
        }
        case 3:
        {
            editEvent3(operation->at(p).par[1], operation->at(p).par[2], operation->at(p).par[3], operation->at(p).par[4], operation->at(p).par[5], operation->at(p).par[6], operation->at(p).par[7], operation->at(p).par[8], operation->at(p).par[9], operation->at(p).par[10], operation->at(p).par[11], operation->at(p).par[12], operation->at(p).par[13], operation->at(p).par[14], operation->at(p).par[15], operation->at(p).par[16], operation->at(p).par[17], operation->at(p).par[18], operation->at(p).par[19], operation->at(p).par[20]);
            break;
        }
        case 4:
        {
            judgeItem_4(operation->at(p).par[1], operation->at(p).par[2], operation->at(p).par[3]);
            break;
        }
        case 5:
        {
            isFight_5(operation->at(p).par[1], operation->at(p).par[2]);
            break;
        }
        case 6:
        {
            break;
        }
        case 7: // 获取随机舞台
        {
            break;
        }
        case 8:
        {
            isAdd_8(operation->at(p).par[1], operation->at(p).par[2]);
            break;
        }
        case 9:
        {
            break;
        }
        case 10:
        {
            break;
        }
        case 11:
        {
            break;
        }
        case 12:
        {
            break;
        }
        case 13:
        {
            break;
        }
        case 14:
        {
            break;
        }
        case 15:
        {
            break;
        }
        case 16:
        {
            break;
        }
        case 17:
        {
            break;
        }
        case 18:
        {
            break;
        }
        case 19:
        {
            break;
        }
        case 20:
        {
            break;
        }
        case 21:
        {
            break;
        }
        case 22:
        {
            break;
        }
        case 23:
        {
            break;
        }
        case 24:
        {
            break;
        }
        case 25:
        {
            break;
        }
        case 26:
        {
            break;
        }
        case 27:
        {
            break;
        }
        case 28:
        {
            break;
        }
        case 29:
        {
            break;
        }
        case 30:
        {
            break;
        }
        case 31:
        {
            break;
        }
        case 32:
        {
            break;
        }
        case 33:
        {
            break;
        }
        case 34:
        {
            break;
        }
        case 35:
        {
            break;
        }
        case 36:
        {
            break;
        }
        case 37:
        {
            break;
        }
        case 38:
        {
            break;
        }
        case 39:
        {
            break;
        }
        case 40:
        {
            break;
        }
        case 41:
        {
            break;
        }
        case 42:
        {
            break;
        }
        case 43:
        {
            break;
        }
        case 44:
        {
            break;
        }
        case 45:
        {
            break;
        }
        case 46:
        {
            break;
        }
        case 47:
        {
            break;
        }
        case 48:
        {
            break;
        }
        case 49:
        {
            break;
        }
        case 50:
        {
            break;
        }
        case 51:
        {
            break;
        }
        case 52:
        {
            break;
        }
        case 53:
        {
            break;
        }
        case 54:
        {
            break;
        }
        case 55:
        {
            break;
        }
        case 56:
        {
            break;
        }
        case 57:
        {

            break;
        }
        case 58:
        {

            break;
        }
        case 59:
        {
            break;
        }
        case 60:
        {
            break;
        }
        case 61:
        {
            break;
        }
        case 62:
        {
            break;
        }
        case 63:
        {
            break;
        }
        case 64:
        {
            break;
        }
        case 65:
        {
            break;
        }
        case 66:
        {
            break;
        }
        case 67:
        {
            break;
        }
        case 68:
        {
            break;
        }
        case 69:
        {
            break;
        }
        case 70:
        {
            break;
        }
        case 71:
        {
            break;
        }

        case 73:
        {
            break;
        }
        case 74:
        {
            break;
        }
        case 75:
        {
            break;
        }
        case 76:
        {
            break;
        }
        case 77:
        {
            break;
        }
        case 78:
        {
            break;
        }
        case 79:
        {
            break;
        }
        case 80:
        {
            break;
        }
        case 81:
        {
            break;
        }
        case 82:
        {
            break;
        }
        case 83:
        {
            break;
        }
        case 84:
        {
            break;
        }
        case 85:
        {
            break;
        }
        case 86:
        {
            break;
        }
        case 87:
        {
            break;
        }
        case 88:
        {
            break;
        }
        case 89:
        {
            break;
        }
        case 90:
        {

            break;
        }
        case 91:
        {

            break;
        }
        case 92:
        {

            break;
        }
        case 93:
        {
            break;
        }
        case 94:
        {
            break;
        }
        case 95:
        {
            break;
        }
        case 96:
        {
            break;
        }
        case 97:
        {
            break;
        }
        case 98:
        {
            break;
        }
        case 99:
        {
            break;
        }
        case 100:
        {
            break;
        }
        case 101:
        {
            break;
        }
        case 102:
        {
            break;
        }
        case 103:
        {
            break;
        }
        case 104:
        {
            break;
        }
        case 105:
        {
            break;
        }
        case 106:
        {
            break;
        }
        case 107:
        {
            break;
        }
        case 108:
        {
            break;
        }
        case 109:
        {
            break;
        }
        case 110:
        {
            break;
        }
        case 111:
        {
            break;
        }
        case 112:
        {
            break;
        }
        case 113:
        {
            break;
        }
        case 114:
        {
            break;
        }
        case 115:
        {
            break;
        }
        case 116:
        {
            break;
        }
        case 117:
        {
            break;
        }
        case 118:
        {
            break;
        }
        case 119:
        {
            break;
        }
        case 120:
        {
            break;
        }
        case 121:
        {
            break;
        }
        case 122: // 读取当前事件触发人.
        {
            break;
        }
        case 123: // 直接将人物放到地图
        {
            break;
        }
        case 124: // 增加或修改任务提示
        {
            break;
        }
        case 125: // 下场战斗增加人员
        {
            break;
        }
        case 126: // 比賽
        {
            break;
        }
        case 127: // 进入堆
        {
            break;
        }
        case 128: // 弹出堆
        {
            break;
        }
        case 129: // 清空堆
        {
            break;
        }
        case 130: // 新增自动检测任务
        {

            TryEventTmpI = p;
            break;
        }
        case 131: // 修改任务
        {
            break;
        }
        case 132: // 武功融合
        {
            break;
        }
        case 133: //搜索人物标签
        {
            break;
        }
        case 134: //搜索门派
        {
            break;
        }
        case 135: //设置时代
        {
            break;
        }
        case 136: //判断时代
        {
            break;
        }
        case 137: //获得时间
        {
            break;
        }
        case 138: //获得标签
        {
            break;
        }
        case 139: //判断标签
        {
            break;
        }
        case 140: //大排名
        {
            break;
        }
        }
        if ((isTry) && (TryEventTmpI + EventEndCount <= p))
        {
            isTry = false;
            p = TryEventTmpI;
            EventEndCount = 0;
            cout << "事件测试通过" << endl;;
        }
        p++;
    }*/
    return 0;
}


void Event::clear()
{
}


void Event::talk_1()
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

void Event::getJueSe(short* rnum)
{
    //未完成，待角色系统建立
    if (*rnum <= -10)
    {

    }
}
void Event::editEvent3(short snum, short ednum, short CanWalk, short Num, short Event1, short Event2, short Event3, short BeginPic1, short EndPic, short BeginPic2, short  PicDelay, short  XPos, short  YPos, short StartTime, short  Duration, short Interval, short  DoneTime, short  IsActive, short  OutTime, short  OutEvent)
{
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




