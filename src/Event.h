#pragma once
#include "Base.h"
#include "SubMap.h"
#include "Talk.h"
#include "Menu.h"

class Event : Base
{
private:
    Event();
    virtual ~Event();
    static Event event_;
public:
    static Event* getInstance()
    {
        return &event_;
    }
private:
    std::vector<int> offset, length;
    std::vector<std::string> talk_;
    std::vector<std::vector<int>> kdef_;

    //两个对话，用于上面和下面，两个可以同时显示
    //视需要可增加更多
    Talk* talkup_ = nullptr;
    Talk* talkdown_ = nullptr;

    //专用于显示确认和取消选项
    MenuText* menu2_ = nullptr;
    TextBox* text_box_ = nullptr;
    int event_id_ = -1;

public:
    bool loadEventData();             //加载事件数据
    //这里再设计
    bool callEvent(int event_id, Base* submap = nullptr, int supmap_id = -1, int item_id = -1, int event_index = -1, int x = -1, int y = -1);     //调用指令的内容写这里

private:
    SubMap* submap_;
    SubMapRecord* submap_record_;
    int submap_id_;
    int x_, y_;
    int event_index_;
    int item_id_;
    Item* item_;
    Save* save_;

public:
    //以下大部分参数为int，请注意游戏数据中使用的是int16_t，有降低效率的可能
    //void clear() {}
    void oldTalk(int talk_id, int head_id, int style);
    void getItem(int item_id, int count);
    void modifyEvent(int submap_id, int event_index, int cannotWalk, int Num, int Event1, int Event2, int Event3,
        int BeginPic1, int EndPic, int BeginPic2, int PicDelay, int x, int y);
    bool isUsingItem(int item_id);
    bool askBattle();
    bool tryBattle(int battle_id, int get_exp) { return true; }
    void changeMainMapMusic(int musixc_id) {}
    bool askJoin() { return false; }
    void join(int role_id) {}
    bool askRest();
    void rest() {}
    void lightScence() {}
    void darkScence() {}
    void dead() {}
    bool inTeam(int role_id);
    void setSubMapMapData(int submap_id, int layer, int x, int y, int v) {}
    bool haveItemBool(int item_id);
    void oldSetScencePosition(int x, int y) {}
    bool teamIsFull();
    void leaveTeam(int tole_id) {}
    void zeroAllMP();
    void setOneUsePoi(int role_id, int v);
    void blank() {}
    void submapFromTo(int x0, int y0, int x1, int y1) {}
    void add3EventNum(int submap_id, int event_index, int v1, int v2, int v3) {}
    void playAnimation(int event_id, int begin_pic, int end_pic) {}
    bool judgeEthics(int role_id, int low, int high) { return false; }
    bool judgeAttack(int role_id, int low, int high) { return false; }
    void walkFromTo(int x0, int y0, int x1, int y1);
    bool judgeMoney(int money_count) { return false; }
    void getItemWithoutHint(int item_id, int count);
    void oldLearnMagic(int role_id, int magic_id, int no_display) {}
    void addAptitude(int role_id, int aptitude) {}
    void setOneMagic(int role_id, int magic_index_role, int magic_id, int level) {}
    bool judgeSexual(int sexual) { return false; }
    void addEthics(int ethics) {}
    void changeScencePic(int submap_id, int layer, int old_pic, int new_pic) {}
    void openScence(int submap_id);
    void setTowards(int towards);
    void roleGetItem(int role_id, int item_id, int count) {}
    bool judgeFemaleInTeam();
    //void haveItemBool() {}
    void play2Amination(int event_index1, int begin_pic1, int end_pic1, int event_index2, int begin_pic2, int end_pic2) {}
    void addSpeed(int role_id, int value) {}
    void addMP(int role_id, int value) {}
    void addAttack(int role_id, int value) {}
    void addHP(int role_id, int value) {}
    void setMPPro(int role_id, int value) {}
    bool judge5Item(int item_id1, int item_id2, int item_id3, int item_id4, int item_id5) { return false; }
    void askSoftStar() {}
    void showEthics() {}
    void showRepute() {}
    void openAllScence() {}
    bool judgeEventNum(int event_index, int value) { return false; }
    void addRepute(int value) {}
    void breakStoneGate() {}
    void fightForTop() {}
    void allLeave();
    bool judgeSubMapPic(int submap_id, int event_index, int pic) { return false; }
    bool judge14BooksPlaced() { return true; }
    void backHome() {}
    void setSexual(int role_id, int value) {}
    void weiShop() {}
    void playMusic(int music_id) {}
    void playWave(int wave_id) {}

private:
    int16_t x50[0x10000];
public:
    int e_GetValue(int bit, int t, int x)
    {
        int i = t & (1 << bit);
        if (i == 0)
        { return x; }
        else
        { return x50[x]; }
    }
    int instruct_50e(int code, int e1, int e2, int e3, int e4, int e5, int e6) { return 8; }
};

