#include "Start.h"
#include "MainMap.h"
#include "Save.h"
#include "SubScene.h"
#include "LoadingScene.h"


USING_NS_CC;
using namespace std;
using namespace cocostudio;
using namespace ui;

Scene* Start::createScene()
{
	auto scene = Scene::create();
	auto layer = Start::create();
	scene->addChild(layer, 0);
	return scene;
}

bool Start::init()
{
	if (!Layer::init())
	{
		return false;
	}
	auto pNode = cocostudio::GUIReader::getInstance()->widgetFromJsonFile("ui/StartUifinal1/StartUifinal_1.ExportJson");
	auto button4 = (ui::Button *)ui::Helper::seekWidgetByName(pNode, "Button4");
	auto button5 = (ui::Button *)ui::Helper::seekWidgetByName(pNode, "Button5");
	auto button6 = (ui::Button *)ui::Helper::seekWidgetByName(pNode, "Button6");
	auto button7 = (ui::Button *)ui::Helper::seekWidgetByName(pNode, "Button7");
	auto button8 = (ui::Button *)ui::Helper::seekWidgetByName(pNode, "Button8");
	auto button9 = (ui::Button *)ui::Helper::seekWidgetByName(pNode, "Button9");
	button4->addTouchEventListener(this, toucheventselector(Start::touchButton));
	button5->addTouchEventListener(this, toucheventselector(Start::touchButton));
	button6->addTouchEventListener(this, toucheventselector(Start::touchButton));
	button7->addTouchEventListener(this, toucheventselector(Start::touchButton));
	button8->addTouchEventListener(this, toucheventselector(Start::touchButton));
	button9->addTouchEventListener(this, toucheventselector(Start::touchButton));
	this->addChild(pNode);
	return true;
}

void Start::touchButton(Ref* pSender, cocos2d::ui::TouchEventType eventType)
{
	auto button = dynamic_cast<Button*>(pSender);
	int tag = button->getTag();
	switch (eventType)
	{
	case TouchEventType::TOUCH_EVENT_ENDED:
		if (tag == 14)
		{
			Save::getInstance()->LoadR(0);
			auto Map = MainMap::createScene();
			Director::getInstance()->replaceScene(Map);
			break;
		}
		else if (tag == 16)
		{
			Save::getInstance()->LoadR(1);
			auto Map = MainMap::createScene();
			Director::getInstance()->replaceScene(Map);
			break;
		}
		else if (tag == 17)
		{
			Save::getInstance()->LoadR(2);
			auto Map = MainMap::createScene();
			Director::getInstance()->replaceScene(Map);
			break;
		}
		else if (tag == 18)
		{
			Save::getInstance()->LoadR(3);
			auto Map = MainMap::createScene();
			Director::getInstance()->replaceScene(Map);
			break;
		}
		else if (tag == 19)
		{
			Save::getInstance()->LoadR(4);
			auto Map = MainMap::createScene();
			Director::getInstance()->replaceScene(Map);
			break;
		}
		else if (tag == 20)
		{
			Save::getInstance()->LoadR(0);
			auto Map = MainMap::createScene();
			Director::getInstance()->replaceScene(Map);
			break;
		}

	}
}



