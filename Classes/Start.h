#pragma once

#include "cocos2d.h"
#include "Common.h"


USING_NS_CC;
using namespace std;

class Start : public CommonScene

{
public:
	static cocos2d::Scene* createScene();
	virtual bool init();
	void touchButton(Ref* pSender, cocos2d::ui::TouchEventType eventType);
	CREATE_FUNC(Start);
};
