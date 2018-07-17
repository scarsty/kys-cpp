#include "Event.h"
#include "MainScene.h"
#include "SubScene.h"
#include "Menu.h"
#include "Save.h"
#include "PotConv.h"
#include "Talk.h"
#include "libconvert.h"
#include "Audio.h"
#include "GameUtil.h"
#include "BattleScene.h"
#include "UIShop.h"
#include "Font.h"
#include "File.h"
#include "Script.h"
#include "GrpIdxFile.h"

Event::Event()
{
    loadEventData();
    talk_box_ = new Element();
    talk_box_up_ = new Talk();
    talk_box_down_ = new Talk();
    talk_box_->addChild(talk_box_up_);
    talk_box_->addChild(talk_box_down_, 0, 400);
    menu2_ = new MenuText({ "�_�J��Y��", "ȡ����N��" });
    menu2_->setPosition(400, 300);
    menu2_->setFontSize(24);
    menu2_->setHaveBox(true);
    menu2_->arrange(0, 50, 150, 0);
    text_box_ = new TextBox();
    text_box_->setPosition(400, 200);
    text_box_->setTextPosition(-20, 100);
}

Event::~Event()
{
}

bool Event::loadEventData()
{
    //��ȡtalk
    auto talk = GrpIdxFile::getIdxContent("../game/resource/talk.idx", "../game/resource/talk.grp", &offset, &length);
    for (int i = 0; i < offset.back(); i++)
    {
        if (talk[i])
        {
            talk[i] = talk[i] ^ 0xff;
        }
    }
    for (int i = 0; i < length.size(); i++)
    {
        std::string str = PotConv::cp950tocp936(talk.data() + offset[i]);
        convert::replaceAllString(str, "*", "");
        talk_contents_.push_back(str);
    }

    //��ȡ�¼���ȫ��תΪ����
    auto kdef = GrpIdxFile::getIdxContent("../game/resource/kdef.idx", "../game/resource/kdef.grp", &offset, &length);
    kdef_.resize(length.size());
    for (int i = 0; i < length.size(); i++)
    {
        kdef_[i].resize(length[i] / sizeof(int16_t), -1);
        for (int k = 0; k < length[i] / sizeof(int16_t); k++)
        {
            kdef_[i][k] = *(int16_t*)(kdef.data() + offset[i] + k * 2);
        }
    }

    //��ȡ����б�
    std::string leave_txt = convert::readStringFromFile("../game/list/leave.txt");
    convert::findNumbers(leave_txt, &leave_event_id_);
    if (leave_event_id_.size() > 0)
    {
        leave_event_0_ = leave_event_id_[0];
        leave_event_id_.erase(leave_event_id_.begin());
    }
    return false;
}

//����ֵΪ�Ƿ�ɹ�ִ���¼�
bool Event::callEvent(int event_id, Element* subscene, int supmap_id, int item_id, int event_index, int x, int y)
{
    bool ret = true;
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

    //���ڵ���ص���ͼջ�����ϣ����������Ի����Ի�����
    talk_box_->setExit(false);
    talk_box_->setVisible(true);
    Element::addOnRootTop(talk_box_);
    int p = 0;
    loop_ = true;
    int i = 0;
    auto e = kdef_[event_id];

    printf("Event %d: ", event_id);
    for (auto c : e)
    {
        printf("%d ", c);
    }
    printf("\n");
    e.resize(e.size() + 20, -1);  //������ǻ��������������

    //��Щ���Ϊ�����¼������м򻯴��룬��Ҫ���������ط�
#define PRINT_E(n) do { for (int __i=1;__i<=n;__i++) { printf("%d, ", e[i+__i]); } printf("\b\b  \n"); } while (0)
#define VOID_0(function) { PRINT_E(0); function(); i+=1; }
#define VOID_1(function) { PRINT_E(1); function(e[i+1]); i+=2; }
#define VOID_2(function) { PRINT_E(2); function(e[i+1],e[i+2]); i+=3; }
#define VOID_3(function) { PRINT_E(3); function(e[i+1],e[i+2],e[i+3]); i+=4; }
#define VOID_4(function) { PRINT_E(4); function(e[i+1],e[i+2],e[i+3],e[i+4]); i+=5; }
#define VOID_5(function) { PRINT_E(5); function(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5]); i+=6; }
#define VOID_6(function) { PRINT_E(6); function(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6]); i+=7; }
#define VOID_7(function) { PRINT_E(7); function(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7]); i+=8; }
#define VOID_8(function) { PRINT_E(8); function(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7],e[i+8]); i+=9; }
#define VOID_9(function) { PRINT_E(9); function(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7],e[i+8],e[i+9]); i+=10; }
#define VOID_10(function) { PRINT_E(10); function(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7],e[i+8],e[i+9],e[i+10]); i+=11; }
#define VOID_11(function) { PRINT_E(11); function((e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7],e[i+8],e[i+9],e[i+10],e[i+11]); i+=12; }
#define VOID_12(function) { PRINT_E(12); function(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7],e[i+8],e[i+9],e[i+10],e[i+11],e[i+12]); i+=13; }
#define VOID_13(function) { PRINT_E(13); function(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7],e[i+8],e[i+9],e[i+10],e[i+11],e[i+12],e[i+13]); i+=14; }

#define BOOL_0(function) { PRINT_E(0); if (function()) {i += e[i+1];} else { i+= e[i+2]; } i += 3; }
#define BOOL_1(function) { PRINT_E(1); if (function(e[i+1])) {i += e[i+2];} else { i+= e[i+3]; } i += 4; }
#define BOOL_2(function) { PRINT_E(2); if (function(e[i+1],e[i+2])) {i += e[i+3];} else { i+= e[i+4]; } i += 5; }
#define BOOL_2_2(function) { PRINT_E(2); if (function(e[i+1],e[i+4])) {i += e[i+2];} else { i+= e[i+3]; } i += 5; }
#define BOOL_3(function) { PRINT_E(3); if (function(e[i+1],e[i+2],e[i+3])) {i += e[i+4];} else { i+= e[i+5]; } i += 6; }
#define BOOL_4(function) { PRINT_E(4); if (function(e[i+1],e[i+2],e[i+3],e[i+4]))) {i += e[i+5];} else { i+= e[i+6]; } i += 7; }
#define BOOL_5(function) { PRINT_E(5); if (function(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5])) {i += e[i+6];} else { i+= e[i+7]; } i += 8; }

#define RUN_INSTRUCT(function, INSTRUCT_TYPE) do { printf("%s: ", #function); INSTRUCT_TYPE(function); } while(0)
#define REGISTER_INSTRUCT(code, function, INSTRUCT_TYPE) { case (code): RUN_INSTRUCT(function, INSTRUCT_TYPE); break; }

    if (use_script_)
    {
        auto script = convert::formatString("../game/script/oldevent/oldevent_%d.lua", event_id);
        ret = Script::getInstance()->runScript(script) == 0;
    }
    else
    {
        while (i < e.size() && loop_)
        {
            //printf("instruct %d\n", e[i]);
            switch (e[i])
            {
                REGISTER_INSTRUCT(1, oldTalk, VOID_3);
                REGISTER_INSTRUCT(2, addItem, VOID_2);
                REGISTER_INSTRUCT(3, modifyEvent, VOID_13);
                REGISTER_INSTRUCT(4, isUsingItem, BOOL_1);
                REGISTER_INSTRUCT(5, askBattle, BOOL_0);
                REGISTER_INSTRUCT(6, tryBattle, BOOL_2_2);    //6���˳��ͬ
                REGISTER_INSTRUCT(8, changeMainMapMusic, VOID_1);
                REGISTER_INSTRUCT(9, askJoin, BOOL_0);

                REGISTER_INSTRUCT(10, join, VOID_1);
                REGISTER_INSTRUCT(11, askRest, BOOL_0);
                REGISTER_INSTRUCT(12, rest, VOID_0);
                REGISTER_INSTRUCT(13, lightScence, VOID_0);
                REGISTER_INSTRUCT(14, darkScence, VOID_0);
                REGISTER_INSTRUCT(15, dead, VOID_0);
                REGISTER_INSTRUCT(16, inTeam, BOOL_1);
                REGISTER_INSTRUCT(17, setSubMapLayerData, VOID_5);
                REGISTER_INSTRUCT(18, haveItemBool, BOOL_1);
                REGISTER_INSTRUCT(19, oldSetScencePosition, VOID_2);

                REGISTER_INSTRUCT(20, teamIsFull, BOOL_0);
                REGISTER_INSTRUCT(21, leaveTeam, VOID_1);
                REGISTER_INSTRUCT(22, zeroAllMP, VOID_0);
                REGISTER_INSTRUCT(23, setRoleUsePoison, VOID_2);
                REGISTER_INSTRUCT(25, subMapViewFromTo, VOID_4);
                REGISTER_INSTRUCT(26, add3EventNum, VOID_5);
                REGISTER_INSTRUCT(27, playAnimation, VOID_3);
                REGISTER_INSTRUCT(28, checkRoleMorality, BOOL_3);
                REGISTER_INSTRUCT(29, checkRoleAttack, BOOL_3);

                REGISTER_INSTRUCT(30, walkFromTo, VOID_4);
                REGISTER_INSTRUCT(31, checkEnoughMoney, BOOL_1);
                REGISTER_INSTRUCT(32, addItemWithoutHint, VOID_2);
                REGISTER_INSTRUCT(33, oldLearnMagic, VOID_3);
                REGISTER_INSTRUCT(34, addIQ, VOID_2);
                REGISTER_INSTRUCT(35, setRoleMagic, VOID_4);
                REGISTER_INSTRUCT(36, checkRoleSexual, BOOL_1);
                REGISTER_INSTRUCT(37, addMorality, VOID_1);
                REGISTER_INSTRUCT(38, changeSubMapPic, VOID_4);
                REGISTER_INSTRUCT(39, openSubMap, VOID_1);

                REGISTER_INSTRUCT(40, setTowards, VOID_1);
                REGISTER_INSTRUCT(41, roleAddItem, VOID_3);
                REGISTER_INSTRUCT(42, checkFemaleInTeam, BOOL_0);
                REGISTER_INSTRUCT(43, haveItemBool, BOOL_1);
                REGISTER_INSTRUCT(44, play2Amination, VOID_6);
                REGISTER_INSTRUCT(45, addSpeed, VOID_2);
                REGISTER_INSTRUCT(46, addMaxMP, VOID_2);
                REGISTER_INSTRUCT(47, addAttack, VOID_2);
                REGISTER_INSTRUCT(48, addMaxHP, VOID_2);
                REGISTER_INSTRUCT(49, setMPType, VOID_2);

                REGISTER_INSTRUCT(51, askSoftStar, VOID_0);
                REGISTER_INSTRUCT(52, showMorality, VOID_0);
                REGISTER_INSTRUCT(53, showFame, VOID_0);
                REGISTER_INSTRUCT(54, openAllSubMap, VOID_0);
                REGISTER_INSTRUCT(55, checkEventID, BOOL_2);
                REGISTER_INSTRUCT(56, addFame, VOID_1);
                REGISTER_INSTRUCT(57, breakStoneGate, VOID_0);
                REGISTER_INSTRUCT(58, fightForTop, VOID_0);
                REGISTER_INSTRUCT(59, allLeave, VOID_0);

                REGISTER_INSTRUCT(60, checkSubMapPic, BOOL_3);
                REGISTER_INSTRUCT(61, check14BooksPlaced, BOOL_0);
                REGISTER_INSTRUCT(62, backHome, VOID_6);
                REGISTER_INSTRUCT(63, setSexual, VOID_2);
                REGISTER_INSTRUCT(64, shop, VOID_0);
                REGISTER_INSTRUCT(66, playMusic, VOID_1);
                REGISTER_INSTRUCT(67, playWave, VOID_1);

            case 50:
                if (e[i + 1] > 128)
                {
                    RUN_INSTRUCT(checkHave5Item, BOOL_5);
                }
                else
                {
                    instruct_50e(e[i + 1], e[i + 2], e[i + 3], e[i + 4], e[i + 5], e[i + 6], e[i + 7], &e[i + 8]);
                    i += 8;
                }
                break;

            case 7:
            case -1:
                i += 1;
                loop_ = false;
                break;
            default:
                //�����ڵ�ָ��ƶ�һ��
                i += 1;
            }
        }
    }
    Element::removeFromRoot(talk_box_);
    clearTalkBox();
    if (subscene_)
    {
        subscene_->forceManPic(-1);
    }
    return ret;
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

//ԭ�Ի�ָ��
void Event::oldTalk(int talk_id, int head_id, int style)
{
    if (talk_id < 0 || talk_id >= talk_contents_.size())
    {
        return;
    }
    Talk* talk;
    if (style % 2 == 0)
    {
        talk = talk_box_up_;
    }
    else
    {
        talk = talk_box_down_;
    }
    talk->setContent(talk_contents_[talk_id]);
    printf("%s\n", PotConv::to_read(talk_contents_[talk_id]).c_str());
    talk->setHeadID(head_id);
    if (style == 2 || style == 3)
    {
        talk->setHeadID(-1);
    }
    if (style == 0 || style == 5)
    {
        talk->setHeadStyle(0);
    }
    if (style == 4 || style == 1)
    {
        talk->setHeadStyle(1);
    }
    talk->run(false);
}

//�����Ʒ������ʾ
void Event::addItem(int item_id, int count)
{
    addItemWithoutHint(item_id, count);
    text_box_->setText(convert::formatString("���%s%d", Save::getInstance()->getItem(item_id)->Name, count));
    text_box_->setTexture("item", item_id);
    text_box_->run();
    text_box_->setTexture("item", -1);
}

//�޸��¼�����
void Event::modifyEvent(int submap_id, int event_index, int cannotWalk, int index, int event1, int event2, int event3, int currentPic, int endPic, int beginPic, int picDelay, int x, int y)
{
    if (submap_id < 0) { submap_id = submap_id_; }
    if (submap_id < 0) { return; }
    if (event_index < 0) { event_index = event_index_; }
    auto e = Save::getInstance()->getSubMapInfo(submap_id)->Event(event_index);
    //�����ֵΪ-2��ʾ��Ҫ�޸�
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

//�Ƿ�ʹ����ĳ��Ʒ
bool Event::isUsingItem(int item_id)
{
    return item_id_ == item_id;
}

//ѯ��ս��
bool Event::askBattle()
{
    menu2_->setText("�Ƿ��c֮�^�У�");
    return menu2_->run() == 0;
}

bool Event::tryBattle(int battle_id, int get_exp)
{
    BattleScene battle(battle_id);
    battle.setHaveFailExp(get_exp);
    int result = battle.run();
    //int result = 0;    //������
    clearTalkBox();

    return result == 0;
}

void Event::changeMainMapMusic(int music_id)
{
    if (subscene_)
    {
        subscene_->changeExitMusic(music_id);
    }
}

bool Event::askJoin()
{
    menu2_->setText("�Ƿ�Ҫ����룿");
    return menu2_->run() == 0;
}

//��ɫ���룬ͬʱ��öԷ����ϵ���Ʒ
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
    menu2_->setText("Ո�x���ǻ��");
    return menu2_->run() == 0;
}

void Event::rest()
{
    for (auto r : Save::getInstance()->Team)
    {
        auto role = Save::getInstance()->getRole(r);
        if (role)
        {
            role->PhysicalPower = Role::getMaxValue()->PhysicalPower;
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
    clearTalkBox();
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
    Element::exitAll(1);
    forceExit();
}

//ĳ���Ƿ��ڶ���
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
        subscene_->setManViewPosition(x, y);
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
            save->Team[TEAMMATE_COUNT - 1] = -1;
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
    if (subscene_ == nullptr) { return; }
    int incx = GameUtil::sign(x1 - x0);
    int incy = GameUtil::sign(y1 - y0);
    if (incx)
    {
        for (int i = x0; i != x1; i += incx)
        {
            subscene_->setViewPosition(i, y0);
            subscene_->drawAndPresent();
        }
    }
    if (incy)
    {
        for (int i = y0; i != y1; i += incy)
        {
            subscene_->setViewPosition(x1, i);
            subscene_->drawAndPresent();
        }
    }
    subscene_->setViewPosition(x1, y1);
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

void Event::playAnimation(int event_index, int begin_pic, int end_pic)
{
    if (subscene_ == nullptr) { return; }
    if (event_index == -1)
    {
        int inc = GameUtil::sign(end_pic - begin_pic);
        for (int i = begin_pic / 2; i != end_pic / 2; i += inc)
        {
            subscene_->forceManPic(i);
            subscene_->drawAndPresent();
        }
        subscene_->forceManPic(end_pic / 2);
        subscene_->drawAndPresent();
        //subscene_->forceManPic(-1);
    }
    else
    {
        auto e = subscene_->getMapInfo()->Event(event_index);
        if (e)
        {
            int inc = GameUtil::sign(end_pic - begin_pic);
            for (int i = begin_pic; i != end_pic; i += inc)
            {
                e->setPic(i);
                subscene_->drawAndPresent();
            }
            e->setPic(end_pic);
        }
    }
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
    if (subscene_ == nullptr) { return; }

    int incx = GameUtil::sign(x1 - x0);
    int incy = GameUtil::sign(y1 - y0);
    if (incx)
    {
        for (int i = x0; i != x1; i += incx)
        {
            subscene_->tryWalk(i, y0);
            subscene_->setTowards(subscene_->calTowards(x0, y0, i, y0));
            subscene_->drawAndPresent();
        }
    }
    if (incy)
    {
        for (int i = y0; i != y1; i += incy)
        {
            subscene_->tryWalk(x1, i);
            subscene_->setTowards(subscene_->calTowards(x1, y0, x1, i));
            subscene_->drawAndPresent();
        }
    }
    subscene_->setManViewPosition(x1, y1);
}

bool Event::checkEnoughMoney(int money_count)
{
    return (Save::getInstance()->getMoneyCountInBag() >= money_count);
}

void Event::addItemWithoutHint(int item_id, int count)
{
    if (item_id < 0 || count == 0) { return; }
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
        if (pos >= 0)
        {
            save->Items[pos].item_id = item_id;
            save->Items[pos].count = count;
        }
    }
    //����Ʒ����Ϊ������Ҫ������
    if (count < 0)
    {
        arrangeBag();
    }
}

void Event::oldLearnMagic(int role_id, int magic_id, int no_display)
{
    auto r = Save::getInstance()->getRole(role_id);
    auto m = Save::getInstance()->getMagic(magic_id);
    r->learnMagic(m);
    if (no_display) { return; }
    text_box_->setText(convert::formatString("%s������W%s", r->Name, m->Name));
    text_box_->run();
}

void Event::addIQ(int role_id, int value)
{
    auto r = Save::getInstance()->getRole(role_id);
    auto v0 = r->IQ;
    r->IQ = GameUtil::limit(v0 + value, 0, Role::getMaxValue()->IQ);
    text_box_->setText(convert::formatString("%s�Y�|����%d", r->Name, r->IQ - v0));
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
    role->Morality = GameUtil::limit(role->Morality + value, 0, Role::getMaxValue()->Morality);
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
    if (item_id < 0 || count == 0) { return; }
    auto role = Save::getInstance()->getRole(role_id);
    int pos = -1;
    if (role == nullptr) { return; }

    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        if (role->TakingItem[i] == item_id)
        {
            pos = i;
            break;
        }
    }
    if (pos >= 0)
    {
        role->TakingItemCount[pos] += count;
    }
    else
    {
        for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
        {
            if (role->TakingItem[i] < 0)
            {
                pos = i;
                break;
            }
        }
        if (pos >= 0)
        {
            role->TakingItem[pos] = item_id;
            role->TakingItemCount[pos] = count;
        }
    }

    //�����ɫ����Ʒ��ע�⣺ʵ���ϲ�û�б�Ҫÿ�ζ�����
    std::map<int, int> item_count;
    for (int i = 0; i < ROLE_TAKING_ITEM_COUNT; i++)
    {
        if (role->TakingItem[i] >= 0 && role->TakingItemCount[i] > 0)
        {
            item_count[role->TakingItem[i]] += role->TakingItemCount[i];
        }
        role->TakingItem[i] = -1;
        role->TakingItemCount[i] = 0;
    }
    int k = 0;
    for (auto i : item_count)
    {
        role->TakingItem[k] = i.first;
        role->TakingItemCount[k] = i.second;
        k++;
    }
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
    auto e1 = subscene_->getMapInfo()->Event(event_index1);
    auto e2 = subscene_->getMapInfo()->Event(event_index2);
    if (e1 && e2)
    {
        int inc1 = GameUtil::sign(end_pic1 - begin_pic1);
        for (int i = 0; i != end_pic1 - begin_pic1; i += inc1)
        {
            e1->setPic(begin_pic1 + i);
            e2->setPic(begin_pic2 + i);
            subscene_->drawAndPresent();
        }
        e1->setPic(end_pic1);
        e2->setPic(begin_pic2 + end_pic1 - begin_pic1);
    }
}

void Event::play3Amination(int event_index1, int begin_pic1, int end_pic1, int event_index2, int begin_pic2, int event_index3, int begin_pic3)
{
    auto e1 = subscene_->getMapInfo()->Event(event_index1);
    auto e2 = subscene_->getMapInfo()->Event(event_index2);
    auto e3 = subscene_->getMapInfo()->Event(event_index2);
    if (e1 && e2 && e3)
    {
        int inc1 = GameUtil::sign(end_pic1 - begin_pic1);
        for (int i = 0; i != end_pic1 - begin_pic1; i += inc1)
        {
            e1->setPic(begin_pic1 + i);
            e2->setPic(begin_pic2 + i);
            e3->setPic(begin_pic3 + i);
            subscene_->drawAndPresent();
        }
        e1->setPic(end_pic1);
        e2->setPic(begin_pic2 + end_pic1 - begin_pic1);
        e3->setPic(begin_pic3 + end_pic1 - begin_pic1);
    }
}

void Event::addSpeed(int role_id, int value)
{
    auto r = Save::getInstance()->getRole(role_id);
    auto v0 = r->Speed;
    r->Speed = GameUtil::limit(v0 + value, 0, Role::getMaxValue()->Speed);
    text_box_->setText(convert::formatString("%s�p������%d", r->Name, r->Speed - v0));
    text_box_->run();
}

void Event::addMaxMP(int role_id, int value)
{
    auto r = Save::getInstance()->getRole(role_id);
    auto v0 = r->MaxMP;
    r->MaxMP = GameUtil::limit(v0 + value, 0, Role::getMaxValue()->MP);
    text_box_->setText(convert::formatString("%s��������%d", r->Name, r->MaxMP - v0));
    text_box_->run();
}

void Event::addAttack(int role_id, int value)
{
    auto r = Save::getInstance()->getRole(role_id);
    auto v0 = r->Attack;
    r->Attack = GameUtil::limit(v0 + value, 0, Role::getMaxValue()->Attack);
    text_box_->setText(convert::formatString("%s��������%d", r->Name, r->Attack - v0));
    text_box_->run();
}

void Event::addMaxHP(int role_id, int value)
{
    auto r = Save::getInstance()->getRole(role_id);
    auto v0 = r->MaxHP;
    r->MaxHP = GameUtil::limit(v0 + value, 0, Role::getMaxValue()->HP);
    text_box_->setText(convert::formatString("%s��������%d", r->Name, r->MaxHP - v0));
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
    oldTalk(2547 + rand_.rand_int(18), 114, 0);
}

void Event::showMorality()
{
    text_box_->setText(convert::formatString("��ĵ���ָ����%d", Save::getInstance()->getRole(0)->Morality));
    text_box_->run();
}

void Event::showFame()
{
    text_box_->setText(convert::formatString("�����ָ����%d", Save::getInstance()->getRole(0)->Fame));
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
    return subscene_->getMapInfo()->Event(event_index)->Event1 == value;
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
    playAnimation(-1, 3832 * 2, 3844 * 2);
    play3Amination(2, 3845 * 2, 3873 * 2, 3, 3874 * 2, 4, 3903 * 2);
}

//���ִ��
void Event::fightForTop()
{
    std::vector<int> heads =
    {
        8, 21, 23, 31, 32, 43, 7, 11, 14, 20, 33, 34, 10, 12, 19,
        22, 56, 68, 13, 55, 62, 67, 70, 71, 26, 57, 60, 64, 3, 69
    };

    for (int i = 0; i < 15; i++)
    {
        int p = rand_.rand_int(2);
        oldTalk(2854 + i * 2 + p, heads[i * 2 + p], rand_.rand_int(2) * 4 + rand_.rand_int(2));
        if (!tryBattle(102 + i * 2 + p, 0))
        {
            dead();
            return;
        }
        darkScence();
        lightScence();
        if (i % 3 == 2)
        {
            oldTalk(2891, 70, 4);
            rest();
            darkScence();
            lightScence();
        }
    }

    oldTalk(2884, 0, 3);
    oldTalk(2885, 0, 3);
    oldTalk(2886, 0, 3);
    oldTalk(2887, 0, 3);
    oldTalk(2888, 0, 3);
    oldTalk(2889, 0, 1);
    addItem(0x8F, 1);
}

//���������
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

void Event::backHome(int event_index1, int begin_pic1, int end_pic1, int event_index2, int begin_pic2, int end_pic2)
{
    subscene_->forceManPic(-2);
    play2Amination(event_index1, begin_pic1, end_pic1, event_index2, begin_pic2, end_pic2);
    Element::exitAll(1);
    forceExit();
}

void Event::setSexual(int role_id, int value)
{
    Save::getInstance()->getRole(role_id)->Sexual = value;
}

void Event::shop()
{
    oldTalk(0xB9E, 0x6F, 0);
    auto shop = new UIShop();
    shop->setShopID(rand_.rand_int(5));
    int result = shop->run();
    if (result < 0)
    {
    }
    else
    {
        oldTalk(0xBA0, 0x6F, 0);
    }
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
        if (save->Items[i].item_id >= 0 && save->Items[i].count > 0)
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

void Event::clearTalkBox()
{
    talk_box_up_->setContent("");
    talk_box_down_->setContent("");
}

//50��չָ��
//��Ȼ��һ���̶ȵ�֧�֣������ⲻ��ʾ�Ƽ�ʹ��
void Event::instruct_50e(int code, int e1, int e2, int e3, int e4, int e5, int e6, int* code_ptr)
{
    int index = 0, len = 0, offset = 0;
    char* char_ptr = nullptr, *char_ptr1 = nullptr;
    int* save_int_ptr = nullptr;
    int i1 = 0;
    int i2 = 0;
    std::string str;
    std::vector<std::string> strs;

    auto save = Save::getInstance();

    switch (code)
    {
    case 0: //��ֵ
        x50[e1] = e2;
        break;
    case 1: //���鸳ֵ��e2��Ϊ0��ʾ��Ҫһ���ֽ�
        index = e3 + e_GetValue(0, e1, e4);
        x50[index] = e_GetValue(1, e1, e4);
        if (e2) { x50[index] = x50[index] & 0xff; }
        break;
    case 2: //����ȡֵ
        index = e3 + e_GetValue(0, e1, e4);
        x50[e5] = x50[index];
        if (e2) { x50[index] = x50[index] & 0xff; }
        break;
    case 3: //��������
        index = e_GetValue(0, e1, e5);
        switch (e2)
        {
        case 0:
            x50[e3] = x50[e4] + index;
            break;
        case 1:
            x50[e3] = x50[e4] - index;
            break;
        case 2:
            x50[e3] = x50[e4] * index;
            break;
        case 3:
            if (index != 0) { x50[e3] = x50[e4] / index; }
            break;
        case 4:
            if (index != 0) { x50[e3] = x50[e4] % index; }
            break;
        case 5:
            if (index != 0) { x50[e3] = uint32_t(x50[e4]) / index; }
            break;
        }
        break;
    case 4: //�жϱ�������д��ת���
        x50[0x7000] = 0;
        index = e_GetValue(0, e1, e4);
        switch (e2)
        {
        case 0:
            if (!(x50[e3] < index)) { x50[0x7000] = 1; }
            break;
        case 1:
            if (!(x50[e3] <= index)) { x50[0x7000] = 1; }
            break;
        case 2:
            if (!(x50[e3] == index)) { x50[0x7000] = 1; }
            break;
        case 3:
            if (!(x50[e3] != index)) { x50[0x7000] = 1; }
            break;
        case 4:
            if (!(x50[e3] >= index)) { x50[0x7000] = 1; }
            break;
        case 5:
            if (!(x50[e3] > index)) { x50[0x7000] = 1; }
            break;
        case 6:
            x50[0x7000] = 0;
            break;
        case 7:
            x50[0x7000] = 1;
        }
        break;
    case 5: //ȫ������
        memset(x50, 0, sizeof(x50));
        break;
    case 6: break;
    case 7: break;
    case 8: //���Ի�
        index = e_GetValue(0, e1, e2);
        char_ptr = (char*)&x50[e3];
        sprintf(char_ptr, "%s", talk_contents_[index].c_str());
        break;
    case 9: //��ʽ���ִ�
        e4 = e_GetValue(0, e1, e4);
        char_ptr = (char*)&x50[e2];
        char_ptr1 = (char*)&x50[e3];
        sprintf(char_ptr, char_ptr1, e4);
        break;
    case 10: //�ִ�����
        x50[e2] = strlen((char*)&x50[e1]);  //�о����������⣬������
        break;
    case 11: //�ϲ��ִ�
        char_ptr = (char*)&x50[e1];
        char_ptr1 = (char*)&x50[e2];
        sprintf(char_ptr, "%s%s", char_ptr, char_ptr1);
        break;
    case 12:  //����һ���ǿո���ִ�
        //Note: here the width of one 'space' is the same as one Chinese character.
        e3 = e_GetValue(0, e1, e3);
        char_ptr = (char*)&x50[e2];
        for (int i = 0; i < e3 / 2; i++)
        {
            *char_ptr = char(0x20);
            char_ptr++;
        }
        *char_ptr = char(0);
        break;
    case 13: break;
    case 14: break;
    case 15: break;
    case 16: //д�浵����
        e3 = e_GetValue(0, e1, e3);
        e4 = e_GetValue(1, e1, e4);
        e5 = e_GetValue(2, e1, e5);
        save_int_ptr = nullptr;
        switch (e2)
        {
        case 0: save_int_ptr = (int*)((char*)(save->getRole(e3)) + e4); break;
        case 1: save_int_ptr = (int*)((char*)(save->getItem(e3)) + e4); break;
        case 2: save_int_ptr = (int*)((char*)(save->getSubMapInfo(e3)) + e4); break;
        case 3: save_int_ptr = (int*)((char*)(save->getMagic(e3)) + e4); break;
        case 4: save_int_ptr = (int*)((char*)(save->getShop(e3)) + e4); break;
        }
        if (save_int_ptr) { *save_int_ptr = e5; }
        break;
    case 17: //���浵����
        e3 = e_GetValue(0, e1, e3);
        e4 = e_GetValue(1, e1, e4);
        switch (e2)
        {
        case 0: x50[e5] = *(int*)((char*)(save->getRole(e3)) + e4); break;
        case 1: x50[e5] = *(int*)((char*)(save->getItem(e3)) + e4); break;
        case 2: x50[e5] = *(int*)((char*)(save->getSubMapInfo(e3)) + e4); break;
        case 3: x50[e5] = *(int*)((char*)(save->getMagic(e3)) + e4); break;
        case 4: x50[e5] = *(int*)((char*)(save->getShop(e3)) + e4); break;
        }
        break;
    case 18: //д��������
        e2 = e_GetValue(0, e1, e2);
        e3 = e_GetValue(1, e1, e3);
        save->Team[e2] = e3;
        break;
    case 19: //����������
        e2 = e_GetValue(0, e1, e2);
        x50[e3] = save->Team[e2];
        break;
    case 20:  //��ȡ��Ʒ����
        e2 = e_GetValue(0, e1, e2);
        x50[e3] = save->getItemCountInBag(e2);
        break;
    case 21: //д�����¼�����
        e2 = e_GetValue(0, e1, e2);
        e3 = e_GetValue(1, e1, e3);
        e4 = e_GetValue(2, e1, e4);
        e5 = e_GetValue(3, e1, e5);
        *(MAP_INT*)(save->getSubMapInfo(e2)->Event(e3) + e4) = e5;
        break;
    case 22: //�������¼�����
        e2 = e_GetValue(0, e1, e2);
        e3 = e_GetValue(1, e1, e3);
        e4 = e_GetValue(2, e1, e4);
        x50[e5] = *(MAP_INT*)(save->getSubMapInfo(e2)->Event(e3) + e4);
        break;
    case 23:
        e2 = e_GetValue(0, e1, e2);
        e3 = e_GetValue(1, e1, e3);
        e4 = e_GetValue(2, e1, e4);
        e5 = e_GetValue(3, e1, e5);
        e6 = e_GetValue(4, e1, e6);
        save->getSubMapInfo(e2)->LayerData(e3, e4, e5) = e6;
        break;
    case 24:
        e2 = e_GetValue(0, e1, e2);
        e3 = e_GetValue(1, e1, e3);
        e4 = e_GetValue(2, e1, e4);
        e5 = e_GetValue(3, e1, e5);
        x50[e6] = save->getSubMapInfo(e2)->LayerData(e3, e4, e5);
        break;
    case 25: //ǿ��д���ڴ��ַ����Ҫ�ˣ��Լ����Ű�
        e5 = e_GetValue(0, e1, e5);
        e6 = e_GetValue(1, e1, e6);
        break;
    case 26: //���ڴ��ַ��ͬ��
        e6 = e_GetValue(0, e1, e6);
        break;
    case 27: //�����ֵ�����
        e3 = e_GetValue(0, e1, e3);
        char_ptr = (char*)&x50[e4];
        switch (e2)
        {
        case 0: sprintf(char_ptr, "%s", save->getRole(e3)->Name); break;
        case 1: sprintf(char_ptr, "%s", save->getItem(e3)->Name); break;
        case 2: sprintf(char_ptr, "%s", save->getSubMapInfo(e3)->Name); break;
        case 3: sprintf(char_ptr, "%s", save->getMagic(e3)->Name); break;
        }
        break;
    case 28: //28~31Ϊս��ָ���Ҫ��
        break;
    case 29: break;
    case 30: break;
    case 31: break;
    case 32: //�޸���һ��ָ��
        e3 = e_GetValue(0, e1, e3);
        *(code_ptr + e3) = x50[e2];
        break;
    case 33: //��һ���ִ�
        e3 = e_GetValue(0, e1, e3);
        e4 = e_GetValue(1, e1, e4);
        e5 = e_GetValue(2, e1, e5);
        char_ptr = (char*)&x50[e2];
        Font::getInstance()->draw(char_ptr, 20, e3, e4/*BP_Color(e5)*/);
        break;
    case 34: //��һ��������
        e2 = e_GetValue(0, e1, e2);
        e3 = e_GetValue(1, e1, e3);
        e4 = e_GetValue(2, e1, e4);
        e5 = e_GetValue(3, e1, e5);
        Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, e2, e3, e4, e5);
        break;
    case 35: //��ͣ�ȴ�����
        text_box_->setText("");
        text_box_->setTexture("", 0);
        x50[e1] = text_box_->run();
        switch (x50[e1])
        {
        case SDLK_LEFT: x50[e1] = 154; break;
        case SDLK_RIGHT: x50[e1] = 156; break;
        case SDLK_UP: x50[e1] = 158; break;
        case SDLK_DOWN: x50[e1] = 152; break;
        }
        break;
    case 36: //������������ִ��ȴ�������������µ���y������תָʾΪ0
        e3 = e_GetValue(0, e1, e3);
        e4 = e_GetValue(1, e1, e4);
        e5 = e_GetValue(2, e1, e5);
        char_ptr = (char*)&x50[e2];
        talk_box_up_->setContent(char_ptr);
        talk_box_up_->setHeadID(-1);
        talk_box_up_->setHeadStyle(2);
        x50[0x7000] = talk_box_up_->run();
        break;
    case 37: //��ʱ
        Engine::getInstance()->delay(e2 = e_GetValue(0, e1, e2));
        break;
    case 38: //�����
        e2 = e_GetValue(0, e1, e2);
        x50[e3] = rand_.rand_int(e2);
        break;
    case 39:
    case 40: //�˵�
	{
		e2 = e_GetValue(0, e1, e2);
		e5 = e_GetValue(1, e1, e5);
		e6 = e_GetValue(2, e1, e6);
        MenuText menu;
		for (int i = 0; i < e2 - 1; i++)
		{
			strs.push_back((char*)x50[x50[e3 + i]]);
		}
		menu.setStrings(strs);
		x50[e4] = menu.run();
		break;
	}
    case 41: //��һ��ͼ
        e3 = e_GetValue(0, e1, e3);
        e4 = e_GetValue(1, e1, e4);
        e5 = e_GetValue(2, e1, e5);
        switch (e2)
        {
        case 0: TextureManager::getInstance()->renderTexture("mmap", e5, e3, e4); break;
        case 1: TextureManager::getInstance()->renderTexture("head", e5, e3, e4); break;
        }
        break;
    case 42: //�ı�����ͼ����
        e2 = e_GetValue(0, e1, e2);
        e3 = e_GetValue(0, e1, e3);
        MainScene::getIntance()->setManPosition(e2, e3);
        break;
    case 43: //���������¼�
        e2 = e_GetValue(0, e1, e2);
        e3 = e_GetValue(1, e1, e3);
        e4 = e_GetValue(2, e1, e4);
        e5 = e_GetValue(3, e1, e5);
        e6 = e_GetValue(4, e1, e6);
        x50[0x7100] = e3;
        x50[0x77101] = e4;
        x50[0x77102] = e5;
        x50[0x77103] = e6;
        callEvent(e2);
        break;
    case 44: //44~47Ϊս��ָ���Ҫ��
        break;
    case 45: break;
    case 46:
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
    case 47: break;
    case 48: //�Լ����԰ɣ����ù�
        for (int i = e1; i < e1 + e2 - 1; i++)
        {
            printf("x50[%d]=%d\n", i, x50[i]);
        }
        break;
    case 49: break;
    case 50: //�������֣�ɾ��
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
        str = "Ոݔ�����֣�";
        char_ptr1 = (char*)&str[1];
        break;
    case 51: break;
    case 52: //�ж�ĳ���Ƿ�������ĳ��ѧ�ȼ�
        e2 = e_GetValue(0, e1, e2);
        e3 = e_GetValue(1, e1, e3);
        e4 = e_GetValue(2, e1, e4);
        x50[0x7000] = 1;
        if (save->getRole(e2)->getMagicLevelIndex(e3) + 1 >= e4) { x50[0x7000] = 0; }
        break;
    case 53: break;
    case 54: break;
    case 55: break;
    case 56: break;
    case 57: break;
    case 58: break;
    case 59: break;
    case 60: break;
    default: break;
    }
}

