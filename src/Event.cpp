#include "Event.h"
#include "MainMap.h"
#include "SubMap.h"
#include "Menu.h"
#include "Save.h"
#include "PotConv.h"
#include "EventMacro.h"
#include "Talk.h"

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
    //读取talk
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
    delete talk;
    //读取事件，全部转为整型
    auto kdef = Save::getIdxContent("../game/resource/kdef.idx", "../game/resource/kdef.grp", &offset, &length);
    kdef_.resize(length.size());
    for (int i = 0; i < length.size(); i++)
    {
        kdef_[i].resize(length[i] + 20, -1);
        for (int k = 0; k < length[i]; k++)
        {
            kdef_[i][k] = *(int16_t*)(kdef + offset[i] + k * 2);
        }
    }
    delete kdef;
    return false;
}

bool Event::callEvent(int num)
{
    int p = 0;
    bool loop = true;
    int i = 0;
    auto& e = kdef_[num];
    switch (e[i])
    {
        VOID_INCTRUCT_3(1, e, i, oldTalk);
        VOID_INCTRUCT_2(2, e, i, getItem);
        VOID_INCTRUCT_13(3, e, i, modifyEvent);
        VOID_INCTRUCT_0(4, e, i, useItem);
        BOOL_INCTRUCT_0(5, e, i, askBattle);
        BOOL_INCTRUCT_0(6, e, i, tryBattle);
        VOID_INCTRUCT_0(8, e, i, changeMMapMusic);
        BOOL_INCTRUCT_0(9, e, i, askJoin);

        VOID_INCTRUCT_0(10, e, i, join);
        BOOL_INCTRUCT_0(11, e, i, askRest);
        VOID_INCTRUCT_0(12, e, i, rest);
        VOID_INCTRUCT_0(13, e, i, lightScence);
        VOID_INCTRUCT_0(14, e, i, darkScence);
        VOID_INCTRUCT_0(15, e, i, dead);
        BOOL_INCTRUCT_0(16, e, i, inTeam);
        VOID_INCTRUCT_0(17, e, i, oldSetScenceMapPro);
        BOOL_INCTRUCT_0(18, e, i, haveItemBool);
        VOID_INCTRUCT_0(19, e, i, oldSetScencePosition);

        BOOL_INCTRUCT_0(20, e, i, teamIsFull);
        VOID_INCTRUCT_0(21, e, i, leaveTeam);
        VOID_INCTRUCT_0(22, e, i, zeroAllMP);
        VOID_INCTRUCT_0(23, e, i, setOneUsePoi);
        VOID_INCTRUCT_0(24, e, i, blank);
        VOID_INCTRUCT_0(25, e, i, scenceFromTo);
        VOID_INCTRUCT_0(26, e, i, add3EventNum);
        VOID_INCTRUCT_0(27, e, i, playAnimation);
        BOOL_INCTRUCT_0(28, e, i, judgeEthics);
        BOOL_INCTRUCT_0(29, e, i, judgeAttack);

        VOID_INCTRUCT_0(30, e, i, walkFromTo);
        BOOL_INCTRUCT_0(31, e, i, judgeMoney);
        VOID_INCTRUCT_0(32, e, i, addItem);
        VOID_INCTRUCT_0(33, e, i, oldLearnMagic);
        VOID_INCTRUCT_0(34, e, i, addAptitude);
        VOID_INCTRUCT_0(35, e, i, setOneMagic);
        BOOL_INCTRUCT_0(36, e, i, judgeSexual);
        VOID_INCTRUCT_0(37, e, i, addEthics);
        VOID_INCTRUCT_0(38, e, i, changeScencePic);
        VOID_INCTRUCT_0(39, e, i, openScence);

        VOID_INCTRUCT_0(40, e, i, setRoleFace);
        VOID_INCTRUCT_0(41, e, i, anotherGetItem);
        BOOL_INCTRUCT_0(42, e, i, judgeFemaleInTeam);
        BOOL_INCTRUCT_0(43, e, i, haveItemBool);
        VOID_INCTRUCT_0(44, e, i, play2Amination);
        VOID_INCTRUCT_0(45, e, i, addSpeed);
        VOID_INCTRUCT_0(46, e, i, addMP);
        VOID_INCTRUCT_0(47, e, i, addAttack);
        VOID_INCTRUCT_0(48, e, i, addHP);
        VOID_INCTRUCT_0(49, e, i, setMPPro);

        BOOL_INCTRUCT_0(50, e, i, judge5Item);
        VOID_INCTRUCT_0(51, e, i, askSoftStar);
        VOID_INCTRUCT_0(52, e, i, showEthics);
        VOID_INCTRUCT_0(53, e, i, showRepute);
        VOID_INCTRUCT_0(54, e, i, openAllScence);
        BOOL_INCTRUCT_0(55, e, i, judgeEventNum);
        VOID_INCTRUCT_0(56, e, i, addRepute);
        VOID_INCTRUCT_0(57, e, i, breakStoneGate);
        VOID_INCTRUCT_0(58, e, i, fightForTop);
        VOID_INCTRUCT_0(59, e, i, allLeave);

        BOOL_INCTRUCT_0(60, e, i, judgeScencePic);
        BOOL_INCTRUCT_0(61, e, i, judge14BooksPlaced);
        VOID_INCTRUCT_0(62, e, i, backHome);
        VOID_INCTRUCT_0(63, e, i, setSexual);
        VOID_INCTRUCT_0(64, e, i, weiShop);
        VOID_INCTRUCT_0(66, e, i, playMusic);
        VOID_INCTRUCT_0(67, e, i, playWave);

    //case 50:
    //    p = instruct_50([e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5], e[i + 6], e[i + 7]]);
    //    i += 8;
    //    if p < 622592 then
    //    i += p
    //        else
    //    { e[i + ((p + 32768) div 655360) - 1] = p mod 655360; }
    case -1:
    case 7:
        loop = false;
        break;

    default:
        //不存在的指令，移动一格
        i += 1;
    }

    return 0;
}

void Event::oldTalk(int talk_id, int head_id, int style)
{
    auto t = new Talk(talk_[talk_id], head_id);
    t->run();
}



