#pragma once

#include "cocos2d.h"
#include "Common.h"
#include "cocostudio/CocoStudio.h"
#include "cocos-ext.h"
#include "ui/CocosGUI.h"

USING_NS_CC;

class RandomAttribute : public CommonScene
{
public:
	static cocos2d::Scene* createScene();
	virtual bool init();
	bool result = false;
	void RandomAttribute::ShowRandomAttribute();
	void chi_words(int x, int y);
	CREATE_FUNC(RandomAttribute);
	ui::Widget* test;
	int genggunum=0;
	void RandomAttribute::touchButton(Ref* pSender, cocos2d::ui::TouchEventType eventType);
};

