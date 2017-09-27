#pragma once
#include "Base.h"
#include "SubMap.h"
#include "Talk.h"

class Event : public Base
{
public:
    Event();
    virtual ~Event();
    static Event* getInstance()
    {
        return &event_;
    }

private:
    std::vector<int> offset, length;
    std::vector<std::string> talk_;
    std::vector<std::vector<int>> kdef_;

    //两个对话，用于上面和下面
    Talk* talkup_ = nullptr;
    Talk* talkdown_ = nullptr;

    int event_id_ = -1;

public:
    bool initEventData();             //加载事件数据
    //这里再设计
    bool callEvent(int event_id);     //调用指令的内容写这里

    void setEvent(SubMap* submap, int x, int y, Item* item = nullptr)
    {
        submap_ = submap;
        x_ = x;
        y_ = y;
        item_ = item;
    }

private:
    static Event event_;
    SubMap* submap_;
    int x_, y_;
    //int event_index_submap_;
    Item* item_;

public:
    //以下大部分参数为int，请注意游戏数据中使用的是int16_t，有降低效率的可能
    //void clear() {}
    void oldTalk(int talk_id, int head_id, int style);
    void getItem(int item_id, int count) {}
    void modifyEvent(int snum, int ednum, int cannotWalk, int Num, int Event1, int Event2, int Event3,
        int BeginPic1, int EndPic, int BeginPic2, int  PicDelay, int  XPos, int  YPos) {}
    void useItem() {}
    bool askBattle() { return false; }
    bool tryBattle() { return false; }
    void changeMMapMusic() {}
    bool askJoin() { return false; }
    void join() {}
    bool askRest() { return false; }
    void rest() {}
    void lightScence() {}
    void darkScence() {}
    void dead() {}
    bool inTeam() { return false; }
    void oldSetScenceMapPro() {}
    bool haveItemBool() { return false; }
    void oldSetScencePosition() {}
    bool teamIsFull() { return false; }
    void leaveTeam() {}
    void zeroAllMP() {}
    void setOneUsePoi() {}
    void blank() {}
    void scenceFromTo() {}
    void add3EventNum() {}
    void playAnimation() {}
    bool judgeEthics() { return false; }
    bool judgeAttack() { return false; }
    void walkFromTo() {}
    bool judgeMoney() { return false; }
    void addItem() {}
    void oldLearnMagic() {}
    void addAptitude() {}
    void setOneMagic() {}
    bool judgeSexual() { return false; }
    void addEthics() {}
    void changeScencePic() {}
    void openScence() {}
    void setRoleFace() {}
    void anotherGetItem() {}
    bool judgeFemaleInTeam() { return false; }
    //void haveItemBool() {}
    void play2Amination() {}
    void addSpeed() {}
    void addMP() {}
    void addAttack() {}
    void addHP() {}
    void setMPPro() {}
    bool judge5Item() { return false; }
    void askSoftStar() {}
    void showEthics() {}
    void showRepute() {}
    void openAllScence() {}
    bool judgeEventNum() { return false; }
    void addRepute() {}
    void breakStoneGate() {}
    void fightForTop() {}
    void allLeave() {}
    bool judgeScencePic() { return false; }
    bool judge14BooksPlaced() { return false; }
    void backHome() {}
    void setSexual() {}
    void weiShop() {}
    void playMusic() {}
    void playWave() {}
};

