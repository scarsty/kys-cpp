#pragma once

#include "Common.h"

class Loading : public CommonScene
{
public:
	static cocos2d::Scene* createScene();
	virtual bool init();
	int count = 0; //loading¼ÆÊýÆ÷
	void Loading::update(float time);
	CREATE_FUNC(Loading);
};


