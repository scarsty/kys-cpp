#pragma once
#include "save.h"
#include "Head.h"
#include <direct.h>
#include <corecrt_io.h>
#include <functional>

//#include "cocos2d.h"
//USING_NS_CC;
using namespace std;

typedef function<int(int)> EventFunc;

class Operation
{
public:
    short num;
    vector <short> par;
};



//内部载入指令的数据流，传入值为当前指令的指针，返回值为指针需要移动的值，用宏简化代码
#define EVENT_FUNC(name) int name(int p)
class EventInstruct  //各种指令的实现写这里
{
public:
    static vector<EventFunc> funcs;
public:
    EventInstruct();
#define EVENT_FUNCS \
    EVENT_FUNC(clear_screen);\
    EVENT_FUNC(talk);\
	EVENT_FUNC(isZhangMen);

    EVENT_FUNCS;

    void XXX();
};
#undef EVENT_FUNC

class EventData
{
public:
    const vector<Operation>* getOperation();
    void setId(int num);
    bool checkId(int num);
    void arrByOperation(unsigned char* Data, int len);          //按指令进行整理数据;以后与事件组织相关的内容就以这个为基础
    int getOperationLen(int num);   //获取指令数据长度;

private:
    int _id;
    vector<Operation>_operations;
};

class EventManager
{
public:
    EventManager();
    virtual ~EventManager();
    static EventManager eventManager;
    static EventManager* getInstance()
    {
        return &eventManager;
    }
    bool initEventData();       //加载事件数据


    bool callEvent(int num);    //调用指令的内容写这里
    std::vector<EventData> eventData;

    int eventCount = 0;
};






