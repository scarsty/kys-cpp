#pragma once
#include "Base.h"

class Event : public Base
{
public:
    Event();
    virtual ~Event();
    static Event* getInstance()
    {
        return &event_;
    }

    std::vector<int> offset, length;

    std::vector<std::string> talk_;
    std::vector<std::vector<int16_t>> kdef_;

    int16_t* getEvent(int i);
    char* getTalk(int i);


    bool initEventData();       //加载事件数据
    bool callEvent(int num);    //调用指令的内容写这里
    int eventCount = 0;

private:
    static Event event_;
    static int NowEenWu;
    static bool isTry;
    static bool TryGo;
    static int TryEventTmpI;
    static int EventEndCount;

public:
    void clear() {}
    void oldTalk(int16_t talk_id, int16_t head_id, int16_t style) {}
    void getItem(int16_t item_id, int16_t count) {}
    void modifyEvent(int16_t snum, int16_t ednum, int16_t CanWalk, int16_t Num, int16_t Event1, int16_t Event2, int16_t Event3, 
        int16_t BeginPic1, int16_t EndPic, int16_t BeginPic2, int16_t  PicDelay, int16_t  XPos, int16_t  YPos) {}
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

