#include "RandomAttribute.h"
#include "MainMap.h"
#include "SubScene.h"

USING_NS_CC;
using namespace cocostudio;
using namespace ui;
using namespace std;

Scene* RandomAttribute::createScene()
{
	auto scene = Scene::create();
	auto layer = RandomAttribute::create();
	scene->addChild(layer, 0);
	return scene;
}



bool RandomAttribute::init()
{
	if (!Layer::init())
	{
		return false;
	}

	test = cocostudio::GUIReader::getInstance()->widgetFromJsonFile("ui/StartUifinal3/StartUifinal_1.ExportJson");
	this->addChild(test, 0);
	auto button1 = (Button *)Helper::seekWidgetByTag(test, 312);
	auto button3 = (Button *)Helper::seekWidgetByTag(test, 317);
	auto button4 = (Button *)Helper::seekWidgetByTag(test, 319);
	button1->addTouchEventListener(this, toucheventselector(RandomAttribute::touchButton));
	button3->addTouchEventListener(this, toucheventselector(RandomAttribute::touchButton));
	button4->addTouchEventListener(this, toucheventselector(RandomAttribute::touchButton));
	button3->setVisible(false);
	button4->setVisible(false);
	return true;
}

void RandomAttribute::touchButton(Ref* pSender, cocos2d::ui::TouchEventType eventType)
{
	auto button = dynamic_cast<Button*>(pSender);
	int tag = button->getTag();
	switch (eventType)
	{
	case TouchEventType::TOUCH_EVENT_ENDED:
			if (tag == 312)
			{
				
				ShowRandomAttribute();
				break;
			}
			
			if (tag == 319)
			{
				auto genggu = (Text*)Helper::seekWidgetByTag(test, 190);
				genggunum = genggunum + 1;
				genggu->setText(to_string(genggunum));
				break;
			}

			if (tag == 317)
			{
				auto genggu = (Text*)Helper::seekWidgetByTag(test, 190);
				genggunum = genggunum - 1;
				genggu->setText(to_string(genggunum));
				break;
			}

	}
}
void RandomAttribute::ShowRandomAttribute()
{
	int maxnum = 100;
	int inhnum = 6;
	int lifnum = 75;
	string str = "xiaowu";
	auto rolname = (Text*)Helper::seekWidgetByTag(test, 30);
	string contentu = CommonScene::a2u(str.c_str());
	rolname->setText(contentu);
	timeval psv;
	gettimeofday(&psv, NULL);
	unsigned long int rand_seed = psv.tv_sec * 1000 + psv.tv_usec / 1000;
	srand(rand_seed);
	int range = lifnum + 1;
	int tmp = rand() % range + 50;
	auto attack = (Text*)Helper::seekWidgetByTag(test, 70);
	attack->setText(to_string(tmp));
}
