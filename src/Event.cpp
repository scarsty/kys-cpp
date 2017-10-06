#include "Event.h"
#include "MainScene.h"
#include "SubScene.h"
#include "Menu.h"
#include "Save.h"
#include "PotConv.h"
#include "EventMacro.h"
#include "Talk.h"
#include "others/libconvert.h"
#include "Audio.h"
#include "GameUtil.h"
#include "Random.h"

Event Event::event_;

Event::Event()
{
    loadEventData();
    talk_box_ = new Element();
    talk_box_up_ = new Talk();
    talk_box_down_ = new Talk();
    talk_box_->addChild(talk_box_up_);
    talk_box_->addChild(talk_box_down_, 0, 400);
    menu2_ = new MenuText({ "確認（Y）", "取消（N）" });
    menu2_->setPosition(300, 200);
    menu2_->arrange(0, 100, 100, 0);
    text_box_ = new TextBox();
    text_box_->setPosition(300, 300);
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
        {
            talk[i] = talk[i] ^ 0xff;
        }
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
        kdef_[i].resize(length[i] / sizeof(int16_t), -1);
        for (int k = 0; k < length[i] / sizeof(int16_t); k++)
        {
            kdef_[i][k] = *(int16_t*)(kdef + offset[i] + k * 2);
        }
    }
    delete kdef;

    //读取离队列表

    std::string leave_txt = convert::readStringFromFile("../game/list/leave.txt");
    convert::findNumbers(leave_txt, &leave_event_id_);
    if (leave_event_id_.size() > 0)
    {
        leave_event_0_ = leave_event_id_[0];
        leave_event_id_.erase(leave_event_id_.begin());
    }
    return false;
}

//返回值为是否成功执行事件
bool Event::callEvent(int event_id, Element* subscene, int supmap_id, int item_id, int event_index, int x, int y)
{
    if (event_id <= 0 || event_id >= kdef_.size()) { return false; }
    save_ = Save::getInstance();
    subscene_ = dynamic_cast<SubScene*>(subscene);
    submap_id_ = -1;
    if (subscene)
    {
        submap_id_ = subscene_->getMapInfo()->ID;
    }

    item_id_ = item_id;
    event_index_ = event_index;
    x_ = x;
    y_ = y;

    //将节点加载到绘图栈的最上，这样两个对话可以画出来
    Element::addOnRootTop(talk_box_);
    int p = 0;
    bool loop = true;
    int i = 0;
    auto e = kdef_[event_id];
    e.resize(e.size() + 20);  //后面的是缓冲区，避免出错
    printf("Event %d: ", event_id);
    for (auto c : e)
    {
        printf("%d ", c);
    }
    printf("\n");
    while (i < e.size() && loop)
    {
        printf("instruct %d\n", e[i]);
        switch (e[i])
        {
            VOID_INSTRUCT_3(1, e, i, oldTalk);
            VOID_INSTRUCT_2(2, e, i, getItem);
            VOID_INSTRUCT_13(3, e, i, modifyEvent);
            VOID_INSTRUCT_1(4, e, i, isUsingItem);
            BOOL_INSTRUCT_0(5, e, i, askBattle);
        case 6:
            //BOOL_INSTRUCT_2(6, e, i, tryBattle);
            if (tryBattle(e[i + 1], e[i + 4]))
            {
                i += e[i + 2];
            }
            else
            {
                i += e[i + 3];
            }
            i += 5;
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
            VOID_INSTRUCT_5(17, e, i, setSubMapLayerData);
            BOOL_INSTRUCT_1(18, e, i, haveItemBool);
            VOID_INSTRUCT_2(19, e, i, oldSetScencePosition);

            BOOL_INSTRUCT_0(20, e, i, teamIsFull);
            VOID_INSTRUCT_1(21, e, i, leaveTeam);
            VOID_INSTRUCT_0(22, e, i, zeroAllMP);
            VOID_INSTRUCT_2(23, e, i, setRoleUsePoison);
            //VOID_INSTRUCT_0(24, e, i, blank);
            VOID_INSTRUCT_4(25, e, i, subMapViewFromTo);
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
            VOID_INSTRUCT_1(37, e, i, addMorality);
            VOID_INSTRUCT_4(38, e, i, changeSubMapPic);
            VOID_INSTRUCT_1(39, e, i, openSubMap);

            VOID_INSTRUCT_1(40, e, i, setTowards);
            VOID_INSTRUCT_3(41, e, i, roleGetItem);
            BOOL_INSTRUCT_0(42, e, i, checkFemaleInTeam);
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
                instruct_50e(e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5], e[i + 6], e[i + 7], &e[i + 8]);
            }
            i += 8;
            break;

            VOID_INSTRUCT_0(51, e, i, askSoftStar);
            VOID_INSTRUCT_0(52, e, i, showMorality);
            VOID_INSTRUCT_0(53, e, i, showFame);
            VOID_INSTRUCT_0(54, e, i, openAllSubMap);
            BOOL_INSTRUCT_2(55, e, i, checkEventID);
            VOID_INSTRUCT_1(56, e, i, addFame);
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
            loop = false;
            break;
        default:
            //不存在的指令，移动一格
            i += 1;
        }
    }
    Element::removeFromRoot(talk_box_);
    talk_box_up_->setContent("");
    talk_box_down_->setContent("");
    return true;
    //if (loop)
    //{ return 0; }
    //else
    //{ return 1; }
}

SubMapInfo* Event::getSubMapRecordFromID(int submap_id)
{
    auto submap_record = save_->getSubMapInfo(submap_id);
    if (submap_record == nullptr) { submap_record = subscene_->getMapInfo(); }
    return submap_record;
}

int Event::getLeaveEvent(Role* role)
{
    for (int i = 0; i < leave_event_id_.size(); i++)
    {
        if (leave_event_id_[i] == role->ID)
        {
            return leave_event_0_ + 2 * i + 1;
        }
    }
    return -1;
}

//原对话指令
void Event::oldTalk(int talk_id, int head_id, int style)
{
    //talkup_->setVisible(true);
    talk_box_up_->setContent(talk_[talk_id]);
    talk_box_up_->setHeadID(head_id);
    talk_box_up_->run(false);
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
    if (submap_id < 0) { submap_id = submap_id_; }
    if (submap_id < 0) { return; }
    if (event_index < 0) { event_index = event_index_; }
    auto e = save_->getSubMapInfo(submap_id)->Event(event_index);
    //下面的值为-2表示不要修改
    if (cannotWalk >= -1) { e->CannotWalk = cannotWalk; }
    if (index >= -1) { e->Index = index; }
    if (event1 >= -1) { e->Event1 = event1; }
    if (event2 >= -1) { e->Event2 = event2; }
    if (event3 >= -1) { e->Event3 = event3; }
    if (currentPic >= -1) { e->CurrentPic = currentPic; }
    if (endPic >= -1) { e->EndPic = endPic; }
    if (beginPic >= -1) { e->BeginPic = beginPic; }
    if (picDelay >= -1) { e->PicDelay = picDelay; }
    if (x < -1) { x = e->X(); }
    if (y < -1) { y = e->Y(); }
    e->setPosition(x, y, save_->getSubMapInfo(submap_id));
}

//是否使用了某物品
bool Event::isUsingItem(int item_id)
{
    return true;
    return item_id_ == item_id;
}

bool Event::askBattle()
{
    menu2_->setTitle("是否與之過招？");
    return menu2_->run() == 0;
}

bool Event::tryBattle(int battle_id, int get_exp)
{
    return true;
}

void Event::changeMainMapMusic(int music_id)
{

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
                    if (role->TakingItemCount[i] == 0) { role->TakingItemCount[i] = 1; }
                    getItem(role->TakingItem[i], role->TakingItemCount[i]);
                    role->TakingItem[i] = -1;
                    role->TakingItemCount[i] = 0;
                }
            }
            return;
        }
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

void Event::lightScence()
{
    if (subscene_)
    {
        subscene_->lightScene();
    }
}

void Event::darkScence()
{
    if (subscene_)
    {
        subscene_->darkScene();
    }
}

void Event::dead()
{

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

void Event::setSubMapLayerData(int submap_id, int layer, int x, int y, int v)
{
    getSubMapRecordFromID(submap_id)->LayerData(layer, x, y) = v;
}

bool Event::haveItemBool(int item_id)
{
    return save_->getItemCountInBag(item_id) > 0;
}

void Event::oldSetScencePosition(int x, int y)
{
    if (subscene_)
    {
        subscene_->setPosition(x, y);
    }
}

bool Event::teamIsFull()
{
    for (auto r : save_->Team)
    {
        if (r < 0) { return false; }
    }
    return true;
}

void Event::leaveTeam(int role_id)
{
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        if (save_->Team[i] == role_id)
        {
            for (int j = i; j < TEAMMATE_COUNT - 1; j++)
            {
                save_->Team[j] = save_->Team[j + 1];
            }
            save_->Team[TEAMMATE_COUNT - 1] == -1;
            break;
        }
    }
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

void Event::subMapViewFromTo(int x0, int y0, int x1, int y1)
{

}

void Event::add3EventNum(int submap_id, int event_index, int v1, int v2, int v3)
{
    auto s = getSubMapRecordFromID(submap_id);
    auto e = s->Event(event_index);
    if (e)
    {
        e->Event1 += v1;
        e->Event2 += v2;
        e->Event3 += v3;
    }
}

void Event::playAnimation(int event_id, int begin_pic, int end_pic)
{

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
    if (subscene_)
    {
        subscene_->setPosition(x1, y1);
    }
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
    //当物品数量为负，需要整理背包
    if (save_->Items[pos].count <= 0)
    {
        for (int i = pos; i < ITEM_IN_BAG_COUNT - 1; i++)
        {
            save_->Items[i].item_id = save_->Items[i + 1].item_id;
            save_->Items[i].count = save_->Items[i + 1].count;
        }
        save_->Items[ITEM_IN_BAG_COUNT - 1].item_id = -1;
        save_->Items[ITEM_IN_BAG_COUNT - 1].count = 0;
    }
}

void Event::oldLearnMagic(int role_id, int magic_id, int no_display)
{

}

void Event::addIQ(int role_id, int aptitude)
{

}

void Event::setRoleMagic(int role_id, int magic_index_role, int magic_id, int level)
{

}

bool Event::checkRoleSexual(int sexual)
{
    if (sexual <= 255)
    {
        return save_->getRole(0)->Sexual == sexual;
    }
    else
    {
        return x50[0x7000] == 0;
    }
}

void Event::addMorality(int value)
{
    save_->getRole(0)->Morality = GameUtil::limit(save_->getRole(0)->Morality + value, 0, MAX_MORALITY);
}

void Event::changeSubMapPic(int submap_id, int layer, int old_pic, int new_pic)
{
    auto s = getSubMapRecordFromID(submap_id);
    if (s)
    {
        for (int i1 = 0; i1 < SUBMAP_COORD_COUNT; i1++)
        {
            for (int i2 = 0; i2 < SUBMAP_COORD_COUNT; i2++)
            {
                if (s->LayerData(layer, i1, i2) == old_pic)
                {
                    s->LayerData(layer, i1, i2) = new_pic;
                }
            }
        }
    }
}

void Event::openSubMap(int submap_id)
{
    save_->getSubMapInfo(submap_id)->EntranceCondition = 0;
}

void Event::setTowards(int towards)
{
    subscene_->towards_ = (Towards)towards;
}

void Event::roleGetItem(int role_id, int item_id, int count)
{

}

bool Event::checkFemaleInTeam()
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

void Event::play2Amination(int event_index1, int begin_pic1, int end_pic1, int event_index2, int begin_pic2, int end_pic2)
{

}

void Event::addSpeed(int role_id, int value)
{
    auto r = save_->getRole(role_id);
    auto v0 = r->Speed;
    r->Speed = GameUtil::limit(v0 + value, 0, MAX_SPEED);
    text_box_->setTitle(convert::formatString("%s輕功增加%d", r->Name, r->Speed - v0));
    text_box_->run();
}

void Event::addMP(int role_id, int value)
{
    auto r = save_->getRole(role_id);
    auto v0 = r->MaxMP;
    r->MaxMP = GameUtil::limit(v0 + value, 0, MAX_MP);
    text_box_->setTitle(convert::formatString("%s內力增加%d", r->Name, r->MaxMP - v0));
    text_box_->run();
}

void Event::addAttack(int role_id, int value)
{
    auto r = save_->getRole(role_id);
    auto v0 = r->Attack;
    r->Attack = GameUtil::limit(v0 + value, 0, MAX_ATTACK);
    text_box_->setTitle(convert::formatString("%s武力增加%d", r->Name, r->Attack - v0));
    text_box_->run();
}

void Event::addHP(int role_id, int value)
{
    auto r = save_->getRole(role_id);
    auto v0 = r->MaxHP;
    r->MaxHP = GameUtil::limit(v0 + value, 0, MAX_HP);
    text_box_->setTitle(convert::formatString("%s生命增加%d", r->Name, r->MaxHP - v0));
    text_box_->run();
}

void Event::setMPType(int role_id, int value)
{
    save_->getRole(role_id)->MPType = value;
}

bool Event::checkHave5Item(int item_id1, int item_id2, int item_id3, int item_id4, int item_id5)
{
    return (haveItemBool(item_id1) && haveItemBool(item_id2) && haveItemBool(item_id3)
        && haveItemBool(item_id4) && haveItemBool(item_id5));
}

void Event::askSoftStar()
{
    oldTalk(2547 + RandomClassical::rand(18), 114, 0);
}

void Event::showMorality()
{
    text_box_->setTitle(convert::formatString("你的道德指數為%d", save_->getRole(0)->Morality));
    text_box_->run();
}

void Event::showFame()
{
    text_box_->setTitle(convert::formatString("你的聲望指數為%d", save_->getRole(0)->Fame));
    text_box_->run();
}

void Event::openAllSubMap()
{
    int i = 0;
    while (save_->getSubMapInfo(i))
    {
        save_->getSubMapInfo(i)->EntranceCondition = 0;
        i++;
    }
    save_->getSubMapInfo(2)->EntranceCondition = 2;
    save_->getSubMapInfo(38)->EntranceCondition = 2;
    save_->getSubMapInfo(75)->EntranceCondition = 1;
    save_->getSubMapInfo(80)->EntranceCondition = 1;
}

bool Event::checkEventID(int event_index, int value)
{
    return subscene_->getMapInfo()->Event(event_index)->Event1 = value;
}

void Event::addFame(int value)
{
    save_->getRole(0)->Fame += value;
    if (save_->getRole(0)->Fame > 200 && save_->getRole(0)->Fame - value <= 200)
    {
        modifyEvent(70, 11, 0, 11, 932, -1, -1, 7968, 7968, 7968, 0, 18, 21);
    }
}

void Event::breakStoneGate()
{

}

void Event::fightForTop()
{
    std::vector<int> heads =
    {
        8, 21, 23, 31, 32, 43, 7, 11, 14, 20, 33, 34, 10, 12, 19,
        22, 56, 68, 13, 55, 62, 67, 70, 71, 26, 57, 60, 64, 3, 69
    };

}

void Event::allLeave()
{
    for (int i = 1; i < TEAMMATE_COUNT; i++)
    {
        save_->Team[i] = -1;
    }
}

bool Event::checkSubMapPic(int submap_id, int event_index, int pic)
{
    auto s = getSubMapRecordFromID(submap_id);
    if (s)
    {
        auto e = s->Event(event_index);
        {
            if (e)
            {
                return (e->CurrentPic == pic || e->BeginPic == pic || e->EndPic == pic);
            }
        }
    }
    return false;
}

bool Event::check14BooksPlaced()
{
    for (int i = 11; i <= 24; i++)
    {
        if (subscene_->getMapInfo()->Event(i)->CurrentPic != 4664)
        {
            return false;
        }
    }
    return true;
}

void Event::setSexual(int role_id, int value)
{
    save_->getRole(role_id)->Sexual = value;
}

void Event::shop()
{

}

void Event::playMusic(int music_id)
{
    Audio::getInstance()->playMusic(music_id);
}

void Event::playWave(int wave_id)
{
    Audio::getInstance()->playASound(wave_id);
}

void Event::arrangeBag()
{

}

void Event::instruct_50e(int code, int e1, int e2, int e3, int e4, int e5, int e6, int* code_ptr)
{
    switch (code)
    {
    case 0:
        //赋值
        x50[e1] = e2;
        break;
    case 1: break;
    case 2: break;
    case 3: break;
    case 4: break;
    case 5: break;
    case 6: break;
    case 7: break;
    case 8: break;
    case 9: break;
    case 10: break;
    case 11: break;
    case 12: break;
    case 13: break;
    case 14: break;
    case 15: break;
    case 16: break;
    case 17: break;
    case 18: break;
    case 19: break;
    case 20: break;
    case 21: break;
    case 22: break;
    case 23: break;
    case 24: break;
    case 25: break;
    case 26: break;
    case 27: break;
    case 28: break;
    case 29: break;
    case 30: break;
    case 31: break;
    case 32: break;
    case 33: break;
    case 34: break;
    case 35: break;
    case 36: break;
    case 37: break;
    case 38: break;
    case 39: break;
    case 40: break;
    case 41: break;
    case 42: break;
    case 43: break;
    case 44: break;
    case 45: break;
    case 46: break;
    case 47: break;
    case 48: break;
    case 49: break;
    case 50: break;
    case 51: break;
    case 52: break;
    case 53: break;
    case 54: break;
    case 55: break;
    case 56: break;
    case 57: break;
    case 58: break;
    case 59: break;
    case 60: break;

    default:
        break;
    }
}

