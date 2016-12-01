#pragma once
#include "save.h"
#include "Head.h"
#include <direct.h>
#include <corecrt_io.h>

//#include "cocos2d.h"
//USING_NS_CC;
using namespace std;

class Operation
{
public:
	short num;
	vector <short> par;
};
class EventInstruct  //各种指令的实现写这里
{
public:
	void XXX();
};
class EventData
{
public:
	const vector<Operation>* getOperation();
	void setId(int num);
	bool checkId(int num);
	void arrByOperation(unsigned char *Data, int len);			//按指令进行整理数据;以后与事件组织相关的内容就以这个为基础
	int getOperationLen(int num);	//获取指令数据长度;

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
	bool initEventData();		//加载事件数据


	bool callEvent(int num);	//调用指令的内容写这里
	std::vector<EventData> eventData;

	int eventCount = 0;
};






