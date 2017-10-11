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
#include "BattleScene.h"

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
    e.resize(e.size() + 20, -1);  //后面的是缓冲区，避免出错
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
            VOID_INSTRUCT_2(2, e, i, addItem);
            VOID_INSTRUCT_13(3, e, i, modifyEvent);
            BOOL_INSTRUCT_1(4, e, i, isUsingItem);
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
            VOID_INSTRUCT_2(32, e, i, addItemWithoutHint);
            VOID_INSTRUCT_3(33, e, i, oldLearnMagic);
            VOID_INSTRUCT_2(34, e, i, addIQ);
            VOID_INSTRUCT_4(35, e, i, setRoleMagic);
            BOOL_INSTRUCT_1(36, e, i, checkRoleSexual);
            VOID_INSTRUCT_1(37, e, i, addMorality);
            VOID_INSTRUCT_4(38, e, i, changeSubMapPic);
            VOID_INSTRUCT_1(39, e, i, openSubMap);

            VOID_INSTRUCT_1(40, e, i, setTowards);
            VOID_INSTRUCT_3(41, e, i, roleAddItem);
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
    auto submap_record = Save::getInstance()->getSubMapInfo(submap_id);
    if (submap_record == nullptr) { submap_record = subscene_->getMapInfo(); }
    return submap_record;
}

int Event::getLeaveEvent(Role* role)
{
    for (int i = 0; i < leave_event_id_.size(); i++)
    {
        if (leave_event_id_[i] == role->ID)
        {
            return leave_event_0_ + 2 * i;
        }
    }
    return -1;
}

void Event::callLeaveEvent(Role* role)
{
    callEvent(getLeaveEvent(role));
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
void Event::addItem(int item_id, int count)
{
    addItemWithoutHint(item_id, count);
    text_box_->setText(convert::formatString("获得物品%s%d个", Save::getInstance()->getItem(item_id)->Name, count));
    text_box_->setTexture(TextureManager::getInstance()->loadTexture("item", item_id));
    text_box_->run();
}

//修改事件定义
void Event::modifyEvent(int submap_id, int event_index, int cannotWalk, int index, int event1, int event2, int event3, int currentPic, int endPic, int beginPic, int picDelay, int x, int y)
{
    if (submap_id < 0) { submap_id = submap_id_; }
    if (submap_id < 0) { return; }
    if (event_index < 0) { event_index = event_index_; }
    auto e = Save::getInstance()->getSubMapInfo(submap_id)->Event(event_index);
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
    e->setPosition(x, y, Save::getInstance()->getSubMapInfo(submap_id));
}

//是否使用了某物品
bool Event::isUsingItem(int item_id)
{
    return item_id_ == item_id;
}

bool Event::askBattle()
{
    menu2_->setText("是否與之過招？");
    return menu2_->run() == 0;
}

bool Event::tryBattle(int battle_id, int get_exp)
{
    auto battle = new BattleScene(battle_id);
    battle->setHaveFailExp(get_exp);
    int result = battle->run();
    delete battle;
    return result == 0;
}

void Event::changeMainMapMusic(int music_id)
{

}

bool Event::askJoin()
{
    menu2_->setText("是否要求加入？");
    return menu2_->run() == 0;
}

//角色加入，同时获得对方身上的物品
void Event::join(int role_id)
{
    for (auto& r : Save::getInstance()->Team)
    {
        if (r < 0)
        {
            r = role_id;
            auto role = Save::getInstance()->getRole(r);
            for (int i = 0; i < ROLE_TAKING_ITEM_COUNT; i++)
            {
                if (role->TakingItem[i] >= 0)
                {
                    if (role->TakingItemCount[i] == 0) { role->TakingItemCount[i] = 1; }
                    addItem(role->TakingItem[i], role->TakingItemCount[i]);
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
    menu2_->setText("請選擇是或否？");
    return menu2_->run() == 0;
}

void Event::rest()
{
    for (auto r : Save::getInstance()->Team)
    {
        auto role = Save::getInstance()->getRole(r);
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
    for (auto r : Save::getInstance()->Team)
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
    return Save::getInstance()->getItemCountInBag(item_id) > 0;
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
    for (auto r : Save::getInstance()->Team)
    {
        if (r < 0) { return false; }
    }
    return true;
}

void Event::leaveTeam(int role_id)
{
    auto save = Save::getInstance();
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        if (save->Team[i] == role_id)
        {
            for (int j = i; j < TEAMMATE_COUNT - 1; j++)
            {
                save->Team[j] = save->Team[j + 1];
            }
            save->Team[TEAMMATE_COUNT - 1] == -1;
            break;
        }
    }
}

void Event::zeroAllMP()
{
    auto save = Save::getInstance();
    for (auto r : save->Team)
    {
        if (r >= 0)
        {
            save->getRole(r)->MP = 0;
        }
    }
}

void Event::setRoleUsePoison(int role_id, int v)
{
    Save::getInstance()->getRole(role_id)->UsePoison = v;
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
    auto role = Save::getInstance()->getRole(role_id);
    return (role->Morality >= low && role->Morality <= high);
}

bool Event::checkRoleAttack(int role_id, int low, int high)
{
    return (Save::getInstance()->getRole(role_id)->Attack >= low);
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
    return (Save::getInstance()->getMoneyCountInBag() >= money_count);
}

void Event::addItemWithoutHint(int item_id, int count)
{
    if (count == 0) { return; }
    int pos = -1;
    auto save = Save::getInstance();
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        if (save->Items[i].item_id == item_id)
        {
            pos = i;
            break;
        }
    }
    if (pos >= 0)
    {
        save->Items[pos].count += count;
    }
    else
    {
        for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
        {
            if (save->Items[i].item_id < 0)
            {
                pos = i;
                break;
            }
        }
        save->Items[pos].item_id = item_id;
        save->Items[pos].count = count;
    }
    //当物品数量为负，需要整理背包
    arrangeBag();
}

void Event::oldLearnMagic(int role_id, int magic_id, int no_display)
{
    auto r = Save::getInstance()->getRole(role_id);
    auto m = Save::getInstance()->getMagic(magic_id);
    r->learnMagic(m);
    if (no_display) { return; }
    text_box_->setText(convert::formatString("%s習得武學%s", r->Name, m->Name));
    text_box_->run();
}

void Event::addIQ(int role_id, int value)
{
    auto r = Save::getInstance()->getRole(role_id);
    auto v0 = r->IQ;
    r->IQ = GameUtil::limit(v0 + value, 0, MAX_IQ);
    text_box_->setText(convert::formatString("%s資質增加%d", r->Name, r->IQ - v0));
    text_box_->run();
}

void Event::setRoleMagic(int role_id, int magic_index_role, int magic_id, int level)
{
    auto r = Save::getInstance()->getRole(role_id);
    r->MagicID[magic_index_role] = magic_id;
    r->MagicLevel[magic_index_role] = level;
}

bool Event::checkRoleSexual(int sexual)
{
    if (sexual <= 255)
    {
        return Save::getInstance()->getRole(0)->Sexual == sexual;
    }
    else
    {
        return x50[0x7000] == 0;
    }
}

void Event::addMorality(int value)
{
    auto role = Save::getInstance()->getRole(0);
    role->Morality = GameUtil::limit(role->Morality + value, 0, MAX_MORALITY);
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
    Save::getInstance()->getSubMapInfo(submap_id)->EntranceCondition = 0;
}

void Event::setTowards(int towards)
{
    subscene_->towards_ = (Towards)towards;
}

void Event::roleAddItem(int role_id, int item_id, int count)
{

}

bool Event::checkFemaleInTeam()
{
    for (auto r : Save::getInstance()->Team)
    {
        if (r >= 0)
        {
            if (Save::getInstance()->getRole(r)->Sexual == 1) { return true; }
        }
    }
    return false;
}

void Event::play2Amination(int event_index1, int begin_pic1, int end_pic1, int event_index2, int begin_pic2, int end_pic2)
{

}

void Event::addSpeed(int role_id, int value)
{
    auto r = Save::getInstance()->getRole(role_id);
    auto v0 = r->Speed;
    r->Speed = GameUtil::limit(v0 + value, 0, MAX_SPEED);
    text_box_->setText(convert::formatString("%s輕功增加%d", r->Name, r->Speed - v0));
    text_box_->run();
}

void Event::addMP(int role_id, int value)
{
    auto r = Save::getInstance()->getRole(role_id);
    auto v0 = r->MaxMP;
    r->MaxMP = GameUtil::limit(v0 + value, 0, MAX_MP);
    text_box_->setText(convert::formatString("%s內力增加%d", r->Name, r->MaxMP - v0));
    text_box_->run();
}

void Event::addAttack(int role_id, int value)
{
    auto r = Save::getInstance()->getRole(role_id);
    auto v0 = r->Attack;
    r->Attack = GameUtil::limit(v0 + value, 0, MAX_ATTACK);
    text_box_->setText(convert::formatString("%s武力增加%d", r->Name, r->Attack - v0));
    text_box_->run();
}

void Event::addHP(int role_id, int value)
{
    auto r = Save::getInstance()->getRole(role_id);
    auto v0 = r->MaxHP;
    r->MaxHP = GameUtil::limit(v0 + value, 0, MAX_HP);
    text_box_->setText(convert::formatString("%s生命增加%d", r->Name, r->MaxHP - v0));
    text_box_->run();
}

void Event::setMPType(int role_id, int value)
{
    Save::getInstance()->getRole(role_id)->MPType = value;
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
    text_box_->setText(convert::formatString("你的道德指數為%d", Save::getInstance()->getRole(0)->Morality));
    text_box_->run();
}

void Event::showFame()
{
    text_box_->setText(convert::formatString("你的聲望指數為%d", Save::getInstance()->getRole(0)->Fame));
    text_box_->run();
}

void Event::openAllSubMap()
{
    int i = 0;
    auto save = Save::getInstance();
    while (save->getSubMapInfo(i))
    {
        save->getSubMapInfo(i)->EntranceCondition = 0;
        i++;
    }
    save->getSubMapInfo(2)->EntranceCondition = 2;
    save->getSubMapInfo(38)->EntranceCondition = 2;
    save->getSubMapInfo(75)->EntranceCondition = 1;
    save->getSubMapInfo(80)->EntranceCondition = 1;
}

bool Event::checkEventID(int event_index, int value)
{
    return subscene_->getMapInfo()->Event(event_index)->Event1 = value;
}

void Event::addFame(int value)
{
    auto save = Save::getInstance();
    save->getRole(0)->Fame += value;
    if (save->getRole(0)->Fame > 200 && save->getRole(0)->Fame - value <= 200)
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
        Save::getInstance()->Team[i] = -1;
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
    Save::getInstance()->getRole(role_id)->Sexual = value;
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
    std::map<int, int> item_count;
    auto save = Save::getInstance();
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        if (save->Items[i].item_id >= 0)
        {
            item_count[save->Items[i].item_id] += save->Items[i].count;
        }
        save->Items[i].item_id = -1;
        save->Items[i].count = 0;
    }
    int k = 0;
    for (auto i : item_count)
    {
        save->Items[k].item_id = i.first;
        save->Items[k].count = i.second;
        k++;
    }
}

void Event::instruct_50e(int code, int e1, int e2, int e3, int e4, int e5, int e6, int* code_ptr)
{
    int nIndex = 0, nLen = 0, nOffset = 0, Result = 0;
    char* pChar = nullptr, *pChar1 = nullptr;
    char* pString = new char(1000);

    auto i1 = 0;
    auto i2 = 0;

    std::string str;

    switch (code)
    {
    case 0:
        //赋值
        x50[e1] = e2;
        break;
    case 1: //Give a value to one member in parameter group.
        nIndex = e3 + e_GetValue(0, e1, e4);
        //nIndex = CutRegion(nIndex);
        x50[nIndex] = e_GetValue(1, e1, e4);
        if (1 == e2)
        {
            x50[nIndex] = x50[nIndex] & 0xff;
        }
        break;
    case 2: //Get the value of one member in parameter group.
        nIndex = e3 + e_GetValue(0, e1, e4);
        //nIndex = CutRegion(nIndex);
        x50[e5] = x50[nIndex];
        if (1 == e2)
        {
            x50[nIndex] = x50[nIndex] & 0xff;
        }
        break;
    case 3:
        nIndex = e_GetValue(0, e1, e5);
        switch (e2)
        {
        case 0:
            x50[e3] = x50[e4] + nIndex;
            break;
        case 1:
            x50[e3] = x50[e4] - nIndex;
            break;
        case 2:
            x50[e3] = x50[e4] * nIndex;
            break;
        case 3:
            if (nIndex != 0)
            {
                x50[e3] = x50[e4] / nIndex;
            }
            break;
        case 4:
            if (nIndex != 0)
            {
                x50[e3] = x50[e4] % nIndex;
            }
            break;
        case 5:
            if (nIndex != 0)
            {
                x50[e3] = Uint16(x50[e4]) / nIndex;
            }
            break;
        }
        break;
    case 4:
        x50[0x7000] = 0;
        nIndex = e_GetValue(0, e1, e4);
        switch (e2)
        {
        case 0:
            if (!(x50[e3] < nIndex))
            {
                x50[0x7000] = 1;
            }
            break;
        case 1:
            if (!(x50[e3] <= nIndex))
            {
                x50[0x7000] = 1;
            }
            break;
        case 2:
            if (!(x50[e3] == nIndex))
            {
                x50[0x7000] = 1;
            }
            break;
        case 3:
            if (!(x50[e3] != nIndex))
            {
                x50[0x7000] = 1;
            }
            break;
        case 4:
            if (!(x50[e3] >= nIndex))
            {
                x50[0x7000] = 1;
            }
            break;
        case 5:
            if (!(x50[e3] > nIndex))
            {
                x50[0x7000] = 1;
            }
            break;
        case 6:
            x50[0x7000] = 0;
            break;
        case 7:
            x50[0x7000] = 1;
        }
        break;
    case 5: //Zero all parameters.
        memset(x50, 0, sizeof(x50));
        break;
    case 6:
        break;
    case 7:
        break;
    case 8: //Read talk to string.
        nIndex = e_GetValue(0, e1, e2);
        nLen = 0;
        if (0 == nIndex)
        {
            nOffset = 0;
            nLen = offset[0];
        }
        else
        {
            nOffset = offset[nIndex - 1];
            nLen = offset[nIndex] - nOffset;
        }
        memcpy(&offset[nOffset], &x50[e3], nLen);
        pChar = (char*)&x50[e3];
        for (int i = 0; i < nLen - 2; i++)
        {
            *pChar = char(*pChar ^ 0xff);
            pChar++;
        }
        *pChar = char(0);
        break;
    case 9: //Format the string.
        e4 = e_GetValue(0, e1, e4);
        pChar = (char*)&x50[e2];
        pChar1 = (char*)&x50[e3];
        snprintf(pString, 1000, "%s", e4); // 这里的整体代码写的都有些问题，主要是用了字符串数组之后，没有保护越界，或者获取字符串数组长度使用strlen 的时候必须要保证是\0 结尾
        for (int i = 0; i < strlen(pString); i++)
        {
            *pChar = pString[i + 1];
            pChar++;
        }
        break;
    case 10: //Get the length of a string.
        x50[e2] = strlen((char*)&x50[e1]);  // 感觉这样有问题把
        break;
        /*case 11: // Combine 2 strings.
            pChar = (char*)&x50[e1];
            pChar1 = (char*)&x50[e2];
            for (int i = 1; i < strlen(pChar1); i++)
            {
                *pChar = *(pChar1 + i);
                pChar++;
            }

            if (1 == strlen(pChar1) % 2)
            {
                *pChar = char(0x20);
                pChar++;
            }
            pChar1 = (char*)&x50[e3];
            for (int i = 1; i < strlen(pChar1); i++)
            {
                *pChar = *(pChar1 + i);
                pChar++;
            }

            break;
        case 12:  //Build a string with spaces.
            //Note: here the width of one 'space' is the same as one Chinese charactor.
            e3 = e_GetValue(0, e1, e3);
            pChar = (char*)&x50[e2];
            for (int i = 0; i < e3 / 2; i++)
            {
                *pChar = char(0x20);
                pChar++;
            }
            *pChar = char(0);
            break;
        case 13: break;
        case 14: break;
        case 15: break;
        case 16: //Write R data.
            e3 = e_GetValue(0, e1, e3);
            e4 = e_GetValue(1, e1, e4);
            e5 = e_GetValue(2, e1, e5);
            switch (e2) // 注释的都需要确认一下
            {
            case 0:
                //0 : Rrole[e3].Data[e4 div 2] : = e5;
                //1: Ritem[e3].Data[e4 div 2] : = e5;
                //2: Rscence[e3].Data[e4 div 2] : = e5;
                //3: Rmagic[e3].Data[e4 div 2] : = e5;
                //4: Rshop[e3].Data[e4 div 2] : = e5;
            }
            break;
        case 17:
            e3 = e_GetValue(0, e1, e3);
            e4 = e_GetValue(1, e1, e4);
            //case e2 of
            //  0: x50[e5] : = Rrole[e3].Data[e4 div 2];
            //  1: x50[e5] : = Ritem[e3].Data[e4 div 2];
            //  2: x50[e5] : = Rscence[e3].Data[e4 div 2];
            //  3: x50[e5] : = Rmagic[e3].Data[e4 div 2];
            //  4: x50[e5] : = Rshop[e3].Data[e4 div 2];
            //  end;
            break;
        case 18:
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            //TeamList[e2] := e3;
            break;
        case 19: //Read team data.
            e2 = e_GetValue(0, e1, e2);
            //x50[e3] : = TeamList[e2];
            break;
        case 20:  //Get the amount of one item.
            e2 = e_GetValue(0, e1, e2);
            x50[e3] = 0;
            //for i : = 0 to MAX_ITEM_AMOUNT - 1 do
            //  if RItemList[i].Number = e2 then
            //      begin
            //      x50[e3] : = RItemList[i].Amount;
            break;
        case 21: //Write event in scence.
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            e4 = e_GetValue(2, e1, e4);
            e5 = e_GetValue(3, e1, e5);
            //Ddata[e2, e3, e4] : = e5;
            break;
        case 22:
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            e4 = e_GetValue(2, e1, e4);
            //x50[e5] : = Ddata[e2, e3, e4];
            break;
        case 23:
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            e4 = e_GetValue(2, e1, e4);
            e5 = e_GetValue(3, e1, e5);
            e6 = e_GetValue(4, e1, e6);
            //Sdata[e2, e3, e5, e4] : = e6;
            break;
        case 24:
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            e4 = e_GetValue(2, e1, e4);
            e5 = e_GetValue(3, e1, e5);
            //x50[e6] : = Sdata[e2, e3, e5, e4];
            break;
        case 25:
            e5 = e_GetValue(0, e1, e5);
            e6 = e_GetValue(1, e1, e6);
            nIndex = Uint16(e3) + Uint16(e4) * 0x10000 + Uint16(e6);
            auto i = Uint16(e3) + Uint16(e4) * 0x10000;
            switch (nIndex)
            {
            case 0x1D295A:
                //sx = e5;
                break;
            case 0x1D295C:
                //Sy := e5;
                break;
            }
            switch (i)
            {
            case 0x18FE2C:
                if (e6 % 4 <= 1)
                {
                    //Ritemlist[e6 div 4].Number : = e5
                }
                else
                {
                    //Ritemlist[e6 div 4].Amount : = e5;
                }
                break;
            case 0x051C83:
                //puint16(@Acol[e6]) ^ : = e5;
                //puint16(@Acol1[e6]) ^ : = e5;
                //puint16(@Acol2[e6]) ^ : = e5;
                break;
            case 0x01D295E:
                //CurScence: = e5;
                break;
            }
            //SDL_UpdateRect2(screen, 0, 0, screen.w, screen.h); 应该是不要了
            break;
        case 26:
            e6 = e_GetValue(0, e1, e6);
            nIndex = nIndex = Uint16(e3) + Uint16(e4) * 0x10000 + Uint16(e6);
            auto i = Uint16(e3) + Uint16(e4) * 0x10000;
            switch (nIndex)
            {
            case 0x1D295E:
                //x50[e5] := CurScence;
                break;
            case 0x1D295A:
                //x50[e5] := Sx;
                break;
            case 0x1D295C:
                //x50[e5] := Sy;
                break;
            case 0x1C0B88:
                //x50[e5] := Mx;
                break;
            case 0x1C0B8C:
                //x50[e5] := My;
                break;
            case 0x05B53A:
                x50[e5] = 1;
                break;
            case 0x0544F2:
                //x50[e5] := Sface;
                break;
            case 0x1E6ED6:
                x50[e5] = x50[28100];
                break;
            case 0x556DA:
                //x50[e5] := Ax;
                break;
            case 0x556DC:
                //x50[e5] := Ay;
                break;
            case 0x1C0B90:
                x50[e5] = SDL_GetTicks() / 55 % 65536;
                break;
            }
            if ((nIndex - 0x18FE2C >= 0) && (nIndex - 0x18FE2C < 800))
            {
                i = nIndex - 0x18FE2C;
                if (i % 4 <= 1)
                {
                    //x50[e5] : = Ritemlist[i div 4].Number
                }
                else
                {
                    //x50[e5] : = Ritemlist[i div 4].Amount;
                }
                if ((nIndex >= 0x1E4A04) && (nIndex < 0x1E6A04))
                {
                    i = (nIndex - 0x1E4A04) / 2;
                    //x50[e5]  = Bfield[2, i mod 64, i div 64];
                }
            }

            break;
        case 27: //Read name to string.
            e3 = e_GetValue(0, e1, e3);
            pChar = (char*)&x50[e4];
            switch (e2)
            {
            case 0:
                //pChar1: = @Rrole[e3].Name;
                break;
            case 1:
                //pChar1: = @Ritem[e3].Name;
                break;
            case 2:
                //pChar1: = @Rscence[e3].Name;
                break;
            case 3:
                //pChar1: = @Rmagic[e3].Name;
                break;
            }

            int nlen = 10 < strlen(pChar1) ? 10 : strlen(pChar1);

            for (size_t i = 0; i < nlen - 1; i++)
            {
                *pChar = *(pChar1 + i);
                pChar++;
            }

            if (1 == nlen % 2)
            {
                *pChar = char(0x20);
                pChar++;
            }
            *pChar = char(0);


            break;
        case 28: //Get the battle number.
            x50[e1] = x50[28005];
            break;
        case 29: //Select aim.
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            if (0 == e5)
            {
                //selectAim(e2, e3);
            }
            //x50[e4] : = bfield[2, Ax, Ay];
            break;
        case 30: //Read battle properties.
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            //x50[e4]  = Brole[e2].Data[e3 div 2];
            break;
        case 31: //Write battle properties.
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            e4 = e_GetValue(2, e1, e4);
            //Brole[e2].Data[e3 div 2] : = e4;
            break;
        case 32: //Modify next instruct.
            e3 = e_GetValue(0, e1, e3);
            Result = 655360 * (e3 + 1) + x50[e2];
            break;
        case 33: //Draw a string.
            e3 = e_GetValue(0, e1, e3);
            e4 = e_GetValue(1, e1, e4);
            e5 = e_GetValue(2, e1, e5);
            int i = 0;
            nIndex = 0;

            pChar = (char*)&x50[e2];
            pChar1 = pChar;
            while (*pChar > 0)
            {
                if (0x2A == *pChar)
                {
                    *pChar = char(0);
                    //DrawBig5ShadowText(screen, p1, e3 - 2, e4 + 22 * i - 3, ColColor(e5 and $FF),ColColor((e5 and $FF00) shl 8));
                    i++;
                    pChar1++;
                }
                pChar++;
            }
            //DrawBig5ShadowText(screen, p1, e3 - 2, e4 + 22 * i - 3, ColColor(e5 and $FF), ColColor((e5 and $FF00) shl 8));
            //SDL_UpdateRect2(screen, 0, 0, screen.w, screen.h);
            break;
        case 34: //Draw a rectangle as background.
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            e4 = e_GetValue(2, e1, e4);
            e5 = e_GetValue(3, e1, e5);
            //DrawRectangle(screen, e2, e3, e4, e5, 0, ColColor($FF), 50);
            //SDL_UpdateRect2(screen,e1,e2,e3+1,e4+1);
            break;
        case 35: // Pause and wait a key.
            //auto i = WaitAnyKey;
            x50[e1] = i;
            switch (i)
            {
            case SDLK_LEFT:
                x50[e1] = 154;
                break;
            case SDLK_RIGHT:
                x50[e1] = 156;
                break;
            case SDLK_UP:
                x50[e1] = 158;
                break;
            case SDLK_DOWN:
                x50[e1] = 152;
                break;
            }
            break;
        case 36: //Draw a string with background then pause, if the key pressed is 'Y' then jump=0.
            e3 = e_GetValue(0, e1, e3);
            e4 = e_GetValue(1, e1, e4);
            e5 = e_GetValue(2, e1, e5);
            pChar = (char*)&x50[e2];
            nIndex = 0;
            e3 = abs(e3);

            while (*pChar > 0)
            {
                if (*pChar == 0x2A)
                {
                    if (nIndex > i2)
                    {
                        i2 = nIndex;
                    }
                    nIndex = 0;
                    i1++;
                }
                if (*pChar == 0x20)
                {
                    nIndex++;
                }
                pChar++;
                nIndex++;
            }

            if (nIndex > i2)
            {
                i2 = nIndex;
            }
            pChar--;
            if (i1 == 0)
            {
                i1 = 1;
            }
            if (*pChar == 0x2A)
            {
                i1--;
            }
            //DrawRectangle(screen, e3, e4, i2 * 10 + 25, i1 * 22 + 5, 0, ColColor(255), 50);
            pChar = (char*)&x50[e2];
            pChar1 = pChar;
            i = 0;

            while (*pChar > 0)
            {
                if (*pChar == 0x2A)
                {
                    *pChar = char(0);
                    //DrawBig5ShadowText(screen, p1, e3 + 3, e4 + 22 * i + 2, ColColor(e5 and $FF),ColColor((e5 and $FF00) shl 8));
                    i++;
                    pChar1 = pChar + 1;
                }
                pChar++;
            }

            //DrawBig5ShadowText(screen, p1, e3 + 3, e4 + 22 * i + 2, ColColor(e5 and $FF), ColColor((e5 and $FF00) shl 8));
            //SDL_UpdateRect2(screen, 0, 0, screen.w, screen.h);

            //i = WaitAnyKey;
            if (i == SDLK_y)
            {
                x50[0x7000] = 0;
            }
            else
            {
                x50[0x7000] = 1;
            }


            break;
        case 37: //Delay.
            e2 = e_GetValue(0, e1, e2);
            SDL_Delay(e2);
            break;
        case 38: //Get a number randomly.
            e2 = e_GetValue(0, e1, e2);
            //default_random_engine e;
            //uniform_int_distribution<unsigned> u(0, e2);
            //x50[e3] = u(e);
            break;
        case 39: //Show a menu to select.
            e2 = e_GetValue(0, e1, e2);
            e5 = e_GetValue(1, e1, e5);
            e6 = e_GetValue(2, e1, e6);
            //setlength(menuString, e2);
            //setlength(menuEngString, 0);
            nIndex = 0;
            for (int i = 0; i < e2 - 1; i++)
            {
                //menuString[i] : = Big5ToUnicode(@x50[x50[e3 + i]]);
                auto i1 = strlen((char*)&x50[x50[e3 + i]]);
                if (i1 > nIndex)
                {
                    nIndex = i1;
                }
                //x50[e4] := CommonMenu(e5, e6, t1 * 10 + 5, e2 - 1, menuString) + 1;
            }
            break;
        case 40: // Show a scroll menu to select.
            e2 = e_GetValue(0, e1, e2);
            e5 = e_GetValue(1, e1, e5);
            e6 = e_GetValue(2, e1, e6);
            //setlength(menuString, e2);
            //setlength(menuEngString, 0);
            auto i2 = 0;
            for (int i = 0; i < e2 - 1; i++)
            {
                //menuString[i] : = Big5ToUnicode(@x50[x50[e3 + i]]);
                auto i1 = strlen((char*)&x50[x50[e3 + i]]);
                if (i1 > i2)
                {
                    i2 = i1;
                }
            }
            nIndex = (e1 << 8) & 0xff;
            if (nIndex == 0)
            {
                nIndex = 5;
            }

            //  x50[e4] : = CommonScrollMenu(e5, e6, i2 * 10 + 5, e2 - 1, t1, menuString) + 1;
            break;
        case 41: //Draw a picture.
            e3 = e_GetValue(0, e1, e3);
            e4 = e_GetValue(1, e1, e4);
            e5 = e_GetValue(2, e1, e5);
            switch (e2)
            {
            case 0:
                //if (where < > 1) or ((ModVersion = 22) and (CurEvent = -1)) then
                //  DrawMPic(e5 div 2, e3, e4)
                //else
                //  DrawSPic(e5 div 2, e3, e4, 0, 0, screen.w, screen.h);
                break;
            case 1:
                //DrawHeadPic(e5, e3, e4);
                break;
            case 2:
                //str: = AppPath + 'pic/' + IntToStr(e5) + '.png';
                //display_img(@str[1], e3, e4);
                break;
            }
            //SDL_UpdateRect2(screen, 0, 0, screen.w, screen.h);
            break;
        case 42: //Change the poistion on world map.
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(0, e1, e3);
            //Mx: = e3;
            //My: = e2;
            break;
        case 43: //Call another event.
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            e4 = e_GetValue(2, e1, e4);
            e5 = e_GetValue(3, e1, e5);
            e6 = e_GetValue(4, e1, e6);
            x50[0x7100] = e3;
            x50[0x77101] = e4;
            x50[0x77102] = e5;
            x50[0x77103] = e6;
            if (e2 == 202) // 得到物品
            {
                if (e5 == 0)
                {
                    instruct_2(e3, e4);
                }
                else
                {
                    instruct_32(e3, e4);
                }
            }
            else if (e2 == 201) //新对话
            {
                //NewTalk(e3, e4, e5, e6 mod 100, (e6 mod 100) div 10, e6 div 100, 0)
            }
            else if (e2 == 999)// && MODVersion == 62)
            {
                //CurScence: = e3;
                //Sx: = e5;
                //Sy: = e4;
                //Cx: = Sx;
                //Cy: = Sy;
                //instruct_14;
                //InitialScence;
                //DrawScence;
                //ShowScenceName(CurScence);
                //CheckEvent3;
            }
            else if (e2 == 176)// && MODVersion == 22) // 菠萝三国输入数字
            {
                //x50[10032]  = EnterNumber(0, 32767, CENTER_X, CENTER_Y - 100);
                x50[0x7000] = 0;
                //Redraw;
            }
            else
            {
                callEvent(e2);
            }

            break;
        case 44: // Play amination.
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            e4 = e_GetValue(2, e1, e4);
            //PlayActionAmination(e2, e3);
            //PlayMagicAmination(e2, e4);
            break;
        case 45: //Show values.
            e2 = e_GetValue(0, e1, e2);
            //ShowHurtValue(e2);
            break;
        case 46: //Set effect layer.
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            e4 = e_GetValue(2, e1, e4);
            e5 = e_GetValue(3, e1, e5);
            e6 = e_GetValue(4, e1, e6);
            for (int i1 = e2; i1 < e2 + e4 - 1; i1++)
            {
                for (int i2 = e3; i1 < e3 + e5 - 1; i2++)
                {
                    //bfield[4, i1, i2] : = e6;
                }
            }
            break;
        case 47: //Here no need to re-set the pic.
            break;
        case 48: //Show some parameters.
            str = "";
            char c[100], s[100];

            for (int i = e1; i < e1 + e2 - 1; i++)
            {
                sprintf(c, "%d", i);
                sprintf(s, "%d", x50[i]);
                str = str + "x" + c + "=" + s + char(13) + char(10);
                //if FULLSCREEN = 0 then
                //  messagebox(0, @str[1], 'KYS Windows', MB_OK);
            }
            break;
        case 49: //In PE files, you can't call any procedure as your wish.
            break;
        case 50: //Enter name for items, magics and roles.
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            e4 = e_GetValue(2, e1, e4);
            e5 = e_GetValue(3, e1, e5);
            switch (e2)
            {
                //0: p: = @Rrole[e3].Name[0];
                //1: p: = @Ritem[e3].Name[0];
                //2: p: = @Rmagic[e3].Name[0];
                //3: p: = @Rscence[e3].Name[0];
            }
            str = "請輸入名字：";
            pChar1 = (char*)&str[1];
            auto nlen = e5 < strlen(pChar1) ? e5 : strlen(pChar1);
            for (int i = 0; i < nlen - 1; i++)
            {
                *(pChar + i) = *(pChar1 + i);
            }

            break;
        case 51: break;
        case 52: //Judge someone grasp some magic.
            e2 = e_GetValue(0, e1, e2);
            e3 = e_GetValue(1, e1, e3);
            e4 = e_GetValue(2, e1, e4);
            x50[0x7000] = 1;

            //if (HaveMagic(e2, e3, e4) = True) then
            //  x50[$7000] : = 0;
            break;
        case 53: break;
        case 54: break;
        case 55: break;
        case 56: break;
        case 57: break;
        case 58: break;
        case 59: break;
        case 60: break;

        default:
            break;*/
    }
}

