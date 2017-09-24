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
    void getItem_2(short tnum, short amount, short rnum = 0);
    void getJueSe(short* rnum);
    void editEvent3(short snum, short ednum, short CanWalk, short Num, short Event1, short Event2, short Event3, short BeginPic1, short EndPic, short BeginPic2, short  PicDelay, short  XPos, short  YPos, short StartTime, short  Duration, short Interval, short  DoneTime, short  IsActive, short  OutTime, short  OutEvent);
    int judgeItem_4(short inum, short jump1, short jump2);
    int isFight_5(short jump1, short jump2);

    int isAdd_8(short jump1, short jump2);
    void StudyMagic(int rnum, int magicnum, int newmagicnum, int level, int dismode);
    void JumpScene(int snum, int x, int y);

private:
    static Event event_;
    static int NowEenWu;
    static bool isTry;
    static bool TryGo;
    static int TryEventTmpI;
    static int EventEndCount;

};






