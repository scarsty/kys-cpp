#pragma once

#include "cocos2d.h"
#include "Common.h"
#include "cocostudio/CocoStudio.h"
#include "cocos-ext.h"
#include "ui/CocosGUI.h"

USING_NS_CC;

class Loading : public CommonScene
{
public:
	static cocos2d::Scene* createScene();
	virtual bool init();
	int count = 0; //loading¼ÆÊýÆ÷
	void Loading::update(float time);
	CREATE_FUNC(Loading);
};


