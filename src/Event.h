#pragma once
#include "Element.h"
#include "Menu.h"
#include "Random.h"
#include "SubScene.h"
#include "Talk.h"

//event_id��ʾ��kdef�еı�ţ�event_index��ʾ�ڳ����еı��

class Event
{
private:
    Event();
    virtual ~Event();

public:
    static Event* getInstance()
    {
        static Event e;
        return &e;
    }

private:
    std::vector<int> offset, length;
    std::vector<std::string> talk_contents_;
    std::vector<std::vector<int>> kdef_;

    int leave_event_0_;
    std::vector<int> leave_event_id_;

    //�����Ի���������������棬��������ͬʱ��ʾ
    //����Ҫ�����Ӹ���
    Element* talk_box_;
    Talk* talk_box_up_ = nullptr;
    Talk* talk_box_down_ = nullptr;

    //ר������ʾȷ�Ϻ�ȡ��ѡ��
    MenuText* menu2_ = nullptr;
    //ר������ʾһ���ı���
    TextBox* text_box_ = nullptr;
    int event_id_ = -1;

    RandomDouble rand_;

public:
    bool loadEventData();    //�����¼�����
    //���������
    bool callEvent(int event_id, Element* subscene = nullptr, int supmap_id = -1, int item_id = -1, int event_index = -1, int x = -1, int y = -1);    //����ָ�������д����

private:
    SubScene* subscene_;
    int submap_id_;
    int x_, y_;
    int event_index_;
    int item_id_;
    Item* item_;
    //Save* save_;
    bool loop_;
    int use_script_ = 0;

private:
    SubMapInfo* getSubMapRecordFromID(int submap_id);

public:
    int getLeaveEvent(Role* role);
    void callLeaveEvent(Role* role);
    void forceExit() { loop_ = false; }
    void setUseScript(int u);

public:
    //���´󲿷ֲ���Ϊint����ע����Ϸ������ʹ�õ���int16_t���н���Ч�ʵĿ���
    //void clear() {}
    void oldTalk(int talk_id, int head_id, int style);
    void addItem(int item_id, int count);
    void modifyEvent(int submap_id, int event_index, int cannotWalk, int Num, int Event1, int Event2, int Event3,
        int BeginPic1, int EndPic, int BeginPic2, int PicDelay, int x, int y);
    bool isUsingItem(int item_id);
    bool askBattle();
    bool tryBattle(int battle_id, int get_exp);
    void changeMainMapMusic(int music_id);
    bool askJoin();
    void join(int role_id);
    bool askRest();
    void rest();
    void lightScence();
    void darkScence();
    void dead();
    bool inTeam(int role_id);
    void setSubMapLayerData(int submap_id, int layer, int x, int y, int v);
    bool haveItemBool(int item_id);
    void oldSetScencePosition(int x, int y);
    bool teamIsFull();
    void leaveTeam(int role_id);
    void zeroAllMP();
    void setRoleUsePoison(int role_id, int v);
    //void blank() {}
    void subMapViewFromTo(int x0, int y0, int x1, int y1);
    void add3EventNum(int submap_id, int event_index, int v1, int v2, int v3);
    void playAnimation(int event_index, int begin_pic, int end_pic);
    bool checkRoleMorality(int role_id, int low, int high);
    bool checkRoleAttack(int role_id, int low, int high);
    void walkFromTo(int x0, int y0, int x1, int y1);
    bool checkEnoughMoney(int money_count);
    void addItemWithoutHint(int item_id, int count);
    void oldLearnMagic(int role_id, int magic_id, int no_display);
    void addIQ(int role_id, int value);
    void setRoleMagic(int role_id, int magic_index_role, int magic_id, int level);
    bool checkRoleSexual(int sexual);
    void addMorality(int value);
    void changeSubMapPic(int submap_id, int layer, int old_pic, int new_pic);
    void openSubMap(int submap_id);
    void setTowards(int towards);
    void roleAddItem(int role_id, int item_id, int count);
    bool checkFemaleInTeam();
    void play2Amination(int event_index1, int begin_pic1, int end_pic1, int event_index2, int begin_pic2, int end_pic2);
    void play3Amination(int event_index1, int begin_pic1, int end_pic1, int event_index2, int begin_pic2, int event_index3, int begin_pic3);
    void addSpeed(int role_id, int value);
    void addMaxMP(int role_id, int value);
    void addAttack(int role_id, int value);
    void addMaxHP(int role_id, int value);
    void setMPType(int role_id, int value);
    bool checkHave5Item(int item_id1, int item_id2, int item_id3, int item_id4, int item_id5);
    void askSoftStar();
    void showMorality();
    void showFame();
    void openAllSubMap();
    bool checkEventID(int event_index, int value);
    void addFame(int value);
    void breakStoneGate();
    void fightForTop();
    void allLeave();
    bool checkSubMapPic(int submap_id, int event_index, int pic);
    bool check14BooksPlaced();
    void backHome(int event_index1, int begin_pic1, int end_pic1, int event_index2, int begin_pic2, int end_pic2);
    void setSexual(int role_id, int value);
    void shop();
    void playMusic(int music_id);
    void playWave(int wave_id);

    void arrangeBag();
    void clearTalkBox();
    void blank() {}

private:
    int x50[0x10000];

public:
    int e_GetValue(int bit, int t, int x)
    {
        int i = t & (1 << bit);
        if (i == 0)
        {
            return x;
        }
        else
        {
            return x50[x];
        }
    }

    //��չ��50ָ�������һ��ָ���ָ�룬ĳһ����Ҫ
    void instruct_50e(int code, int e1, int e2, int e3, int e4, int e5, int e6, int* code_ptr = nullptr);
};
