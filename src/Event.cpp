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
    loadEventData();
    talkup_ = new Talk();
    talkdown_ = new Talk();
    addChild(talkup_);
    addChild(talkdown_);
    instance_ = true;
}

Event::~Event()
{
}

bool Event::loadEventData()
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
    submap_record_ = SubMap::getCurrentSubMapRecord();
    save_ = Save::getInstance();
    //将时间的节点加载到绘图栈的最上，这样两个对话可以画出来
    addOnRootTop(this);
    int p = 0;
    bool loop = true;
    int i = 0;
    auto& e = kdef_[num];
    switch (e[i])
    {
        VOID_INSTRUCT_3(1, e, i, oldTalk);
        VOID_INSTRUCT_2(2, e, i, getItem);
        VOID_INSTRUCT_13(3, e, i, modifyEvent);
        VOID_INSTRUCT_1(4, e, i, isUsingItem);
        BOOL_INSTRUCT_0(5, e, i, askBattle);
        BOOL_INSTRUCT_1(6, e, i, tryBattle);
        VOID_INSTRUCT_1(8, e, i, changeMainMapMusic);
        BOOL_INSTRUCT_0(9, e, i, askJoin);

        VOID_INSTRUCT_1(10, e, i, join);
        BOOL_INSTRUCT_0(11, e, i, askRest);
        VOID_INSTRUCT_0(12, e, i, rest);
        VOID_INSTRUCT_0(13, e, i, lightScence);
        VOID_INSTRUCT_0(14, e, i, darkScence);
        VOID_INSTRUCT_0(15, e, i, dead);
        BOOL_INSTRUCT_0(16, e, i, inTeam);
        VOID_INSTRUCT_5(17, e, i, setSubMapMapData);
        BOOL_INSTRUCT_1(18, e, i, haveItemBool);
        VOID_INSTRUCT_2(19, e, i, oldSetScencePosition);

        BOOL_INSTRUCT_0(20, e, i, teamIsFull);
        VOID_INSTRUCT_1(21, e, i, leaveTeam);
        VOID_INSTRUCT_0(22, e, i, zeroAllMP);
        VOID_INSTRUCT_2(23, e, i, setOneUsePoi);
        VOID_INSTRUCT_0(24, e, i, blank);
        VOID_INSTRUCT_4(25, e, i, submapFromTo);
        VOID_INSTRUCT_5(26, e, i, add3EventNum);
        VOID_INSTRUCT_3(27, e, i, playAnimation);
        BOOL_INSTRUCT_3(28, e, i, judgeEthics);
        BOOL_INSTRUCT_3(29, e, i, judgeAttack);

        VOID_INSTRUCT_4(30, e, i, walkFromTo);
        BOOL_INSTRUCT_1(31, e, i, judgeMoney);
        VOID_INSTRUCT_2(32, e, i, getItemWithoutHint);
        VOID_INSTRUCT_3(33, e, i, oldLearnMagic);
        VOID_INSTRUCT_2(34, e, i, addAptitude);
        VOID_INSTRUCT_4(35, e, i, setOneMagic);
        BOOL_INSTRUCT_1(36, e, i, judgeSexual);
        VOID_INSTRUCT_1(37, e, i, addEthics);
        VOID_INSTRUCT_4(38, e, i, changeScencePic);
        VOID_INSTRUCT_1(39, e, i, openScence);

        VOID_INSTRUCT_1(40, e, i, setTowards);
        VOID_INSTRUCT_3(41, e, i, roleGetItem);
        BOOL_INSTRUCT_0(42, e, i, judgeFemaleInTeam);
        BOOL_INSTRUCT_1(43, e, i, haveItemBool);
        VOID_INSTRUCT_6(44, e, i, play2Amination);
        VOID_INSTRUCT_2(45, e, i, addSpeed);
        VOID_INSTRUCT_2(46, e, i, addMP);
        VOID_INSTRUCT_2(47, e, i, addAttack);
        VOID_INSTRUCT_2(48, e, i, addHP);
        VOID_INSTRUCT_2(49, e, i, setMPPro);

        VOID_INSTRUCT_0(51, e, i, askSoftStar);
        VOID_INSTRUCT_0(52, e, i, showEthics);
        VOID_INSTRUCT_0(53, e, i, showRepute);
        VOID_INSTRUCT_0(54, e, i, openAllScence);
        BOOL_INSTRUCT_2(55, e, i, judgeEventNum);
        VOID_INSTRUCT_1(56, e, i, addRepute);
        VOID_INSTRUCT_0(57, e, i, breakStoneGate);
        VOID_INSTRUCT_0(58, e, i, fightForTop);
        VOID_INSTRUCT_0(59, e, i, allLeave);

        BOOL_INSTRUCT_3(60, e, i, judgeSubMapPic);
        BOOL_INSTRUCT_0(61, e, i, judge14BooksPlaced);
        VOID_INSTRUCT_0(62, e, i, backHome);
        VOID_INSTRUCT_2(63, e, i, setSexual);
        VOID_INSTRUCT_0(64, e, i, weiShop);
        VOID_INSTRUCT_1(66, e, i, playMusic);
        VOID_INSTRUCT_1(67, e, i, playWave);

    case 50:
        if (e[i + 1] > 128)
        {
            if (judge5Item(e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5])) { i += e[i + 6]; }
            else { i += e[i + 7]; }
        }
        else
        {
            i += instruct_50e(e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5], e[i + 6], e[i + 7]);
        }
        break;
    case -1:
    case 7:
        loop = false;
        break;
    default:
        //不存在的指令，移动一格
        i += 1;
    }
    removeFromRoot(this);
    if (loop)
    { return 0; }
    else
    { return 1; }
}

void Event::oldTalk(int talk_id, int head_id, int style)
{
    talkup_->setVisible(true);
    talkup_->setContent(talk_[talk_id]);
    talkup_->run(false);
    talkup_->setVisible(false);
}



