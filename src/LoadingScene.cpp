#include "LoadingScene.h"
#include "MainMap.h"
#include "SubScene.h"

USING_NS_CC;
using namespace cocostudio;
using namespace ui;
using namespace std;

Scene* Loading::createScene()
{
	auto scene = Scene::create();
	auto layer = Loading::create();
	scene->addChild(layer);
	return scene;
}

bool Loading::init()
{

	if (!Layer::init())
	{
		return false;
	}
	auto pNode = cocostudio::GUIReader::getInstance()->widgetFromJsonFile("ui/StartUifinal2/StartUifinal_1.ExportJson");
	this->addChild(pNode);
	count = 0;
	LoadingBar * loading = LoadingBar::create("sliderProgress.png");
	loading->setPosition(Vec2(520, 70));
	this->addChild(loading);
	loading->setTag(110);
	this->schedule(schedule_selector(Loading::update), 0.1);
	
	return true;
}


void Loading::update(float time)
{
	log("loading.");
	LoadingBar * load = dynamic_cast<LoadingBar*>(this->getChildByTag(110));
	count = count + 10;
	if (count >= 100)
	{
		Save::getInstance()->LoadR(1);
		auto Map = MainMap::createScene();
		Director::getInstance()->replaceScene(Map);
		this->unschedule(schedule_selector(Loading::update));
	}
	load->setPercent(count);
}