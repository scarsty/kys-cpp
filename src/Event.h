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

    //所有事件函数均应返回一个整数，为指令指针移动的数目
    void clear();  //0
    void talk_1();
    void getItem_2(int16_t tnum, int16_t amount, int16_t rnum = 0);
    void getJueSe(int16_t* rnum);
    void editEvent3(int16_t snum, int16_t ednum, int16_t CanWalk, int16_t Num, int16_t Event1, int16_t Event2, int16_t Event3, int16_t BeginPic1, int16_t EndPic, int16_t BeginPic2, int16_t  PicDelay, int16_t  XPos, int16_t  YPos, int16_t StartTime, int16_t  Duration, int16_t Interval, int16_t  DoneTime, int16_t  IsActive, int16_t  OutTime, int16_t  OutEvent);
    int judgeItem_4(int16_t inum, int16_t jump1, int16_t jump2);
    int isFight_5(int16_t jump1, int16_t jump2);

    int isAdd_8(int16_t jump1, int16_t jump2);
    void StudyMagic(int rnum, int magicnum, int newmagicnum, int level, int dismode);
    void JumpScene(int snum, int x, int y);

private:
    static Event event_;
    static int NowEenWu;
    static bool isTry;
    static bool TryGo;
    static int TryEventTmpI;
    static int EventEndCount;

public:
    void clear() {}
    void oldTalk(int16_t talk_id, int16_t head_id, int16_t style);
    void getItem(int16_t item_id, int16_t count);
    void modifyEvent(int16_t snum, int16_t ednum, int16_t CanWalk, int16_t Num, int16_t Event1, int16_t Event2, int16_t Event3, 
        int16_t BeginPic1, int16_t EndPic, int16_t BeginPic2, int16_t  PicDelay, int16_t  XPos, int16_t  YPos);
    void useItem();
    void askBattle();
    void tryBattle();
    void changeMMapMusic();
    void askJoin();
    void join();
    void askRest();
    void rest();
    void lightScence();
    void darkScence();
    void dead();
    void inTeam();
    void oldSetScenceMapPro();
    void haveItemBool();
    void oldSetScencePosition();
    void teamIsFull();
    void leaveTeam();
    void zeroAllMP();
    void setOneUsePoi();
    void blank();
    void scenceFromTo();
    void add3EventNum();
    void playAnimation();
    void judgeEthics();
    void judgeAttack();
    void walkFromTo();
    void judgeMoney();
    void addItem();
    void oldLearnMagic();
    void addAptitude();
    void setOneMagic();
    void judgeSexual();
    void addEthics();
    void changeScencePic();
    void openScence();
    void setRoleFace();
    void anotherGetItem();
    void judgeFemaleInTeam();
    void haveItemBool();
    void play2Amination();
    void addSpeed();
    void addMP();
    void addAttack();
    void addHP();
    void setMPPro();
    void judge5Item();
    void askSoftStar();
    void showEthics();
    void showRepute();
    void openAllScence();
    void judgeEventNum();
    void addRepute();
    void breakStoneGate();
    void fightForTop();
    void allLeave();
    void judgeScencePic();
    void judge14BooksPlaced();
    void backHome();
    void setSexual();
    void weiShop();
    void playMusic();
    void playWave();
};

