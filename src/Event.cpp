#include "Event.h"
#include "MainMap.h"
#include "SubMap.h"
#include "Menu.h"
#include "Save.h"
#include "PotConv.h"
#include "EventMacro.h"
#include "Talk.h"
#include "others/libconvert.h"
#include "Audio.h"

Event Event::event_;

Event::Event()
{
    loadEventData();
    talkup_ = new Talk();
    talkdown_ = new Talk();
    addChild(talkup_);
    addChild(talkdown_, 0, 400);
    menu2_ = new MenuText({ "確認（Y）", "取消（N）" });
    text_box_ = new TextBox();
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
        std::string str = PotConv::cp950tocp936(talk + offset[i]);
        convert::replaceAllString(str, "*", "");
        talk_.push_back(str);
    }
    delete talk;
    //读取事件，全部转为整型
    auto kdef = Save::getIdxContent("../game/resource/kdef.idx", "../game/resource/kdef.grp", &offset, &length);
    kdef_.resize(length.size());
    for (int i = 0; i < length.size(); i++)
    {
        kdef_[i].resize(length[i] + 20, -1);  //20个-1是缓冲区
        for (int k = 0; k < length[i]; k++)
        {
            kdef_[i][k] = *(int16_t*)(kdef + offset[i] + k * 2);
        }
    }
    delete kdef;
    return false;
}

bool Event::callEvent(int event_id, Base* submap, int supmap_id, int item_id, int event_index, int x, int y)
{
    save_ = Save::getInstance();
    submap_ = dynamic_cast<SubMap*>(submap);
    submap_record_ = nullptr;
    submap_id_ = -1;
    if (submap)
    {
        submap_record_ = submap_->getRecord();
        submap_id_ = submap_record_->ID;
    }

    item_id_ = item_id;
    event_index_ = event_index;
    x_ = x;
    y_ = y;

    //将节点加载到绘图栈的最上，这样两个对话可以画出来
    addOnRootTop(this);
    int p = 0;
    bool loop = true;
    int i = 0;
    auto& e = kdef_[event_id];
    while (i < e.size() && loop)
    {
        LOG("%d\n", e[i]);
        switch (e[i])
        {
            VOID_INSTRUCT_3(1, e, i, oldTalk);
            VOID_INSTRUCT_2(2, e, i, getItem);
            VOID_INSTRUCT_13(3, e, i, modifyEvent);
            VOID_INSTRUCT_1(4, e, i, isUsingItem);
            BOOL_INSTRUCT_0(5, e, i, askBattle);
        case 6:
            //BOOL_INSTRUCT_1(6, e, i, tryBattle);
            if (tryBattle(e[i + 1], e[i + 4]))
            { i += e[i + 2]; }
            else
            { i += e[i + 3]; }
            break;
        case 7:
            loop = false;
            break;
            VOID_INSTRUCT_1(8, e, i, changeMainMapMusic);
            BOOL_INSTRUCT_0(9, e, i, askJoin);

            VOID_INSTRUCT_1(10, e, i, join);
            BOOL_INSTRUCT_0(11, e, i, askRest);
            VOID_INSTRUCT_0(12, e, i, rest);
            VOID_INSTRUCT_0(13, e, i, lightScence);
            VOID_INSTRUCT_0(14, e, i, darkScence);
            VOID_INSTRUCT_0(15, e, i, dead);
            BOOL_INSTRUCT_1(16, e, i, inTeam);
            VOID_INSTRUCT_5(17, e, i, setSubMapMapData);
            BOOL_INSTRUCT_1(18, e, i, haveItemBool);
            VOID_INSTRUCT_2(19, e, i, oldSetScencePosition);

            BOOL_INSTRUCT_0(20, e, i, teamIsFull);
            VOID_INSTRUCT_1(21, e, i, leaveTeam);
            VOID_INSTRUCT_0(22, e, i, zeroAllMP);
            VOID_INSTRUCT_2(23, e, i, setRoleUsePoison);
            VOID_INSTRUCT_0(24, e, i, blank);
            VOID_INSTRUCT_4(25, e, i, submapFromTo);
            VOID_INSTRUCT_5(26, e, i, add3EventNum);
            VOID_INSTRUCT_3(27, e, i, playAnimation);
            BOOL_INSTRUCT_3(28, e, i, checkRoleMorality);
            BOOL_INSTRUCT_3(29, e, i, checkRoleAttack);

            VOID_INSTRUCT_4(30, e, i, walkFromTo);
            BOOL_INSTRUCT_1(31, e, i, checkEnoughMoney);
            VOID_INSTRUCT_2(32, e, i, getItemWithoutHint);
            VOID_INSTRUCT_3(33, e, i, oldLearnMagic);
            VOID_INSTRUCT_2(34, e, i, addIQ);
            VOID_INSTRUCT_4(35, e, i, setRoleMagic);
            BOOL_INSTRUCT_1(36, e, i, checkRoleSexual);
            VOID_INSTRUCT_1(37, e, i, addEthics);
            VOID_INSTRUCT_4(38, e, i, changeScencePic);
            VOID_INSTRUCT_1(39, e, i, openSubMap);

            VOID_INSTRUCT_1(40, e, i, setTowards);
            VOID_INSTRUCT_3(41, e, i, roleGetItem);
            BOOL_INSTRUCT_0(42, e, i, judgeFemaleInTeam);
            BOOL_INSTRUCT_1(43, e, i, haveItemBool);
            VOID_INSTRUCT_6(44, e, i, play2Amination);
            VOID_INSTRUCT_2(45, e, i, addSpeed);
            VOID_INSTRUCT_2(46, e, i, addMP);
            VOID_INSTRUCT_2(47, e, i, addAttack);
            VOID_INSTRUCT_2(48, e, i, addHP);
            VOID_INSTRUCT_2(49, e, i, setMPType);
        case 50:
            if (e[i + 1] > 128)
            {
                if (checkHave5Item(e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5])) { i += e[i + 6]; }
                else { i += e[i + 7]; }
            }
            else
            {
                i += instruct_50e(e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5], e[i + 6], e[i + 7]);
            }
            break;

            VOID_INSTRUCT_0(51, e, i, askSoftStar);
            VOID_INSTRUCT_0(52, e, i, showEthics);
            VOID_INSTRUCT_0(53, e, i, showRepute);
            VOID_INSTRUCT_0(54, e, i, openAllScence);
            BOOL_INSTRUCT_2(55, e, i, checkEventNum);
            VOID_INSTRUCT_1(56, e, i, addRepute);
            VOID_INSTRUCT_0(57, e, i, breakStoneGate);
            VOID_INSTRUCT_0(58, e, i, fightForTop);
            VOID_INSTRUCT_0(59, e, i, allLeave);

            BOOL_INSTRUCT_3(60, e, i, checkSubMapPic);
            BOOL_INSTRUCT_0(61, e, i, check14BooksPlaced);
            VOID_INSTRUCT_0(62, e, i, backHome);
            VOID_INSTRUCT_2(63, e, i, setSexual);
            VOID_INSTRUCT_0(64, e, i, shop);
            VOID_INSTRUCT_1(66, e, i, playMusic);
            VOID_INSTRUCT_1(67, e, i, playWave);

        case -1:
        default:
            //不存在的指令，移动一格
            i += 1;
        }
    }
    removeFromRoot(this);
    if (loop)
    { return 0; }
    else
    { return 1; }
}

//原对话指令
void Event::oldTalk(int talk_id, int head_id, int style)
{
    //talkup_->setVisible(true);
    talkup_->setContent(talk_[talk_id]);
    talkup_->setHeadID(head_id);
    talkup_->run(false);
    //talkup_->setVisible(false);
}

//获得物品，有提示
void Event::getItem(int item_id, int count)
{
    getItemWithoutHint(item_id, count);
    text_box_->setTitle(convert::formatString("获得物品%s%d个", save_->getItem(item_id)->Name, count));
    text_box_->setTexture(TextureManager::getInstance()->loadTexture("item", item_id));
    text_box_->run();
}

//修改事件定义
void Event::modifyEvent(int submap_id, int event_index, int cannotWalk, int index, int event1, int event2, int event3, int currentPic, int endPic, int beginPic, int picDelay, int x, int y)
{
    if (submap_id == -2) { submap_id = submap_id_; }
    if (submap_id < 0) { return; }
    if (event_index == -2) { event_index = event_index_; }
    auto e = save_->getSubMapRecord(submap_id)->Event(event_index);
    if (cannotWalk != -2) { e->CannotWalk = cannotWalk; }
    if (index != -2) { e->Index = index; }
    if (event1 != -2) { e->Event1 = event1; }
    if (event2 != -2) { e->Event2 = event2; }
    if (event3 != -2) { e->Event3 = event3; }
    if (currentPic != -2) { e->CurrentPic = currentPic; }
    if (endPic != -2) { e->EndPic = endPic; }
    if (beginPic != -2) { e->BeginPic = beginPic; }
    if (picDelay != -2) { e->PicDelay = picDelay; }
    e->setPosition(x, y, save_->getSubMapRecord(submap_id));
}

//是否使用了某物品
bool Event::isUsingItem(int item_id)
{
    return item_id_ == item_id;
}

bool Event::askBattle()
{
    menu2_->setTitle("是否與之過招？");
    return menu2_->run() == 0;
}

bool Event::askJoin()
{
    menu2_->setTitle("是否要求加入？");
    return menu2_->run() == 0;
}

//角色加入，同时获得对方身上的物品
void Event::join(int role_id)
{
    for (auto& r : save_->Team)
    {
        if (r < 0)
        {
            r = role_id;
            auto role = save_->getRole(r);
            for (int i = 0; i < ROLE_TAKING_ITEM_COUNT; i++)
            {
                if (role->TakingItem[i] >= 0)
                {
                    if (role->TakingItemCount[i] == 0)  { role->TakingItemCount[i] = 1; }
                    getItem(role->TakingItem[i], role->TakingItemCount[i]);
                    role->TakingItem[i] = -1;
                    role->TakingItemCount[i] = 0;
                }
            }
        }
        return;
    }
}

bool Event::askRest()
{
    menu2_->setTitle("請選擇是或否？");
    return menu2_->run() == 0;
}

void Event::rest()
{
    for (auto r : save_->Team)
    {
        auto role = save_->getRole(r);
        if (role)
        {
            role->HP = role->MaxHP;
            role->MP = role->MaxMP;
            role->Hurt = 0;
            role->Poison = 0;
        }
    }
}

//某人是否在队伍
bool Event::inTeam(int role_id)
{
    for (auto r : save_->Team)
    {
        if (r == role_id)
        {
            return true;
        }
    }
    return false;
}

bool Event::haveItemBool(int item_id)
{
    return save_->getItemCountInBag(item_id) > 0;
}

bool Event::teamIsFull()
{
    for (auto r : save_->Team)
    {
        if (r < 0) { return false; }
    }
    return true;
}

void Event::zeroAllMP()
{
    for (auto r : save_->Team)
    {
        if (r >= 0)
        {
            save_->getRole(r)->MP = 0;
        }
    }
}

void Event::setRoleUsePoison(int role_id, int v)
{
    save_->getRole(role_id)->UsePoison = v;
}

bool Event::checkRoleMorality(int role_id, int low, int high)
{
    return (save_->getRole(role_id)->Morality >= low && save_->getRole(role_id)->Morality <= high);
}

bool Event::checkRoleAttack(int role_id, int low, int high)
{
    return (save_->getRole(role_id)->Attack >= low);
}

void Event::walkFromTo(int x0, int y0, int x1, int y1)
{
    if (submap_)
    { submap_->setPosition(x1, y1); }
}

bool Event::checkEnoughMoney(int money_count)
{
    return (save_->getMoneyCountInBag() >= money_count);
}

void Event::getItemWithoutHint(int item_id, int count)
{
    int pos = -1;
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        if (save_->Items[i].item_id == item_id)
        {
            pos = i;
            break;
        }
    }
    if (pos >= 0)
    {
        save_->Items[pos].count += count;
    }
    else
    {
        for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
        {
            if (save_->Items[i].item_id < 0)
            {
                pos = i;
                break;
            }
        }
        save_->Items[pos].item_id == item_id;
        save_->Items[pos].count == count;
    }
}

void Event::openSubMap(int submap_id)
{
    save_->getSubMapRecord(submap_id)->EntranceCondition = 0;
}

void Event::setTowards(int towards)
{
    Scene::towards_ = (Towards)towards;
}

bool Event::judgeFemaleInTeam()
{
    for (auto r : save_->Team)
    {
        if (r >= 0)
        {
            if (save_->getRole(r)->Sexual == 1) { return true; }
        }
    }
    return false;
}

void Event::allLeave()
{
    for (int i = 1; i < TEAMMATE_COUNT; i++)
    {
        save_->Team[i] = -1;
    }
}

void Event::setSexual(int role_id, int value)
{
    save_->getRole(role_id)->Sexual = value;
}

void Event::playMusic(int music_id)
{
    Audio::getInstance()->playMusic(music_id);
}

void Event::playWave(int wave_id)
{
    Audio::getInstance()->playSound(wave_id);
}

