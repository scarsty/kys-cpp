#pragma once

#include "Common.h"

class Start : public CommonScene

{
public:
	static cocos2d::Scene* createScene();
	virtual bool init();
	void touchButton(Ref* pSender, cocos2d::ui::TouchEventType eventType);
	CREATE_FUNC(Start);
};
